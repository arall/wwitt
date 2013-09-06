
import urllib   # For urlencode

class Index_Crawler:

	# Querys all indexes using HTTP requests
	def getIndexes():
		

	def exeBingIpScan():
		search = urllib.urlencode("IP:"+self._ip)
		current = 1
		end = False
		while not end:
			url = 'http://www.bing.com/search?q='.search."&first=".str(current)
			content
			if "no_results" in content:
				end = True
			else:
				



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
