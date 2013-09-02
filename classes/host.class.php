<?php
class Host extends Model {
	var $id;
	var $ip;
	var $status;
	var $hostname;
	var $os;
	var $dateAdd;
	var $dateUpdate;
	
	public function init(){
		parent::$dbTable = "hosts";
	}

	function preInsert(){
		$this->dateAdd = date("Y-m-d H:i:s");
	}

	function preUpdate(){
		$this->dateUpdate = date("Y-m-d H:i:s");
	}

	public function getHosts(){
		global $db;
		$query = "SELECT * FROM hosts WHERE status=1";
		if($db->Query($query)){
			if($db->getNumRows()){
				$rows = $db->loadArrayList();
				foreach($rows as $row){
					$result[] = new Host($row);
				}
				return $result;
			}
		}
	}

	public function getServices(){
		global $db;
		$query = "SELECT * FROM services WHERE ipId=".(int)$this->id;
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

	public function exeBingIpScan(){
		global $config;
		$search = urlencode("IP:".$this->ip);
		$r = array();
		$current = 1;
		do{
			$url = 'http://www.bing.com/search?q='.$search."&first=".$current;
			$content = curl($url);
			if($content && !strstr($content, "no_results") ){
				$html = str_get_html($content);
				if(is_object($html)){
					foreach($html->find('div.sb_meta cite') as $index=>$link){
						if(is_object($link)){
							$url = urldecode($link->plaintext);
							$tmp = parse_url(cleanHostUrl($url));
							if(!VirtualHost::getVirtualHostByIpIdHost($this->id, $tmp['host'])){
								$virtualHost = new VirtualHost();
								$virtualHost->ipId = $this->id;
								$virtualHost->host = $tmp['host'];
								$virtualHost->url = getLastEffectiveUrl($tmp['host'], $virtualHost->index);
								$virtualHost->head = curlHead($virtualHost->host);
								$virtualHost->insert();
								echo colorize($tmp['host']);
							}else{
								echo colorize("VirtualHost '".$tmp['host']."' already added", "error");
							}
						}
						unset($link);
					}
					$html->clear();
				}
				
			}else{
				echo colorize("No results found", "error");
			}
			unset($html);
			$current += 10;
		}while($current<$config['bing']['maxPage']);
	}

	public function exePortScan(){
		global $config;
		//Define the target to scan
		try {
			$target = array($this->ip);
			$options = array('nmap_binary' => 'nmap');
		    $nmap = new Net_Nmap($options);
		    //Enable nmap options
		    $nmap_options = array('os_detection' => true,
		                          'service_info' => true,
		                          'port_ranges' => implode(",", $config['general']['ports'])
		                          );
		    $nmap->enableOptions($nmap_options);
		    //Scan target
		    $res = @$nmap->scan($target);
		    //Parse XML Output to retrieve Hosts Object
		    $hosts = @$nmap->parseXMLOutput();
		    //Print results
		    if(count($hosts)){
			    foreach ($hosts as $key => $h) {
			        $hostname = $h->getHostname();
			        if($hostname!="unknown"){
			        	$this->hostname = $hostname;
			        }
			        $this->status = 1;
			        $this->os = $h->getOS();
			        $services = $h->getServices();
			        $this->update();
			        if(count($services)){
				        foreach ($services as $key => $s) {
				        	if($s->product){
					            $service = new Service();
					            $service->ipId = $this->id;
					            $service->name = $s->name;
					            $service->port =  $s->port;
					            $service->protocol =  $s->protocol;
					            $service->product =  $s->product;
					            $service->version =  $s->version;
					            $service->info = $s->extrainfo;
					            $service->insert();
					            echo colorize("Port ".$service->port.": ".$service->product." ".$service->version);
				        	}
				        }
				    }
			    }
			}
		} catch (Net_Nmap_Exception $ne) {
		    echo $ne->getMessage();
		}
	}

	public static function getHostFromIp($ip){
		global $db;
		$query = "SELECT * FROM hosts WHERE ip='".htmlentities(mysql_real_escape_string($ip))."'";
		if($db->Query($query)){
			if($db->getNumRows()){
				$row = $db->fetcharray();
				return new Host($row);
			}
		}
	}
}
?>