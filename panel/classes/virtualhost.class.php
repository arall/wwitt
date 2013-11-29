<?php
class VirtualHost extends Model {
	
	var $id;
	var $ip;
	var $host;
	var $head;
	var $index;
	var $robots;
	var $dateAdd;
	
	public function init(){
		parent::$idField = "id";
		parent::$dbTable = "virtuahosts";
		parent::$reservedVarsChild = self::$reservedVarsChild;
	}

	public function selectVirtualHosts(){
		$db = Registry::getDb();
		$query = "SELECT * FROM virtualhosts";
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