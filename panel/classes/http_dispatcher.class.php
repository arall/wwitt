<?php

/* Dispatcher daemon receives full HTTP queries, processes them 
   and return the result of the query.
   The format is:
    UUID\n           # A unique numerical ID to identify the transaction.
    URL\n            # The full URL of the query (proto://host:port/path)
    Request-size\n   # The size in bytes of the request that follows (head+body)
    Request

   The answers come in the form of: 
   UUID\n
   Response-size\n
   Response
*/

class HttpResponse {
	public $unique_id;
	public $response;
	public $body;
	public $head;
	public $httpCode;

	function parse($buffer) {
		$p1 = strpos($buffer,"\n",0);
		if ($p1 === false) return 0;
		$p2 = strpos($buffer,"\n",$p1+1);
		if ($p2 === false) return 0;
		
		$this->unique_id = substr($buffer,0,$p1);
		$rsize = substr($buffer,$p1+1,$p2-2);

		if (strlen($buffer) - $p2 - 1 < $rsize) return 0;

		echo "OK " . $p1 . " ". $p2. " " . $rsize."--\n";
		$this->response = substr($buffer, $p2 + 1, $rsize);
		echo $this->response;

		$this->process();

		return $p2 + 1 + $rsize;
	}

	function process() {
		$sep = strpos($this->response,"\r\n\r\n");
		if ($sep === false) {
			$this->head = "";
			$this->body = "";
		}else{
			$this->head = substr($this->response, 0, $sep);
			$this->body = substr($this->response, $sep + 4);
		}
		$codes = explode(" ", $this->head);
		if (count($codes) > 1)
			$this->httpCode = $codes[1];
		else
			$this->httpCode = 0;
	}
}

class HttpRequest {
	public $unique_id;
	public $url, $body, $header;
	public $code;
	public $user_agent;
}

class HttpDispatcher {

	static $uid_gen = 0;
	static $resp_array = array();
	static $out_queue = "";
	static $in_queue = "";
	var $fdin, $fdout;

	// Create a connection with the dispatcher
	function HttpDispatcher($in_pipe, $out_pipe) {
		echo "Connecting to dispatcher...\n";
		$this->fdin  = fopen($in_pipe,  "rb");
		if ($this->fdin === false) echo "ERROR!\n";
		echo "Connected 1st step...\n";
		$this->fdout = fopen($out_pipe, "wb");
		if ($this->fdout === false) echo "ERROR!\n";
		echo "Connected 2nd step...\n";
		
		stream_set_blocking($this->fdin,  0);
		stream_set_blocking($this->fdout, 0);

		$this->default_user_agent = "Mozilla/5.0 (Windows; U; Windows NT 5.1; rv:22.0) Gecko/20100101 Firefox/22.0";
		$this->out_queue = "";
		$this->in_queue = "";
	}
	
	// Process IN/OUT data
	function process() {
		do {
			$data = fread($this->fdin, 1024);
			$this->in_queue .= $data;
		} while ($data != "");

		do {
			$written = fwrite($this->fdout, $this->out_queue);
			$this->out_queue = substr($this->out_queue, $written);
		} while ($written != 0);
		
		// Process data read from the dispatcher and process pending requests
		do {
			$resp = new HttpResponse();
			$ok = $resp->parse($this->in_queue);
			if ($ok) {
				$this->in_queue = substr($this->in_queue, $ok);
				echo "Received answer: *" . $ok . "* *" . $resp->unique_id. "* ##\n";
				echo "Bytes remaining " . strlen($this->in_queue). "\n";
				self::$resp_array[$resp->unique_id] = $resp;
			}
		} while ($ok);
	}
	
	function schedule($req) {
		$data_req = $req->head . $req->body;
		$enc_req =  $req->unique_id . "\n";
		$enc_req .= $req->url . "\n";
		$enc_req .= strlen($data_req) . "\n";
		$enc_req .= $data_req;

		echo "Scheduled request: " . $enc_req . "\n";

		$this->out_queue .= $enc_req;
	}

	function fetchGET($url, $user = "", $pass = "", $user_agent = "") {
		return $this->fetchRequest("GET", $url, "", $user, $pass, $user_agent);
	}

	function fetchPOST($url, $post, $user = "", $pass = "", $user_agent = "") {
		return $this->fetchRequest("GET", $url, $post, $user, $pass, $user_agent);
	}

	// More options to come, like NoBody, timeout... whatever
	function fetchRequest($method, $url, $content = "", $user = "", $pass = "", $user_agent = "") {
		if ($user_agent == "") $user_agent = $this->default_user_agent;
		self::$uid_gen += 1;

		$urlo = parse_url($url);
		
		$req = new HttpRequest();
		$req->unique_id = self::$uid_gen;
		$req->url = $url;
		$req->user_agent = $user_agent;

		if (!array_key_exists('path',$urlo)) $urlo['path'] = "/";
		$req->head = $method . " ".  $urlo['path'] . " HTTP/1.1\r\n";
		$req->head .= "Host: " . $urlo['host'] . "\r\n";
		$req->head .= "User-Agent: " . $user_agent . "\r\n";
		if ($content != "")
			$req->head .= "Content-Length: " . strlen($content) . "\r\n";
		if ($user != "" || $pass != "")
			$req->head .= "Authorization: Basic " . base64_encode($user . ":" . $pass) . "\r\n";
		$req->head .= "Connection: close\r\n\r\n";

		if ($content != "")
			$req->body = $content;
		
		$this->schedule($req);
		
		return self::$uid_gen;
	}

	function getResult($uid) {
		if (!array_key_exists($uid,self::$resp_array))
			return NULL;
		return self::$resp_array[$uid];
	}
}

?>
