<?php
class Host extends Model {
	var $id;
	var $ip;
	var $status;
	var $dateAdd;
	var $dateUpdate;
	
	public function init(){
		parent::$dbTable = "hosts";
	}

	function preInsert(){
		$this->dateAdd = date("Y-m-d H:i:s");
	}

	function preUpdate(){
		$this->dateUpdate = date("Y-m-d H:i:s");
	}

	public function exeHeaders($port){
		$banner = trim(getHeader($this->ip, $port));
        if($banner){
        	$head = new Head();
        	$head->ipId = $this->id;
        	$head->port = $port;
        	$head->head = $banner;
        	$head->insert();
            echo colorize("Port ".$port.": ".$banner, "success");
        }
	}

	public static function getHostFromIp($ip){
		global $db;
		$query = "SELECT * FROM hosts WHERE ip='".htmlentities(mysql_real_escape_string($ip))."'";
		if($db->Query($query)){
			if($db->getNumRows()){
				$row = $db->fetcharray();
				return new Host($row);
			}
		}
	}
}
?>