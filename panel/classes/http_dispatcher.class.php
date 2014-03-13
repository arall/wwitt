<?php

class HttpRequest {
	public $unique_id;
	public $url, $body, $header;
	public $code;
	public $user_agent;
}

class HttpDispatcher {

	static $uid_gen = 0;
	static $req_array = array();
	static $out_queue = "";
	static $in_queue = "";
	var $fdin, $fdout;

	// Create a connection with the dispatcher
	function HttpDispatcher($in_pipe, $out_pipe) {
		$fdin  = fopen($in_pipe,  "rb");
		$fdout = fopen($out_pipe, "wb");
		
		stream_set_blocking($fdin,  0);
		stream_set_blocking($fdout, 0);
	}
	
	// Process IN/OUT data
	function process() {
		do {
			$data = fread($fdin, 1024);
			$in_queue += $data;
		} while ($data != "");

		do {
			$written = fwrite($fdout, $out_queue);
			$out_queue = substr($written, $out_queue);
		} while ($written != 0);
		
		// Process data read from the dispatcher and process pending requests
		// TODO
	}
	
	function schedule($req) {
		$out_queue += $req->url + "\n";
	}
	
	// More options to come, like NoBody, timeout... whatever
	function fetchGET($url, $user = "", $pass = "", $user_agent = "") {
		$uid_gen += 1;
		
		$req = new HttpRequest();
		$req->unique_id = $uid_gen;
		$req->url = $url;
		$req->user_agent = $user_agent;
		
		$req_array[$uid_gen] = $req;
		
		schedule($req);
		
		return $uid_gen;
	}
}

?>
