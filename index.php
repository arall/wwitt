<?php

define("_EXE", 1);
ini_set("display_errors", 1);
error_reporting(E_ERROR | E_WARNING | E_PARSE);
set_time_limit(0);

/**
* Config
**/
$config['database']['host'] = "localhost";
$config['database']['username'] = "root";
$config['database']['password'] = "";
$config['database']['database'] = "wwitt";
$config['general']['timeout'] = 1;
$config['general']['vervose'] = 1; //0, 1, 2 (none, infos, debug)
$config['general']['ports'] = array(21, 22, 25, 80, 8080, 3306);

/**
* Classes
**/
include("classes/model.class.php");
include("classes/host.class.php");
include("classes/head.class.php");

/**
* Includes
**/
include("libs/database.php");
include("libs/functions.php");

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

//Demo
echo colorize("Starting Demo", "title");
for($ip1=87;$ip1<=87;$ip1++){
    for($ip2=98;$ip2<=98;$ip2++){
        for($ip3=227;$ip3<=227;$ip3++){
            for($ip4=50;$ip4<=60;$ip4++){
                $ip = $ip1.".".$ip2.".".$ip3.".".$ip4;
                //Exist?
                $host = Host::getHostFromIp($ip);
                if(!$host){
                    echo colorize("New IP: ".$ip);
                    $host = new Host();
                    $host->ip = $ip;
                    $host->insert();
                }else{
                    echo colorize("IP already exist: ".$ip);
                }
                //Getting Headers
                foreach($config['general']['ports'] as $port){
                    $host->exeHeaders($port);
                }
            }
        }
    }
}

?>
