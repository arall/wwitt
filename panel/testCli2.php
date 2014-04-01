<?php
/**
 * This is just for faster exploit development.
 */

//Dont save the results
define("CLI_DEBUG", true);

include("index.php");

$virtualHost = new VirtualHost("writeHostHere");
$info = $virtualHost->cmsDetector();
print_r($info);

//Show messages
print_r(Registry::getMessages());
echo "\n";

echoCli("Time: ".(microtime(true)-$execTime)."ms", "notice");

?>