#include <sensor.h>
#include <gpio.h>
#include <pinmux.h>
#include "hcsr04.h"
#include <misc/util.h>
#include <misc/printk.h>
#include "../boards/x86/galileo/board.h"
#include "../boards/x86/galileo/pinmux_galileo.h"
#include "../drivers/gpio/gpio_dw_registers.h"
#include "../drivers/gpio/gpio_dw.h"
#include <version.h>
#include <zephyr.h>
#include <device.h>
#include <misc/util.h>
#include <misc/printk.h>
#include <stdlib.h>
#include <string.h>


#define DEBUG
#define EDGE_RISING		(GPIO_INT_EDGE | GPIO_INT_ACTIVE_HIGH)
#define EDGE_FALLING    (GPIO_INT_EDGE | GPIO_INT_ACTIVE_LOW)
#define PULL_UP 0

#define STACKSIZE 8192
K_THREAD_STACK_DEFINE(threadDistance_stackSize,STACKSIZE);

#if defined(DEBUG) 
	#define DPRINTK(fmt, args...) printk("DEBUG: %s():%d: " fmt, \
   		 __func__, __LINE__, ##args)
#else
 	#define DPRINTK(fmt, args...) /* do nothing if not defined*/
#endif

/* Mutex to be used by HCSR0 and HCSR1 */
K_MUTEX_DEFINE(onGoingHCSR0_mutex); 
K_MUTEX_DEFINE(onGoingHCSR1_mutex);

/* Device data to hold the information related to GPIO pins/port/device for HCSR0 and HCSR1 */
struct device_data {
    struct device *gpio_port_trig;
    struct device *gpio_port_echo;
    u32_t port_number_trig;
    u32_t port_number_echo;
    u32_t trigger_pin;
    u32_t echo_pin;     
}; 

struct device_data d0,d1;

/* Device structure to access pin multiplexer of galileo */
struct device *pinmux;

/* Interrupt flag for switching edge sensitivity (rising to falling & falling to rising) */
int flag_interrupt_zero;
int flag_interrupt_one;

/* Interrupt callback structure */
struct gpio_callback callbzero;
struct gpio_callback callbone;

/* Variables to recording time stamps for distance measurement, inside interrupt handler for each device*/
unsigned long long tscBefore_zero;
unsigned long long tscAfter_zero;

unsigned long long tscBefore_one;
unsigned long long tscAfter_one;

/* Variable to record TSC when measurement is done */
unsigned long long TSC_zero, TSC_one;

/* Variable to hold distance for each device */
unsigned int distance_0,distance_1;

/* Flag to indicate on going measurement for each device */
bool onGoing_0,onGoing_1;

/* Flag to indicate whether buffer is full or not, for each device */
bool bufferFull_0, bufferFull_1;

/* Variable to store timeout for each device */
unsigned long long timeout_0 = 1000000;
unsigned long long timeout_1 = 1000000;

/* Driver data for HCSR0  */
static struct hcsr_data hcsr0_data = {
    .echo_pin = (u32_t) CONFIG_HCSR0_ECHO,
    .trigger_pin = (u32_t) CONFIG_HCSR0_TRIG,
};

/* Driver data for HCSR1  */
static struct hcsr_data hcsr1_data = {
    .echo_pin = (u32_t) CONFIG_HCSR1_ECHO,
    .trigger_pin = (u32_t) CONFIG_HCSR1_TRIG,
};


/* Function to read time stamp */
static inline unsigned long long getticks(void)
{
	unsigned int lo, hi;
	// RDTSC copies contents of 64-bit TSC into EDX:EAX
	__asm__ volatile("rdtsc" : "=a" (lo), "=d" (hi));
	return (unsigned long long)hi << 32 | lo;
}

/*
 * Function interrupt handler for device 0
 * Description: Interrupt flag is set to zero during initialisation phase. So, for the first call, flag is zero and TSC is recorded in 
 * tscBefore_zero variable. Flag is set to one and echo pin's sensitivity is changed to falling edge for device 0. The next time, the handler is invoked 
 * is during falling edge, then tscAfter_zero records the time stamp. Flag is set to zero again for future measurements and echo pin's sensitivity is configured back
 * to rising edge.
 */
void interrupt_function(struct device *gpiob, struct gpio_callback *cb, u32_t pins)
{   
    if(flag_interrupt_zero==0)                      /* Rising edge invocation of the handler */
    {
        tscBefore_zero = getticks();                /* Record time stamp */
        flag_interrupt_zero=1;                      /* Set flag to 1 */                
        gpio_pin_configure(d0.gpio_port_echo,d0.port_number_echo, GPIO_DIR_IN | GPIO_INT | EDGE_FALLING); /* Change sensitivity to falling edge */
        return;
    }
    else                                           /* Falling edge invocation of the handler */
    {
        tscAfter_zero = getticks();                 /* Record time stamp */
        flag_interrupt_zero=0;                        /* Set flag to 0 */
        gpio_pin_configure(d0.gpio_port_echo,d0.port_number_echo, GPIO_DIR_IN | GPIO_INT | EDGE_RISING); /* Change sensitivity to rising edge */
    }
}

/*
 * Function interrupt handler for device 1
 * Description: Interrupt flag is set to zero during initialisation phase. So, for the first call, flag is zero and TSC is recorded in 
 * tscBefore_one variable. Flag is set to one and echo pin's sensitivity is changed to falling edge for device 0. The next time, the handler is invoked 
 * is during falling edge, then tscAfter_one records the time stamp. Flag is set to zero again for future measurements and echo pin's sensitivity is configured back
 * to rising edge.
 */
void other_interrupt_function(struct device *gpiob, struct gpio_callback *cb, u32_t pins)
{
    if(flag_interrupt_one==0)                   /* Rising edge invocation of the handler */
    {
        tscBefore_one = getticks();             /* Record time stamp */
        flag_interrupt_one = 1;                 /* Set flag to 1 */ 
        gpio_pin_configure(d1.gpio_port_echo,d1.port_number_echo, GPIO_DIR_IN | GPIO_INT | EDGE_FALLING); /* Change sensitivity to rising edge */
        return;
    }
    else
    {
        tscAfter_one = getticks();              /* Record time stamp */
        flag_interrupt_one = 0;                 /* Set flag to 1 */ 
        gpio_pin_configure(d1.gpio_port_echo,d1.port_number_echo, GPIO_DIR_IN | GPIO_INT | EDGE_RISING); /* Change sensitivity to rising edge */
    }
}

/*
 * Function device setup function for device 0
 * Description: flag_interrupt_zero is set to zero for future interrupt handling. echo pin, trigger pin, gpio number of echo pin and trigger pin, device pointer to access
 * echo pin and gpio pin are stored inside device_data structrure. trigger pin and echo pin are configured for measurement. 
 */
void device_zero_setup(struct device *device)
{
    int ret;
    flag_interrupt_zero=0;                          /* Set flag to zero for future interrupt handling */
    struct hcsr_data *data = device->driver_data;   /* Get driver data stored in HCSR0 device */
    struct galileo_data *ptr = pinmux->driver_data; /* Access pin multiplexer device of the galileo */
    d0.echo_pin = data->echo_pin;                   /* Store echo pin number */
    d0.trigger_pin = data->trigger_pin;              /* Store trigger pin number */
    DPRINTK("e=%d \t t=%d \n",d0.echo_pin,d0.trigger_pin);

    /* Check if trigger pin's device is gpio_dw or not */
    if(d0.trigger_pin == 0 || d0.trigger_pin == 1 || d0.trigger_pin == 2 || d0.trigger_pin == 3 ||  d0.trigger_pin == 5 || d0.trigger_pin == 6 || d0.trigger_pin == 10 || d0.trigger_pin == 12)
    {
        DPRINTK("gpio_dw \n");
        d0.gpio_port_trig = ptr->gpio_dw;  /* Store gpio_dw as device pointer for future configuration */
    }

    /* Check if trigger pin's device is exp1 or not */
    else if(d0.trigger_pin == 7 || d0.trigger_pin == 8) 
    {
        DPRINTK("exp1 \n");
        d0.gpio_port_trig = ptr->exp1;      /* Store exp1 as device pointer for future configuration */
    }

    /* Check if trigger pin's device is exp2 or not */
    else
    {
        DPRINTK("exp2 \n");
        d0.gpio_port_trig = ptr->exp2;      /* Store exp2 as device pointer for future configuration */
    }

    /* Figure out the corresponding gpio port number associated with trigger pin number */
    switch(d0.trigger_pin)
    {
        case 0:
            d0.port_number_trig = 3;
            break;

        case 1:
            d0.port_number_trig = 4;
            break;

        case 2:
            d0.port_number_trig = 5;
            break;
        
        case 3:
            d0.port_number_trig = 6;
            break;
        
        case 5:
            d0.port_number_trig = 8;
            break;

        case 6:
            d0.port_number_trig = 9;
            break;
        
        case 7:
            d0.port_number_trig = 6;
            break; 
        
        case 8:
            d0.port_number_trig = 10;
            break;
        
        case 10:
            d0.port_number_trig = 2;
            break;
        
        case 12:
            d0.port_number_trig = 7;
            break;
        
        case 14:
            d0.port_number_trig = 0;
            break;

        case 15:
            d0.port_number_trig = 2;
            break;

        case 16:
            d0.port_number_trig = 4;
            break;

        case 17:
            d0.port_number_trig = 6;
            break;
        
        case 18:
            d0.port_number_trig = 10;
            break;

        case 19:
            d0.port_number_trig = 12;
            break;
    }
    DPRINTK("Trigger port = %u \n",d0.port_number_trig);

    /* Check if echo pin's device is gpio_dw or not */
    if(d0.echo_pin == 0 || d0.echo_pin == 1 || d0.echo_pin == 2 || d0.echo_pin == 3 ||  d0.echo_pin == 5 || d0.echo_pin == 6 || d0.echo_pin == 10 || d0.echo_pin == 12)
    {
        DPRINTK("gpio_dw \n");
        d0.gpio_port_echo = ptr->gpio_dw;  /* Store gpio_dw as device pointer for future configuration */
    }

    /* Check if echo pin's device is exp1 or not */
    else if(d0.echo_pin == 7 || d0.echo_pin == 8) 
    {
        DPRINTK("exp1 \n");
        d0.gpio_port_echo = ptr->exp1;   /* Store exp1 as device pointer for future configuration */
    }

    /* Check if echo pin's device is exp2 or not */
    else
    {   
        DPRINTK("exp2 \n");
        d0.gpio_port_echo = ptr->exp2;  /* Store exp2 as device pointer for future configuration */
    }

    /* Figure out the corresponding gpio port number associated with echo pin number */
    switch(d0.echo_pin)
    {   
        case 0:
            d0.port_number_echo = 3;
            break;

        case 1:
            d0.port_number_echo = 4;
            break;

        case 2:
            d0.port_number_echo = 5;
            break;
        
        case 3:
            d0.port_number_echo = 6;
            break;
        
        case 5:
            d0.port_number_echo = 8;
            break;

        case 6:
            d0.port_number_echo = 9;
            break;
        
        case 7:
            d0.port_number_echo = 6;
            break; 
        
        case 8:
            d0.port_number_echo = 10;
            break;
        
        case 10:
            d0.port_number_echo = 2;
            break;
        
        case 12:
            d0.port_number_echo = 7;
            break;
        
        case 14:
            d0.port_number_echo = 0;
            break;

        case 15:
            d0.port_number_echo = 2;
            break;

        case 16:
            d0.port_number_echo = 4;
            break;

        case 17:
            d0.port_number_echo = 6;
            break;
    }
    DPRINTK("Echo port = %u \n",d0.port_number_echo);

    /* Configure trigger pin */
    ret = pinmux_pin_set(pinmux,d0.trigger_pin,PINMUX_FUNC_A);
    if(ret<0)
    {
        DPRINTK("error setting pin for IO%d \n",d0.trigger_pin);
    }  

    ret=gpio_pin_configure(d0.gpio_port_trig,d0.port_number_trig,GPIO_DIR_OUT);
    if(ret<0)
    {
        DPRINTK("error configuring pin for for IO%d \n",d0.trigger_pin);
    }

    /* Configure echo pin */
    ret = pinmux_pin_set(pinmux,d0.echo_pin,PINMUX_FUNC_B);
    if(ret<0)
    {
        DPRINTK("error setting pin for IO%d \n",d0.echo_pin);
    }  

    ret=gpio_pin_configure(d0.gpio_port_echo,d0.port_number_echo, GPIO_DIR_IN | GPIO_INT | EDGE_RISING);  /* Configuring the echo pin with appropriate flags. */
    
    gpio_init_callback(&callbzero,interrupt_function,BIT(d0.port_number_echo));

    ret=gpio_add_callback(d0.gpio_port_echo, &callbzero);   // Adding a callback with the gpio device for the interrupt pin.
	if(ret<0)
    {
        DPRINTK("error adding callback\n");      
    }
	
	ret=gpio_pin_enable_callback(d0.gpio_port_echo, d0.port_number_echo);   // Enabling the callback for GPIO 7 (IO12)
	if(ret<0)
    {
        DPRINTK("error enabling callback\n");
    }	
}

/*
 * Function device setup function for device 0
 * Description: flag_interrupt_zero is set to zero for future interrupt handling. echo pin, trigger pin, gpio number of echo pin and trigger pin, device pointer to access
 * echo pin and gpio pin are stored inside device_data structrure. trigger pin and echo pin are configured for measurement. 
 */
void device_one_setup(struct device *device)
{   
    int ret;
    flag_interrupt_one=0;                             /* Set flag to zero for future interrupt handling */
    struct hcsr_data *data = device->driver_data;     /* Get driver data stored in HCSR0 device */
    struct galileo_data *ptr = pinmux->driver_data;   /* Access pin multiplexer device of the galileo */
    d1.echo_pin = data->echo_pin;                       /* Store echo pin number */
    d1.trigger_pin = data->trigger_pin;                  /* Store trigger pin number */
    DPRINTK("e=%d \t t=%d \n",d1.echo_pin,d1.trigger_pin);

    /* Check if trigger pin's device is gpio_dw or not */
    if(d1.trigger_pin == 0 || d1.trigger_pin == 1 || d1.trigger_pin == 2 || d1.trigger_pin == 3 ||  d1.trigger_pin == 5 || d1.trigger_pin == 6 || d1.trigger_pin == 10 || d1.trigger_pin == 12)
    {
        DPRINTK("gpio_dw \n");                  /* Store gpio_dw as device pointer for future configuration */
        d1.gpio_port_trig = ptr->gpio_dw;
    }

    /* Check if trigger pin's device is exp1 or not */
    else if(d1.trigger_pin == 7 || d1.trigger_pin == 8) 
    {
        DPRINTK("exp1 \n");
        d1.gpio_port_trig = ptr->exp1;          /* Store exp1 as device pointer for future configuration */
    }

    /* Check if trigger pin's device is exp2 or not */
    else
    {   
        DPRINTK("exp2 \n");                     /* Store exp2 as device pointer for future configuration */
        d1.gpio_port_trig = ptr->exp2;
    }

    /* Figure out the corresponding gpio port number associated with trigger pin number */
    switch(d1.trigger_pin)
    {
        case 0:
            d1.port_number_trig = 3;
            break;

        case 1:
            d1.port_number_trig = 4;
            break;

        case 2:
            d1.port_number_trig = 5;
            break;
        
        case 3:
            d1.port_number_trig = 6;
            break;
        
        case 5:
            d1.port_number_trig = 8;
            break;

        case 6:
            d1.port_number_trig = 9;
            break;
        
        case 7:
            d1.port_number_trig = 6;
            break; 
        
        case 8:
            d1.port_number_trig = 10;
            break;
        
        case 10:
            d1.port_number_trig = 2;
            break;
        
        case 12:
            d1.port_number_trig = 7;
            break;
        
        case 14:
            d1.port_number_trig = 0;
            break;

        case 15:
            d1.port_number_trig = 2;
            break;

        case 16:
            d1.port_number_trig = 4;
            break;

        case 17:
            d1.port_number_trig = 6;
            break;
        
        case 18:
            d1.port_number_trig = 10;
            break;

        case 19:
            d1.port_number_trig = 12;
            break;
    }
    DPRINTK("Trigger port = %u \n",d1.port_number_trig);

    /* Check if echo pin's device is gpio_dw or not */
    if(d1.echo_pin == 0 || d1.echo_pin == 1 || d1.echo_pin == 2 || d1.echo_pin == 3 ||  d1.echo_pin == 5 || d1.echo_pin == 6 || d1.echo_pin == 10 || d1.echo_pin == 12)
    {
        DPRINTK("gpio_dw \n");              /* Store gpio_dw as device pointer for future configuration */
        d1.gpio_port_echo = ptr->gpio_dw;
    }

    /* Check if echo pin's device is exp1 or not */
    else if(d1.echo_pin == 7 || d1.echo_pin == 8) 
    {
        DPRINTK("exp1 \n");
        d1.gpio_port_echo = ptr->exp1;    /* Store exp1 as device pointer for future configuration */
    }

    /* Check if echo pin's device is exp2 or not */
    else
    {   
        DPRINTK("exp2\n");
        d1.gpio_port_echo = ptr->exp2;   /* Store exp2 as device pointer for future configuration */
    }

    /* Figure out the corresponding gpio port number associated with echo pin number */
    switch(d1.echo_pin)
    {   
        case 0:
            d1.port_number_echo = 3;
            break;

        case 1:
            d1.port_number_echo = 4;
            break;

        case 2:
            d1.port_number_echo = 5;
            break;
        
        case 3:
            d1.port_number_echo = 6;
            break;
        
        case 5:
            d1.port_number_echo = 8;
            break;

        case 6:
            d1.port_number_echo = 9;
            break;
        
        case 7:
            d1.port_number_echo = 6;
            break; 
        
        case 8:
            d1.port_number_echo = 10;
            break;
        
        case 10:
            d1.port_number_echo = 2;
            break;
        
        case 12:
            d1.port_number_echo = 7;
            break;
        
        case 14:
            d1.port_number_echo = 0;
            break;

        case 15:
            d1.port_number_echo = 2;
            break;

        case 16:
            d1.port_number_echo = 4;
            break;

        case 17:
            d1.port_number_echo = 6;
            break;
    }
    DPRINTK("Echo port = %u \n",d1.port_number_echo);

    /* Configure trigger pin */
    ret = pinmux_pin_set(pinmux,d1.trigger_pin,PINMUX_FUNC_A);
    if(ret<0)
    {
        DPRINTK("error setting pin for IO%d \n",d1.trigger_pin);
    }  

    ret=gpio_pin_configure(d1.gpio_port_trig,d1.port_number_trig,GPIO_DIR_OUT);
    if(ret<0)
    {
        DPRINTK("error configuring pin for for IO%d \n",d1.trigger_pin);
    }

    /* Configure echo pin */
    ret = pinmux_pin_set(pinmux,d1.echo_pin,PINMUX_FUNC_B);
    if(ret<0)
    {
        DPRINTK("error setting pin for IO%d \n",d1.echo_pin);
    }  

    ret=gpio_pin_configure(d1.gpio_port_echo,d1.port_number_echo, GPIO_DIR_IN | GPIO_INT | EDGE_RISING);  /* Configuring the echo pin with appropriate flags. */
    
    gpio_init_callback(&callbone,other_interrupt_function,BIT(d1.port_number_echo));

    ret=gpio_add_callback(d1.gpio_port_echo, &callbone);   // Adding a callback with the gpio device for the interrupt pin.
	if(ret<0)
    {
        DPRINTK("error adding callback\n");      
    }
	
	ret=gpio_pin_enable_callback(d1.gpio_port_echo, d1.port_number_echo);   // Enabling the callback for GPIO 7 (IO12)
	if(ret<0)
    {
        DPRINTK("error enabling callback\n");
    }

}


/*
 * Function init function for HCSR0 and HCSR1
 * Description: Based on device, it will invoke appropriate setup function. 
 */
int hcsr_init(struct device *dev)
{   
    struct device *ptr = dev;
    pinmux = device_get_binding(CONFIG_PINMUX_NAME);
    
    /* Check if init is invoked by HCSR0 */
    if(!strcmp(CONFIG_HCSR0_NAME,ptr->config->name))
    {
        DPRINTK("device 0\n");
        device_zero_setup(ptr);       /* Invoke setup function for HCSR0 */
    }
    /* Else init is invoked by HCSR1 */
    else
    {
        DPRINTK("device 1\n");     
        device_one_setup(ptr);       /* Invoke setup function for HCSR0 */
    }

    return 0;
}


/*
 * Function callback for distance thread for device HCSR0
 * Description: It will set onGoing flag to true. Set 1 to trigger pin and then clear it. It will then calculate distance based on TSC recorded by interrupt handler and record TSC when done.
 * It will store the distance and TSC in global variable.  It will onGoing flag to false and bufferFull to true. 
 */
void getDistance_zero(void *arg1, void *arg2, void *arg3)
{
    unsigned long long temp = getticks();
    k_mutex_lock(&onGoingHCSR0_mutex,K_FOREVER);
    onGoing_0 = true;                               /* Set the on going measrurement flag to true */
    k_mutex_unlock(&onGoingHCSR0_mutex);
    DPRINTK("entered kthread \n");
    unsigned int tempDistance;
    int div_1 = 400;
    int div_2 = 58;
    int i;
    
    gpio_pin_write(d0.gpio_port_trig,d0.port_number_trig,1);        /* Set 1 to trigger pin */
    for(i=0; i<4000; i++);
    gpio_pin_write(d0.gpio_port_trig,d0.port_number_trig,0);        /* Set 0 to trigger pin */
    DPRINTK("%d \n",i);
    tempDistance = (unsigned int) (tscAfter_zero - tscBefore_zero);
    tempDistance = tempDistance/div_1;
    tempDistance = tempDistance/div_2;                                  
    TSC_zero = getticks();                                           
    TSC_zero = TSC_zero - temp;                                         /* Store time */
    distance_0 = tempDistance;                                       /* Store distance */
    DPRINTK("Distance = %u \n",distance_0);
    k_mutex_lock(&onGoingHCSR0_mutex,K_FOREVER);
    onGoing_0 = false;                                /* Set the on going measrurement flag to false */
    bufferFull_0 = true;                              /* Set the on bufferFull flag to true */
    k_mutex_unlock(&onGoingHCSR0_mutex);
    return;

}

/*
 * Function callback for distance thread for device HCSR0
 * Description: It will set onGoing flag to true. Set 1 to trigger pin and then clear it. It will then calculate distance based on TSC recorded by interrupt handler and record TSC when done.
 * It will store the distance and TSC in global variable.  It will onGoing flag to false and bufferFull to true. 
 */
void getDistance_one(void *arg1, void *arg2, void *arg3)
{   
    unsigned long long temp = getticks();
    k_mutex_lock(&onGoingHCSR1_mutex,K_FOREVER);
    onGoing_1 = true;                               /* Set the on going measrurement flag to true */
    k_mutex_unlock(&onGoingHCSR1_mutex);
    DPRINTK("entered kthread \n");
    unsigned int tempDistance;
    int div_1 = 400;
    int div_2 = 58;
    int i;
    gpio_pin_write(d1.gpio_port_trig,d1.port_number_trig,1);     /* Set 1 to trigger pin */
    for(i=0; i<4000; i++);
    gpio_pin_write(d1.gpio_port_trig,d1.port_number_trig,0);     /* Set 0 to trigger pin */
    DPRINTK("%d \n",i);
    tempDistance = (unsigned int) (tscAfter_one - tscBefore_one);
    tempDistance = tempDistance/div_1;
    tempDistance = tempDistance/div_2;
    TSC_one = getticks();                                            
    TSC_one = TSC_one - temp;                                       /* Store time */
    distance_1 = tempDistance;                                       /* Store distance */
    DPRINTK("Distance = %u \n",distance_1);
    k_mutex_lock(&onGoingHCSR1_mutex,K_FOREVER);
    onGoing_1 = false;
    bufferFull_1 = true;
    k_mutex_unlock(&onGoingHCSR1_mutex);
    return;
}


/*
 * Function sensor_sample_fetch to trigger measurement. 
 * Description: It will identify the device (HCSR0/HCSR1) and appropriately check if there is an ongoing measurement for that device.
 * If yes, then it will return and if not, then it will create the thread to record distance and TSC.  
 */
int hcsr_sample_fetch(struct device *dev, enum sensor_channel chan)
{   
    bool check;
    DPRINTK("[%s] hcsr_sample_fetch \n",dev->config->name);
    if(!strcmp(CONFIG_HCSR0_NAME,dev->config->name))    /* Check if device is HCSR0 */
    {       
        k_mutex_lock(&onGoingHCSR0_mutex,K_FOREVER);
        check = onGoing_0;
        k_mutex_unlock(&onGoingHCSR0_mutex);
        /* Check if measurement is going on */
        if(check==1)
        {
            DPRINTK("On going measurement \n");
            return 0;                           /* Return as there is an ongoing measurement */
        }
        else
        {
            /* Create a thread to record distance and TSC */
            struct k_thread distanceThread;        
            k_thread_create(&distanceThread, threadDistance_stackSize, K_THREAD_STACK_SIZEOF(threadDistance_stackSize), getDistance_zero, NULL, NULL, NULL,1,0,K_NO_WAIT);
            DPRINTK("device 0 creating thread \n");
            return 0;
        }
           
    }
    else                                                /* else device is HCSR1 */
    {
        k_mutex_lock(&onGoingHCSR1_mutex,K_FOREVER);
        check = onGoing_1;
        k_mutex_unlock(&onGoingHCSR1_mutex);
        /* Check if measurement is going on */
        if(check==1)
        {
            DPRINTK("On going measurement \n");
            return 0;                                    /* Return as there is an ongoing measurement */
        }
        else
        {   
            /* Create a thread to record distance and TSC */
            struct k_thread distanceThread;        
            k_thread_create(&distanceThread, threadDistance_stackSize, K_THREAD_STACK_SIZEOF(threadDistance_stackSize), getDistance_one, NULL, NULL, NULL,1,0,K_NO_WAIT);
            DPRINTK("device 1 creating thread \n");
            return 0;
        }
    }

    
}

/*
 * Function sensor_channel_get to fetch measurement. 
 * Description: It will identify the device (HCSR0/HCSR1) and appropriately check the following:
 * 1. If buffer is full for the device, then it will copy the measurements in sensor_value pointer.
 * 2. Else buffer is empty, then it will check if there is an on going measurement. 
 * 2a. If there is an ongoing measurement, it will wait for it to finish and then copy the measurements in sensor_value pointer.
 * 2b. If there is no ongoing measurement, it will create a thread to record measurements and wait for it to finish. Then it will copy the measurements in sensor_value pointer.
 * 3. Set bufferFlag to false to indicate buffer is empty in each case before returning. 
 * 4. It will return -1 if timeout set for the device is expired.
 */
int hcsr_channel_get(struct device *dev, enum sensor_channel chan, struct sensor_value *val)
{
    unsigned long long timeout_before = getticks();
    unsigned long long timeout_after;
    timeout_before = timeout_before/400;
    struct sensor_value *sensor_val = val;
    DPRINTK("hcsr_channel_get \n");
    if(!strcmp(CONFIG_HCSR0_NAME,dev->config->name))            /* Check if device is HCSR0 */
    {
        bool check;
        DPRINTK("DEVICE %s \n",dev->config->name);
        k_mutex_lock(&onGoingHCSR0_mutex,K_FOREVER);
        check = bufferFull_0;                                    /* Check if buffer is full */
        k_mutex_unlock(&onGoingHCSR0_mutex);

        timeout_after = getticks();
        timeout_after = timeout_after/400;
        if(timeout_0 < (timeout_after-timeout_before))
        {
            DPRINTK("timeout\n");
            return -1;
        }

        if(check==true)                                          
        {   
            DPRINTK("[%s] buffer is full",dev->config->name);       /* Return after copying the measurements */
            sensor_val->val1 = (s32_t)distance_0;
            DPRINTK("TSC_ZERO = %u \n",(u32_t)(TSC_zero/400));
            sensor_val->val2 = (s32_t)(TSC_zero/400);            
            k_mutex_lock(&onGoingHCSR0_mutex,K_FOREVER);
            bufferFull_0 = 0;                                       /* Clear buffer */
            k_mutex_unlock(&onGoingHCSR0_mutex);

            return 0;
        }
        else                                                            /* Else buffer is empty */
        {   
            DPRINTK("[%s] buffer is empty \n",dev->config->name);
            k_mutex_lock(&onGoingHCSR0_mutex,K_FOREVER);
            check = onGoing_0;
            k_mutex_unlock(&onGoingHCSR0_mutex);

            timeout_after = getticks();
            timeout_after = timeout_after/400;
            if(timeout_0 < (timeout_after-timeout_before))
            {
                DPRINTK("timeout\n");
                return -1;
            }

            if(check==true)                                              /* Check if there is an ongoing measurement */
            {   
                DPRINTK("[%s] on going measurement \n",dev->config->name);
                while(check!=false)                                       /* Wait for it to finish  */
                {
                    k_mutex_lock(&onGoingHCSR0_mutex,K_FOREVER);
                    check = onGoing_0;
                    k_mutex_unlock(&onGoingHCSR0_mutex);
                    timeout_after = getticks();
                    timeout_after = timeout_after/400;
                    if(timeout_0 < (timeout_after-timeout_before))
                    {
                        DPRINTK("timeout\n");
                        return -1;
                    }
                }
                sensor_val->val1 = (s32_t)distance_0;                       /* Return after copying the measurements */
                sensor_val->val2 = (s32_t)(TSC_zero/400);                
                k_mutex_lock(&onGoingHCSR0_mutex,K_FOREVER);
                bufferFull_0 = 0;                                           /* Clear buffer */
                k_mutex_unlock(&onGoingHCSR0_mutex);
                
                return 0;
            }
            else                                                        /* Else there is no ongoing measurement*/
            {
                DPRINTK("[%s] creating kthread \n",dev->config->name);

                /* Create a thread to record measurement */
                struct k_thread distanceThread;        
                k_thread_create(&distanceThread, threadDistance_stackSize, K_THREAD_STACK_SIZEOF(threadDistance_stackSize), getDistance_zero, NULL, NULL, NULL,1,0,K_NO_WAIT);
                check = true;
                while(check!=false)                                 /* Wait for it to finish */
                {
                    k_mutex_lock(&onGoingHCSR0_mutex,K_FOREVER);
                    check = onGoing_0;
                    k_mutex_unlock(&onGoingHCSR0_mutex);

                    timeout_after = getticks();
                    timeout_after = timeout_after/400;
                    if(timeout_0 < (timeout_after-timeout_before))
                    {
                        DPRINTK("timeout\n");
                        return -1;
                    }
                }
                sensor_val->val1 = (s32_t)distance_0;
                sensor_val->val2 = (s32_t)(TSC_zero/400);                  /* Return after copying the measurements */
                k_mutex_lock(&onGoingHCSR0_mutex,K_FOREVER);            
                bufferFull_0 = 0;                                               /* Clear buffer */
                k_mutex_unlock(&onGoingHCSR0_mutex);
                return 0;
            }
        }
    }
    else                                                    /* Else device is HCSR1 */
    {
        bool check;
        DPRINTK("DEVICE %s \n",dev->config->name);
        k_mutex_lock(&onGoingHCSR1_mutex,K_FOREVER);
        check = bufferFull_1;                               /* Check if buffer is full */
        k_mutex_unlock(&onGoingHCSR1_mutex);

        timeout_after = getticks();
        timeout_after = timeout_after/400;
        if(timeout_1 < (timeout_after-timeout_before))
        {
            DPRINTK("timeout\n");
            return -1;
        }

        if(check==true)
        {
            DPRINTK("[%s] buffer is full",dev->config->name);       /* Return after copying the measurements */
            sensor_val->val1 = (s32_t)distance_1;
            sensor_val->val2 = (s32_t)(TSC_one/400);
            k_mutex_lock(&onGoingHCSR1_mutex,K_FOREVER);
            bufferFull_1 = 0;                                       /* Clear buffer */
            k_mutex_unlock(&onGoingHCSR1_mutex);
            return 0;
        }
        else                                                         /* Else buffer is empty */
        {
            DPRINTK("[%s] buffer is empty \n",dev->config->name);
            k_mutex_lock(&onGoingHCSR1_mutex,K_FOREVER);
            check = onGoing_1;
            k_mutex_unlock(&onGoingHCSR1_mutex);

            timeout_after = getticks();
            timeout_after = timeout_after/400;
            if(timeout_1 < (timeout_after-timeout_before))
            {
                DPRINTK("timeout\n");
                return -1;
            }

            if(check==true)                                                  /* Check if there is an ongoing measurement */
            {
                DPRINTK("[%s] on going measurement \n",dev->config->name);
                while(check!=false)                                              /* Wait for it to finish  */
                {
                    k_mutex_lock(&onGoingHCSR1_mutex,K_FOREVER);
                    check = onGoing_1;
                    k_mutex_unlock(&onGoingHCSR1_mutex);

                    timeout_after = getticks();
                    timeout_after = timeout_after/400;
                    if(timeout_1 < (timeout_after-timeout_before))
                    {
                        DPRINTK("timeout\n");
                        return -1;
                    }
                }
                k_mutex_lock(&onGoingHCSR1_mutex,K_FOREVER);    
                bufferFull_1 = 0;                                           /* Clear buffer */
                k_mutex_unlock(&onGoingHCSR1_mutex);                
                sensor_val->val1 = (s32_t)distance_1;                       /* Return after copying the measurements */
                sensor_val->val2 = (s32_t)(TSC_one/400);
                return 0;
            }   
            else                                                             /* Else there is no ongoing measurement*/
            {
                DPRINTK("[%s] creating kthread \n",dev->config->name);
                 /* Create a thread to record measurement */
                struct k_thread distanceThread;        
                k_thread_create(&distanceThread, threadDistance_stackSize, K_THREAD_STACK_SIZEOF(threadDistance_stackSize), getDistance_one, NULL, NULL, NULL,1,0,K_NO_WAIT);
                check = true;
                while(check!=false)                                                  /* Wait for it to finish  */
                {
                    k_mutex_lock(&onGoingHCSR1_mutex,K_FOREVER);
                    check = onGoing_1;
                    k_mutex_unlock(&onGoingHCSR1_mutex);

                    timeout_after = getticks();
                    timeout_after = timeout_after/400;
                    if(timeout_1 < (timeout_after-timeout_before))
                    {
                        DPRINTK("timeout\n");
                        return -1;
                    }
                }   
                sensor_val->val1 = (s32_t)distance_1;                               /* Return after copying the measurements */
                sensor_val->val2 = (s32_t)(TSC_one/400);
                k_mutex_lock(&onGoingHCSR1_mutex,K_FOREVER);
                bufferFull_1 = 0;                                                   /* Clear buffer */
                k_mutex_unlock(&onGoingHCSR1_mutex);
                return 0;
            }
        }
        
    }
}


/*
 * Function sensor_attr_set to record timeout duration. 
 * Description: It will identify the device (HCSR0/HCSR1) and appropriately store the timeout duration parameter:
 */
int hcsr_attribute_set(struct device *dev, enum sensor_channel chan, enum sensor_attribute attr,const struct sensor_value *val)
{   
    DPRINTK("hcsr_attributre_set \n");
    struct device *device = dev;
    if(!strcmp(CONFIG_HCSR0_NAME,device->config->name))
    {   
        timeout_0 = val->val1;
    }
    else
    {
        timeout_1 = val->val1;
    }
    return 0;
}

/* Initializing driver APIs */
static const struct sensor_driver_api hcsr_api = {
    .sample_fetch = hcsr_sample_fetch,
    .channel_get = hcsr_channel_get,
    .attr_set = hcsr_attribute_set,
};

/* Create HCSR0 device object and set it up for boot time initialization with driver_api*/
DEVICE_AND_API_INIT(hcsr0, CONFIG_HCSR0_NAME, hcsr_init, &hcsr0_data,
		    NULL, POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,
		    &hcsr_api);

/* Create HCSR1 device object and set it up for boot time initialization with driver_api*/
DEVICE_AND_API_INIT(hcsr1, CONFIG_HCSR1_NAME, hcsr_init, &hcsr1_data,
		    NULL, POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,
		    &hcsr_api);
            