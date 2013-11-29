<?php
class Vuln extends Model {
	
	var $id;
	var $name;
	var $type;
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
			if($module->preRun){
				$res = $module->run();
				$module->save();
				return $res;
			}
		}else{
			Registry::addMessage("Module '".$this->exploitModule."' not found", "danger");
		}
		return false;
	}

	public function selectVulns(){
		$db = Registry::getDb();
		$query = "SELECT * FROM vulns";
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