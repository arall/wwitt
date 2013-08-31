<?php

define("_EXE", 1);
ini_set("display_errors", 1);
error_reporting(E_ALL ^ E_NOTICE ^ E_WARNING);
set_time_limit(0);

/**
* Functions
**/
include("libs/functions.php");

/**
* Config
**/
$config['general']['curlTimelimit'] = 5;
$config['general']['vervose'] = 1;          //0, 1, 2 (none, infos, debug)

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
$banner = colorize("\n[#] WWITT - World Wide Internet Takeover Tool [#]", "banner");
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

echo colorize(":D");

?>
