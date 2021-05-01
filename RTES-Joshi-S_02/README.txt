Name: Shyam Bhaveshbhai Joshi
ID: 1218594676
------------------------------

This is a readme file to compile and test the RTES Assignment2 .
Current directory has structures like below. Extract the zip file.
------------------------
RTES-Joshi-S_02/
├── CSE 522: RTES Assignment 2.pdf
└── hcsr04.patch
└── README.md


Steps to run main.c
--------------------------
The project_nn folder contains the following:
CSE 522: RTES Assignment 2.pdf:           PDF that contains the report for assignment 2
hcsr04.patch:                             Patch file 
README.md:                                Readme file for instructions



Follow steps to apply patch and compile

Navigate to zephyr directory and apply the following command:

$ patch -p3 < hcsr04.patch

$ source zephyr-env.sh
$ export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
$ export ZEPHYR_SDK_INSTALL_DIR=~/zephyr-sdk

=> Go to HCSR_app directory and run the following command

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

=> Connect power to the galileo board and connect FTDI cable between your host device and galileo board

=> Connect the following pins of the galileo board:

1. 5V pin to VCC pin of HCSR sensor1.
2. IO pin 1 to trigger pin of HCSR sensor1.
3. IO pin 3 to trigger pin of HCSR sensor1.
4. GND pin to HCSR sensor1.
5. 5V pin to VCC pin of HCSR sensor2.
6. IO pin 10 to trigger pin of HCSR sensor2.
7. IO pin 12 to trigger pin of HCSR sensor2.
8. GND pin to HCSR sensor2.

=> run the following command

$ putty

=> Fill in the appropriate device name and set the baud rate to 115200. Following log will be visible after loading the boot image.

uart:~$ DEBUG: hcsr_init():619: device 1
DEBUG: device_one_setup():388: e=12      t=10
DEBUG: device_one_setup():393: gpio_dw
DEBUG: device_one_setup():478: Trigger port = 2
DEBUG: device_one_setup():483: gpio_dw
DEBUG: device_one_setup():560: Echo port = 7
DEBUG: hcsr_init():613: device 0
DEBUG: device_zero_setup():164: e=3      t=1
DEBUG: device_zero_setup():169: gpio_dw
DEBUG: device_zero_setup():254: Trigger port = 4
DEBUG: device_zero_setup():259: gpio_dw
DEBUG: device_zero_setup():336: Echo port = 6
DEBUG: main():250: main

=> Run the following commands in putty terminal.

HCSR select 1
DEBUG: cmd_project2_select_device():122: select = 1
DEBUG: cmd_project2_select_device():130: DEVICE FOUND: HCSR0


HCSR start 5
DEBUG: cmd_project2_start_samples():160: samples = 5
DEBUG: hcsr_sample_fetch():710: [HCSR0] hcsr_sample_fetch
DEBUG: getDistance_zero():642: entered kthread
DEBUG: getDistance_zero():651: 4000
DEBUG: getDistance_zero():658: Distance = 10
DEBUG: hcsr_sample_fetch():727: device 0 creating thread
uart:~$ DEBUG: hcsr_channel_get():771: hcsr_channel_get
DEBUG: hcsr_channel_get():775: DEVICE HCSR0
DEBUG: hcsr_channel_get():790: [HCSR0] buffer is fullDEBUG: hcsr_channel_get():792: TSC_ZERO = 7410
DEBUG: distance_function():78: distance=10 tsc=7410
DEBUG: hcsr_sample_fetch():710: [HCSR0] hcsr_sample_fetch
DEBUG: getDistance_zero():642: entered kthread
DEBUG: getDistance_zero():651: 4000
DEBUG: getDistance_zero():658: Distance = 10
DEBUG: hcsr_sample_fetch():727: device 0 creating thread
DEBUG: hcsr_channel_get():771: hcsr_channel_get
DEBUG: hcsr_channel_get():775: DEVICE HCSR0
DEBUG: hcsr_channel_get():790: [HCSR0] buffer is fullDEBUG: hcsr_channel_get():792: TSC_ZERO = 7409
DEBUG: distance_function():78: distance=10 tsc=7409
DEBUG: hcsr_sample_fetch():710: [HCSR0] hcsr_sample_fetch
DEBUG: getDistance_zero():642: entered kthread
DEBUG: getDistance_zero():651: 4000
DEBUG: getDistance_zero():658: Distance = 10
DEBUG: hcsr_sample_fetch():727: device 0 creating thread
DEBUG: hcsr_channel_get():771: hcsr_channel_get
DEBUG: hcsr_channel_get():775: DEVICE HCSR0
DEBUG: hcsr_channel_get():790: [HCSR0] buffer is fullDEBUG: hcsr_channel_get():792: TSC_ZERO = 7411
DEBUG: distance_function():78: distance=10 tsc=7411
DEBUG: hcsr_sample_fetch():710: [HCSR0] hcsr_sample_fetch
DEBUG: getDistance_zero():642: entered kthread
DEBUG: getDistance_zero():651: 4000
DEBUG: getDistance_zero():658: Distance = 10
DEBUG: hcsr_sample_fetch():727: device 0 creating thread
DEBUG: hcsr_channel_get():771: hcsr_channel_get
DEBUG: hcsr_channel_get():775: DEVICE HCSR0
DEBUG: hcsr_channel_get():790: [HCSR0] buffer is fullDEBUG: hcsr_channel_get():792: TSC_ZERO = 7412
DEBUG: distance_function():78: distance=10 tsc=7412
DEBUG: hcsr_sample_fetch():710: [HCSR0] hcsr_sample_fetch
DEBUG: getDistance_zero():642: entered kthread
DEBUG: getDistance_zero():651: 4000
DEBUG: getDistance_zero():658: Distance = 10
DEBUG: hcsr_sample_fetch():727: device 0 creating thread
DEBUG: hcsr_channel_get():771: hcsr_channel_get
DEBUG: hcsr_channel_get():775: DEVICE HCSR0
DEBUG: hcsr_channel_get():790: [HCSR0] buffer is fullDEBUG: hcsr_channel_get():792: TSC_ZERO = 7414
DEBUG: distance_function():78: distance=10 tsc=7414


HCSR dump 3 5
DEBUG: cmd_project2_dump_p1p2():171: DUMP p1 p2
DEBUG: cmd_project2_dump_p1p2():196: [HCSR0] DISTANCE[2]=10cm TSC[2]=7411us
DEBUG: cmd_project2_dump_p1p2():196: [HCSR0] DISTANCE[3]=10cm TSC[3]=7412us
DEBUG: cmd_project2_dump_p1p2():196: [HCSR0] DISTANCE[4]=10cm TSC[4]=7414us


uart:~$ HCSR clear
DEBUG: cmd_project2_clear_buffer():225: clear buffer

uart:~$ HCSR dump 2 4
DEBUG: cmd_project2_dump_p1p2():171: DUMP p1 p2
DEBUG: cmd_project2_dump_p1p2():181: buffer is empty







