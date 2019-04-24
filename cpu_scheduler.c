/**************************    cpu_scheduler.c    ***************************
* 
* Programmer:       Sean Anderson
*
* School:           University of Houston - Clear Lake
*
* Course:           CSCI 4354 - Operating Systems
*
* Date:             April 25, 2019
*
* Assignment:       Programming Assignment #4
*
* Environment:      Unix with GNU C Compiler
*
* Files Included:   cpu_scheduler.c, pcb_client.c, pcb_structs.h, mem_structs.h
*
* Purpose:          To start simulate a CPU Scheduler. The program will receive PCBs
*                   via a common FIFO which contain the burst time and amount of 
*                   memory allocation required to perform the task. The CPU Scheduler
*                   will confirm that there is sufficient memory available, allocate
*                   the appropriate amount of memory pagefiles and then complete each
*                   task according to a round-robin scheduling algorithm.
*
* Input:            (2) Parameters, total memory size and page file size, each passed
*                   as parameters when program is run. Program will default to 1024 and
*                   64 respectively if no parameters are passed. 
*
*                   Receives PCBs from PCB_Client program via fifo named cpu_fifo.
*
* Preconditions:    Fifo "cpu_fifo" must be successfully created and opened in order for
*                   program to enter into service loop.
*
* Output:           Prints running clock time. Prints PCB details when a new PCB is
*                   received or completed. Prints remaining burst and remaining round-
*                   robin time for current PCB in "running" state for each clock cycle.
*
* Postconditions:   Must close cpu_fifo and unlink it.
*
* Algorithm:        ** PROCESS INITIALIZATION **
*                   Initialize CPU statistic variables
*                   Declare currentPCB
*                   Initialize Ready_Queue
*                   Initialize Memory_Queue
*                   Make Fifo cpu_fifo
*                   Open FIFO cpu_fifo in Read-Only Mode
*
*                   ** MAIN LOOP **
*                   Print current value of cpu_clock
*                   Sleep for 2 seconds (to allow pcb_clients to connect and write)
*
*                   PART I
*                   Try to read new PCB from cpu_fifo
*                       If read, try to allocate memory
*                           If success, add PCB to ready queue and print PCB details
*                           If fail, write failed PCB back to sender
*
*                   Increase time_waiting_in_ready by the ready_queue size
*
*                   PART II
*                   Process PCB currently in "running" state (running_pcb)
*                       Increment active_cpu_time
*                       Decrement remaining_burst and remaining_rr_time
*                       If process is completed (remaining_time = 0)
*                           Set end_time to cpu_clock
*                           Increase CPU statistics (total_wait_time and total_turnaround_time)
*                               based upon running_pcb's times, writeback to sender via pcb->fifoname
*                           Print running_pcb details
*                           Point running_pcb to NULL
*                           Increment completed_tasks
*                       If round-robin time quantum is completed (remaining_rr_time = 0)
*                           Enqueue current_pcb
*                           Point current_pcb to NULL
*
*                   PART III
*                   Check if there is still a PCB in the running state (running_pcb != NULL)
*                       If not:
*                           Dequeue the first PCB from the ready queue and point to it with 
*                               current_pcb
*                           Set remaining_rr_time to round_robin_max
*                   
*                   ** FINAL CLEANUP **
*                   Close and unlink "cpu_fifo"
*                   Calculate CPU Scheduler Statistics
*                   Print CPU Scheduler Statistics
*                   Free all allocated memory variables
*                   Exit
*
* ---------- PROJECT TOTALS ----------
* Time Commitment   Estimate    Actual
* Design             2 hr       2.5 hr (refactored) 
* Implementation     2.5 hr     3.5 hr (refactored)     
* Testing            2.5 hr     2.0 hr
*
***********************************************************************/ 
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "pcb_structs.h"
#include "mem_structs.h"

int round_robin_max = 4; // Sets the maximum round robin time
const int total_clocks = 50; // THIS VARIABLE DETERMINES TOTAL CLOCKS RUN

// Initialized CPU Statistic Variables
int cpu_clock = 0;
int active_cpu_time = 0;
int time_waiting_in_ready = 0;
int total_turnaround_time = 0;
int completed_tasks = 0;
int remaining_rr_time = 4;

// Declared CPU Scheduling Variables
PCB *running_pcb;
pcb_queue *rdy_q;
MemQueue *mem_q;
int fd_in;
    
// Forward Declared Functions
PCB* receiveNewPCB(int);
PCB* allocatePCBMemory(PCB*, MemQueue*);
void addPCBToQueue(pcb_queue *, PCB *);
PCB* processCurrentPCB(pcb_queue *, PCB *, MemQueue *);
PCB* updateCurrentPCBfromReadyQueue(pcb_queue *, PCB *);    
void shutDownProcedures();

// START OF MAIN PROGRAM
/* Run using: 
*   ./[filename] (defaults to 1024, 64, 4)
*   ./[filename] total_memory pagefile_size (positive integers)
*   ./[filename] total_memory pagefile_size round_robin_quanta (positive integers)
*/
int main(int argc, char** argv)
{
    // Establish Signal Handler to terminate program upon Ctrl-C
    signal(SIGINT, shutDownProcedures);
    
    // Initialize Ready_Queue
    rdy_q = new_pcb_queue();

    // Initialize MemQueue
    int serverTotalMemory = 1024;
    int serverPageSize = 64;
    if(argc == 3)
    {
        serverTotalMemory = atoi(argv[1]);
        serverPageSize = atoi(argv[2]);
    }
    if(argc == 4)
    {
        serverTotalMemory = atoi(argv[1]);
        serverPageSize = atoi(argv[2]);
        round_robin_max = atoi(argv[3]);
    }
    mem_q = new_MemQueue(serverTotalMemory, serverPageSize); 
    
    // Print Initial Server Settings
    printf("\n----- Starting CPU Scheduler -----\n");
    printf("Server Duration: %d\n", total_clocks);
    printf("Total Memory: %d\n", serverTotalMemory);
    printf("Pagefile Size: %d\n", serverPageSize);
    printf("----------------------------------\n");

    // Make Fifo cpu_fifo
    if((mkfifo("cpu_fifo",0666)<0) && errno != EEXIST)
    {
        perror("Unable to create FIFO. Server will terminate.\n");
        exit(1);
    }

    // Open FIFO cpu_fifo in Read-Only Mode, Non-Blocking
    if((fd_in = open("cpu_fifo", O_RDONLY | O_NONBLOCK))<0)
    {
        perror("Unable to open FIFO. Server will terminate.\n");
        unlink("cpu_fifo\n");
        exit(1);
    }    

    // START OF MAIN SCHEDULING LOOP. RUNS FOR "total_clocks" seconds.
    for (cpu_clock = 0; cpu_clock <total_clocks; cpu_clock++)
    {
        // Print current value of cpu_clock
        printf("\n|-------- CPU Time: %d --------|\n", cpu_clock);

        // Sleep for 2 seconds (to allow pcb_clients to connect and write)
        sleep(1);

        // Increase total wait time variable
        time_waiting_in_ready += rdy_q->size;

        // Check fifo for new pcbs. If one exists, allocate memory and add to ready queue.
        PCB *temp_pcb;
        temp_pcb = receiveNewPCB(fd_in);
        temp_pcb = allocatePCBMemory(temp_pcb, mem_q); // Sends rejection to sender upon fail
        addPCBToQueue(rdy_q, temp_pcb);
        temp_pcb = NULL;
        
        // Do work on the PCB currently in "working" state.
        running_pcb = processCurrentPCB(rdy_q, running_pcb, mem_q);
        // If no pcbs in the working state, move one in from ready.
        running_pcb = updateCurrentPCBfromReadyQueue(rdy_q, running_pcb);                
    
    }   // End of Service For Loop

    // Shutdown on completion.
    shutDownProcedures(rdy_q, fd_in, running_pcb, mem_q);
   
    // Exit
    return 0;
}


/* Allocates memory for a new PCB, and fills it from the cpu_fifo. If fifo read is
*  successful, returns a pointer. Otherwise, free's allocated memory and returns NULL.
*/
PCB* receiveNewPCB(int fd_cpu_in)
{
    // Allocate temp_pcb
    PCB *this_pcb = (PCB*)malloc(sizeof(PCB));
    
    /* Tries to read from fd_in. If empty, continue. If bytesRead = sizeof(PCB) then
    *  continue. If read is partially completed, loop until finished, then continue.
    */
    int bytesRead = 0;
    do {
        bytesRead = read(fd_cpu_in, this_pcb, sizeof(PCB));
    } while (bytesRead >0 && bytesRead<sizeof(PCB));
    if (bytesRead == sizeof(PCB)) // If PCB is read
    {
        printf("Received: PCB #%d.\n", this_pcb->pcbnumber);
        return this_pcb;
    }
    else    // If NO PCB is read
    {
        free(this_pcb);
        return NULL;
    }
}

/* Receives a PCB* and MemQueue*. Returns Null if PCB* is null. Otherwise sets 
*  PCB start time, allocates a block of memory for the PCB, according to its 
*  memoryNeeded variable. If memory allocation is unsuccessful it sets the end time 
*  to -1 and returns the PCB to the sender. If all is successful, returns the PCB.
*/
PCB* allocatePCBMemory(PCB* this_pcb, MemQueue* mem)
{
    // NULL check for this_pcb
    if (this_pcb == NULL) 
    {
        return this_pcb;
    }

    setStart(this_pcb, cpu_clock);
    int blocksNeeded = (int)ceil((double)this_pcb->memoryNeeded / (double)mem->PageFile_size);
    if (blocksNeeded > mem->size) // If memory allocation unsuccessful
    {
        printf("Insufficient memory to run PCB#%d. Process will be terminated.\n", this_pcb->pcbnumber);
        int fd_out = open(this_pcb->fifoname, O_WRONLY);
        this_pcb->endTime = -1;
        write(fd_out, this_pcb, sizeof(PCB));
        this_pcb = NULL;
    }
    else    // If memory write allocation successful
    {    
        this_pcb->pcb_memory_block = requestBlockOfMemory(mem, this_pcb->memoryNeeded);
        this_pcb->pcb_memory_block->page_size = mem->PageFile_size;
        printNewlyAllocatedPCB(this_pcb);
    }
    return this_pcb;
}

// Receives pointers for a pcb_queue and PCB. If the PCB is not null it enqueues it. 
void addPCBToQueue(pcb_queue* Q, PCB *this_pcb)
{
    if (this_pcb != NULL)
    {
        enqueue(Q, *this_pcb);
    }
}

/*  Handles operations for the PCB that is in the RUNNING state. If there is one
*   the round_robin and burst times are decremented. If the process is completed
*   it is sent back to its originator. If the round_robin time quantum is out the
*   process is returned to the ready_queue. This process also updates the CPU
*   statistic variables.
*/
PCB* processCurrentPCB(pcb_queue* Q, PCB* in_pcb, MemQueue* mem)
{
    PCB* this_pcb = in_pcb;
    if (this_pcb != NULL) {
        
        // Increment active_cpu_time
        ++active_cpu_time;

        // Decrement Remaining Round Robin Time
        --remaining_rr_time;
        printf("Remaining RR time: %d\n", remaining_rr_time);
        
        // Decrement burst time from currentPCB
        decrementPCB(this_pcb);
        printf("Remaining Task Burst: %d\n", this_pcb->remainingBurst);

        // Check if current PCB has finished this clock cycle
        if(this_pcb->remainingBurst == 0)
        {
            // Set CurrentPCB endTime to cpu_clock
            setEnd(this_pcb, cpu_clock);
            
            // Print details of current pcb
            printCompletedPCB(this_pcb);

            // Return block of memory
            returnBlockOfMemory(mem,this_pcb->pcb_memory_block);

            // Increase total_turnaround_time by the turnaround time of running_pcb
            total_turnaround_time += (this_pcb->endTime - this_pcb->startTime);

            // Open FIFO to client in write-only mode
            int fd_to_client;
            if((fd_to_client = open(this_pcb->fifoname, O_WRONLY))<0)
            {
                printf("Unable to open FIFO to return PCB data.\n");
            }
            else
            {
                // Write PCB back to client via FIFO
                write(fd_to_client, this_pcb, sizeof(PCB));
            }
                
            // Increment Completed Tasks
            completed_tasks++;

            // Set running_pcb to null
            free(this_pcb);
            this_pcb = NULL;


            remaining_rr_time = round_robin_max;
        } 
        // If Round Robin Time has ended. Push PCB back to queue and clear running_pcb
        else if (remaining_rr_time == 0)
        {
            printf("Returning PCB #%d to Queue\n", this_pcb->pcbnumber);
            enqueue(Q, *this_pcb);
            this_pcb = NULL;
        }
        
    }
    return this_pcb;
}

PCB* updateCurrentPCBfromReadyQueue(pcb_queue* Q, PCB* in_pcb)
{
    PCB* this_pcb = in_pcb;
    // *** Update running_pcb (if empty or completed) from front of Ready Queue ***
    if (this_pcb == NULL)
    {
        // If queue is not empty
        if(Q->size >0)
        {
            // Set running_pcb to the first item in the queue and print start 
            this_pcb = dequeue(Q);
            remaining_rr_time = round_robin_max;
            printf("PCB #%d Started\n", this_pcb->pcbnumber);
            return this_pcb;
        }
        else
        {
            return NULL;
        }   
    }
    else
    {
        return this_pcb;
    }
}

/* Prints out final statistics for utilization, average turnaround time, and average
*  time spent in the waiting queue.
*/
void printFinalServerStatistics()
{
    // Declare Server Statistics variables
    double CPU_utilization = 0.0;
    double averageTurnaround = 0.0;
    double averageWaitTime = 0.0;
    // Calculate server statistics
    if (cpu_clock>0) 
    {
        CPU_utilization = ((double)active_cpu_time / (double)cpu_clock);
    }
    if (completed_tasks >0)
    {
        averageTurnaround = ((double)total_turnaround_time / (double)completed_tasks);
        averageWaitTime = ((double)time_waiting_in_ready / (double)completed_tasks);
    }
    // Printout server statistics
    printf("\n-------------------------\n");
    printf("CPU Scheduling Statistics\n");
    printf("CPU Utilization: %f\n",CPU_utilization);
    printf("Average Turnaround: %f\n", averageTurnaround);
    printf("Average Wait Time: %f\n", averageWaitTime);
    printf("-------------------------\n");
}

/* Performs all maintenance before program shutdown: 
*  *  Calls for server statistics to be printed
*  *  Closes and unlinks all fifos
*  *  Deallocates all variables from memory
*/
void shutDownProcedures() {
    
    // ***** AFTER SERVICE LOOP *****
    printFinalServerStatistics();

    // Push any running PCB back onto queue before queue is cleared out
    if (running_pcb != NULL)
    {
        enqueue(rdy_q, *running_pcb);
    }
    // Sets the end time for each PCB to 0 and returns them. EndTime 0 is read
    // by the client as an error due to server shutdown.
    while(rdy_q->size != 0)
    {
        int fd_out;
        running_pcb = dequeue(rdy_q);
        if((fd_out = open(running_pcb->fifoname, O_WRONLY))<0)
        {
            printf("Unable to writeback PCB#%d\n", running_pcb->pcbnumber);
        }
        else
        {
            write(fd_out, running_pcb, sizeof(PCB));
        }
        
    }

    // Close and unlink inbound fifo
    close(fd_in);
    unlink("cpu_fifo");

    // Free all allocated variables
    if (running_pcb != NULL)
    {
        free(running_pcb);
    }
    if (rdy_q != NULL)
    {
        free(rdy_q);
    }
    if(mem_q != NULL)
    {
        free(mem_q);
    }
    printf("Process terminated.\n");
    exit(0);
}
