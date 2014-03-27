<?php

include "http_dispatcher.class.php";

// Create a HTTP dispatcher class, use two named pipes
$dispatcher = new HttpDispatcher("/home/david/wwitt/src/resp_pipe", "/home/david/wwitt/src/req_pipe");

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
