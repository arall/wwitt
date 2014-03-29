<?php

//Configuration
include("config.php");

//Session start
session_start();

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
//SQL Formater (For better debugging)
include 'system/libs/SqlFormatter.php';
//Colorize CLI
include("system/libs/colorize.php");
//Telnet
include('system/libs/telnet.class.php');
//SSH
set_include_path("system/libs/phpseclib");
include('Net/SSH2.php');
//Dispatcher
include('system/libs/http_dispatcher.class.php');

//Registry
$registry = new Registry();

if(!defined('CLI_DEBUG')){
    //Router
    $router = new Router();
    $router->delegate();
}
