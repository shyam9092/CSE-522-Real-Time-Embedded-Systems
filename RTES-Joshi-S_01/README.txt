Name: Shyam Bhaveshbhai Joshi
ID: 1218594676
------------------------------

This is a readme file to compile and test the RTES Assignment1 .
Current directory has structures like below. Extract the zip file.
------------------------
RTES-Joshi-S_01/
├── project1_nn
│   ├── src
│   ├── prj.conf
│   └── CmakeLists.txt
└── README.txt


Steps to run main.c
--------------------------
The project_nn folder contains the following:
src:            the source folder which contains main.c
prj.conf:       project configuration 
CmakeLists.txt: build system generator



Follow steps to compile

=> Copy project1_nn to the sample directory of the zephyr project

=> Go to directory zephyr folder and run the following command

$ source zephyr-env.sh
$ export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
$ export ZEPHYR_SDK_INSTALL_DIR=~/zephyr-sdk

=> Go to project1_nn directory and run the following command

$ mkdir build && cd build
$ cmake -DBOARD=galileo ..

=> this will create a Makefile for you to make.

$ make

=> It will generate zephyr.strip which is stored inside /project1_nn/build/zephyr/

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

=> Connect power to the galileo board and connect FTDI cable between your host device and galileo board

=> Connect the following pins of the galileo board:

1. IO pin 3 to IO pin 12.
2. GND pin to RGB LED's ground pin.
3. IO pin 5 to red pin of RGB LED.
4. IO pin 6 to green pin of RGB LED.
5. IO pin 9 to blue pin of RGB LED.

=> run the following command

$ putty

=> Fill in the appropriate device name and set the baud rate to 115200.

=> Run the following commands in putty terminal.

uart:~$ project1 RGB-display 100 100 100 

x = 100          y = 100         z = 100

uart:~$ project1 int-latency 10

DEBUG: cmd_project1_int_latency():246: Interrupt Samples = 10
DEBUG: kthread_interruptFunc():180: INTERRUPT KTHREAD CREATED
DEBUG: kthread_interruptFunc():181: SAMPLES = 10
DEBUG: kthread_interruptFunc():185: onGoing=1
DEBUG: kthread_interruptFunc():188: triggering 0th time
uart:~$ DEBUG: kthread_interruptFunc():188: triggering 1th time
DEBUG: kthread_interruptFunc():188: triggering 2th time
DEBUG: kthread_interruptFunc():188: triggering 3th time
DEBUG: kthread_interruptFunc():188: triggering 4th time
DEBUG: kthread_interruptFunc():188: triggering 5th time
DEBUG: kthread_interruptFunc():188: triggering 6th time
DEBUG: kthread_interruptFunc():188: triggering 7th time
DEBUG: kthread_interruptFunc():188: triggering 8th time
DEBUG: kthread_interruptFunc():188: triggering 9th time
DEBUG: kthread_interruptFunc():199: Interrupt Latency Found 1807 ns
DEBUG: kthread_interruptFunc():205: Exiting

uart:~$ project1 cs-latency 15

Context switch samples to be collected = 15
D
EBUG: kthread_contextLow():132: Context Switch Latency = 4902 ns



