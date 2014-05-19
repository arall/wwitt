<?php
class Service extends Model
{
    public $id;
    public $ip;
    public $port;
    public $filtered;
    public $head;
    public $protocol;
    public $product;
    public $version;
    public $dateInsert;

    public $statusesCss = array(
        0 => "success",
        1 => "danger",
    );
    public $statuses = array(
        0 => "Open",
        1 => "Filtered",
    );
    public static $reservedVarsChild = array("statuses", "statusesCss");

    public function init()
    {
        parent::$idField = "id";
        parent::$dbTable = "services";
        parent::$reservedVarsChild = self::$reservedVarsChild;
    }

    public function getFilteredString()
    {
        return $this->statuses[$this->filtered];
    }

    public function getFilteredCssString()
    {
        return $this->statusesCss[$this->filtered];
    }

    public static function select($data=array(), $limit=0, $limitStart=0, &$total=null)
    {
        $db = Registry::getDb();
        //Query
        $query = "SELECT * FROM `services` WHERE 1=1 ";
        //Where
        if ($data["ip"]) {
            $query .= " AND `ip`=".(int) $data["ip"];
        }
        if ($data["port"]) {
            $query .= " AND `port`=".(int) $data["port"];
        }
        //Total
        if ($db->Query($query)) {
            $total = $db->getNumRows();
            //Order
            if ($data['order'] && $data['orderDir']) {
                //Secure Field
                $orders = array("ASC", "DESC");
                if (@in_array($data['order'], array_keys(get_class_vars(__CLASS__))) && in_array($data['orderDir'], $orders)) {
                    $query .= " ORDER BY `".mysql_real_escape_string($data['order'])."` ".mysql_real_escape_string($data['orderDir']);
                }
            }
            //Limit
            if ($limit) {
                $query .= " LIMIT ".(int) $limitStart.", ".(int) $limit;
            }
            if ($total) {
                if ($db->Query($query)) {
                    if ($db->getNumRows()) {
                        $rows = $db->loadArrayList();
                        foreach ($rows as $row) {
                            $results[] = new Service($row);
                        }

                        return $results;
                    }
                }
            }
        }
    }
}
