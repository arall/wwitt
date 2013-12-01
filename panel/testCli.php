<?php

/**
 * This is just for faster exploit development.
 */

define("CLI_DEBUG", true); //Dont save the results

include("index.php");

//Define
$vulnId = 2;

//Creating Models
$host = new Host();
$host->ipAdress = $argv[1];
if(!$host->ipAdress){
	echoCli("Host not found", "failure");
	exit;
}
$virtualhost = null;

//Launch vuln exploit
$vuln = new Vuln($vulnId);
if(!$vuln->id){
	echoCli("Vuln not found", "failure");
	exit;
}
$res = $vuln->exploit($host, $vuln->port, $virtualhost);

//Result
echoCli("Result:", "title");
if($res){
	echoCli("Vuln exploited!", "success");
}else{
	echoCli("Exploit failed", "failure");
}
//Show messages
print_r(Registry::getMessages());
echo "\n";

?>