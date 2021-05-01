#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "SchedulabilityAnalysis.h"

FILE *filePtr; // File Pointer for writing analysis report to a text file. 


void swap(struct task_list *ptr1, struct task_list *ptr2)
{
    struct task_list *temp = (struct task_list *)malloc(sizeof(struct task_list));
    temp->WCET = ptr1->WCET;
    temp->period = ptr1->period;

    ptr1->WCET = ptr2->WCET;
    ptr1->period = ptr2->period; 

    ptr2->WCET = temp->WCET;
    ptr2->period = temp->period;

    free(temp);
}

struct task_list *sortbyDeadline(struct task *ptr, int num_task)
{
    struct task_list *task_list = (struct task_list *)malloc(num_task*sizeof(struct task_list));  // Allocate memory to store all the tasks in ascending order of their deadlines

    for(int iter=0; iter<num_task; iter++)          // Temporarily copy the task's data in the order as mentioned in input file
    {
        task_list[iter].WCET = ptr[iter].WCET;
        task_list[iter].period = ptr[iter].period;
        task_list[iter].deadline = ptr[iter].deadline;
    }

    int i,j, min_index;             

    for(i=0; i<num_task-1; i++)
    {
        min_index = i;                      // Iterate from begining to the end

        for(j=i+1; j<num_task; j++)
        {
            if(task_list[j].deadline < task_list[min_index].deadline)
            {
                min_index = j;
            } 
        }

        if(i!=min_index)
        {
            swap(&task_list[min_index],&task_list[i]);
        }   
    }
    return task_list;
}

bool RM_test(struct task_list *taskList, int taskNum)
{

    float period = taskList[taskNum].period;    // Store the period of the task who failed the RM utilization test 
    float e = taskList[taskNum].WCET;           // Store the WCET of the task who failed the RM utilization test   
    int i;                                      
    float a_next=0;                             
    float a_prev=0;
    
    for(i=0; i<=taskNum; i++)
    {
        a_next += taskList[i].WCET;        // Calculate a0
    }

    while(a_next <= period)               // Perform analysis till we have not reached or exceeded the period
    {
        a_prev = a_next;
        a_next = 0;
        for(i=0; i<=taskNum-1; i++)
        {
            a_next += (ceil(a_prev/taskList[i].period) * taskList[i].WCET); // Calcualte a_next
        }

        a_next += e;            

        if(a_prev==a_next)      // If this condition is satisfied then that task is schedulable 
        {
            fprintf(filePtr,"RM test is schedulable at a:%f \n",a_next);
            return 1;           // Return true to indicate that task is schedulable
        }
        else if(a_next > period) // If this condition is satisfied then the task is not schedulable
        {
            fprintf(filePtr,"not schedulable. WCET: %f \n",a_next);
            return 0;           // Return false to indicate that task is not schedulable
        }
    }
    return 0;                
}

bool DM_test(struct task_list *taskList, int taskNum)
{

    float deadline = taskList[taskNum].deadline;    // Store the period of the task who failed the RM utilization test 
    float e = taskList[taskNum].WCET;               // Store the WCET of the task who failed the RM utilization test  

    int i;
    float a_next=0;
    float a_prev=0;
    
    for(i=0; i<=taskNum; i++)
    {
        a_next += taskList[i].WCET;                 // Calculate a0
    }

    while(a_next <= deadline)                       // Perform analysis till we have not reached or exceeded the period
    {
        a_prev = a_next;
        a_next = 0;
        for(i=0; i<=taskNum-1; i++)
        {
            a_next += (ceil(a_prev/taskList[i].deadline) * taskList[i].WCET); 
        }

        a_next += e;

        if(a_prev==a_next)                          // If this condition is satisfied then that task is schedulable 
        {
            fprintf(filePtr,"DM test is schedulable at a:%f \n",a_next);
            return 1;                                // Return true to indicate that task is schedulable
        }
        else if(a_next > deadline)                  // If this condition is satisfied then the task is not schedulable
        {
            fprintf(filePtr,"not schedulable. WCET: %f \n",a_next);
            return 0;                              // Return false to indicate that task is not schedulable
        }
    }
    return 0;
}

bool utilizationTest_DM(struct task_list *ptr, int num_task)
{
    float utilization=0;  // Variable to store utilization for a task
    float oneByN;
    float num;
    int ret;

    for(int i=0; i<num_task; i++)  // Iterate over all the tasks
    {
        utilization += (ptr[i].WCET/ptr[i].deadline); // Calculate the utilization 
        oneByN = (1/((float)(i+1)));
        num = pow(2,oneByN);
        num = num - 1;
        num = num*(i+1);
        if(utilization > num) // If the condition fails, then the taskset may or may not be schedulable
        {   

            fprintf(filePtr,"utilization test of DM fails task #%d \n",i+1);
            ret = DM_test(ptr,i);  // We need to perform response time analysis
            if(ret==0)
            {   
                free(ptr);  // Free the task structure which contains the sorted tasks by deadline
                return 0;   // Return false to indicate that DM test failed
            }
        }
    }
    fprintf(filePtr,"Utilization test passed for DM\n");
    free(ptr); // Free the task structure which contains the sorted tasks by deadline
    return 1;  // Return true to indicate that DM test passed
}

bool utilizationTest_RM(struct task_list *ptr, int num_task)
{
    float utilization=0;  // Variable to store utilization for a task
    float oneByN;
    float num;
    int ret;

    for(int i=0; i<num_task; i++)  // Iterate over all the tasks
    {
        utilization += (ptr[i].WCET/ptr[i].deadline);  // Calculate the utilization 
        oneByN = (1/((float)(i+1)));                            
        num = pow(2,oneByN);
        num = num - 1;
        num = num*(i+1);                            
        if(utilization > num)  // If the condition fails, then the taskset may or may not be schedulable
        {   

            fprintf(filePtr,"utilization test of RM fails for task #%d \n",i+1);  
            ret = RM_test(ptr,i); // We need to perform response time analysis
            if(ret==0)
            {   
                free(ptr);        // Free the task structure which contains the sorted tasks by deadline
                return 0;         // Return true to indicate that RM test failed
            }
        }
    }
    fprintf(filePtr,"Utilization test passed for RM\n");
    free(ptr);  // Free the task structure which contains the sorted tasks by deadline
    return 1;   // Return true to indicate that RM test passed
}

bool utilizationTest_EDF(struct task *ptr, int num_task)
{
    float executionTime=0;
    float deadline=0;
    float utilization=0;
    for(int i=0; i<num_task; i++)
    {
        executionTime = ptr[i].WCET;            
        deadline = ptr[i].deadline;
        utilization += executionTime/deadline; // Iteratively calculate and check e/d criteria 

        if(utilization > 1)                    // If uptil a particular task, e/d > 1 then stop. That particular taskset failed EDF utilization test
        {   
            fprintf(filePtr,"EDF Utilization test failed \n");
            return 0;                           // Return false indicating the taskset failed EDF utilization test 
        }
    }
    fprintf(filePtr,"EDF Utilization test passed \n"); // e/d < 1 for all the tasks
    return 1;                                   // Return true indicating the taskset passed EDF utilization test

}

bool loadingFactor(struct task *ptr, int num_task)
{
    double syncBusy_period = 0;  // Variable to store synchronous busy period 
    double temp;                 // Variable to store previous l values
    float time=0;                // Variable to store the time gone while calculating loading factor   
    float load=0;                // Variable to store the load   

   
    for(int i=0; i<num_task; i++)
    {
        syncBusy_period += ptr[i].WCET;   // L0
    }
    
    while(temp!=syncBusy_period)          // Iterate till L_i-1 != L_i
    {   
        temp = syncBusy_period;           // Store the previously calculated value
        syncBusy_period=0;                  
        for(int j=0; j<num_task; j++)
        {
            syncBusy_period += ptr[j].WCET * ceil(temp/ptr[j].period);  // Calculate the synchronous busy period 
        }      
    }

    syncBusy_period = temp; 

    temp = 0;
    double min_deadline=0;          // Variable to store the next deadline 
    float similar=0;

    while(time<syncBusy_period)      // Iterate till time expired is < synchronous busy period 
    {   
        similar=0;
        min_deadline = syncBusy_period + 1;  // min_deadline is initialized to a value > synchronous busy period
        temp=0;                              
        for(int i=0;i<num_task;i++)         // Iterate over all the tasks to find out the next deadline and that deadline's task's WCET
        {  
            temp = (floor(time/ptr[i].period) * ptr[i].period) + ptr[i].deadline;    // Calculate the next deadline
            if(temp>time)                                                           // Consider this deadline only if it is greater than the previous deadline
            {                               
                if(temp<=min_deadline)                                              
                {   
                    if(temp==min_deadline)             // If new deadline = previously calculated deadline, then minimum deadline remains same but WCET of all the tasks needed to considered which fulfills this condition
                    {
                        similar += ptr[i].WCET;             
                    }
                    else                                        
                    {
                        similar = ptr[i].WCET;       // Store the WCET of the task whose deadline is minimum
                    }
                    min_deadline = temp;            // Update the minimum deadline
                }    
            }
        }

        time = min_deadline;      // Update the expired time
        load += similar;          // Add the WCET of the task whose deadline is next
    
        if(load/time > 1)       // Check if load/time > 1, if yes, then that particular task set is not schedulable
        {
            fprintf(filePtr,"At time instance %f, using loading factor, task is not schedulable \n",time);
            return 0;
        }
    }

    fprintf(filePtr,"Using loading factor, task is schedulable \n");  // If the function has not returned yet, then the task is schedulable

    return 1;
}

int main(void)
{
    char inputFileName[256];                    // Variable to store the input file name
    printf("Enter the input file name \n");
    scanf("%s",inputFileName);        
    FILE *filePointer;                          // File pointer to read input file. 
    filePointer = fopen(inputFileName,"r");     // Open input file
    filePtr = fopen("Analysis.txt","w");        // Open output file
    if(filePtr == NULL)                         // Exit if output file cannot be opened.
    {
        printf("Analysis.txt failed to open \n");
        return 0;
    }
    char *dataToBeRead = NULL;                  // Variable to temporarily store the values returned by getline function
    int tasks;                                  // Variable to store total number of tasks for each tasksets in input file
    int taskSets;                               // Variable to store the number of task sets in input file
    struct task *task_ptr;
    size_t ignored = 0;                         // Variable to be used in getline function. 
    char space[2] = " ";                        // Variable to be used for seperating strings 
    char *token;                                // Variable to be used to store the value returned from strotk 
    int ret;                                    // Variable to store the return value from various functions                    
    int flag, counter;                          // Flag variable to indicate when all the tasks inside a taskset has deadline = period. Counter to calculate how many tasks has deadline = period
    int taskNumber=0;                           // Variable to indicate which taskset is being analysed

    if(filePointer == NULL)                     // If input file is NULL then don't do anything
    {
        printf("A4_test.txt failed to open \n");
    }
    else                                        // Perform schedulability analysis if filePointer!=Null
    {       
        getline(&dataToBeRead,&ignored,filePointer);                    // Get the total number of tasksets
        taskSets = atoi(dataToBeRead);                                  // Convert the string to integer
        fprintf(filePtr,"Total number of task sets: %d \n",taskSets);
        getline(&dataToBeRead,&ignored,filePointer);                    // Get total number of tasks inside each taskset
        while (taskSets > 0)                                            // Perform schedulability analysis till the last tasket
        {   
            counter=0;                                                  // Initialize the counter to zero for each taskset
            flag=0;                                                     // Initialize the deadline=period flag to zero for each taskset
            taskNumber++;                                                       
            tasks = atoi(dataToBeRead);                                   
            fprintf(filePtr,"Number of tasks in taskset-%d : %d \n",taskNumber,tasks);
            task_ptr = (struct task *)malloc(tasks*sizeof(struct task));  // Allocate memory of the task structure to store the WCET, deadline and period for each tasks
            getline(&dataToBeRead,&ignored,filePointer);
            for(int i=0; i<tasks; i++)                                  // Store WCET, deadline and period for each tasks
            {
                token = strtok(dataToBeRead,space);                                         
                task_ptr[i].WCET = atof(token);
                token = strtok(NULL,space);
                task_ptr[i].deadline = atof(token);
                token = strtok(NULL,space);
                task_ptr[i].period = atof(token);
                if(task_ptr[i].period == task_ptr[i].deadline)          // Check if deadline=period
                {
                    counter++;
                }
                getline(&dataToBeRead,&ignored,filePointer);
            }
            if(counter==tasks)                                          // If counter = total number of tasks then set flag to 1
            {
                flag=1;
            }
            ret = utilizationTest_EDF(task_ptr,tasks);                  // Perform EDF utilization test

            if(ret==0)                                                  // If EDF utilization test fails then perform loading test analysis
            {
                loadingFactor(task_ptr,tasks);
            }
            if(ret==1)                                                  // Perform RM/DM test only if EDF utilization test is passed
            {
                if(flag)                                                // Perform RM test if deadline=period for each task
                {   
                    fprintf(filePtr,"performing RM test\n");
                    struct task_list *taskList = sortbyDeadline(task_ptr,tasks);  // Sort the task structure according to deadline
                    utilizationTest_RM(taskList,tasks);                           // Perform RM utilization test and if needed then response time analysis
                }
                else                                                        // Perform DM test 
                {
                    fprintf(filePtr,"performing DM test \n");                       
                    struct task_list *taskList = sortbyDeadline(task_ptr,tasks); // Sort the task structure according to deadline
                    utilizationTest_DM(taskList,tasks);                          // Perform RM utilization test and if needed then response time analysis
                }
            }
            free(task_ptr);                                                 // Free the memory allocated to task structure after schedulability analysis is done
            taskSets--;                                                    
        }
    }

    fclose(filePointer);    // Close the file pointer to input file
    fclose(filePtr);        // Close the file pointer to output file
    return 0;
}