<?php

function echoCli($text, $status){
	if(php_sapi_name()=="cli"){
    $out = "";
   	switch(strtolower($status)){
    		case "title":
    			$text = "\n[+] ".$text;
     			//$out = "[0;32m"; //Green
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
  	}
  	if($text){
  		if($out)
  	 		echo chr(27).$out.$text.chr(27)."[0m \n";
  	 	else
  	 		echo $text."\n";
  	}
  }
}

?>