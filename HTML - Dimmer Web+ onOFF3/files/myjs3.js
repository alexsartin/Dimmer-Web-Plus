var endereco, timeOutError, erro, refresh, inicio=false, start=false, lampadas, count=0, offline=false;
$.ajaxSetup({ timeout: 3000});

window.onload = function (){
	//Checa carregamento online/offline
	if( $('#offline').length == 1 ){
		offline=true;
		adicionaInterface();
		checaEndereco();
	}
	else{
		endereco="";
		adicionaInterface();
		startRequest();
	}
};

function checaEndereco() {
	//Verifica cookie:s
	var dado = getCookie('endereco');
	if (dado === "")
		$('#endereco').slideDown(1000);
	else{
		endereco=dado;
		$('#end1').text(endereco);
		$('#txt1').val(endereco);
		startRequest();
	} 
}

///////////////////////////COMEÇA AQUISIÇAO DO ARDUINO /////////////////////////
function startRequest() {
	refresh=true;
	if(!start){
		start=true;
		//primeira aquisiçao p/ deixar o carregamento incial mais rapido
		$.getJSON(endereco+'/slide', function(data) {
			atualizaDados(data); //funcao localizada no final do codido
		  });
		setInterval(function(){  contador(); }, 1000);
		setInterval(function(){ getRequest(); }, 1700);
	}
}

/////////////////////////// ADICIONA ELEMENTOS ///////////////////////////////
function contador(value) {
	if(value){
		if(count<=0)
			count=value;	
	}
	else{
		if(count>0)
			$('#count h5').html("Atualizando em " + count-- + "s");
		else 
			$('#count h5').html('<i class="fa fa-refresh fa-spin"></i>');
	}
}

function adicionaInterface() {
	//Alerta: Sem conexão
	var texto1="";
	if(offline)
		texto1='no endereço:';
	$('<div>', {
		id: 'alerta',
		class: 'caixa',
		html: '<h3> Sem Conexao com o Arduino '+texto1+'</h3><div id="end1"></div><div id="count"><h5></h5></div>'
	}).insertAfter('a');

	//Interface Offline:
	if (offline){
		//Botao: Trocar de endereço:
		$('<input type="button" value="Trocar endereço" id="btr" class="button btn btn-default">').appendTo('#alerta');
	
		//Input: Endereço
		$('<div>', {
			id: 'endereco',
			class: 'caixa',
			html: '<h4>Digite o endereço mostrado no terminal do Arduino:</h2><input type="text" value="http://192.168.1.150" id="txt1"><input type="button" value="Conectar" id="btc" class="button btn btn-default">'
		}).insertAfter('#alerta');
		
		//Enter no input
		$('#txt1').keyup(function(event){
			if(event.keyCode == 13)
				$('#btc').focus().click();
		});
		
		//Aciona botão do endereco
		$('#btc').click( function() {
			var text1 = $('#txt1').val();
			//verifica IP digitado:
			if ( validaIP(text1) ) {
				endereco = text1;
				setCookie('endereco', endereco);
				$('#end1').text(endereco);
				$('#endereco').slideUp(700);
				startRequest();
			}
		});
		
		//Aciona botão troca de endereço
		$('#btr').click( function() {
			clearTimeout(timeOutError);
			$('#endereco').slideDown(1000);
			$('#alerta').slideUp(1000);
		});
	}
}

function adicionaLampadas() {
	inicio=true;
	//Versao:
	$('<h5>Dimmer Web+ v4.0</h5>').insertAfter('a:first')
	//Botao: alterar lampadas e Botao: cenario automatico
	$('<div>', {
		id: 'lampada',
		class: 'caixa',
		html: '<div id="hide"><h3>Alteração das portas do Ardunino</h3><h4>Digite o pino atribuído para cada triac, <br>entre espaços:</h2><input type="text" value="Ex: 3 5 6 9" id="txt2"></div><input type="button" value="Cenário automático" id="btca" class="button btn btn-default"><input type="button" value="Alterar portas" id="bta" class="button btn btn-default">'
	}).insertAfter('a:first');

	//Enter no input
	$('#txt2').keyup(function(event){
		if(event.keyCode == 13)
			$('#bta').focus().click();
	});
	
	//Ação do botao Altera-Lampada:
	$('#bta').click( function () {
		if ( $('#hide').is(':hidden') ){
			$.get( endereco+'/lampada', function( data ) {
				if(data!="" && data!=null)
					$('#txt2').val(data);
				else
					$('#txt2').val('Ex: 3 5 6 9');
			});
			$('#btca').fadeOut();
			$('#hide').slideDown(700);
		} 
		else{
			var lamps=$('#txt2').val();
			if( lamps!="" && lamps!=null ){
				lamps=lamps.split(' ').map(function(item) {
					return parseInt(item, 10);
				});
				lamps = lamps.filter(function(val) {
					return !(isNaN(val));
				});
				postLampada(lamps)
			}
			$('#hide').slideUp(700);
			$('#btca').fadeIn();
		}
	});
	
	//Ação do botao cenario-automatico:
	$('#btca').click( function () {
		//Manda um request HEAD
		$.ajax({ type: 'HEAD', url: endereco+'/lampada' });
	});
	
	//Lampadas:
	for (var i = 0; i < lampadas; i++) {
		$('<div>', {
			id: 'lampada',
			class: 'caixa',
			html: '<div id="legenda"><a>Lâmpada '+(lampadas-i)+'</a></div><button id="botao" class="btn btn-default btn-lg"><i class="fa fa-lightbulb-o"></i> <i id="onOff" class="fa fa-toggle-on"></i></button><div id="result">0</div><div id="slide"></div>'
		}).insertAfter('a:first');
	}
	
	//Ação dos botoes:
	$('[id=botao]').each( function (index,ui) {
		$(ui).click( function () {
			postBotao(index);
			//Atualizacao da imagem dos switch on/off
			if($('[id=onOff]').eq(index).hasClass('fa-toggle-on'))
				$('[id=onOff]').eq(index).switchClass('fa-toggle-on', 'fa-toggle-off');
			else
				$('[id=onOff]').eq(index).switchClass('fa-toggle-off', 'fa-toggle-on');
		});	
	});
	
	//Adiciona slides:
	$('[id=slide]').each( function (index,ui) {
		$(ui).slider({
			animate: true,
			range: 'min',
			start: function () {
				refresh=false;
			},
			stop: function (event, ui) {
				postSlide(index, ui.value);
				$('[id=result]').eq(index).text(ui.value);
			}
		});
	});
	
	//Alterar legendas:
	getLegenda();
	$('[id=legenda] a').each( function (index,ui) {
		var aux;
		$(ui).click( function () {
			aux = prompt('Digite outro nome para esta legenda:', $(ui).text());
			if(aux!=="" && aux!==null && aux!=$(ui).text()){
				$(ui).text(aux);
				//posta pra EEPROM do arduino:
				postLegenda(index, aux)
			}
		});
	});
}

////////////////////////////////// POST/GET FUNC /////////////////////////////////
function postSlide(num, value){
	$.post(endereco+'/slide', { n: num, slide: value }, function() {
		refresh=true;
	});
}

function postBotao(num){
	$.post(endereco+'/botao', {n: num});
}

function postLampada(lamps) {
	$.post(endereco+'/lampada', {lamp: lamps});
}

function postLegenda(num, name) {
	$.post(endereco+'/nome', {n: num, leg: name});
}

function getLegenda(){
	$.getJSON(endereco+'/nome', function (data){
		$('[id=legenda] a').each( function (i,ui) {
			$(ui).text(String(data.l[i].lg));
		});
	});
}

function getRequest(){
	if(refresh){
		$.getJSON(endereco+'/slide')
			.fail( function(){
				if(erro){
					$('[id=lampada]').slideUp(900);
					$('#alerta').delay(900).slideDown(700);
					contador(5);
					refresh=false;
					timeOutError=setTimeout(function(){ refresh=true; }, 5100);
				}
				erro=!erro;
			})
			.done( function(data){
				atualizaDados(data);
			});
	}
}

//////////////////////////ATUALIZACAO DOS ELEMENTOS DA PAGINA /////////////////////////
function atualizaDados(data) {
	erro=false;
	if(!inicio){
		//primeira leitura do Arduino:
		lampadas=data.s.length;	//salva o numero de lampadas
		adicionaLampadas(); //contrsuçao dos elementos da pagina
	}
	
	//Atualizacão dos valores do slide/botao/numeroLampadas
	if(lampadas!=data.s.length)//se o num de lampadas mudou no arduino, atualiza a pag.
		location.reload();
	else{
		//Atualiza os elementos
		for(var i = 0; i < lampadas; i++){
			//atualiza texto e slide:
			$('[id=result]').eq(i).text(data.s[i].sl);
			$("[id=slide]").eq(i).slider('option','value',data.s[i].sl);
			//atualiza botao:
			if(data.b[i].bt)
				$('[id=onOff]').eq(i).switchClass('fa-toggle-off', 'fa-toggle-on');
			else
				$('[id=onOff]').eq(i).switchClass('fa-toggle-on', 'fa-toggle-off');
      		}
		//Mostra elementos:
		$('#alerta').slideUp(700);
		$('[id=lampada]').slideDown(700);
	}
}

///////////////////////////// VERIFICA IP DIGITADO ///////////////////////////////
function validaIP(inputText) {  
	inputText=inputText.replace('http://', '');
	var ipformat = /^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$/;
	ipformat = /^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$/; 
	if(!inputText.match(ipformat)) {  
 		alert("O IP digitado é inválido!");
	 	return false;
	} 
	return true; 				 
}

/////////////////////////////////// COOKIES /////////////////////////////////////
function getCookie(cname) {
	var name = cname + "=";
	var ca = document.cookie.split(';');
	for(var i=0; i<ca.length; i++) {
		var c = ca[i];
		while (c.charAt(0)===' ')
			c = c.substring(1);
		if (c.indexOf(name) === 0) 
			return c.substring(name.length, c.length);
	}
	return "";
}

function setCookie(name, value) {
	var d = new Date();
	d.setTime(d.getTime() + (3*24*60*60*1000)); //3dias
	var expires = "expires=" + d.toUTCString();
	document.cookie = name + "=" + value + "; " + expires;
}