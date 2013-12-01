<?php

//Configuration
include("config.php");

//Autoload Classes
function __autoload($class_name) {
	//System
	$file = "system".DIRECTORY_SEPARATOR."classes".DIRECTORY_SEPARATOR.strtolower($class_name).".class.php";
    if (is_file($file)){
        include $file;
        return;
    }
	//Custom
    $file = "classes".DIRECTORY_SEPARATOR.strtolower($class_name).".class.php";
    if (is_file($file)){
        include $file;
        return;
    }
}

//Libs
//Telnet
include('libs'.DIRECTORY_SEPARATOR."telnet.class.php");
//SSH
set_include_path("libs".DIRECTORY_SEPARATOR.'phpseclib');
include('Net/SSH2.php');
//Colorize CLI
include("libs".DIRECTORY_SEPARATOR."colorize.php");

//Registry
$registry = new Registry();

//Non-command  line mode?
if(php_sapi_name()!="cli"){
    //Router
    $router = new Router();
    $router->delegate();
}

?>