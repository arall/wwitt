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
		$hosts = Host::selectHosts();
		$this->setData("hosts", $hosts);
		$html .= $this->view("views.list");
		$this->render($html);
	}

	public function details(){
		$url = Registry::getUrl();
		$host = new Host($url->vars[0]);
		if($host->ip){
			$services = $host->getServices();
			$virtualHosts = $host->getVirtualHosts();
			$this->setData("virtualHosts", $virtualHosts);
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
			$vuln = new Vuln($_REQUEST['vulnId']);
			if($vuln->id){
				$virtualHost = null;
				$port = $_REQUEST['port'];
				if($_REQUEST['virtualHostId']){
					$virtualHost = new virtualHost($_REQUEST['virtualHostId']);
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
?>