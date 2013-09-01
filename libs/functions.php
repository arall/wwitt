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
	  		case "error":
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
	echo colorize("<curlH> ".$url, "debug");
	$ch = curl_init();
	curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
	curl_setopt($ch, CURLOPT_URL, $url);
	curl_setopt($ch, CURLOPT_CONNECTTIMEOUT, $config['general']['timeout']);
	curl_setopt($ch, CURLOPT_TIMEOUT, $config['general']['timeout']);
	curl_setopt($ch, CURLOPT_USERAGENT, 'Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.8.1.11) Gecko/20071127 Firefox/2.0.0.11');
	curl_setopt($ch, CURLOPT_HEADER, true);
	curl_setopt($ch, CURLOPT_NOBODY, true);
	$content = curl_exec($ch);
	curl_close($ch);
	if($content){
		return $content;
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

function curl($url, $post="") {
	global $config;
	//debug
	$print = "<curl> ".$url;
	if($post){
		$print .= " | ".$post;
	}
	echo colorize($print, "debug");
	$ch = curl_init($url);
	curl_setopt($ch, CURLOPT_USERAGENT, 'Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.8.1.11) Gecko/20071127 Firefox/2.0.0.11');
	curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
	curl_setopt($ch, CURLOPT_URL, $url);
	if($post){
		curl_setopt($ch,CURLOPT_POST, 1);
		curl_setopt($ch,CURLOPT_POSTFIELDS, $post);
	}
	curl_setopt($ch, CURLOPT_FOLLOWLOCATION, true);
	curl_setopt($ch, CURLOPT_CONNECTTIMEOUT, $config['general']['timeout']);
	curl_setopt($ch, CURLOPT_TIMEOUT, $config['general']['timeout']);
	$result = curl_exec($ch);
	$statusCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
	curl_close($ch);
	if($statusCode<400)
		return $result;
}

function getLastEffectiveUrl($url, &$content=""){
	global $config;
	echo colorize("<curlLEU> ".$url, "debug");
	$url = cleanHostUrl($url);
	$ch = curl_init($url);
	curl_setopt($ch, CURLOPT_USERAGENT, 'Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.8.1.11) Gecko/20071127 Firefox/2.0.0.11');
	curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
	curl_setopt($ch, CURLOPT_FOLLOWLOCATION, 1);
	curl_setopt($ch, CURLOPT_CONNECTTIMEOUT, $config['general']['timeout']);
	curl_setopt($ch, CURLOPT_TIMEOUT, $config['general']['timeout']);
	$result = strtolower(curl_exec($ch));
	if($result===false){
		curl_close($ch);
		return null;
	}
	$redirectUrl = strtolower(curl_getinfo($ch, CURLINFO_EFFECTIVE_URL));
	if($redirectUrl==$url){
		if(strstr($result, 'http-equiv="refresh"')){
			$r = get_between($result, 'url=', '">');
			if($r[0]=="/"){
				$redirectUrl = $url.$r;
			}else{
				$redirectUrl = $r;
			}
		}
	}
	curl_close($ch);
	$redirectUrl = parse_url($redirectUrl);
	$host = $redirectUrl['host'];
	$content = $result;
	return cleanHostUrl($host);
}

function cleanHostUrl($url){
	$url = trim(strtolower($url));
	if(strstr($url, "http://")){
		$url = str_replace("http://","",$url);
		$url = "http://".$url;
	}elseif(strstr($url, "https://")){
		$url = str_replace("https://","",$url);
		$url = "https://".$url;
	}else{
		$url = "http://".$url;
	}
	if(substr($url, -1)!="/"){
		$url .= "/";
	}
	return $url;
}

?>