<?php
//No direct access
defined('_EXE') or die('Restricted access');

class virtualhostsController extends Controller {
	
	public function init(){
		//Must be logged
		$user = Registry::getUser();
		if(!$user->id){
			redirect(Url::site("login"));
		}
	}
	
	public function index(){
		$virtualHosts = VirtualHost::selectVirtualHosts();
		$this->setData("virtualHosts", $virtualHosts);
		$html .= $this->view("views.list");
		$this->render($html);
	}
}
?>