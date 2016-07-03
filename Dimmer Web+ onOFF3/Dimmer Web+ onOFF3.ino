/*
Programa Dimmer Web+
Exemplo de como acionar MULTIPLAS lampadas dimerizadas via web,
utilizando os NanoShields Zero Cross, Triac e Ethernet.

Versao 3.4 - Cenario automatico
-Modo offline e online implementado.
Criado por Alex Monteiro Sartin - Circuitar Eletronicos

http://www.circuitar.com.br
*/

#include <SPI.h>
#include <Ethernet.h>
#include <Wire.h>
#include <EEPROM.h>
#include "Nanoshield_EEPROM.h"
#include "WebServer.h"
#include "TimerOne.h"
void handler  (WebServer &server, WebServer::ConnectionType type, char *, bool);
void handlerB (WebServer &server, WebServer::ConnectionType type, char *, bool);
void handlerS (WebServer &server, WebServer::ConnectionType type, char *, bool);
void handlerL (WebServer &server, WebServer::ConnectionType type, char *, bool);
void handlerN (WebServer &server, WebServer::ConnectionType type, char *, bool);
void getLegenda(byte numero);
void setLegenda(byte numero);

//Tamanho do vetor legenda
#define TAMANHO  10

//Numero max. de lampadas
#define N_MAX  10

//Portas do Triac
byte P_triac[N_MAX] = {};

//Endereco EEPROM dos Scenarios
#define ADDR_S  100

//Endereco EEPROM das portas do triac
#define ADDR_L  150

//Parametro do servidor
#define PARAM_LEN 15

//Delay para armazenar cenário
#define STORE_DELAY 3000

//Tempo de incremento da rampa de 0 a 160
#define RAMPA 155

//Tempo de espera do Timer1 em relacao ao Valor da potencia do slide de 0 a 99
short tempo[] ={
160, 146, 142, 138, 134, 132, 130, 128, 126, 124, 
122, 121, 120, 118, 117, 116, 114, 113, 112, 111, 
110, 109, 108, 107, 106, 105, 104, 103, 102, 101, 
100,  99,  98,  97,  96,  95,  94,  93,  92,  91,  
 90,  89,  88,  87,  86,  85,  84,  83,  82,  81,  
 80,  79,  78,  77,  76,  75,  74,  73,  72,  71,  
 70,  69,  68,  67,  66,  65,  64,  63,  62,  61,  
 60,  59,  58,  57,  56,  55,  54,  53,  52,  51,  
 50,  48,  47,  46,  45,  44,  42,  41,  40,  38,  
 36,  34,  32,  30,  28,  26,  24,  22,  18,  12, 6 };

// Pinos do zero cross
byte zc = 2; 
//numero de portas selecionadas
byte triacs;
//estado do botao liga-e-desliga
boolean slideState[sizeof(P_triac)];
//valor do dimmer
short slide[sizeof(P_triac)];
//Variavel auxiliar para rampa
short slide2[sizeof(P_triac)];
//Tempo de espera do triac
unsigned int tempo0;

//Parâmetros de rede
byte mac[6];
byte ip[]      = { 192, 168, 1, 150 };
byte gateway[] = { 192, 168, 1, 1 };
byte subnet[]  = { 255, 255, 255, 0 };
//Legenda
char legenda[TAMANHO];

// Interruptores simples
int toggleSwitches[] = { A0, A1, A3 };
int toggleSwitchesState[3];

// Interruptores momentâneos
int pushButtons[] = { 4, 7, 8 };
int pushButtonsState[3];
unsigned long pushButtonsTimer[3];

// Cenários
byte scenarios[3][3];
byte scenarioA[14][3]=
{{  0,    0,    0},
 {  0,    0,    0},
 {  0,    0,   30},
 {  0,    0,  100},
 {  0,   30,  100},
 {  0,  100,  100},
 { 30,  100,  100},
 {100,  100,  100},
 { 30,   30,   30},
 {  0,    0,    0},
 {100,    0,    0},
 {  0,  100,    0},
 {  0,    0,  100},
 {100,  100,  100}};
//Cenario automatico:
boolean Auto=false;
unsigned int tempo1=0;
byte count=0;
  
//Inicializacao da memoria EEPROM e do servidor web
Nanoshield_EEPROM eeprom(1, 1, 0, true);
WebServer webserver;

void setup() {
  Serial.begin(115200);
  while(!Serial){}
  delay(500);
  
  //contagem de portas inicialmente configuradas
  for(byte aux=0; aux<sizeof(P_triac); aux++)
    if(P_triac[aux]!=0)
      triacs++;
    
  if (triacs>0)  //Se foi setado o vetor P_triac inicialmente 
    setLampada(ADDR_L);  //salva o vetor na EEPROM
  else  //Se nao, carrega valores da EEPROM
    getLampada(ADDR_L); //pega os pinos na EEPROM no endereco 150
  
  // configura pinos e parametros do triac
  configura_triac();
  imprime_ip(); 
  pinMode(zc, INPUT);

  // Associa a borda de subida do detetor de zero com
  //   a funcao ftriac(), que desliga o triac e espera
  //   o momento necessario para ativa-lo
  attachInterrupt(0, zeroCross, RISING);
 
  //Configuracao do servidor
  webserver.setDefaultCommand(&handler);
  webserver.addCommand("index.html", &handler);
  webserver.addCommand("botao", &handlerB);
  webserver.addCommand("slide", &handlerS);
  webserver.addCommand("lampada", &handlerL);
  webserver.addCommand("nome", &handlerN);
  webserver.begin();
  
  // Inicializa o Timer1
  Timer1.initialize();
  // Associa o Timer1 a funcao triac()
  //  a cada 50 microSegundos
  Timer1.attachInterrupt(triac, 50);
  
  // Inicializa interruptores simples
  for (byte i = 0; i < sizeof(toggleSwitches)/sizeof(int); i++) {
    pinMode(toggleSwitches[i], INPUT_PULLUP);
    toggleSwitchesState[i] = digitalRead(toggleSwitches[i]);
  }
  
  // Inicializa interruptores momentâneos
  for (byte i = 0; i < sizeof(pushButtons)/sizeof(int); i++) {
    pinMode(pushButtons[i], INPUT_PULLUP);
    pushButtonsState[i] = digitalRead(pushButtons[i]);
  }
  
  //Carrega Scenarios da EEPROM:
  for (byte s=0; s<triacs; s++){
    for(byte i=0; i<sizeof(scenarios)/3; i++){
      scenarios[s][i]=EEPROM.read(ADDR_S + s*triacs + i);
    }
  }
}

void loop() {
  char buff[64];
  int len = 64;
  
  // Processa servidor web
  webserver.processConnection(buff, &len);
  
  //Processa os cenarios automaticos
  cenarioAutomatico();
  
  // Processa interruptores do painel
  processSwitches();
}

void cenarioAutomatico(){
  //Se flag Auto ligada: 
  //Altera os estados das lampadas automaticamente
  if(Auto && tempo1 % 300 == 0){
    for(byte i=0; i<triacs; i++){
      slide[i] = scenarioA[count][i];
    }
    count = count>14 ? 0 : count+1;
  }
}
void acionaAutomatico(){
  //Liga ou desliga os cenarios automaticos
  Auto=!Auto;
  if(Auto){
    count=0;
    for(byte i=0; i<triacs; i++)
      slideState[i]=HIGH;
  } 
  else{
    for(byte i=0; i<triacs; i++){
      slideState[i]=HIGH;
      slide[i]=50;
    }
  }
}

void processSwitches() {
  // Processa interruptores simples
  for (int i = 0; i < sizeof(toggleSwitches)/sizeof(int); i++) {
    int state = digitalRead(toggleSwitches[i]);
    if(state != toggleSwitchesState[i]) {
      slideState[i] = !slideState[i];
      
      // Se o valor está em zero, liga em 15%
      if (slideState[i] && slide[i] < 15) {
        slide[i] = 15;
      }
    }
    toggleSwitchesState[i] = state;
  }  

  // Processa interruptores momentâneos
  for (int i = 0; i < sizeof(pushButtons)/sizeof(int); i++) {
    int state = digitalRead(pushButtons[i]);
    if (state && !pushButtonsState[i]) {
      //Terceiro botao:Aciona o cenario automatico
      if(i==2){
        acionaAutomatico();
      }
      else{
        applyScenario(i);
      }
    }
    if (!state && pushButtonsState[i]) {
      pushButtonsTimer[i] = millis();
    }
    if (!state && millis() - pushButtonsTimer[i] > STORE_DELAY) {
      storeScenario(i);
      pushButtonsTimer[i] = millis();
    }
    pushButtonsState[i] = state;
  }
}

void applyScenario(byte s) {
  for (byte i = 0; i < 3; i++) {
    slideState[i] = scenarios[s][i] > 0 ? HIGH : LOW;
    slide[i] = slideState[i] ? scenarios[s][i] : 0;
  }
}

void storeScenario(byte s) {
  for (byte i = 0; i < 3; i++) {
    scenarios[s][i] = slideState[i] ? slide[i] : 0;

    // Desliga tudo para indicar que armazenou o cenário
    slideState[i] = 0;
    
    //Salva na EEPROM
    EEPROM.write(ADDR_S + s*triacs + i, scenarios[s][i] );
  }
}

void zeroCross(){
  //zera o timer
  tempo0=0;
  //desliga os triacs
  for (byte i=0; i<triacs; i++)
    digitalWrite(P_triac[i], LOW); 
}

void triac(){
  //Acionamento em rampa:
  if(tempo0 % RAMPA == 0){
    tempo1++;
    for(byte a=0; a<triacs; a++){
      if(slide2[a] > slide[a])
        slide2[a]--;
      else if(slide2[a] < slide[a])
        slide2[a]++;
    }
  }    
  tempo0++;
  //liga o triac (lampada) no tempo desejado
  for(byte i=0; i<triacs; i++){
    if(tempo[ slide2[i] ] == tempo0){
      if(slideState[i] && slide2[i]>0)
        digitalWrite(P_triac[i], HIGH);
      }
    }
}

//Imprime o IP da placa no terminal
void imprime_ip(){
  eeprom.begin();
  eeprom.startReading(0x00FA);
  Serial.print(F("Endereco MAC: "));
  for (byte i=0; i<6; i++) {
      mac[i] = eeprom.read();
      Serial.print(mac[i], HEX);
      if (i < 5)
        Serial.print(":");
      else
        Serial.println();
      }
  Serial.println(F("Inicializando Ethernet..."));
  Ethernet.begin(mac, ip, gateway, gateway, subnet);
  delay(200);
  Serial.print(F("Endereco IP: "));
  Serial.println(Ethernet.localIP());
}

//Salva as portas na EEPROM:
void setLampada(byte addr){
  Serial.print("Salvando portas na EEPROM...\n");
  byte aux, data1, data2, addr2=addr;
  for(aux=0; aux<N_MAX; aux++){
    if(P_triac[aux]!=0){
      data1=P_triac[aux]/10;
      data2=P_triac[aux]%10;
      EEPROM.write(addr++, data1);
      EEPROM.write(addr++, data2);
    }
  }
  for(; addr<2*N_MAX+addr2; addr++)
    EEPROM.write(addr, 0);
}

//le portas salvas no EEPROM:
void getLampada(byte addr){
  Serial.print("Acessando EEPROM...\nPortas Lidas:");
  triacs=0;
  byte aux, i, data1, data2;
  for(aux=0, i=0; i<N_MAX; i++){
    data1 = EEPROM.read(addr++);
    data2 = EEPROM.read(addr++);
    if(data1>=0 && data1<10 && data2>0 && data2<10){
      P_triac[aux]=10*data1+data2;
      Serial.print(P_triac[aux]);
      Serial.print("  ");
      aux++;
    }
  }
  triacs=aux;
}

//Configura e imprime as portas dos triacs como saida e inicializa as variaveis
void configura_triac(){ 
  Serial.println(); 
  Serial.print(triacs);
  Serial.print(" portas configuradas: ");
  for(byte i=0; i < triacs; i++){ 
    if(P_triac[i]!=0){
      Serial.print(P_triac[i]);
      Serial.print("  ");
      pinMode(P_triac[i], OUTPUT);
      slideState[i]=HIGH;
      slide[i]= 50;
    }
  }
  Serial.println(); 
}

//Pagina Index:
void handler(WebServer &server, WebServer::ConnectionType type, char *, bool){
  short nSlide=-1;
  if (type == WebServer::GET) {
    server.httpSuccess();
    //Pagina HTML:
    P(defaultPage) =
      "<!DOCTYPE html>\n"
      "<html>\n"
      "<head>\n"
      "    <meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">\n"
      "    <title>Dimmer Web+</title>\n"
      "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
      "    <link rel=\"stylesheet\" href=\"http://maxcdn.bootstrapcdn.com/bootstrap/3.3.1/css/bootstrap.min.css\">\n"
      "    <link rel=\"stylesheet\" href=\"http://maxcdn.bootstrapcdn.com/font-awesome/4.2.0/css/font-awesome.min.css\">\n"
      "    <script src=\"https://ajax.googleapis.com/ajax/libs/jquery/2.1.3/jquery.min.js\"></script>\n"
      "    <script src=\"https://ajax.googleapis.com/ajax/libs/jqueryui/1.11.2/jquery-ui.min.js\"></script>\n"
      "    <link rel=\"stylesheet\" href=\"https://doc-14-6g-docs.googleusercontent.com/docs/securesc/ha0ro937gcuc7l7deffksulhg5h7mbp1/7r75comka4qt6vkjapjqpecrh5hpissc/1423828800000/01595529376970354395/*/0By9QO5Pl2iCrRHVJTEVfWC1oSU0\">\n"
      "    <script src=\"https://doc-0o-1s-docs.googleusercontent.com/docs/securesc/ha0ro937gcuc7l7deffksulhg5h7mbp1/vmc4hev1jhqeoe2qf79lqp9luda5l3ad/1423828800000/16678327774422887772/*/0B0Rwpf_6lh1mN3c3M2YxRG5SbEU\"></script>\n"
      "</head>\n"
      "<body>\n"
      "<div class=\"container text-center \">\n"
      "    <div class=\"head1 col-md-4 col-md-offset-4\">\n"
      "        <a href=\"http://www.circuitar.com.br/\"><img id=\"logo\" src=\"http://www.circuitar.com.br/static/images/circuitar_logo.png\" alt=\"Circuitar\"></a>\n"
      "    </div>\n"
      "</div>\n"
      "</body>\n"
      "</html>";
    server.printP(defaultPage);
  }     
}

//le parametros do botao liga-desliga
void handlerB(WebServer &server, WebServer::ConnectionType type, char *, bool){
  char name[PARAM_LEN];
  char value[PARAM_LEN];
  
  if (type == WebServer::POST) {
    server.httpSuccess();
    server.readPOSTparam(name, PARAM_LEN, value, PARAM_LEN);
    if (strcmp(name, "n") == 0) {
      byte aux = atoi(value);
      slideState[aux]=!slideState[aux];
      }
  }
}

//Le parametros do slide
void handlerS(WebServer &server, WebServer::ConnectionType type, char *, bool){
  char name[PARAM_LEN];
  char value[PARAM_LEN];
  short nSlide=-1;

  if (type == WebServer::GET) {
    server.httpSuccess();
    //atualiza os valores da pagina (em jSON)
    server.print(F("{\"s\":["));
    //slides:
    if(!Auto){
      for(byte i=0; i<triacs; i++){
        server.print(F("{\"sl\":"));
        server.print(slide[i]);
        server.print(F("}"));
        if(i<triacs-1)
          server.print(F(","));
        } 
    }
    server.print(F("],\n \"b\":["));
    //botoes:
    if(!Auto){
      for(byte i=0; i<triacs; i++){
        server.print(F("{\"bt\":"));
        server.print(slideState[i]);
        server.print(F("}"));
        if(i<triacs-1)
          server.print(F(","));
        } 
    }
    //fecha JSON  
    server.print(F("]}")); 
    }

  if (type == WebServer::POST) {
    server.httpSuccess();
    while(server.readPOSTparam(name, PARAM_LEN, value, PARAM_LEN)){
      if (strcmp(name, "n") == 0) 
          nSlide = atoi(value);
      if (name[0]=='s' && nSlide >= 0)
          slide[nSlide] = atoi(value);
      }
  }
}


//Le o numero das lampadas do Arduino
void handlerL(WebServer &server, WebServer::ConnectionType type, char *, bool){
  char name[PARAM_LEN];
  char value[PARAM_LEN];
  byte aux, Vaux;
  boolean conf=false;
  
  //Liga-desliga o cenario automatico
  if (type == WebServer::HEAD) {
    server.httpSuccess();
    acionaAutomatico();
  }
  
  if (type == WebServer::GET) {
    server.httpSuccess();
    for(aux=0; aux<triacs; aux++){
      if(P_triac[aux]!=0){
        server.print(P_triac[aux]);
        if(P_triac[aux+1]!=0)
          server.print(" ");
      }
    }
  } 
    
  if (type == WebServer::POST) {
    server.httpSuccess();
    aux=0;
    while(server.readPOSTparam(name, PARAM_LEN, value, PARAM_LEN)){
      Vaux=atoi(value);
      if (name[0]=='l' && Vaux!=0){
        P_triac[aux++]=Vaux;
        conf=true;
      }
    }
    //Configuracao das novas portas
    if(conf){
      triacs=aux;  //atualiza quantidade
      for(; aux<N_MAX; aux++)
        P_triac[aux]=0;  //Reseta o resto do vetor
      configura_triac();
      setLampada(ADDR_L);
    }
  }
}


//Le e envia nomes das lampadas (salva na eeprom)
void handlerN(WebServer &server, WebServer::ConnectionType type, char *, bool){
  char name[PARAM_LEN];
  char value[PARAM_LEN];
  int num=-1;
 
  if (type == WebServer::GET) {
    server.httpSuccess();
    //atualiza os valores da pagina (em jSON)
    server.print(F("{\"l\":["));
    //lampadas:
    for(byte i=0; i<triacs; i++){
      server.print(F("{\"lg\":"));
      getLegenda(i);
      if(legenda[0]==0){
        server.print(F("\"Lampada "));
        server.print(i+1);
        server.print("\"");
      }
      else{
        server.print("\"");
        server.print(legenda);
        server.print("\"");
      }
      server.print(F("}"));
      if(i<triacs-1)
        server.print(F(","));
      } 
    //fecha JSON  
    server.print(F("]}")); 
    }

  if (type == WebServer::POST) {
    byte i;
    server.httpSuccess();
    //Le a legendas postadas do site e salva na EEPROM do Arduino
    while(server.readPOSTparam(name, PARAM_LEN, value, PARAM_LEN)){
      if (name[0]=='n')
        num = atoi(value);
      if (name[0]=='l' && num >= 0){
        for(i=0; i<TAMANHO-1; i++)
          legenda[i]=value[i];
        setLegenda(num); //Salva a legenda (num) na EEPROM
      }
    }
  }
}

void getLegenda(byte numero){
  byte aux=0;
  char data;
  numero *= TAMANHO;
  do{
    data = EEPROM.read(numero++);
    legenda[aux]= data;
    aux++;
  } while(data!=0 && aux<TAMANHO-1);
}

void setLegenda(byte numero){
  numero *=TAMANHO;
  for(byte aux=0; aux<TAMANHO; aux++ )
    EEPROM.write(numero+aux, legenda[aux] );
}
