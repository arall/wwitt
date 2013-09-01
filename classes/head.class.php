<?php
class Head extends Model {
	var $id;
	var $ipId;
	var $port;
	var $head;
	var $dateAdd;

	public function init(){
		parent::$dbTable = "heads";
	}

	function preInsert(){
		$this->dateAdd = date("Y-m-d H:i:s");
	}
}
?>