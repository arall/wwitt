<? 
abstract class Model{
	public static $className;
	public static $dbTable;
	public static $reservedVars = array("className", "dbTable", "reservedVars", "reservedVarsChild", "idField");
	public static $reservedVarsChild = array();
	public static $idField = "";

	public function init(){}
	
    public function __construct($id=""){
    	global $db;
		$this->init();
		$this->className = get_class($this);
		$this->dbTable = self::$dbTable;
		$this->reservedVars = self::$reservedVars;
		$this->reservedVarsChild = self::$reservedVarsChild;
		$this->idField = self::$idField;
		if(!$this->idField){
			$this->idField = "id";
		}
		if($id){
			if(is_array($id)){
				$this->loadVarsArray($id);
			}else{
				$query = "SELECT * FROM ".$this->dbTable." WHERE ".$this->idField."=".(int)$id;
				if($db->query($query)){
					if($db->getNumRows()){
						$row = $db->fetcharray();
						$vars = array_keys(get_class_vars($this->className));
						foreach($row as $name=>$value) {
							if(in_array($name, $vars)){
								$this->$name = $value;
							}
						}
					}
				}
			}
		}
    }
    
    public function preUpdate(){}
    public function postUpdate(){}
   
    public function preInsert(){}
    public function postInsert(){}
    
    public function loadVarsArray($array){
		$vars = get_class_vars($this->className);
	    foreach($vars as  $name=>$value){
		    if(in_array($name,self::$reservedVars)) continue;
		    if(in_array($name,array_keys($vars))){
				$this->$name=($array[$name]);
			}
	    }
    }
    
    public function update($array=""){
	    global $db;
		//Load Array
	    if(is_array($array)){
		    self::loadVarsArray($array);
	    }
	    //Pre Update
	    $this->preUpdate($array);
	    //Prepare SQL vars
	    $values = array();
		foreach(get_class_vars($this->className) as  $name=>$value){
			if($name==$this->idField) continue;
			if(in_array($name,$this->reservedVars)) continue;
			if(in_array($name,$this->reservedVarsChild)) continue;
		    $values[$name] = "`".$name."`"."='".mysql_real_escape_string($this->$name)."'";
	    }
	    //SQL
	    $query = "UPDATE ".$this->dbTable." SET ".implode(" , ",$values)." WHERE ".$this->idField."=".(int)$this->id; 
		if($db->query($query)) {
	    	//Post Update
	    	$this->postUpdate();
	    	return 1;
	    }else{
	    	echo colorize($db->getError(), "error");
	    }
    }

    public function insert($array=""){
	    global $db;
	    //Load Array
	    if(is_array($array)){
	    	self::loadVarsArray($array);
	    }
	    //Pre Insert
		$this->preInsert();
		 //Prepare SQL vars
		foreach(get_class_vars($this->className) as $name=>$value) {
			if($name==$this->idField) continue;
		   	if(in_array($name,$this->reservedVars)) continue;
			if(in_array($name,$this->reservedVarsChild)) continue;
		    $values1[$name] = "`".$name."`";
		    $values2[$name]=" '".mysql_real_escape_string($this->$name)."' ";
		}
		//SQL
		$query = "INSERT INTO ".$this->dbTable." (".implode(" , ",$values1).") VALUES (".implode(" , ",$values2).")";
		if($db->query($query)) {
			$idField = $this->idField;
			$this->$idField = $db->lastid();
			//Post Insert
			$this->postInsert();
			return 1;
		}else{
	    	echo colorize($db->getError(), "error");
	    }
    }
}
?>