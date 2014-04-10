<?php
/**
 * This is just for faster exploit development.
 */

//Dont save the results
define("CLI_DEBUG", true);

include("index.php");

$execTime = microtime(true);

$host = $argv[1];

$virtualHost = new VirtualHost($host);
$info = $virtualHost->cmsDetector();
if(isset($info["name"])){
	$webapp = new WebApp();
	$webapp->host = $virtualHost->host;
	$webapp->url = $virtualHost->url;
	$webapp->name = $info["name"];
	$webapp->version = $info["version"];
	$webapp->insert();
}

//Show messages
print_r(Registry::getMessages());
echo "\n";

echoCli("Time: ".(microtime(true)-$execTime)."ms", "notice");

