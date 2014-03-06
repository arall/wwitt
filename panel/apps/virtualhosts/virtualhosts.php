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
		$config = Registry::getConfig();
		$pag['total'] = 0;
		$pag['limit'] = $_REQUEST['limit'] ? $_REQUEST['limit'] : $config->get("defaultLimit");
		$pag['limitStart'] = $_REQUEST['limitStart'];
		$virtualHosts = VirtualHost::select($_REQUEST, $pag['limit'], $pag['limitStart'], $pag['total']);
		$this->setData("virtualHosts", $virtualHosts);
		$this->setData("pag", $pag);
		$html .= $this->view("views.list");
		$this->render($html);
	}
}
