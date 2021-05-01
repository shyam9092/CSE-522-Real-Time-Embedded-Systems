#include <tracing_thread_events.h>

#include <zephyr.h>
#include <kernel_structs.h>
#include <init.h>
#include <misc/printk.h>


// #define DEBUG 

#if defined(DEBUG) 
	#define DPRINTK(fmt, args...) printk("DEBUG: %s():%d: " fmt, \
   		 __func__, __LINE__, ##args)
#else
 	#define DPRINTK(fmt, args...) /* do nothing if not defined*/
#endif


/* Index for custom structure for recording thread events */
static int tEvents_index=0;

/* Custom structure for recording thread events */
struct threadInfo threadEvents[fix_size];

/* Flag for starting trace and ending trace */
bool trace=false;

/* Function to read time stamp */
static inline unsigned long long getticks(void)
{
	unsigned int lo, hi;
	// RDTSC copies contents of 64-bit TSC into EDX:EAX
	__asm__ volatile("rdtsc" : "=a" (lo), "=d" (hi));
	return (unsigned long long)hi << 32 | lo;
}

/* Function to dump collected trace */
void tracing_dump(void)
{	
	printk("tracing_dump \n");	
	for(int i=0; i<tEvents_index-1; i++)
	{
		printk("%s-%c-%u \n",threadEvents[i].thread_name,threadEvents[i].event,threadEvents[i].TSC);
	}
	tEvents_index = 0;
}

/* Function to end collection of trace */
void sys_trace_end_call(unsigned int id)
{	
	unsigned int k;
	if(id==TRACING_END)
	{	
		k = irq_lock();
		trace=false;         // Set trace flag to false
		irq_unlock(k);
		DPRINTK("tace=0 \n");
	}
}

/* Function to turn on tracing, record mutex lock and unlock events */
void sys_trace_void(unsigned int id)
{
	unsigned long long temp = getticks();
	unsigned int k;
	bool check;
	k = irq_lock();				/* Disable interrupts for the processor that is running the thread */
	check = trace;				/* Copy the tracing flag to a local variable */
	irq_unlock(k);				/* Enable back the interrupts for the processor */

	if(check)
	{	
		/* Check if id is for mutex_lock or mutex_unlock */
		if(id==SYS_TRACE_ID_MUTEX_LOCK || id==SYS_TRACE_ID_MUTEX_UNLOCK)
		{	
			if(tEvents_index > fix_size) {return;}
			k_tid_t id = k_current_get();
			const char *name = k_thread_name_get(id);
			threadEvents[tEvents_index].event='e';
			strcpy(threadEvents[tEvents_index].thread_name,name);
			threadEvents[tEvents_index].TSC = (temp/400);
			tEvents_index++;
			DPRINTK("[MUTEX] %s \n",name);
		}
	}

	/* Check if id is for tracing start */
	if(id==TRACING_START)
	{	
		k = irq_lock();
		trace=true;
		irq_unlock(k);
	}

}


/*
 * Function thread_switched in for tracing thread when they are switched in
 * Description: Time stamp is recorded first. Then, interrupts are disabled for the processor and tracing flag is copied into a local variable and interrupts are enabled back.
 * Check if the size of custom structure is reached, if yes then return. Else, get the name of the calling thread and store it along with time stamp and event type in the custom 
 * structure. 
 */
void sys_trace_thread_switched_in(void)
{
	unsigned long long temp = getticks();
	unsigned int k;
	bool check;
	k = irq_lock();
	check = trace;
	irq_unlock(k);

	if(check)
	{	
		if(tEvents_index > fix_size) {return;}
		k_tid_t id = k_current_get();
		const char *name = k_thread_name_get(id);
		threadEvents[tEvents_index].event='i';
		strcpy(threadEvents[tEvents_index].thread_name,name);
		threadEvents[tEvents_index].TSC = (temp/400);
		tEvents_index++;
		DPRINTK("%s \n",name);
	}
}

/*
 * Function thread_switched out for tracing thread when they are switched out
 * Description: Time stamp is recorded first. Then, interrupts are disabled for the processor and tracing flag is copied into a local variable and interrupts are enabled back.
 * Check if the size of custom structure is reached, if yes then return. Else, get the name of the calling thread and store it along with time stamp and event type in the custom 
 * structure. 
 */
void sys_trace_thread_switched_out(void)
{
	unsigned long long temp = getticks();
	unsigned int k;
	bool check;
	k = irq_lock();
	check = trace;
	irq_unlock(k);

	if(check)
	{
		if(tEvents_index > fix_size) {return;}
		k_tid_t id = k_current_get();
		const char *name = k_thread_name_get(id);
		threadEvents[tEvents_index].event='o';
		strcpy(threadEvents[tEvents_index].thread_name,name);
		threadEvents[tEvents_index].TSC = (temp/400);
		tEvents_index++;
		DPRINTK("%s \n",name);
	}
}

/*
 * Function thread create for tracing thread when they are created
 * Description: Time stamp is recorded first. Then, interrupts are disabled for the processor and tracing flag is copied into a local variable and interrupts are enabled back.
 * Check if the size of custom structure is reached, if yes then return. Else, get the name of the calling thread and store it along with time stamp and event type in the custom 
 * structure. 
 */
void sys_trace_thread_create(struct k_thread *thread)
{	
	unsigned long long temp = getticks();
	unsigned int k;
	bool check;
	k = irq_lock();
	check = trace;
	irq_unlock(k);

	if(check)
	{
		k_tid_t id = k_current_get();
		const char *name = k_thread_name_get(id);
		DPRINTK("CREATE: %s \n",name);
		threadEvents[tEvents_index].event='e';
		strcpy(threadEvents[tEvents_index].thread_name,name);
		threadEvents[tEvents_index].TSC = (temp/400);
	}
}

/*
 * Function thread ready in for tracing thread when they are ready
 * Description: Time stamp is recorded first. Then, interrupts are disabled for the processor and tracing flag is copied into a local variable and interrupts are enabled back.
 * Check if the size of custom structure is reached, if yes then return. Else, get the name of the calling thread and store it along with time stamp and event type in the custom 
 * structure. 
 */
void sys_trace_thread_ready(struct k_thread *thread)
{
	unsigned long long temp = getticks();
	unsigned int k;
	bool check;
	k = irq_lock();
	check = trace;
	irq_unlock(k);
	if(check)
	{	
		if(tEvents_index > fix_size) {return;}
		k_tid_t id = k_current_get();
		const char *name = k_thread_name_get(id);
		DPRINTK("ready: %s \n",name);
		threadEvents[tEvents_index].event='e';
		strcpy(threadEvents[tEvents_index].thread_name,name);
		threadEvents[tEvents_index].TSC = (temp/400);
		tEvents_index++;
	}
}

/*
 * Function thread_switched in for tracing thread when they are pending
 * Description: Time stamp is recorded first. Then, interrupts are disabled for the processor and tracing flag is copied into a local variable and interrupts are enabled back.
 * Check if the size of custom structure is reached, if yes then return. Else, get the name of the calling thread and store it along with time stamp and event type in the custom 
 * structure. 
 */
void sys_trace_thread_pend(struct k_thread *thread)
{	
	unsigned long long temp = getticks();
	unsigned int k;
	bool check;
	k = irq_lock();
	check = trace;
	irq_unlock(k);
	if(check)
	{
		if(tEvents_index > fix_size) {return;}
		k_tid_t id = k_current_get();
		const char *name = k_thread_name_get(id);
		DPRINTK("pending: %s \n",name);
		threadEvents[tEvents_index].event='e';
		strcpy(threadEvents[tEvents_index].thread_name,name);
		threadEvents[tEvents_index].TSC = (temp/400);
		tEvents_index++;
	}
}



void sys_trace_idle(void)
{
}

void sys_trace_isr_enter(void)
{
}

void z_sys_trace_idle(void)
{
	sys_trace_idle();
}

void z_sys_trace_isr_enter(void)
{
	sys_trace_isr_enter();
}

void z_sys_trace_isr_exit(void)
{
	sys_trace_isr_exit();
}

void z_sys_trace_thread_switched_in(void)
{
	sys_trace_thread_switched_in();
}

void z_sys_trace_thread_switched_out(void)
{
	sys_trace_thread_switched_out();
}
