<?php
//No direct access
defined('_EXE') or die('Restricted access');

class loginController extends Controller {

	public function init(){}

	public function index(){
		$this->login();
	}

	public function login(){
		$html = $this->view("views.login");
		$this->render($html);
	}

	public function doLogin(){
		$user = new User();
		$res = $user->login($_REQUEST['login'], $_REQUEST['password']);
		if($res==true){
			Registry::addMessage("", "", "", Url::site());
		}else{
			Registry::addMessage("Login incorrect", "error", "login");
		}
		$this->ajax();
	}

	public function doLogout(){
		$user = new User();
		$user->logout();
		redirect(Url::site());
	}
}
