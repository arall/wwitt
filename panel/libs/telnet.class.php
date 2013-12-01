<?php

class Telnet
{
  public $host;
  public $port;
  public $user;
  public $pass;
  public $connected;
  private $connect_timeout;
  private $stream_timetout;

  private $socket;

  public function Telnet()
  {
    $this->port = 23;
    $this->connected = false;         // connected?
    $this->connect_timeout = 10;      // timeout while asking for connection
    $this->stream_timeout = 380000;   // timeout between I/O operations
  }

  public function __destruct()
  {
    if($this->connected) { fclose($this->socket); }
  }

  // Connects to host
  // @$_host - addres (or hostname) of host
  // @$_user - name of user to log in as
  // $@_pass - password of user
  //
  // Return: TRUE on success, other way function will return error string got by fsockopen()
  public function Connect($_host, $_port)
  {
    // If connected successfully
    if( ($this->socket = @fsockopen($_host, $_port, $errno, $errorstr, $this->connect_timeout)) !== FALSE )
    {
      $this->host = $_host;
      $this->port = $_port;

      $this->connected = true;

      stream_set_timeout($this->socket, 0, 380000);
      stream_set_blocking($this->socket, 1);

      return true;
    }
  }


  // LogIn to host
  //
  // RETURN: will return true on success, other way returns false
  public function LogIn($_user, $_pass)
  {
    if($_user) $this->user = $_user;
    if($_pass) $this->pass = $_pass;
    if(!$this->connected) return false;

    // Send name and password
    $this->SendString($this->user, true);
    $this->SendString($this->pass, true);

    // read answer
    $data = $this->ReadTo(array('#'));

    // did we get the prompt from host?
    if( strtolower(trim($data[count($data)-1])) == strtolower($this->host).'#' ) return true;
    else return false;
  }


  // Function will execute command on host and returns output
  //
  // @$_command - command to be executed, only commands beginning with "show " can be executed, you can change this by adding
  //              "true" (bool type) as the second argument for function SendString($command) inside this function (3rd line)
  //
  function GetOutputOf($_command)
  {
    if(!$this->connected) return false;

    $this->SendString($_command);

    $output = array();
    $work = true;

    //
    // Read whole output
    //
    // read_to( array( STRINGS ) ), STRINGS are meant as possible endings of outputs
    while( $work && $data = $this->ReadTo( array("--More--","#") ) )
    {
      // CHeck wheter we actually did read any data
      $null_data = true;
      foreach($data as $line)
      {
        if(trim($line) != "") {$null_data = false;break;}
      }
      if($null_data) { break;}

      // if device is paging output, send space to get rest
      if( trim($data[count($data)-1]) == '--More--')
      {
        // delete line with prompt (or  "--More--")
        unset($data[count($data)-1]);

        // if second line is blank, delete it
        if( trim($data[1]) == '' ) unset($data[1]);
        // If first line contains send command, delete it
        if( strpos($data[0], $_command)!==FALSE ) unset($data[0]);

        // send space
        fputs($this->socket, " ");
      }

      // ak ma vystup max dva riadky
      // alebo sme uz nacitali prompt
      // IF we got prompt (line ending with #)
      // OR string that we've read has only one line
      //    THEN we reached end of data and stop reading
      if( strpos($data[count($data)-1], '#')!==FALSE /* || (count($data) == 1 && $data[0] == "")*/  )
      {
        // delete line with prompt
        unset($data[count($data)-1]);

        // if second line is blank, delete it
        if( trim($data[1]) == '' ) unset($data[1]);
        // If first line contains send command, delete it
        if( strpos($data[0], $_command)!==FALSE ) unset($data[0]);

        // stop while cyclus
        $work = false;
      }

      // get rid of empty lines at the end
      for($i = count($data)-1; $i>0; $i--)
      {
        if(trim($data[$i]) == "") unset($data[$i]);
        else break;
      }

      // add new data to $output
      foreach($data as $v)
      { $output[] = $v; }
    }

    // return output
    return $output;
  }


  // Read from host until occurence of any index from $array_of_stops
  // @array_of_stops - array that contains strings of texts that may be at the end of output
  // RETURNS: output of command as array of lines
  function ReadTo($array_of_stops)
  {
    $ret = array();
    $max_empty_lines = 3;
    $count_empty_lines = 0;

    while( !feof($this->socket) )
    {
      $read = fgets($this->socket);
      $ret[] = $read;

      //
      // Stop reading after (int)"$max_empty_lines" empty lines
  //
      if(trim($read) == "")
      {
        if($count_empty_lines++ > $max_empty_lines) break;
      }
      else $count_empty_lines = 0;

      //
      // Does last line of readed data contain any of "Stop" strings ??
      $found = false;
      foreach($array_of_stops AS $stop)
      {
        if( strpos($read, $stop) !== FALSE ) { $found = true; break; }
      }
      // If so, stop reading
      if($found) break;
    }

    return $ret;
  }



  // Send string to host
  // If force is set to false (default), function sends to host only strings that begins with "show "
  //
  // @$string - command to be executed
  // @$force - force command? Execute if not preceeded by "show " ?
  // @$newLine - append character of new line at the end of command?
  function SendString($string, $force=false, $newLine=true)
  {
    $t1 = microtime(true);
    $string = trim($string);

    // execute only strings that are preceded by "show"
    // and execute only one command (no new line characters) !
    if(!$force && strpos($string, 'show ') !== 0 && count(explode("\n", $string)) == 1)
    {
      return 1;
    }


    if($newLine) $string .= "\n";
    fputs($this->socket, $string);

    $t2 = microtime(true);
  }

}

?>