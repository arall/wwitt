<?php
class Vuln extends Model {
	
	var $id;
	var $name;
	var $type;
	var $port;
	var $exploitModule;
	var $dateAdd;

	public function init(){
		parent::$idField = "id";
		parent::$dbTable = "vulns";
		parent::$reservedVarsChild = self::$reservedVarsChild;
	}

	public function exploit($host, $port, $virtualHost){
		//Search for exploit module
		if(file_exists("classes/exploitModules/".$this->exploitModule.".php")){
			include("classes/exploitModules/".$this->exploitModule.".php");
			$module = new $this->exploitModule($this, $host, $port, $virtualHost);
			if($module->preRun()){
				echoCli("Executing ".$this->exploitModule." exploit against ".$host->ipAdress.":".$port, "title");
				$res = $module->run();
				if(!CLI_DEBUG){
					$module->save();
				}
				return $res;
			}
		}else{
			Registry::addMessage("Module '".$this->exploitModule."' not found", "danger");
		}
		return false;
	}

	public function selectVulns($port=0){
		$db = Registry::getDb();
		$query = "SELECT * FROM vulns WHERE 1=1 ";
		if($port){
			$query .= "AND port=".(int)$port;
		}
		if($db->Query($query)){
			if($db->getNumRows()){
				$rows = $db->loadArrayList();
				foreach($rows as $row){
					$result[] = new Vuln($row);
				}
				return $result;
			}
		}
	}
}
?>