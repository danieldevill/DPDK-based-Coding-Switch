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
});