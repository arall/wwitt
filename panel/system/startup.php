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
//SQL Formater (For better debugging)
include 'system/libs/SqlFormatter.php';
//Colorize CLI
include("system/libs/colorize.php");
//Telnet
include('system/libs/telnet.class.php');
//SSH
set_include_path("system/libs/phpseclib");
include('Net/SSH2.php');

//Registry
$registry = new Registry();

//Router
$router = new Router();
$router->delegate();

