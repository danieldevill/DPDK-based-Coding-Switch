# DPDK-based-Coding-Switch
A network coding capable switch implemented using Intel's DPDK framework.

## File descriptions:
	* wf: The file that calls all other scripts and sets up the appropriate workflow. This file also starts the switch VM.
	* swcli_config: Sets up two terminator windows that SSH into the switch vm.
	* hostscli_config: Sets up four terminator windows, starting up and displaying the output of the respective host VM (Not via SSH).
	* ifup: Creates the respective network between the hosts and the switch, via linux bridges and TAPs.
	* ifdown: Destroys the network between the hosts and the switch. 
	* /vm-images: Directory for VM images of switch and hosts.

## Dependencies:
	* libconfig-dev
	* uml-utilities (For vm network topology setup)
	* bridge-utils (For vm network topology setup)

## Notes:
	* The switch vm might have to have the kernel recompiled based on the cpu type.

## Running the switch:
	* 5 ports on core 1: 
	* run -l 1 -n 1 -m 200 --file-prefix l2 -- -p fffff -q 5


