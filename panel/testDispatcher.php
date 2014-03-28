<?php

/**
 * This is just for faster exploit development.
 */

define("CLI_DEBUG", true); //Dont save the results

include("index.php");

$config = Registry::getConfig();

// Create a HTTP dispatcher class, use two named pipes
$dispatcher = new HttpDispatcher($config->get("path")."/../src/resp_pipe", $config->get("path")."/../src/req_pipe");

// Schedule requests
$req[0] = $dispatcher->fetchGET("http://davidgf.net");
$req[1] = $dispatcher->fetchGET("http://es.yahoo.com/");

echo "Req: " . $req[0] . " " . $req[1] . "\n";

while (1) {
	$dispatcher->process();
	for ($i = 0; $i < 2; $i++) {
		$resp = $dispatcher->getResult($req[$i]);
		if (!is_null($resp)) {
			echo "CODE: " . $resp->httpCode;
			echo $resp->head;
			echo $resp->body;
		}
	}
}

?>