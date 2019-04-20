/**************************    pcb_client.c    ***************************
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
* Purpose:          To simulate the creation of a Process Control Block and its
*                   submission to a CPU_Process_Scheduler. This program creates
*                   a PCB struct based upon input parameters and writes the PCB
*                   to the CPU_scheduler via FIFO. Once the CPU_scheduler has
*                   completed the operation, it will send the completed PCB back
*                   to this process via a return fifo. Finally, this program will
*                   print the PCB statistics before closing itself.
*
* Input:            PCB burst time and memory requirements, passed from commmand line
*
* Preconditions:    CPU_Scheduler must be running.
*
* Output:           Prints the details of the PCB when it is completed.
*
* Postconditions:   Must close inbound fifo and unlink it.
*
* Algorithm:        Capture burst time from command line (argv[1])
*                   Capture memory requirement from command line (argv[2])
*                   Create new PCB struct called this_pcb
*                   Get fifoname string using pid ("FIFO_#pid#")
*                   Set this_pcb.fifoname to created name
*                   Set this_pcb.totalBurst and this_pcb.remainingBurst to
*                       input parameter amount
*                   Set this_pcb.memoryNeeded to input parameter amount
*                   Create fifo named this_pcb.fifoname
*                   Open cpu_fifo in write-only mode
*                   Write this_pcb to fifo
*                   
*                   Open fifo this_pcb.fifoname
*                   read (loop) from fifo to this_pcb
*                   print out pcb name
*                   print out pcb turnaround time (end - start)
*                   Print pcb time waiting (end - start - burst)
*                   close fifo this_pcb.fifoname
*                   Unlink FIFO this_pcb.fifoname
*                   exit
*
* ---------- PROJECT TOTALS ----------
* Time Commitment   Estimate    Actual
* Design             3 hr        
* Implementation     2.0 hr         
* Testing            1.5 hr        
*
***********************************************************************/ 

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include "pcb_structs.h"

/* Run using: 
*   ./[filename]
*   ./[filename] pcb_burst_time pcb_memory_needed (positive integers)
*/
int main(int argc, char **argv)
{
    // Check for invalid arguments.
    if (argc != 3)
    {
        printf("Command must contain 2 arguments: total burst time and total memory allocation required.\n");
        printf("PCB Request Terminating.\n");
        exit(1);
    }

    // Capture burst time from command line (argv)
    int burst = atoi(argv[1]);
    int memoryRequired = atoi(argv[2]);
    
    
    // Create new PCB struct called this_pcb
    PCB *this_pcb = (PCB*)malloc(sizeof(PCB));
    this_pcb->totalBurst = burst;
    this_pcb->remainingBurst = burst;
    this_pcb->memoryNeeded = memoryRequired;
    
    // Get fifoname string using pid ("FIFO_#pid#")
    this_pcb->pcbnumber = getpid();
    
    // Set this_pcb.fifoname to created name
    sprintf(this_pcb->fifoname, "fifo_%d", this_pcb->pcbnumber);
    
    // Create fifo named this_pcb.fifoname
    if((mkfifo(this_pcb->fifoname, 0666)<0 && errno != EEXIST))
    {
        perror("Unable to create return FIFO from CPU. Program will terminate.\n");
        exit(1);
    }
    // Open cpu_fifo in write-only mode
    int fd_to_cpu;
    if((fd_to_cpu = open("cpu_fifo", O_WRONLY))<0)
    {
        perror("Unable to open FIFO to CPU. Process will terminate.\n");
        unlink(this_pcb->fifoname);
        exit(1);
    }
    printf("\n-------------------------\n");
    printf("Sending PCB #%d\n", this_pcb->pcbnumber);
    printf("Total PCB Burst: %d\n", this_pcb->totalBurst);
    printf("Requesting %dB memory\n", this_pcb->memoryNeeded);
    printf("-------------------------\n");

    // Write this_pcb to fifo
    write(fd_to_cpu,this_pcb, sizeof(PCB));
    
    // Open fifo this_pcb.fifoname
    int fd_from_cpu;
    if((fd_from_cpu = open(this_pcb->fifoname, O_RDONLY))<0)
    {
        perror("Unable to open FIFO from CPU. Process will terminate.");
        close(fd_to_cpu);
        unlink(this_pcb->fifoname);
        exit(1);
    }

    // Close fifo to CPU.
    close(fd_to_cpu);

    // read from fifo to this_pcb
    sleep(1); // Wait one second to read. Should avoid race with server.
    int dataReceived = read(fd_from_cpu, this_pcb, sizeof(PCB));
    if(dataReceived < sizeof(PCB))
    {
        printf("Error in reading PCB");
    }

    // print out pcb statistics (see pcb_structs for more detail)
    if(this_pcb->endTime == -1)
    {
        printf("Insufficient Memory: Process terminated.\n");
    }
    else if(this_pcb->endTime == 0)
    {
        printf("Server Shutdown: Process Terminated.\n");
    }
    else
    {
        printCompletedPCB(this_pcb);
    }

    // Close fifo from cpu    
    close(fd_from_cpu);

    // Unlink this_pcb.fifoname FIFO
    unlink(this_pcb->fifoname);
    
    // Free this_pcb
    free(this_pcb);

    // Exit Successfully.
    return 0;
}

/* Print out pcb turnaround time (end - start)
*  Print pcb time waiting (end - start - burst)
*/                 
