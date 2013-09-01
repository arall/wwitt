<?php
class Service extends Model {
	var $id;
	var $ipId;
	var $port;
	var $head;
	var $name;
	var $protocol;
	var $product;
	var $version;
	var $info;
	var $dateAdd;

	public function init(){
		parent::$dbTable = "services";
	}

	function preInsert(){
		$this->dateAdd = date("Y-m-d H:i:s");
	}
}
?>