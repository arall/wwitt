<?php

define("_EXE", 1);
ini_set("display_errors", 1);
error_reporting(E_ERROR | E_WARNING | E_PARSE);
set_time_limit(0);

/**
* Config
**/
//Database
$config['database']['host'] = "localhost";
$config['database']['username'] = "root";
$config['database']['password'] = "";
$config['database']['database'] = "wwitt";
//Bing
$config['bing']['maxPage'] = 1;
//General
$config['general']['timeout'] = 1;
$config['general']['vervose'] = 1; //0, 1, 2 (none, infos, debug)
$config['general']['ports'] = array(21, 22, 25, 80, 8080, 3306);

/**
* Classes
**/
include("classes/model.class.php");
include("classes/host.class.php");
include("classes/service.class.php");
include("classes/virtualhost.class.php");

/**
* Includes
**/
include("libs/database.php");
include("libs/functions.php");
include("libs/simple_html_dom.php");

/**
* PHP Pear
**/
require_once 'Net/Nmap.php'; //http://pear.php.net/package/Net_Nmap

/**
* Args
**/
include("libs/pharse.php"); //https://github.com/chrisallenlane/pharse/
$options = array(
    'url' => array(
        'description'   => 'Url',
        'required'      => false,
        'short'         => 'u',
    ),
    'verbose' => array(
        'description'   => 'Verbosity level: 0-2 (default 1)',
        'required'      => false,
        'short'         => 'v',
    ),
);

/**
* Banner
**/
$banner = colorize("____________________________________________________
  __      __  __      __  ______  ______  ______   
 /\ \  __/\ \/\ \  __/\ \/\__  _\/\__  _\/\__  _\  
 \ \ \/\ \ \ \ \ \/\ \ \ \/_/\ \/\/_/\ \/\/_/\ \/  
  \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \   \ \ \   \ \ \  
   \ \ \_/ \_\ \ \ \_/ \_\ \ \_\ \__ \ \ \   \ \ \ 
    \ `\___x___/\ `\___x___/ /\_____\ \ \_\   \ \_\
     '\/__//__/  '\/__//__/  \/_____/  \/_/    \/_/
                                                  
        World Wide Internet Takeover Tool
____________________________________________________\n", "banner");
Pharse::setBanner($banner);
echo $banner;

/**
* Args
**/
$args = Pharse::options($options);
if($args['verbose']){
    $config['general']['vervose'] = $args['verbose'];
}

/**
* Main
**/
//DB Connection
$db = new Database($config['database']['host'], $config['database']['user'], $config['database']['password'], $config['database']['database']);

//OVH IP Rank (Devel demos)
for($ip1=5;$ip1<=5;$ip1++){
    for($ip2=135;$ip2<=135;$ip2++){
        for($ip3=0;$ip3<=255;$ip3++){
            for($ip4=0;$ip4<=255;$ip4++){
                $ip = $ip1.".".$ip2.".".$ip3.".".$ip4;
                //Exist?
                $host = Host::getHostFromIp($ip);
                if(!$host){
                    echo colorize("New IP: ".$ip, "notice");
                    $host = new Host();
                    $host->ip = $ip;
                    $host->insert();
                    echo colorize("Starting Port Scan...", "success");
                    $host->exePortScan();
                    if($host->status){
                        echo colorize("Starting Bing IP Search...", "success");
                        $host->exeBingIpScan();
                        if($host->hostname && $host->hostname!="unknown"){
                            if(!VirtualHost::getVirtualHostByIpIdHost($host->id, $host->hostname)){
                                echo colorize("Self IP Virtual Host (".$host->hostname.")...", "success");
                                $virtualHost = new VirtualHost();
                                $virtualHost->ipId = $host->id;
                                $virtualHost->host = $host->hostname;
                                $virtualHost->url = getLastEffectiveUrl($host->hostname, $virtualHost->index);
                                $virtualHost->head = curlHead($virtualHost->hostname);
                                $virtualHost->insert();
                                echo colorize($host->hostname);
                            }
                        }
                    }
                }else{
                    echo colorize("IP already exist: ".$ip);
                }
            }
        }
    }
}

?>
