#!/bin/bash

#Start OVS with DPDK setup. 

#Add hugepages. Specifically 4GB of 2mb page files
#sudo  bash -c "echo 2048 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages"

#Clean ovs enviroment
sudo killall ovsdb-server ovs-vswitchd
sudo rm -f /var/run/openvswitch/vhost-user*
sudo rm -f /etc/openvswitch/conf.db

#Bind devices to DPDK
sudo ip link set ens4 down
sudo ip link set ens5 down
sudo ip link set ens6 down
sudo ip link set ens7 down
sudo ip link set ens8 down
#sudo modprobe uio_pci_generic
sudo modprobe igb_uio
#sudo insmod /usr/src/dpdk-stable-17.11.1/build/kmod/igb_uio.ko
sudo $RTE_SDK/usertools/dpdk-devbind.py --bind=igb_uio ens4 ens5 ens6 ens7 ens8

#Start ovsdb
# export DB_SOCK=/var/run/openvswitch/db.sock
# sudo ovsdb-tool create /etc/openvswitch/conf.db /usr/share/openvswitch/vswitch.ovsschema
# sudo ovsdb-server --remote=punix:$DB_SOCK --remote=db:Open_vSwitch,Open_vSwitch,manager_options --pidfile --detach

#Start OVS with dpdk config enabled
# sudo ovs-vsctl --no-wait init
# sudo ovs-vsctl --no-wait set Open_vSwitch . other_config:dpdk-init=true
# sudo ovs-vswitchd unix:$DB_SOCK --pidfile --detach --log-file=/var/log/openvswitch/ovs-vswitchd.log

#Create OVS-DPDK Bridge and Ports
# sudo ovs-vsctl add-br br0 -- set bridge br0 datapath_type=netdev
# sudo ovs-vsctl add-port br0 ens4_dpdk -- set Interface ens4_dpdk type=dpdk options:dpdk-devargs=0000:00:04.0 ofport_request=1
# sudo ovs-vsctl add-port br0 ens5_dpdk -- set Interface ens5_dpdk type=dpdk options:dpdk-devargs=0000:00:05.0 ofport_request=2
# sudo ovs-vsctl add-port br0 ens6_dpdk -- set Interface ens6_dpdk type=dpdk options:dpdk-devargs=0000:00:06.0 ofport_request=3
# sudo ovs-vsctl add-port br0 ens7_dpdk -- set Interface ens7_dpdk type=dpdk options:dpdk-devargs=0000:00:07.0 ofport_request=4

#Setup test flows to forward packets between DPDK ports
# sudo ovs-ofctl del-flows br0
# sudo ovs-ofctl add-flow br0 in_port=1,action=output:2
# sudo ovs-ofctl add-flow br0 in_port=2,action=output:1
# sudo ovs-ofctl add-flow br0 in_port=3,action=output:4
# sudo ovs-ofctl add-flow br0 in_port=4,action=output:3

#Print outputs of dpdk drivers and ovs-vsctl/ovs-ofctl to confirm.
sudo $RTE_SDK/usertools/dpdk-devbind.py -s
# sudo ovs-vsctl show
# sudo ovs-ofctl dump-flows br0

#Print number of Hugepages
grep Huge /proc/meminfo

#Check if Crontabs are all setup
