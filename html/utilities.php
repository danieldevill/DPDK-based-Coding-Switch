<?php

	if($_POST["func"] == "reboot")
	{
		reboot();
	}
	if($_POST["func"] == "relaunch")
	{
		relaunch();
	}
	if($_POST["func"] == "getSettings")
	{
		getSettings();
	}
	if($_POST["func"] == "setSettings")
	{
		setSettings($_POST["cfg"]);
	}
	if($_POST["func"] == "getStats")
	{
		getStats();
	}

	function reboot() //Sets a reboot flag for a cron to read and then do the reboot.
	{
		$tmpfile = fopen("/tmp/rbootflg", 'w') or die("Reboot failed!");
		fwrite($tmpfile, "reboot_true");
		fclose($tmpfile);
	}

	function relaunch() //Sets a reboot flag for a cron to read and then do the reboot.
	{
		$tmpfile = fopen("/tmp/rlaunchflg", 'w') or die("Relaunch failed!");
		fwrite($tmpfile, "relaunch_true");
		fclose($tmpfile);
	}

	function getSettings() //Gets value of all settings from file.
	{
		$cfgfile = fopen("/home/switch/l2fwd-nc/l2fwd-nc.cfg",'r');
		echo(fread($cfgfile,filesize("/home/switch/l2fwd-nc/l2fwd-nc.cfg")));
		fclose($cfgfile);
	}

	function setSettings(string $cfg) //Applies settings to cfg file.
	{
		$cfgfile = fopen("/home/switch/l2fwd-nc/l2fwd-nc.cfg",'w');
		fwrite($cfgfile,$cfg);
		fclose($cfgfile);
	}

	function getStats() //Gets stats on rx,tx,cpu/mem info and uptime.
	{
		$statsfile = fopen("/home/switch/l2fwd-nc/txrx.stats",'r');
		echo(fread($statsfile,filesize("/home/switch/l2fwd-nc/txrx.stats")));
		fclose($statsfile);
	}

?>