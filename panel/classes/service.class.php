<?php
class Service extends Model {
	
	public $id;
	public $ip;
	public $port;
	public $filtered;
	public $head;
	public $name;
	public $protocol;
	public $product;
	public $version;
	public $info;
	public $dateInsert;

	public $statusesCss = array(
		0 => "danger",
		1 => "success",
	);
	public $statuses = array(
		0 => "Filtered",
		1 => "Open",
	);
	public static $reservedVarsChild = array("statuses", "statusesCss");
	
	public function init(){
		parent::$idField = "id";
		parent::$dbTable = "services";
		parent::$reservedVarsChild = self::$reservedVarsChild;
	}

	public function getFilteredString(){
		return $this->statuses[$this->filtered];
	}

	public function getFilteredCssString(){
		return $this->statusesCss[$this->filtered];
	}
}
