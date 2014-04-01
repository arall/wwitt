<?php

function curl($url, $post=null){
	$ch = curl_init($url);
	curl_setopt($ch, CURLOPT_USERAGENT, 'Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.8.1.11) Gecko/20071127 Firefox/2.0.0.11');
	curl_setopt($ch, CURLOPT_FOLLOWLOCATION, true);
	curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
	if($post){
		curl_setopt($ch,CURLOPT_POST, true);
		curl_setopt($ch,CURLOPT_POSTFIELDS, $post);
	}
	$result = curl_exec($ch);
	curl_close($ch);
	return $result;	
}

function get_between($string,$start,$end){
	$string = " ".$string;
	$ini = strpos($string, $start);
	if($ini==0) return "";
	$ini += strlen($start);
	$len = strpos($string, $end, $ini) - $ini;
	return substr($string, $ini, $len);
}

function get_between_multi($content,$start,$end){
    $r = explode($start, $content);
    if (isset($r[1])){
        array_shift($r);
        $end = array_fill(0,count($r),$end);
        $r = array_map('get_between_help',$end,$r);
        if(count($r)>1)
        	return $r;
        else
        	return $r[0];
    } else {
        return array();
    }
}

function get_between_help($end,$r){
   $r = explode($end,$r);
   return $r[0];   
}

?>