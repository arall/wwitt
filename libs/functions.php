<?php
define("_EXE", 1);

function colorize($text, $status) {
	global $config;
	//http://softkube.com/blog/generating-command-line-colors-with-php/
	if($config['general']['vervose']){
		$out = "";
	 	switch(strtolower($status)){
	  		case "banner":
	   			$out = "[0;32m"; //Green
	  		break;
	  		case "title":
	  			$text = "\n[+] ".$text;
	   			$out = "[0;32m"; //Green
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

?>