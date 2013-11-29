<?php
class User extends Model {
	
	var $id;
	var $username;
	var $password;
	var $lastvisitDate;

	public function init(){
		parent::$idField = "id";
		parent::$dbTable = "users";
		parent::$reservedVarsChild = self::$reservedVarsChild;
	}
	
	public function login($login, $password){
		$db = Registry::getDb();
		$query = "SELECT * FROM users WHERE 
		username='".htmlspecialchars(mysql_real_escape_string(trim($login)))."'
		AND password='".md5(sha1(trim($password)))."' LIMIT 1;";
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
	
	public function logout(){
		session_start();
		$_SESSION = array();
		session_unset();
		session_destroy();
		return true;
	}
}
?>