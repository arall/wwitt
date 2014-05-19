<?php

//Dont save the results
define("CLI_DEBUG", true);

include 'index.php';

define("RESULTS_LIMIT", 10);

$execTime = microtime(true);

if (!$argv[1]) {
    die(echoCli("Example: php cli.php cmsid [vhost]", "failure"));
}

switch ($argv[1]) {
    case 'cmsid':
        //Single target
        if ($argv[2]) {
            $virtualHosts[] = new VirtualHost($argv[2]);
        //DB targets
        } else {
            $virtualHosts = VirtualHost::select(
                array(
                    "status" => 2,
                ),
                RESULTS_LIMIT
            );
        }
        if (count($virtualHosts)) {
            foreach ($virtualHosts as $virtualHost) {
                echoCli("CMSID running @".$virtualHost->host, "notice");
                $info = $virtualHost->cmsDetector();
                //Update vhosts status
                $virtualhost->status = 3;
                $virtualHost->update();
                //CMS Found?
                if (isset($info["name"])) {
                    echoCli("CMS identified: ".$info["name"], "success");
                    //Save WebApp
                    $webapp = new WebApp();
                    $webapp->host = $virtualHost->host;
                    $webapp->url = $virtualHost->url;
                    $webapp->name = $info["name"];
                    $webapp->version = $info["version"];
                    $webapp->insert();
                } else {
                    echoCli("CMS not identified: ".$info["name"], "failure");
                }
            }
        } else {
            echoCli("No virtualhosts found", "failure");
        }
    break;
    case 'sshWeakLogin':
        //Single target
        if ($argv[2]) {
            $host[] = new Host($argv[2]);
        //DB targets
        } else {
            $hosts = Host::select(
                array(
                    "port" => 22,
                ),
                RESULTS_LIMIT
            );
        }
        if (count($hosts)) {
            //Vuln
            $vuln = new Vuln("sshWeakLogin");
            foreach ($hosts as $host) {
                //Launch vuln exploit
                $res = $vuln->exploit($host, 22);
                //Result
                echoCli("Result:", "title");
                if ($res) {
                    echoCli("Vuln exploited!", "success");

                } else {
                    echoCli("Exploit failed", "failure");
                }
            }
        } else {
            echoCli("No hosts found", "failure");
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
