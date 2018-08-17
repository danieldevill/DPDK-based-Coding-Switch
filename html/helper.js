$(document).ready(function() 
{
	$("#btn_reboot").click(function(e)
	{	
		e.preventDefault(); //Prevent a form from being submitted.
		$.ajax({
			url: 'utilities.php',
			type: 'POST',
			data: {'func': 'reboot'},
			success: function(data) {
				$('#btn_reboot').html('Rebooting..'); 
			}
		});
	});

	if($("#" + "mac_entries").length != 0) //Check if in settings.php by looking for a ID that only exists in settings.php
	{
		alert("In settings.php");
		$.ajax({
			url: 'utilities.php',
			type: 'POST',
			data: {'func': 'getSettings'},
			success: function(data) { 
				alert("Got Settings");
			}
		});
	}

});