/*
 * Copyright (c) 2016 Open-RnD Sp. z o.o.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>
#include <gpio.h>
#include <pwm.h>
#include <misc/util.h>
#include <misc/printk.h>
#include <shell/shell.h>
#include <shell/shell_uart.h>
#include <version.h>
#include <pinmux.h>
#include "../boards/x86/galileo/board.h"
#include "../boards/x86/galileo/pinmux_galileo.h"
#include "../drivers/gpio/gpio_dw_registers.h"
#include "../drivers/gpio/gpio_dw.h"
#include <stdlib.h>

#define STACKSIZE 4096
K_THREAD_STACK_DEFINE(threadInterrupt_stack_area,STACKSIZE);
K_THREAD_STACK_DEFINE(threadContext_stack_area_LOW,STACKSIZE);
K_THREAD_STACK_DEFINE(threadContext_stack_area_HIGH,STACKSIZE);



#define PERIOD 4096
#define PULL_UP 0
#define DEBUG 
#define EDGE_RISING		(GPIO_INT_EDGE | GPIO_INT_ACTIVE_HIGH)
#if defined(DEBUG) 
	#define DPRINTK(fmt, args...) printk("DEBUG: %s():%d: " fmt, \
   		 __func__, __LINE__, ##args)
#else
 	#define DPRINTK(fmt, args...) /* do nothing if not defined*/
#endif

/* Default value for duty cycle and number of samples to be collected */
int dutyCycle_x=60,dutyCycle_y=20, dutyCycle_z=90;
int intSamples =10,csSamples=10;

/* Device structure to access pin multiplexer of galileo */
static struct device *pinmux;  

/* pwm_dev to acces PWM device
   gpiob to acces gpio device */
struct device *pwm_dev;
struct device *gpiob;

/* Interrupt callback structure */
struct gpio_callback gpio_cb;

/* Varialbes to record time stamps*/
unsigned long long interrupt_tscBefore;
unsigned long long interrupt_tscAfter;
unsigned long long cs_tscBefore;
unsigned long long cs_tscAfter;

/* Variable to calculate context switch latency*/
unsigned int cs_diff=0;

/* Variables to store the average delay in writing to gpio and locking a semaphore */
unsigned int average_delay = 0;
unsigned int average_delay_lock = 0;

/* Flags to indicate there is an ongoing measurement */
int onGoing=0;
int onGoing_CS=0;

/* k_thread structures for interrupt thread, low priority thread, high priority thread */
struct k_thread thread_Interrupt;
struct k_thread thread_contextLow;
struct k_thread thread_contextHigh;

/* Mutex for accessing OnGoing flag */
K_MUTEX_DEFINE(threadInterrupt_mutex);

/* Semaphore for calulating context switch latency */
K_SEM_DEFINE(threadContext_sem, 0, 1);
K_SEM_DEFINE(threadContext_sem2,1,1);

/*Semaphore for calculating average delay for locking */
K_SEM_DEFINE(test_delay,1,1);

K_MUTEX_DEFINE(threadFlag_mutex);

/* Function to read time stamp */
static inline unsigned long long getticks(void)
{
	unsigned int lo, hi;
	// RDTSC copies contents of 64-bit TSC into EDX:EAX
	__asm__ volatile("rdtsc" : "=a" (lo), "=d" (hi));
	return (unsigned long long)hi << 32 | lo;
}

/* Interrupt callback */
void interrupt_cb(struct device *gpiob, struct gpio_callback *cb, u32_t pins)
{	
	interrupt_tscAfter = getticks();
	gpio_pin_write(gpiob,6,0);
}

// High priority thread is created before low priority thread

/*
 * Function callback for low priority thread 
 * Description: High priority thread is created first, it tries to acquire a lock whose initial value is 0, thus it goes to sleep
 * Low priority thread is then created, it records the timestamp and unlocks the sempahore that high priority thread is contending.
 * Thus, context switch will happen, Low priority thread then contends for a lock that will be unlocked by the high priority thread.
 * This will repeat till i<*arg, where *arg = arg1 and arg1 holds the address of the variable where number of samples to be taken is stored.
 * After the loop is finished, average of the samples is calulated and average delay to lock a sempahore is subtracted for accuracy.
 * Then, clock ticks is converted to nano seconds.
 */
void kthread_contextLow(void *arg1, void *arg2, void *arg3)
{
	int *arg = arg1; 											//arg1 contains the number of samples to be taken.
	int x=10,y=4;
	for(int i=0; i<*arg;i++)
	{
		cs_tscBefore = getticks();
		k_sem_give(&threadContext_sem);
		k_sem_take(&threadContext_sem2,K_FOREVER);
		cs_diff += (unsigned int)(cs_tscAfter - cs_tscBefore);
	}
	cs_diff=cs_diff/(*arg);
	cs_diff=cs_diff-average_delay_lock;
	cs_diff=cs_diff*x;
	cs_diff=cs_diff/y;
	DPRINTK("Context Switch Latency = %u ns \n",cs_diff);
	cs_diff=0;

}

/*
 * Function callback for high priority thread 
 * Description: High priority thread is created first, it tries to acquire a lock whose initial value is 0, thus it goes to sleep
 * When low priority thread unlocks the semaphore that high priority thread is conteding for, context switch happens and time is recorded in high priority thread. 
 * High priority thread then unlocks the semaphore that low priority thread is contending for and the cycle is repeated. 
 */
void kthread_contextHigh(void *arg1, void *arg2, void *arg3)
{
	int counter=0;
	int *arg = arg1;								//arg1 contains the number of samples to be taken.
	k_mutex_lock(&threadFlag_mutex,K_FOREVER);
	onGoing_CS=1;
	k_mutex_unlock(&threadFlag_mutex);
	while(counter<*arg)
	{
		k_sem_take(&threadContext_sem,K_FOREVER);
		cs_tscAfter = getticks();
		counter++;
		k_sem_give(&threadContext_sem2);
	}
	k_mutex_lock(&threadFlag_mutex,K_FOREVER);
	onGoing_CS=0;
	k_mutex_unlock(&threadFlag_mutex);
}


/*
 * Function callback for interrupt thread 
 * Description: It sets onGoing flag to 1 to let module know that there is an ongoing measurement.
 * It records time stamp and triggers interrupt by setting gpio pin 3 to 1. 
 * Interrupt callback function will be executed and this thread will go to sleep. 
 * Interrupt handler records the time stamp and sets the gpio pin 3 to 0 and returns.
 * This thread is then awaken, it will calculate the difference between the two readings and this cycle is repeated sample number of times.
 * Then, the clock ticks are converted to nano seconds and the output is rendered.
 * It again sets the onGoing flag to zero to indicate measurement is finished and then returns. 
 */
void kthread_interruptFunc(void *arg1, void *arg2, void *arg3)
{	
	int *argum = (int *)arg1;                                  //arg1 contains the number of samples to be taken.
	int samples = *argum;
	unsigned int intLatency=0;
	unsigned int x=10;
	unsigned int y=4; 
	DPRINTK("INTERRUPT KTHREAD CREATED \n");
	DPRINTK("SAMPLES = %d \n",samples);
	k_mutex_lock(&threadInterrupt_mutex,K_FOREVER);              
	onGoing=1;
	k_mutex_unlock(&threadInterrupt_mutex);
	DPRINTK("onGoing=1\n");
	for(int i=0; i<samples; i++)
	{	
		DPRINTK("triggering %dth time \n",i);
		interrupt_tscBefore = getticks();
		gpio_pin_write(gpiob,6,1);
		k_sleep(100);
		intLatency += (unsigned int)(interrupt_tscAfter - interrupt_tscBefore);
	}
	intLatency = intLatency/samples;
	
	intLatency = intLatency - average_delay;
	intLatency = x*intLatency;
	intLatency = intLatency/y;
	DPRINTK("Interrupt Latency Found %u ns \n",intLatency);
	interrupt_tscBefore = 0;
	interrupt_tscAfter = 0;
	k_mutex_lock(&threadInterrupt_mutex,K_FOREVER);
	onGoing=0;
	k_mutex_unlock(&threadInterrupt_mutex);
	DPRINTK("Exiting \n");
	return;
}

/*
 * Function callback for shell command RGB 
 * Description: It checks whether the values it recieved is in correct range, if not it returns.
 * If the values are within range, it sets PWM3,5,7 with appropriate duty cycle and then returns. 
 */
static int cmd_project1_RGB_display(const struct shell *shell, size_t argc, char **argv)
{

	dutyCycle_x = atoi(argv[1]);
	dutyCycle_y = atoi(argv[2]);
	dutyCycle_z = atoi(argv[3]);
	
	if(dutyCycle_x < 0 || dutyCycle_x > 100 || dutyCycle_y < 0 || dutyCycle_y > 100 || dutyCycle_z < 0 || dutyCycle_z > 100)
	{
		shell_print(shell,"Invalid duty cycle \n");
		return 0;
	}

	pwm_pin_set_cycles(pwm_dev, 3,PERIOD,(PERIOD*dutyCycle_x)/100);
	pwm_pin_set_cycles(pwm_dev, 5,PERIOD,(PERIOD*dutyCycle_y)/100);
	pwm_pin_set_cycles(pwm_dev, 7,PERIOD,(PERIOD*dutyCycle_z)/100);

	shell_print(shell,"x = %d \t y = %d \t z = %d \n",dutyCycle_x,dutyCycle_y,dutyCycle_z);
	
	return 0;
}


/*
 * Function callback for int latency shell command 
 * Description: It will check whether there is an ongoing measurement, if yes then it returns. 
 * If not, then it will create an interrupt thread whose callback function is thread_Interrupt and then it returns.
 */
static int cmd_project1_int_latency(const struct shell *shell, size_t argc, char **argv)
{
	int check;
	int samplesInterrupt = atoi(argv[1]);
	DPRINTK("Interrupt Samples = %d \n",samplesInterrupt);
	k_mutex_lock(&threadInterrupt_mutex,K_FOREVER);
	check = onGoing;
	k_mutex_unlock(&threadInterrupt_mutex);
	if(check==1)
	{
		DPRINTK("ON GOING MEASUREMENT, TRY AGAIN LATER \n");
		return 0;
	}
	k_thread_create(&thread_Interrupt, threadInterrupt_stack_area, K_THREAD_STACK_SIZEOF(threadInterrupt_stack_area),kthread_interruptFunc,&samplesInterrupt, NULL, NULL,5, 0, K_NO_WAIT);
	return 0;
}

/*
 * Function callback for cs latency shell command 
 * Description: It will check whether there is an ongoing measurement, if yes then it returns. 
 * If not, then it will create a high priority thread, then a low priority thread and returns.
 */
static int cmd_project1_cs_latency(const struct shell *shell, size_t argc, char **argv)
{
	int check;
	k_mutex_lock(&threadFlag_mutex,K_FOREVER);
	check=onGoing_CS;
	k_mutex_unlock(&threadFlag_mutex);
	if(check==1)
	{
		DPRINTK("ONGOING MEASUREMENT, PLEASE TRY AGAIN LATER \n");
	}
	csSamples = atoi(argv[1]);
	shell_print(shell,"Context switch samples to be collected = %d \n",csSamples);
	k_thread_create(&thread_contextHigh,threadContext_stack_area_HIGH,K_THREAD_STACK_SIZEOF(threadContext_stack_area_HIGH),kthread_contextHigh,&csSamples,NULL,NULL,1,0,K_NO_WAIT);
	k_thread_create(&thread_contextLow,threadContext_stack_area_LOW,K_THREAD_STACK_SIZEOF(threadContext_stack_area_LOW),kthread_contextLow,&csSamples,NULL,NULL,7,0,K_NO_WAIT);
	return 0;
}

/*
 * Macro for registering and creating shell commands and associating them with appropriate handlers. 
 */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_project1,
	SHELL_CMD(RGB-display, NULL, "Take duty cycles of RGB LED", cmd_project1_RGB_display),
	SHELL_CMD(int-latency, NULL, "Take interrupt latency", cmd_project1_int_latency),
	SHELL_CMD(cs-latency, NULL, "Take context switch latency", cmd_project1_cs_latency),

	SHELL_SUBCMD_SET_END /* Array terminated. */
);

/*
 * Macro to register shell command defined above.
 */
SHELL_CMD_REGISTER(project1, &sub_project1, "project1 commands", NULL);



void main(void)
{
	int ret;
	int counter=100;
	unsigned long long before;
	unsigned long long after;
	unsigned int avg_delay=0;

	pinmux=device_get_binding(CONFIG_PINMUX_NAME);       
	struct galileo_data *dev = pinmux->driver_data;		// Access pin multiplexer device of the galileo. 

	pwm_dev=dev->pwm0; 								    // Galileo board has bunch of devices which can be controlle, we want to access the PWM device, therefore pwm0.
						
	if (!pwm_dev) {
		DPRINTK("error\n");
		return;
	}

	gpiob=dev->gpio_dw;									// Galileo board has bunch of devices which can be controlle, we want to access the gpio device, therefore gpiob.

	if (!gpiob) {
		DPRINTK("error\n");
		return;
	}


	ret=pinmux_pin_set(pinmux,5,PINMUX_FUNC_C); //IO 5 -- PWM3
	if(ret<0)
		DPRINTK("error setting pin for IO5\n");

	ret=pinmux_pin_set(pinmux,6,PINMUX_FUNC_C); //IO 6 -- PWM5
	if(ret<0)
		DPRINTK("error setting pin for IO5\n");

	ret=pinmux_pin_set(pinmux,9,PINMUX_FUNC_C); //IO 9 -- PWM7
	if(ret<0)
		DPRINTK("error setting pin for IO6\n");


	ret=gpio_pin_configure(pwm_dev, 3, GPIO_DIR_OUT);  // Setting the mode of the PWM pins to output
	if(ret<0)
		DPRINTK("error setting pin for IO5\n");

	ret=gpio_pin_configure(pwm_dev, 5, GPIO_DIR_OUT); 
	if(ret<0)
		DPRINTK("error setting pin for IO6\n");
	
	ret=gpio_pin_configure(pwm_dev, 7, GPIO_DIR_OUT); 
	if(ret<0)
		DPRINTK("error setting pin for IO9\n");

	ret=pinmux_pin_set(pinmux,3,PINMUX_FUNC_A); //IO3 -- gpio 6
	if(ret<0)
		DPRINTK("error setting pin for IO3\n");

		
	ret=pinmux_pin_set(pinmux,12,PINMUX_FUNC_B); //IO12  -- gpio7 [interrupt pin]
	if(ret<0)
		DPRINTK("error setting pin for IO12\n");

	ret=gpio_pin_configure(gpiob, 7, PULL_UP| GPIO_DIR_IN | GPIO_INT | EDGE_RISING);  // Configuring the interrupt pin.

	gpio_init_callback(&gpio_cb, interrupt_cb, BIT(7));

	ret=gpio_add_callback(gpiob, &gpio_cb);   // Adding a callback with the gpio device for the interrupt pin.
	if(ret<0)
		DPRINTK("error adding callback\n");
	
	ret=gpio_pin_enable_callback(gpiob, 7);   // Enabling the callback for GPIO 7 (IO12)
	if(ret<0)
		DPRINTK("error enabling callback\n");

	ret=gpio_pin_configure(gpiob, 6, GPIO_DIR_OUT);  // Setting the mode of the GPIO 6 (IO3) to output

/*
* IO pin 1 is used for testing purposes. It is used to calculate the average delay associated with writing a value to the pin. 
*/
	ret=pinmux_pin_set(pinmux,1,PINMUX_FUNC_A); //IO 1 -- GPIO4  
	if(ret<0)
		DPRINTK("error setting pin for IO1\n");

	ret=gpio_pin_configure(gpiob, 4, GPIO_DIR_OUT); 

	/* Calculating the average delay for exectuing gpio_pin_write */
	for(int i=0; i<counter; i++)
	{
		before = getticks();
		gpio_pin_write(gpiob,4,1);
		after = getticks();
		gpio_pin_write(gpiob,4,0);
		avg_delay += (unsigned int)(after - before);
	}

	avg_delay = avg_delay/100;
	average_delay = avg_delay;

	avg_delay = 0;
	
	/* Calculating the average delay for executing k_sem_take */
	for(int j=0; j<counter; j++)
	{
		before = getticks();
		k_sem_take(&test_delay,K_FOREVER);
		after = getticks();
		avg_delay += (unsigned int)(after - before);
	}

	avg_delay = avg_delay/100;
	average_delay_lock = avg_delay;

	return;
}