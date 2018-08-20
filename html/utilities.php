<?php

	if($_POST["func"] == "reboot")
	{
		reboot();
	}
	if($_POST["func"] == "getSettings")
	{
		getSettings();
	}
	if($_POST["func"] == "setSettings")
	{
		setSettings($_POST["cfg"]);
	}

	function reboot() //Sets a reboot flag for a cron to read and then do the reboot.
	{
		$tmpfile = fopen("/tmp/rbootflg", 'w') or die("Reboot failed!");
		fwrite($tmpfile, "reboot_true");
		fclose($tmpfile);
	}

	function getSettings() //Gets value of all settings from file.
	{
		$cfgfile = fopen("/home/switch/l2fwd-nc/l2fwd-nc.cfg",'r');
		
		echo(fread($cfgfile,filesize("/home/switch/l2fwd-nc/l2fwd-nc.cfg")));

		fclose($cfgfile);
	}

	function setSettings(string $cfg)
	{
		echo($cfg);
		$cfgfile = fopen("/home/switch/l2fwd-nc/l2fwd-nc_temp.cfg",'w');
		fwrite($cfgfile,$cfg);
		fclose($cfgfile);
	}

?>