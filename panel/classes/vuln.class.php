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
			$module = new $this->module($this, $host, $port, $virtualHost);
			if($module->preRun()){
				echoCli("Executing ".$this->name." exploit against ".$host->ipAdress.":".$port, "title");
				$res = $module->run();
				if(!defined('CLI_DEBUG')){
					$module->save();
				}
				return $res;
			}
		}else{
			Registry::addMessage("Module '".$this->module."' not found", "error");
		}
		return false;
	}

	public static function select($data=array(), $limit=0, $limitStart=0, &$total=null){
		$db = Registry::getDb();
        //Query
		$query = "SELECT * FROM `vulns` WHERE 1=1 ";
		//Where
		if($data["protocol"]){
			$query .= " AND protocol='".mysql_real_escape_string($data["protocol"])."'";
		}
		//Total
		if($db->Query($query)){
			$total = $db->getNumRows();
			//Order
			if($data['order'] && $data['orderDir']){
				//Secure Field
				$orders = array("ASC", "DESC");
				if(@in_array($data['order'], array_keys(get_class_vars(__CLASS__))) && in_array($data['orderDir'], $orders)){
					$query .= " ORDER BY `".mysql_real_escape_string($data['order'])."` ".mysql_real_escape_string($data['orderDir']);
				}
			}
			//Limit
			if($limit){
				$query .= " LIMIT ".(int)$limitStart.", ".(int)$limit;
			}
			if($total){
				if($db->Query($query)){
					if($db->getNumRows()){
						$rows = $db->loadArrayList();
						foreach($rows as $row){
							$results[] = new Vuln($row);
						}
						return $results;
					}
				}
			}
		}
	}
}
