<?php
class VulnHost extends Model {

	public $id;
	public $ip;
	public $host;
	public $vuln;
	public $port;
	public $status;
	public $data;
	public $dateInsert;
	public $dateUpdate;

	public function init(){
		parent::$idField = "id";
		parent::$dbTable = "vulns_hosts";
		parent::$reservedVarsChild = self::$reservedVarsChild;
	}

	public function preInsert(){
		$this->dateInsert = date("Y-d-m H:i:s");
	}

	public function preUpdate(){
		$this->dateUpdate = date("Y-d-m H:i:s");
	}

	public function getVulnHostByVulnIpPortHost($ip=null, $port=null, $host=null){
		$db = Registry::getDb();
		$query = "SELECT * FROM `vulns_hosts` WHERE `ip`='".mysql_real_escape_string($ip)."'
		AND `port`='".mysql_real_escape_string($port)."' AND `host`='".mysql_real_escape_string($host)."'";
		if($db->query($query)){
			if($db->getNumRows()){
				$row = $db->fetcharray();
				return new VulnHost($row);
			}
		}
	}

	public static function select($data=array(), $limit=0, $limitStart=0, &$total=null){
		$db = Registry::getDb();
        //Query
		$query = "SELECT * FROM `vulns_hosts` WHERE 1=1 ";
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
							$results[] = new VulnHost($row);
						}
						return $results;
					}
				}
			}
		}
	}
}
