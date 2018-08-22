$(document).ready(function() 
{
	if($("#" + "cpu_mem_info").length != 0) //Check if in index.php by looking for an ID that only exists in index.php
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
					setTimeout(function(){location.reload()},20000);
				}
			});
		});

		$.ajax({
			url: 'utilities.php',
			type: 'POST',
			data: {'func': 'getStats'},
			success: function(data) { 
				//Get Stats and populate cards approprietly.
				setting_names = ["RX","TX"];
				for(setting_name_index in setting_names)
				{
					setting_name = setting_names[setting_name_index];
					var setting_value = data.substring((data.search(setting_name)+setting_name.length+1),data.indexOf(";",data.indexOf(setting_name)));
					$('#'+setting_name.toLowerCase()).text(setting_name+": "+setting_value + " pkts");
				}
			}
		});

		$.ajax({
			url: 'utilities.php',
			type: 'POST',
			data: {'func': 'getSystemStats'},
			success: function(data) { 
				//Get Stats on CPU/Mem info and uptime.
				//alert(data);	
				var uptime = parseInt($.now()/1000)-data.substring(0,data.indexOf(";"));
				var days = Math.floor(uptime/(60*60*24));
				var hours = Math.floor(uptime/3600)%24;
				var mins = Math.floor(uptime/60)%60;
				var secs = uptime%60;
				$('#uptime').text("Uptime: "+days+"d "+hours+"h:"+mins+"m:"+secs+"s");
				var total_mem = $.trim(data.substring((data.search("MemTotal")+"MemTotal".length+1),data.indexOf("kB",data.indexOf("MemTotal"))));
				var avl_mem = $.trim(data.substring((data.search("MemAvailable")+"MemAvailable".length+1),data.indexOf("kB",data.indexOf("MemAvailable"))));
				$('#meminfo').text("RAM: "+Math.floor(((total_mem-avl_mem)/1000))+" / "+Math.floor(total_mem/1000)+" mB");
				var cpu_use = data.substring((data.search("cpuload")+"cpuload".length+2),data.indexOf(";",data.indexOf("cpuload")));
				$('#cpuinfo').text("CPU: "+(cpu_use*100)+"%");
			}
		});


		$.ajax({
			url: 'utilities.php',
			type: 'POST',
			data: {'func': 'getLinkStats'},
			success: function(data) { 
				//Get Stats on port links.
				var pos_offset = 0;
				var temp_offset = 0;
				var num_ports = parseInt(data.substring( data.lastIndexOf("port")+5,data.lastIndexOf(";mac_address")))+1;
				var table_headings = ["port","mac_address","link_speed","link_duplex","link_status","ipackets","opackets","ibytes","obytes","ierrors","oerrors"];
				for(portid = 0; portid < num_ports; portid++)
				{
					var table_row = "";
					for(heading_index in table_headings)
					{
						table_heading = table_headings[heading_index];
						table_val = data.substring( data.indexOf(table_heading, pos_offset)+table_heading.length+1, data.indexOf(";",data.indexOf(table_heading,pos_offset)));
						if(table_heading == "link_duplex")
						{
							if(table_val == "1")
							{
								table_row += "<td>FULL</td>";
							}
							else
							{
								table_row += "<td>HALF</td>";
							}
						}
						else if(table_heading == "link_status")
						{
							if(table_val == "1")
							{
								table_row += "<td>UP</td>";
							}
							else
							{
								table_row += "<td>DOWN</td>";
							}
						}
						else
						{
							table_row += "<td>"+table_val+"</td>";
						}
					}
					pos_offset = parseInt(data.indexOf(";",data.indexOf("oerrors:", pos_offset)));
					$('#link_table tbody').append("<tr>"+table_row+"</tr>");
				}
			}
		});
	}

	if($("#" + "mac_entries").length != 0) //Check if in settings.php by looking for a ID that only exists in settings.php
	{
		$.ajax({
			url: 'utilities.php',
			type: 'POST',
			data: {'func': 'getSettings'},
			success: function(data) { 
				//Get config settings and update fields approprietly.
				var setting_txtinputs = ["MAC_ENTRIES","NB_MBUF","MAX_PKT_BURST","RTE_TEST_RX_DESC_DEFAULT","RTE_TEST_TX_DESC_DEFAULT","MAX_SYMBOLS","MAX_SYMBOL_SIZE"];
				for(setting_name_index in setting_txtinputs)
				{
					setting_name = setting_txtinputs[setting_name_index];
					var setting_value = data.substring((data.search(setting_name)+setting_name.length+3),data.indexOf(";",data.indexOf(setting_name)));
					$('#'+setting_name.toLowerCase()).val(setting_value);
				}
				var setting_radioinputs = ["network_coding","codec","finite_field"];
				for(setting_name_index in setting_radioinputs)
				{
					setting_name = setting_radioinputs[setting_name_index];
					if(setting_name_index<1)
					{
						var setting_value = data.substring((data.search(setting_name)+setting_name.length+3),data.indexOf(";",data.indexOf(setting_name)));
					}
					else
					{
						var setting_value = data.substring((data.search(setting_name)+setting_name.length+4),data.indexOf("\";",data.indexOf(setting_name)));
					}
					$('#'+setting_name+'-'+setting_value).prop("checked",true);
				}
			}
		});

		//Update config and reboot
		$("#btn_update_and_relaunch").click(function(e)
		{	
			$.ajax({
				url: 'utilities.php',
				type: 'POST',
				data: {'func': 'getSettings'},
				success: function(data) { 
					//Get values from fields and send to php for file writing.
					var setting_txtinputs = ["MAC_ENTRIES","NB_MBUF","MAX_PKT_BURST","RTE_TEST_RX_DESC_DEFAULT","RTE_TEST_TX_DESC_DEFAULT","MAX_SYMBOLS","MAX_SYMBOL_SIZE"];
					for(setting_name_index in setting_txtinputs)
					{
						setting_name = setting_txtinputs[setting_name_index];
						var setting_value = $('#'+setting_name.toLowerCase()).val();
						data = data.substring(0,(data.search(setting_name)+setting_name.length+3)) + setting_value + data.substring(data.indexOf(";",data.indexOf(setting_name)));
					}
					var setting_radioinputs = ["network_coding","codec","finite_field"];
					for(setting_name_index in setting_radioinputs)
					{
						setting_name = setting_radioinputs[setting_name_index];
						var setting_value;
						if(setting_name == "network_coding")
						{
							if($('#network_coding-1').is(":checked"))
							{
								setting_value = "1";
							}
							else
							{
								setting_value = "0";
							}
						}
						else if(setting_name == "codec")
						{
							var codec_values = ["kodoc_full_vector","kodoc_on_the_fly","kodoc_sliding_window"];
							for(codec_values_index in codec_values)
							{
								var codec_value = codec_values[codec_values_index];
								if($('#'+setting_name+'-'+codec_value).is(":checked"))
								{
									setting_value = '\"'+codec_value+'\"';
								}
							}
						}
						else if(setting_name == "finite_field")
						{
							var field_values = ["kodoc_binary","kodoc_binary4","kodoc_binary8"];
							for(field_values_index in field_values)
							{
								var field_value = field_values[field_values_index];
								if($('#'+setting_name+'-'+field_value).is(":checked"))
								{
									setting_value = '\"'+field_value+'\"';
								}
							}
						}
						data = data.substring(0,(data.search(setting_name)+setting_name.length+3)) + setting_value + data.substring(data.indexOf(";",data.indexOf(setting_name)));
					}
					var new_cfg = data;
					$.ajax({
						url: 'utilities.php',
						type: 'POST',
						data: {'func': 'setSettings',
							   'cfg': new_cfg
						},
						success: function(data) { 
							//Reboot switch.
							e.preventDefault(); //Prevent a form from being submitted.
							$.ajax({
								url: 'utilities.php',
								type: 'POST',
								data: {'func': 'relaunch'},
								success: function(data) {
									$('#btn_update_and_relaunch').html('Changes saved. Relaunching..'); 
									setTimeout(function(){location.reload()},4000);
								}
							});
						}
					});
				}
			});

		});
	}


});