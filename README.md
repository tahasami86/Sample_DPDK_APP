# Sample_DPDK_APP

## Requirements

You should have dpdk installed and running on your system before running this script.You can find the detail instruction in the getting started guide at the DPDK web page.

## building
Run the init script for installing the necessary changes 

```shell
$ ./init_script.sh
```

## Running the application
The application can be run by using the following cmd

```shell
sudo ./dpdk-test_app_parse -l 0-7 -n 4 -a $pci_address -a $pci_address -- -d <delay time [us]> \
                                                                          -q <number of queues (max_queue < 1024)>
```
Currently only 1 queue is supported so -q should be 1
