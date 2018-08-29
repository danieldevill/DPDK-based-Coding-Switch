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
	if($_POST["func"] == "getSystemStats")
	{
		getSystemStats();
	}
	if($_POST["func"] == "getLinkStats")
	{
		getLinkStats();
	}
	if($_POST["func"] == "isSwitchOn")
	{
		isSwitchOn();
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

	function getSystemStats() //Gets stats on rx,tx,cpu/mem info and uptime.
	{
		$timefile = fopen("/home/switch/l2fwd-nc/start.time",'r');
		echo(fread($timefile,filesize("/home/switch/l2fwd-nc/start.time")));
		fclose($timefile);
		//Get CPU load average.
		$load = sys_getloadavg();
		echo(";\n+cpuload: ". $load[0] . ";");
		//Get Memory Info.
		echo("\n");
		$meminfo = fopen("/proc/meminfo",'r');
		echo(fread($meminfo,100)); //Just read top bit of meminfo
		fclose($meminfo);
		echo("end.");
	}

	function getLinkStats() //get stats on all port links.
	{
		$linkstatsfile = fopen("/home/switch/l2fwd-nc/link.stats",'r');
		echo(fread($linkstatsfile,filesize("/home/switch/l2fwd-nc/link.stats")));
		fclose($linkstatsfile);
	}

	function isSwitchOn() //Returns 1 if the switch is on, else 0.
	{
		exec("pidof l2fwd",$output);
		if(file_exists('/proc/'.$output[0]) and $output[0] != null)
		{
			echo("1");
		}
		else
		{
			echo("0");
		}
	}

?>