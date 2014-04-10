<?php
class User extends Model {

	public $id;
	public $username;
	public $password;
	public $dateInsert;
	public $dateUpdate;
	public $lastvisitDate;

	public function init(){
		parent::$dbTable = "users";
		parent::$reservedVarsChild = self::$reservedVarsChild;
	}

	public function preInsert($data=array()){
		$config = Registry::getConfig();
		//Passwd encryption
		$this->password = User::encrypt($this->password);
		//Register Date
		$this->dateInsert = date("Y-m-d H:i:s");
	}

	public function preUpdate($data=array()){
		//Prevent blank password override
		if($data['password']){
			$this->password = User::encrypt($data['password']);
		}else{
			$this->password = null;
		}
		//Update Date
		$this->dateUpdate = date("Y-m-d H:i:s");
	}

	public function login($login, $password){
		$db = Registry::getDb();
		$query = "SELECT * FROM `users` WHERE
		`username`='".htmlspecialchars(mysql_real_escape_string(trim($login)))."'
		AND `password`='".User::encrypt($password)."'LIMIT 1;";
		if($db->query($query)){
			if($db->getNumRows()){
				$row = $db->fetcharray();
				$user = new User($row);
				//Set Session
				session_start();
				$_SESSION['userId'] = $user->id;
				//Update lastVisitDate
				$user->lastvisitDate = date("Y-m-d H:i:s");
				$user->update();
                return true;
			}
		}
	}

	public static function logout(){
		session_start();
		$_SESSION = array();
		session_unset();
		session_destroy();
		return true;
	}

	public static function encrypt($password=""){
		return md5(sha1(trim($password)));
	}

	public static function select($data=array(), $limit=0, $limitStart=0, &$total=null){
		$db = Registry::getDb();
        //Query
		$query = "SELECT * FROM `users` WHERE 1=1 ";
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
							$results[] = new User($row);
						}
						return $results;
					}
				}
			}
		}
	}

	public function getUserByUsername($username, $ignoreId=0){
		$db = Registry::getDb();
		$query = "SELECT * FROM `users` WHERE `username`='".htmlentities(mysql_real_escape_string($username))."'";
		if($ignoreId){
			$query .= " AND `id` !=".(int)$ignoreId;
		}
		if($db->Query($query)){
			if($db->getNumRows()){
				$row = $db->fetcharray();
				return new User($row);
			}
		}
	}
}
