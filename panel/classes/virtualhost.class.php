<?php
class VirtualHost extends Model {
	
	public $ip;
	public $host;
	public $url;
	public $head;
	public $index;
	public $robots;
	public $dateInsert;
	
	public function init(){
		parent::$idField = "ip";
		parent::$dbTable = "virtuahosts";
		parent::$reservedVarsChild = self::$reservedVarsChild;
	}
	
	public function select($data=array(), $limit=0, $limitStart=0, &$total=null){
		$db = Registry::getDb();
        //Query
		$query = "SELECT * FROM `virtualhosts` WHERE 1=1 ";
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
							$results[] = new VirtualHost($row);
						}
						return $results;
					}
				}
			}
		}
	}
}
