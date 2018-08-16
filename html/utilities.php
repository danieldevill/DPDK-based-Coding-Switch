<?php

	if($_POST["func"] == "reboot")
	{
		reboot();
	}

	function reboot() //Sets a reboot flag for a cron to read and then do the reboot.
	{
		$tmpfile = fopen("/tmp/rbootflg", 'w') or die('fopen failed');
		fwrite($tmpfile, "reboot_true");
		fclose($tmpfile);
	}

?>