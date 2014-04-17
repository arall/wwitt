<?php

//Dont save the results
define("CLI_DEBUG", true);

include("index.php");

define("RESULTS_LIMIT", 10);

$execTime = microtime(true);

if(!$argv[1]){
	die(echoCli("Example: php cli.php cmsid [vhost]", "failure"));
}

switch($argv[1]){
	case 'cmsid':
		//Single target
		if($argv[2]){
			$virtualHosts[] = new VirtualHost($argv[2]);
		//DB targets
		}else{
			$virtualHosts = VirtualHost::select(
				array(
					"status" => 2,
				),
				RESULTS_LIMIT
			);
		}
		if(count($virtualHosts)){
			foreach($virtualHosts as $virtualHost){
				echoCli("CMSID running @".$virtualHost->host, "notice");
				$info = $virtualHost->cmsDetector();
				//Update vhosts status
				$virtualhost->status = 3;
				$virtualHost->update();
				//CMS Found?
				if(isset($info["name"])){
					echoCli("CMS identified: ".$info["name"], "success");
					//Save WebApp
					$webapp = new WebApp();
					$webapp->host = $virtualHost->host;
					$webapp->url = $virtualHost->url;
					$webapp->name = $info["name"];
					$webapp->version = $info["version"];
					$webapp->insert();
				}else{
					echoCli("CMS not identified: ".$info["name"], "failure");
				}
			}
		}else{
			echoCli("No virtualhosts found", "failure");
		}
	break;
	default:
		die(echoCli("Action not found", "failure"));
	break;
}

//Show messages
print_r(Registry::getMessages());
echo "\n";

echoCli("Time: ".(microtime(true)-$execTime)."ms", "notice");

