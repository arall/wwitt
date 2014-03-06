<?php
class Host extends Model {
	
	public $ip;
	public $ipAdress;
	public $status;
	public $hostname;
	public $os;
	public $dateInsert;
	public $dateUpdate;
	
	public $totalServices;
	public $totalVirtualhosts;

	public $statusesCss = array(
		0 => "danger",
		1 => "success",
	);
	public $statuses = array(
		0 => "Not explited",
		1 => "Exploited",
	);
	public static $reservedVarsChild = array("statusesCss", "statuses");

	public function init(){
		parent::$idField = "ip";
		parent::$dbTable = "viewHosts";
		parent::$reservedVarsChild = self::$reservedVarsChild;
	}

	public function getStatusString(){
		return $this->statuses[$this->status];
	}

	public function getStatusCssString(){
		return $this->statusesCss[$this->status];
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
	
	public function select($data=array(), $limit=0, $limitStart=0, &$total=null){
		$db = Registry::getDb();
        //Query
		$query = "SELECT * FROM `viewHosts` WHERE 1=1 ";
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
							$results[] = new Host($row);
						}
						return $results;
					}
				}
			}
		}
	}
}
