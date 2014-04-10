<?php
class VirtualHost extends Model {

	public $host;
	public $status;
	public $url;
	public $head;
	public $index;
	public $robots;
	public $dateInsert;

	public $statusesCss = array(
		0 => "danger",
		1 => "success",
	);
	public $statuses = array(
		0 => "Not crawled",
		1 => "Crawled",
	);
	public static $reservedVarsChild = array("statusesCss", "statuses");

	public function init(){
		parent::$idField = "host";
		parent::$dbTable = "virtualhosts";
		parent::$reservedVarsChild = self::$reservedVarsChild;
	}

	public function getStatusString(){
		return $this->statuses[$this->status];
	}

	public function getStatusCssString(){
		return $this->statusesCss[$this->status];
	}

	public static function select($data=array(), $limit=0, $limitStart=0, &$total=null){
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

	public function cmsDetector($index="", $robots=""){
		if(!$index){
			$index = $this->index;
		}
		if(!$robots){
			$robots = $this->robots;
		}
		if($index){
			//CMS Identification
			preg_match_all("|<meta[^>]+name=\"([^\"]*)\"[^>]" . "+content=\"([^\"]*)\"[^>]+>|i", $index, $out, PREG_PATTERN_ORDER);
			//Got meta-generator?
			if(isset($out["generator"])){
				$metaGenerator = strtolower($out["generator"]);
				if(strstr($metaGenerator, "joomla")){
					$info['cms']['name'] = "joomla";
				}elseif(strstr($metaGenerator, "wordpress")){
					$info['cms']['name'] = "wordpress";
				}elseif(strstr($metaGenerator, "drupal")){
					$info['cms']['name'] = "drupal";
				}elseif(strstr($metaGenerator, "prestashop")){
					$info['cms']['name'] = "prestashop";
				}elseif(strstr($metaGenerator, "prestashop")){
					$info['cms']['name'] = "vbulletin";
				}elseif(strstr($metaGenerator, "phpbb")){
					$info['cms']['name'] = "phpbb";
				}
			}
			//Try other methods (In relevance order)
			if(!$info['cms']['name']){
				//Robots.txt
				if(!$info['cms']['name'] && $robots){
					if(strstr($robots, "administrator") && strstr($robots, "components")){
						$info['cms']['name'] = "joomla";
					}
				}
				//Joomla admin?
				if(!$info['cms']['name']){
					$administrator = curl($this->url."/administrator");
					if(strstr($administrator, "form-login") || strstr($administrator, "login-form"))
						$info['cms']['name'] = "joomla";
				}
				//Wordpress admin?
				if(!$info['cms']['name']){
					$admin = curl($this->url."/wp-login.php");
					if(strstr($admin, "user_login"))
						$info['cms']['name'] = "wordpress";
				}
				//Wordpres css?
				if(!$info['cms']['name']){
					if(strstr($index, "/wp-content/themes/"))
						$info['cms']['name'] = "wordpress";
				}
				//Drupal CHANGELOG.txt
				if(!$info['cms']['name']){
					$changelog = curl($this->url."/CHANGELOG.txt");
					if(strstr($changelog, "drupal"))
						$info['cms']['name'] = "drupal";
				}
				//Joomla Configuration.php
				if(!$info['cms']['name']){
					$statusCode = curlHead($this->url."/configuration.php");
					if($statusCode==200){
						$configContent = trim(curl($this->url."/config.php"));
						if(!$configContent){
							$info['cms']['name'] = "joomla";
						}
					}
				}
				//phpBB (powered by)
				if(!$info['cms']['name']){
					if(strstr($index, "&copy; phpBB Group"))
						$info['cms']['name'] = "phpbb";
				}

			}
			//Got cms-name?
			if($info['cms']['name']){
				//Get Version
				switch($info['cms']['name']){
					case 'joomla':
						$info['cms']['version'] = $this->getJoomlaVersion();
					break;
					case 'wordpress':
						$info['cms']['version'] = $this->getWordpressVersion();
					break;
					case 'drupal':
						$info['cms']['version'] = $this->getDrupalVersion();
					break;
					case 'vbulletin':
						$info['cms']['version'] = $this->getVBulletinVersion();
					break;
				}
				//Get Theme
				switch($info['cms']['name']){
					case 'joomla':
						$info['cms']['theme'] = (string)get_between($index, "/templates/", "/");
					break;
					case 'wordpress':
					case 'prestashop':
						$info['cms']['theme'] = (string)get_between($index, "/themes/", "/");
					break;
					case 'phpbb':
						$info['cms']['theme'] = (string)get_between($index, "/styles/", "/");
					break;
				}
				//Get Plugins
				switch($info['cms']['name']){
					case 'wordpress':
						$plugins = get_between_multi($index, "/plugins/", "/");
						if(count($plugins)){
							$plugins = array_unique($plugins);
							foreach($plugins as $plugin){
								//Clean (js)
								if(strstr($plugin, "'")){
									$plugin = substr($plugin, 0, strpos($plugin, "'"));
								}
								if(strstr($plugin, '"')){
									$plugin = substr($plugin, 0, strpos($plugin, '"'));
								}
								$plugins_clean[] = $plugin;
							}
							$info['cms']['plugins']['plugin'] = array_unique($plugins_clean);
						}
					break;
					case 'prestashop':
						$modules = get_between_multi($index, "/modules/", "/");
						if(count($modules)){
							$modules = array_unique($modules);
							$info['cms']['modules']['module'] = array_unique($modules);
						}
					break;
				}
			}
			return $info['cms'];
		}
	}

	public function getVBulletinVersion($index=null){
		if(!$index){
			$index = $this->index;
		}
		$version = get_between($index, '<meta name="generator" content="vBulletin ', '" />');
		if($version)
			return (string)$version;
		if(strstr($index, "vbulletin-core.js?v=")){
			$version = get_between($index, 'vbulletin-core.js?v=', '"');
			if($version)
				return (string)$version;
		}
		/* TO DO */
	}

	public function getWordpressVersion($index=null){
		if(!$index){
			$index = $this->index;
		}
		$version = get_between($index, '<meta name="generator" content="WordPress ', '" />');
		if($version)
			return (string)$version;
		/* TO DO */
	}

	public function getDrupalVersion(){
		//http://drupal.org/node/27362
		//Changelog
		$changelog = curl($this->url."/CHANGELOG.txt");
		$version = get_between($changelog, 'Drupal ', ',');
		if($version)
			return (string)$version;
		//includes/bootstrap.inc
		$bootstrap = curl($this->url."/includes/bootstrap.inc");
		$version = get_between($bootstrap, "define('VERSION', '", "'");
		if($version)
			return (string)$version;
		/* TO DO */
	}

	public function getJoomlaVersion($index=null){
		if(!$index){
			$index = $this->index;
		}
		//https://www.gavick.com/magazine/how-to-check-the-version-of-joomla.html
		$siteTemplateCss = curl($this->url."/templates/system/css/template.css");
		$siteSystemCss = curl($this->url."/templates/system/css/system.css");
		$siteMootools = curl($this->url."/media/system/js/mootools-more.js");
		$siteLang = curl($this->url."/language/en-GB/en-GB.ini");
		$siteLangXML = curl($this->url."language/en-GB/en-GB.xml");
		$joomlaVersion = "Unknown";
		//Joomla 1.0.x
		if(strstr($index, '2005 - 2007 Open Source Matters')){
			$joomlaVersion = "1.0.x";
		}
		//Joomla 1.5
		if(strstr($index, 'Joomla! 1.5 ') || strstr($siteTemplateCss, "2005 - 2010 Open Source Matters")){
			$joomlaVersion = "1.5";
		}
		//Joomla 1.5.26
		if(strstr($siteLang, 'Id: en-GB.ini 11391 2009-01-04 13:35:50Z ian')){
			$joomlaVersion = "1.5.26";
		}
		//Joomla 1.6
		if(strstr($index, 'Joomla! 1.6 ') ||
		strstr($siteSystemCss, "20196 2011-01-09 02:40:25Z")){
			$joomlaVersion = "1.6";
		}
		//Joomla 1.6.0 && Joomla 1.6.5
		if(strstr($siteLang, 'Id: en-GB.ini 20196 2011-01-09 02:40:25Z ian')){
			$joomlaVersion = "1.6.0 - 1.6.5";
		}
		//Joomla 1.7
		if(strstr($index, 'Joomla! 1.7 ') ||
		strstr($siteSystemCss, "21322 2011-05-11 01:10:29Z dextercowley")){
			$joomlaVersion = "1.7";
		}
		//Joomla 1.7.1 && Joomla 1.7.3
		if(strstr($siteLang, 'Id: en-GB.ini 20990 2011-03-18 16:42:30Z infograf768')){
			$joomlaVersion = "1.7.1 - 1.7.3";
		}
		//Joomla 2.5
		if(strstr($index, 'Joomla! 2.5 ') ||
		strstr($siteTemplateCss, "2005 - 2012 Open Source Matters")){
			$joomlaVersion = "2.5";
		}
		//Joomla 1.5.15, 1.7.1, 2.5.0-2.5.6
		if($siteLangXML){
			$siteXmlLang = @simplexml_load_string($siteLangXML);
			if(is_object($siteXmlLang )){
				if($siteXmlLang->version){
					$joomlaVersion = $siteXmlLang->version;
				}
			}
		}
		//Joomla 3.0 alpha 2
		if(strstr($siteMootools, 'MooTools.More={version:"1.4.0.1"') && !$joomlaVersion){
			$joomlaVersion = "3.0 alpha 2";
		}
		return (string)$joomlaVersion;
	}
}
