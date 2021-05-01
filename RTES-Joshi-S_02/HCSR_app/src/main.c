/*
 * Copyright (c) 2016 Open-RnD Sp. z o.o.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>

#include <misc/util.h>
#include <misc/printk.h>
#include <shell/shell.h>
#include <shell/shell_uart.h>
#include <version.h>

#include "../boards/x86/galileo/board.h"

#include <stdlib.h>
#include <sensor.h>


#define STACKSIZE 4096
K_THREAD_STACK_DEFINE(threadDistance_stackArea,STACKSIZE);

#define DEBUG 
#if defined(DEBUG) 
	#define DPRINTK(fmt, args...) printk("DEBUG: %s():%d: " fmt, \
   		 __func__, __LINE__, ##args)
#else
 	#define DPRINTK(fmt, args...) /* do nothing if not defined*/
#endif

#define hcsr_0 "HCSR0"
#define hcsr_1 "HCSR1"

/* kthread structure used in HCSR start command*/
struct k_thread thread_Distance;

/* Variables to store select, samples, p1, p2 */
int select;
int samples;
int p1,p2;

/* Identifying the device during select */
struct device *hcsr0;
struct device *hcsr1;

/* storing distance and TSC */
struct sensor_value ptr1[256];
struct sensor_value ptr2[256];


/*
 * Function callback for distance thread . 
 * Description: It will identify the device (HCSR0/HCSR1) based on select value.
 * Then invoke sensor_sample_fetch and sensor_channel_get samples number of times. 
 */
void distance_function(void *arg1, void *arg2, void *arg3)
{	

	if(samples>256)
	{
		DPRINTK("samples > 256 \n");
		return;
	}

	switch(select)
	{
		case 0:
			return;

		case 1:
			for(int i=0; i<samples; i++)
			{
				sensor_sample_fetch(hcsr0);
				k_sleep(500);
				sensor_channel_get(hcsr0,0,&ptr1[i]);
				DPRINTK("distance=%d tsc=%d \n",ptr1[i].val1,ptr1[i].val2);
			}
			break;

		case 2:
			for(int i=0; i<samples; i++)
			{
				sensor_sample_fetch(hcsr1);
				k_sleep(500);
				sensor_channel_get(hcsr1,0,&ptr2[i]);
				DPRINTK("distance=%d tsc=%d \n",ptr2[i].val1,ptr2[i].val2);

			}
			break;
		
		case 3:

			for(int i=0; i<samples; i++)
			{
				sensor_sample_fetch(hcsr0);
				k_sleep(500);
				sensor_channel_get(hcsr0,0,&ptr1[i]);
				sensor_sample_fetch(hcsr1);	
				k_sleep(500);
				sensor_channel_get(hcsr1,0,&ptr2[i]);
				
				DPRINTK("[HCSR0] distance=%d tsc=%u \n",ptr1[i].val1,ptr1[i].val2);
				DPRINTK("[HCSR1] distance=%d tsc=%u \n",ptr2[i].val1,ptr2[i].val2);			
			}
			break;

		return;
	}
	return;
}


/*
 * Function callback for shell command select
 * Description: It will store the value of select and initialize the device pointer accordingly. 
 */
static int cmd_project2_select_device(const struct shell *shell, size_t argc, char **argv)
{
	select = atoi(argv[1]);
	DPRINTK("select = %d \n",select);
	switch(select)
	{
		case 0:
			return 0;

		case 1:
			hcsr0 = device_get_binding(hcsr_0);
			DPRINTK("DEVICE FOUND: %s \n",hcsr0->config->name);
			break;

		case 2:
			hcsr1 = device_get_binding(hcsr_1);
			DPRINTK("DEVICE FOUND: %s \n",hcsr1->config->name);
			break;

		case 3:
			hcsr0 = device_get_binding(hcsr_0);
			DPRINTK("DEVICE FOUND: %s \n",hcsr0->config->name);

			hcsr1 = device_get_binding(hcsr_1);
			DPRINTK("DEVICE FOUND: %s \n",hcsr1->config->name);
			break;
	}	
	return 0;
}

/*
 * Function callback for shell command samples
 * Description: It will store the value of samples and create distance thread to store distance and TSC inside buffer. 
 */
static int cmd_project2_start_samples(const struct shell *shell, size_t argc, char **argv)
{
	samples = atoi(argv[1]);
	if(samples > 256)
	{
		return 0;
	}
	DPRINTK("samples = %d \n",samples);
 	k_thread_create(&thread_Distance,threadDistance_stackArea,K_THREAD_STACK_SIZEOF(threadDistance_stackArea),distance_function,NULL,NULL,NULL,5,0,K_NO_WAIT);
	return 0;
}

/*
 * Function callback for shell command dump
 * Description: It will check and print the values stored inside the buffer. 
 */
static int cmd_project2_dump_p1p2(const struct shell *shell, size_t argc, char **argv)
{
	DPRINTK("DUMP p1 p2 \n");
	p1 = atoi(argv[1]);
	p2 = atoi(argv[2]);
	if(p1>p2)
	{
		DPRINTK("Invalid argument \n");
		return 0;
	}
	if(samples==-1)
	{
		DPRINTK("buffer is empty \n");
		return 0;
	}
	p1--;
	p2--;

	switch(select)
	{
		case 0:
			DPRINTK("Select the device first and then try again later \n");
			return 0;
		
		case 1:
			for(int i=p1; i<=p2; i++)
			{
				DPRINTK("[%s] DISTANCE[%d]=%dcm TSC[%d]=%uus\n",hcsr0->config->name,i,ptr1[i].val1,i,ptr1[i].val2);
			}
			break;

		case 2:
			for(int i=p1; i<=p2; i++)
			{
				DPRINTK("[%s] DISTANCE[%d]=%dcm TSC[%d]=%uus\n",hcsr1->config->name,i,ptr2[i].val1,i,ptr1[i].val2);
			}
			break;

		case 3:
			for(int i=p1; i<=p2; i++)
			{
				DPRINTK("[%s] DISTANCE[%d]=%dcm TSC[%d]=%uus\n",hcsr0->config->name,i,ptr1[i].val1,i,ptr1[i].val2);
				DPRINTK("[%s] DISTANCE[%d]=%dcm TSC[%d]=%uus\n",hcsr1->config->name,i,ptr2[i].val1,i,ptr1[i].val2);
			}
			break;
	}

	return 0;
}

/*
 * Function callback for shell command clear
 * Description: It will set samples to -1, which will make the command dump to return without dumping any values. 
 */
static int cmd_project2_clear_buffer(const struct shell *shell, size_t argc, char **argv)
{
	DPRINTK("clear buffer \n");
	samples = -1;
	return 0;
}

/*
 * Macro for registering and creating shell commands and associating them with appropriate handlers. 
 */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_project2,
	SHELL_CMD(select, NULL, "Take the device(s)", cmd_project2_select_device),
	SHELL_CMD(start, NULL, "Take distance samples", cmd_project2_start_samples),
	SHELL_CMD(dump, NULL, "Take p1 p2", cmd_project2_dump_p1p2),
	SHELL_CMD(clear, NULL, "Clear internal buffer", cmd_project2_clear_buffer),

	SHELL_SUBCMD_SET_END /* Array terminated. */
);

/*
 * Macro to register shell command defined above.
 */
SHELL_CMD_REGISTER(HCSR, &sub_project2, "project1 commands", NULL);


void main(void)
{
	DPRINTK("main \n");

}
