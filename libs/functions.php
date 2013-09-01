<?php
define("_EXE", 1);

//http://softkube.com/blog/generating-command-line-colors-with-php/
function colorize($text, $status=null) {
	global $config;
	if($config['general']['vervose']){
		$out = "";
	 	switch(strtolower($status)){
	  		case "banner":
	   			$out = "[0;32m"; //Green
	  		break;
	  		case "title":
	  			$text = "[+] ".$text;
	   			$out = "[0;34m"; //Blue
	  		break;
	  		case "debug":
	  			if($config['general']['vervose']>=2){
		  			$text = " * ".$text;
	  			}else{
		  			$text = "";
	  			}
	   		break;
	  		case "success":
	  			$text = " - ".$text;
	   			$out = "[0;32m"; //Green
	   		break;
	  		case "failure":
	   			$text = " ! ".$text;
	   			$out = "[0;31m"; //Red
	   		break;
	  		case "warning":
	  			$text = " X ".$text;
	   			$out = "[1;33m"; //Yellow
	   		break;
	  		case "notice":
	  			$text = " - ".$text;
	   			$out = "[0;34m"; //Blue
	   		break;
	   		default:
	   			$text = " | ".$text;
	   		break;
	   		case "table":
	  			$lines = explode("\n", $text);
	  			$text = "";
	  			if($lines){
	  				$l = count($lines)-1;
	  				foreach($lines as $i=>$line){
	  					$text .= " ".$line;
	  					if($i<$l) $text .= "\n";
	  				}
	  			}
	   		break;
		}
		if($text){
			if($out)
		 		return chr(27).$out.$text.chr(27)."[0m \n";
		 	else
		 		return $text."\n";
		 }
	 }
}

//http://php.net/manual/en/function.socket-connect.php
function getHeader($host, $port){
	global $config;
	//Fix Port 80
	if($port==80){
		$res = curlHead($host);
		return $res['server'];
	}else{
		echo colorize("<socket> ".$host.":".$port, "debug");
		$socket = @socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
		@socket_set_option($socket, SOL_SOCKET, SO_SNDTIMEO, array('sec' => 1, 'usec' => 0)); 
		@socket_connect($socket, $host, $port);
	    $r = array($socket);
	    $c = @socket_select($r, $w = NULL, $e = NULL, 1);
	    if($r){
	    	foreach ($r as $read_socket) {
		        if ($r = negotiate($read_socket)) {
		            return $r;
		        }
		    }
	    }
	}
}

//http://php.net/manual/en/function.socket-connect.php
function negotiate ($socket) {
    @socket_recv($socket, $buffer, 1024, 0);
    for ($chr = 0; $chr < strlen($buffer); $chr++) {
        if ($buffer[$chr] == chr(255)) {
            $send = (isset($send) ? $send . $buffer[$chr] : $buffer[$chr]);
            $chr++;
            if (in_array($buffer[$chr], array(chr(251), chr(252)))) 
            	$send .= chr(254);
            if (in_array($buffer[$chr], array(chr(253), chr(254)))) 
            	$send .= chr(252);
            $chr++;
            $send .= $buffer[$chr];
        } else {
            break;
        }
    }
    if (isset($send)) 
    	@socket_send($socket, $send, strlen($send), 0);
    if ($chr - 1 < strlen($buffer)) 
    	return substr($buffer, $chr);
}

function curlHead($url) {
	global $config;
	$ch = curl_init();
	curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
	curl_setopt($ch, CURLOPT_URL, $url);
	curl_setopt($ch, CURLOPT_CONNECTTIMEOUT, 20);
	curl_setopt($ch, CURLOPT_USERAGENT, 'Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.8.1.11) Gecko/20071127 Firefox/2.0.0.11');
	curl_setopt($ch, CURLOPT_HEADER, true);
	curl_setopt($ch, CURLOPT_CUSTOMREQUEST, 'HEAD');
	$statusCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
	$content = curl_exec($ch);
	curl_close($ch);
	if($content){
		$return = array();
		$return['status'] = $statusCode;
		$return['server'] = get_between($content, "Server: ", "\n");
		$return['xpowered'] = get_between($content, "X-Powered-By: ", "\n");
		//debug
		echo colorize("<curlH> ".$url, "debug");
		return $return;
	}
}

function get_between($string,$start,$end){
	$string = " ".$string;
	$ini = strpos($string, $start);
	if($ini==0) return "";
	$ini += strlen($start);
	$len = strpos($string, $end, $ini) - $ini;
	return substr($string, $ini, $len);
}

?>