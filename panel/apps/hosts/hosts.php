<?php
//No direct access
defined('_EXE') or die('Restricted access');

class hostsController extends Controller {
	
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
		$hosts = Host::select($_REQUEST, $pag['limit'], $pag['limitStart'], $pag['total']);
		$this->setData("hosts", $hosts);
		$this->setData("pag", $pag);
		$html .= $this->view("views.list");
		$this->render($html);
	}

	public function details(){
		$url = Registry::getUrl();
		$host = new Host($url->vars[0]);
		if($host->ip){
			$services = $host->getServices();
			$this->setData("services", $services);
			$this->setData("host", $host);
			$html .= $this->view("views.view");
			$this->render($html);
		}else{
			redirect(Url::site("hosts"), "Invalid IP", "danger");
		}
	}

	public function exploit(){
		$host = new Host($_REQUEST['ip']);
		if($host->ip){
			$vuln = new Vuln($_REQUEST['vuln']);
			if($vuln->module){
				$virtualHost = null;
				$port = $_REQUEST['port'];
				if($_REQUEST['host']){
					$virtualHost = new virtualHost($_REQUEST['host']);
				}
				$res = $vuln->exploit($host, $port, $virtualHost);
				if($res){
					Registry::addMessage("Vuln exploited successfully", "success");
				}else{
					Registry::addMessage("Exploited failed", "danger");
				}
			}else{
				Registry::addMessage("Invalid Vuln", "danger");
			}
			redirect(Url::site("hosts/details/".$host->ip));
		}else{
			Registry::addMessage("Invalid Host", "danger");
			redirect(Url::site("hosts"));
		}
	}
}
