Name: Shyam Bhaveshbhai Joshi
ID: 1218594676
------------------------------

This is a readme file to compile and test the RTES Assignment4.
Current directory has structures like below. Extract the zip file.
------------------------
RTES-Joshi-S_04/
├── SchedulabilityAnalysis.c
└── SchedulabilityAnalysis.h
└── Makefile
└── README.txt


Steps to run SchedulabilityAnalysis.c
--------------------------
The RTES-Joshi-S_04 folder contains the following:
SchedulabilityAnalysis.c:    C program to run the schedulability analysis 
SchedulabilityAnalysis.h:    Header file for above mentioned C file
Makefile:                    Makefile to generate executable file
README.txt:                  Readme file for instructions


Follow steps to compile and run the executable file in the folder where you have extracted the zip file
Open terminal and run the following commands: 

$ make

=> Following log will appear:
gcc -o assignment4 SchedulabilityAnalysis.c -Wall -lm
=> The above command will create an executable file name assignment4

$ ./assignment4

=> It will ask you to enter the name of the input file

$ Enter the input file name
$ A4_test.txt

=> A text file containing the schedulability analysis report will be generated under the name Analysis.txt

$ cat Analysis.txt 

=> An exmaple log like the following will appear

Total number of task sets: 5 
Number of tasks in taskset-1 : 3 
EDF Utilization test passed 
performing RM test
utilization test of RM fails for task #3 
not schedulable. WCET: 73.000000 
Number of tasks in taskset-2 : 3 
EDF Utilization test failed 
Using loading factor, task is schedulable 
Number of tasks in taskset-3 : 7 
EDF Utilization test passed 
performing RM test
utilization test of RM fails for task #5 
RM test is schedulable at a:118.000000 
utilization test of RM fails for task #6 
not schedulable. WCET: 352.000000 
Number of tasks in taskset-4 : 7 
EDF Utilization test failed 
At time instance 240.000000, using loading factor, task is not schedulable 
Number of tasks in taskset-5 : 3 
EDF Utilization test passed 
performing RM test
utilization test of RM fails for task #3 
RM test is schedulable at a:300.000000 
Utilization test passed for RM
