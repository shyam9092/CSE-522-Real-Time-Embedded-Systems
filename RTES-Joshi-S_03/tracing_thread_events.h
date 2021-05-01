#ifndef _TRACE_TRACING_THREAD_EVENETS_H
#define _TRACE_TRACING_THREAD_EVENETS_H

#include <kernel.h>
#include <kernel_structs.h>
#include <init.h>

void sys_trace_thread_switched_in(void);
void sys_trace_thread_switched_out(void);
void sys_trace_thread_create(struct k_thread *thread);
void sys_trace_thread_ready(struct k_thread *thread);
void sys_trace_thread_pend(struct k_thread *thread);

void sys_trace_void(unsigned int id);
void sys_trace_end_call(unsigned int id);
void tracing_dump(void);

#define TRACING_START 10
#define TRACING_END 20

#define sys_trace_thread_priority_set(thread)
#define sys_trace_thread_abort(thread)
#define sys_trace_thread_suspend(thread)
#define sys_trace_thread_resume(thread)
#define sys_trace_thread_info(thread)

#define sys_trace_isr_exit_to_scheduler()

#define sys_trace_isr_exit()


#define fix_size 5000                     // size of custom structure for recording thread events

struct threadInfo                          // custom structure for recording time stamps
{
    char event;                           // name of the event {i: task_switched_in, o: task_switched_out, e: other tasks}
    char thread_name[26];                 // name of the thread
    unsigned int TSC;                       // time stamp variable
};


#endif /* _TRACE_TRACING_THREAD_EVENETS_H */