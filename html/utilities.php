<?php

	if($_POST["func"] == "reboot")
	{
		reboot();
	}
	if($_POST["func"] == "getSettings")
	{
		getSettings();
	}

	function reboot() //Sets a reboot flag for a cron to read and then do the reboot.
	{
		$tmpfile = fopen("/tmp/rbootflg", 'w') or die("Reboot failed!");
		fwrite($tmpfile, "reboot_true");
		fclose($tmpfile);
	}

	function getSettings() //Gets value of all settings from file.
	{
		$cfgfile = fopen("/tmp/rbootflg",'r');_
		while(!feof($cfgfile))
		{
			echo(fgets($cfgfile) . "<br>");
		}
		fclose($cfgfile);
	}

?>