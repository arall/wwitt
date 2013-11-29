<?php
class Vuln_Host extends Model {
	
	var $id;
	var $ip;
	var $vulnId;
	var $port;
	var $virtualhostId;
	var $status;
	var $data;
	var $dateAdd;
	var $dateUpdate;

	public function init(){
		parent::$idField = "id";
		parent::$dbTable = "vulns_hosts";
		parent::$reservedVarsChild = self::$reservedVarsChild;
	}

	public function preInsert(){
		$this->dateAdd = date("Y-d-m H:i:s");
	}

	public function preUpdate(){
		$this->dateUpdate = date("Y-d-m H:i:s");
	}

	public function selectVulns(){
		$db = Registry::getDb();
		$query = "SELECT * FROM vulns_hosts";
		if($db->Query($query)){
			if($db->getNumRows()){
				$rows = $db->loadArrayList();
				foreach($rows as $row){
					$result[] = new Vuln_Host($row);
				}
				return $result;
			}
		}
	}

	public function getVuln_hostByVulnIdIpPortVirtualHostId($vulnId, $ip, $port, $virtualhostId=0){
		$db = Registry::getDb();
		$query = "SELECT * FROM vulns_hosts WHERE 
			vulnId=".(int)$vulnId." AND 
			ip=".(int)$ip." AND 
			port=".(int)$port." AND 
			virtualhostId=".(int)$virtualhostId;
		if($db->Query($query)){
			if($db->getNumRows()){
				$rows = $db->loadArrayList();
				return new Vuln_Host($rows[0]);
			}
		}
	}
}
?>