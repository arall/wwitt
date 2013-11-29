<?php
class Host extends Model {
	
	var $ip;
	var $ipAdress;
	var $status;
	var $hostname;
	var $os;
	var $dateAdd;
	var $dateUpdate;
	
	var $totalServices;
	var $totalVirtualhosts;

	public function init(){
		parent::$idField = "ip";
		parent::$dbTable = "viewHosts";
		parent::$reservedVarsChild = self::$reservedVarsChild;
	}

	public function selectHosts(){
		$db = Registry::getDb();
		$query = "SELECT * FROM viewHosts";
		if($db->Query($query)){
			if($db->getNumRows()){
				$rows = $db->loadArrayList();
				foreach($rows as $row){
					$result[] = new Host($row);
				}
				return $result;
			}
		}
	}

	public function getServices(){
		$db = Registry::getDb();
		$query = "SELECT * FROM services WHERE ip=".(int)$this->ip;
		if($db->Query($query)){
			if($db->getNumRows()){
				$rows = $db->loadArrayList();
				foreach($rows as $row){
					$result[] = new Service($row);
				}
				return $result;
			}
		}	
	}

	public function getVirtualHosts(){
		$db = Registry::getDb();
		$query = "SELECT * FROM virtualhosts WHERE ip=".(int)$this->ip;
		if($db->Query($query)){
			if($db->getNumRows()){
				$rows = $db->loadArrayList();
				foreach($rows as $row){
					$result[] = new VirtualHost($row);
				}
				return $result;
			}
		}
	}
}
?>