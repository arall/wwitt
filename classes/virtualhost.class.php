<?php
class VirtualHost extends Model {
	var $id;
	var $ipId;
	var $host;
	var $url;
	var $head;
	var $index;
	var $dateAdd;

	public function init(){
		parent::$dbTable = "virtualhosts";
	}

	function preInsert(){
		$this->dateAdd = date("Y-m-d H:i:s");
	}

	public function getVirtualHostByIpIdHost($ipId, $host){
		global $db;
		$query = "SELECT * FROM virtualhosts WHERE ipId='".htmlentities(mysql_real_escape_string($ipId))."' AND host='".htmlentities(mysql_real_escape_string($host))."'";
		if($db->Query($query)){
			if($db->getNumRows()){
				$row = $db->fetcharray();
				return new VirtualHost($row);
			}
		}
	}
}
?>