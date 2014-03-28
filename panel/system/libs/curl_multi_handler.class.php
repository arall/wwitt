<?php
class CurlFork{
    private $_handles = array();
    private $_mh      = array();
 
    function __construct(){
        $this->_mh = curl_multi_init();
    }
 
    function add($ch){
        curl_multi_add_handle($this->_mh, $ch);
        $this->_handles[] = $ch;
        return $this;
    }
 
    function run(){
        $running=null;
        do {
            curl_multi_exec($this->_mh, $running);
        } while ($running > 0);
        for($i=0; $i < count($this->_handles); $i++) {
            $data[$i]["code"] = curl_multi_getcontent($this->_handles[$i]);;
            $data[$i]["data"] = curl_getinfo($this->_handles[$i], CURLINFO_HTTP_CODE);
            curl_multi_remove_handle($this->_mh, $this->_handles[$i]);
        }
        curl_multi_close($this->_mh);
        return $data;
    }
}
?>