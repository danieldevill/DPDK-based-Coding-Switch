# DPDK-based-Coding-Switch
A network coding capable switch implemented using Intel's DPDK framework.

## File descriptions:
	* wf: The file that calls all other scripts and sets up the appropriate workflow. This file also starts the switch VM.
	* swcli_config: Sets up two terminator windows that SSH into the switch vm.
	* hostscli_config: Sets up four terminator windows, starting up and displaying the output of the respective host VM (Not via SSH).
	* ifup: Creates the respective network between the hosts and the switch, via linux bridges and TAPs.
	* ifdown: Destroys the network between the hosts and the switch. 
	* /vm-images: Directory for VM images of switch and hosts.

