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
set_include_path("libs".DIRECTORY_SEPARATOR.'phpseclib');
include('Net/SSH2.php');

//Registry
$registry = new Registry();

//Router
$router = new Router();
$router->delegate();

?>