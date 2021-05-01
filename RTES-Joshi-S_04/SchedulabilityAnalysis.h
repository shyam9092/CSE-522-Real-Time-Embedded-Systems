/* Structure to store the task data */
struct task       
{
    float WCET;
    float deadline;
    float period;
};

/* Structure to store the task data in form of array in sorted manner */
struct task_list
{
    float WCET;
    float deadline;
    float period;
};