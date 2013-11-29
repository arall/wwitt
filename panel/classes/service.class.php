<?php
class Service extends Model {
	
	var $id;
	var $ip;
	var $port;
	var $status;
	var $head;
	var $name;
	var $protocol;
	var $product;
	var $version;
	var $info;
	var $dateAdd;

	var $statuses = array("Filtered", "Open");
	public static $reservedVarsChild = array("statuses");
	
	public function init(){
		parent::$idField = "id";
		parent::$dbTable = "services";
		parent::$reservedVarsChild = self::$reservedVarsChild;
	}

	public function getStatusToString(){
		return $this->statuses[$this->status];
	}
}
?>