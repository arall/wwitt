<?php
//No direct access
defined('_EXE') or die('Restricted access');

class loginController extends Controller {
	
	public function init(){
	}
	
	public function index(){
		$this->login();
	}
	public function login(){
		//Load View to Template
		$html .= $this->view("views.login");
		//Render the Template
		$this->render($html);
	}

	public function doLogin(){
		//Try to login
		$user = new User();
		$res = $user->login($_REQUEST['login'], $_REQUEST['password']);
		//If login its ok...
		if($res==true){
			//Redirect to main page thought Message URL parameter
			Registry::addMessage("", "", "", Url::site());
		//If not...
		}else{
			//Create message
			Registry::addMessage("Login incorrect", "error", "login");
		}
		//Do not render the template, just ajax (Messages)
		$this->ajax();
	}

	public function doLogout(){
		User::logout();
		redirect(Url::site());
	}
}
?>