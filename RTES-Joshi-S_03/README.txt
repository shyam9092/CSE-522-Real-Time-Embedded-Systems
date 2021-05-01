Name: Shyam Bhaveshbhai Joshi
ID: 1218594676
------------------------------

This is a readme file to compile and test the RTES Assignment3 .
Current directory has structures like below. Extract the zip file.
------------------------
RTES-Joshi-S_03/
├── CSE 522: RTES Assignment 3.pdf
└── assignment3.patch
└── trace.patch
└── README.md


Steps to run main.c
--------------------------
The project_nn folder contains the following:
CSE 522: RTES Assignment 3.pdf:           PDF that contains the report for assignment 3
assignment3.patch:                        Patch file 
README.txt:                               Readme file for instructions



Follow steps to apply patch and compile

Navigate to zephyr directory and apply the patch by executing the following command:

$ patch -p3 < assignment3.patch

$ source zephyr-env.sh
$ export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
$ export ZEPHYR_SDK_INSTALL_DIR=~/zephyr-sdk

=> Go to trace_app directory and run the following command

$ mkdir build && cd build
$ cmake -DBOARD=galileo ..

=> this will create a Makefile for you to make.

$ make

=> It will generate zephyr.strip which is stored inside /HCSR_app/build/zephyr/

=> Connect your SD card and create the followin directories: 

efi
efi/boot
kernel

=> Copy the kernel file zephyr.strip to $SDCARD/kernel folder

=> Copy boot image to efi/boot and name it as bootia32.efi

=> Create $SDCARD/efi/boot/grub.cfg containing the following

set default=0
set timeout=10

menuentry "Zephyr Kernel" {
   multiboot /kernel/zephyr.strip
}

=> Remove the SD card and insert it in galileo board. 

=> Connect power to the galileo board and connect FTDI cable between your host device and galileo board. 
=> run the following command

$ putty

=> Fill in the appropriate device name and set the baud rate to 115200. In the logging configurations, define the file name to log.txt and add the file to ~/trace_app/.
Also, select Printable output option in session logging field of Logging. 

Following log will be visible after loading the boot image.


uart:~$ tracing_dump 
main-o-12731929 
main-i-12731930 
idle-e-13719888 
idle-i-13719891 
logging-i-13719894 
logging-o-13719910 
logging-i-13719911 
idle-e-13729864 
idle-e-13729867 
idle-e-13729871 
idle-e-13729874 
idle-e-13729878 
idle-e-13729882 
idle-i-13729885 
task 0-e-13739266 
task 0-o-13739272 
task 0-i-13739273 
task 0-i-13739275 
task 0-e-13748058 
task 0-o-13748063 
task 0-i-13748064 
task 0-i-13748065 
task 0-o-13756856 
task 0-i-13756857 
task 1-e-13766237 
task 1-o-13766242 
task 1-i-13766243 
task 1-i-13766245 
task 1-e-13769772 

=> Run the following command in the to execute the script in /trace_app/ folder

$ python3 script.py

=> Run the following command to open the simulation.vcd file in gtkwave

$ gtkwave simulation.vcd






