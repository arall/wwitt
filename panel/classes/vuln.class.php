<?php
class Vuln extends Model {
	
	public $module;
	public $name;
	public $type;
	public $protocol;
	public $minVersion;
	public $maxVersion;
	public $dateInsert;

	public function init(){
		parent::$idField = "module";
		parent::$dbTable = "vulns";
		parent::$reservedVarsChild = self::$reservedVarsChild;
	}

	public function exploit($host, $port, $virtualHost){
		//Search for exploit module
		if(file_exists("classes/exploitModules/".$this->module.".php")){
			include("classes/exploitModules/".$this->module.".php");
			$module = new $this->exploitModule($this, $host, $port, $virtualHost);
			if($module->preRun()){
				echoCli("Executing ".$this->name." exploit against ".$host->ipAdress.":".$port, "title");
				$res = $module->run();
				if(!CLI_DEBUG){
					$module->save();
				}
				return $res;
			}
		}else{
			Registry::addMessage("Module '".$this->module."' not found", "error");
		}
		return false;
	}

	public function select($data=array()){
		$db = Registry::getDb();
		$query = "SELECT * FROM vulns WHERE 1=1 ";
		if($data["protocol"]){
			$query .= "AND protocol=".(int)$data["protocol"];
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
