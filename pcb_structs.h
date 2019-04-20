/**************************    pcb_structs.h    ***************************
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
* Purpose:          Header file which contains data types for PCB, PCB_Node,
*                   PCB_Queue and various methods associated with each.
*
***********************************************************************/


#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include "mem_structs.h"

#ifndef pcb_structs_h
#define pcb_structs_h

// PCB struct
typedef struct pcb
{
    pid_t pcbnumber;
    char fifoname[12];
    int totalBurst;
    int remainingBurst;
    int startTime;
    int endTime;
    MemBlock* pcb_memory_block;
    int memoryNeeded;

} PCB;

// Sets the startTime valule for the PCB p
void setStart (PCB *p, int start)
{
    p->startTime = start;
}

// Sets the endTime value for the PCB p
void setEnd (PCB *p, int end)
{
    p->endTime = end;
}

void setMemoryNeeded(PCB *p, int amount)
{
    p->memoryNeeded = amount;
}

// Decrement the remaining burst time for PCB p
void decrementPCB(PCB *p)
{
    p->remainingBurst--;
}

// pcb_node struct
typedef struct node
{
    PCB element;
    struct node *next;
} pcb_node;

// pcb_queue struct
typedef struct queue
{
    pcb_node *head;
    pcb_node *tail;
    int size;
} pcb_queue;

/** Null constructor for a new pcb_queue. Returns a pointer
 *  to the newly created pcb_queue. */
pcb_queue* new_pcb_queue()
{
    pcb_queue *temp = (pcb_queue*)malloc(sizeof(pcb_queue));
    temp->head = NULL;
    temp->tail = NULL;
    temp->size = 0;
    return temp;
}

/** Returns a 1 if pcb_queue q is empty, else 0. */
int isEmpty(pcb_queue *q)
{
    if (q->size == 0 ) return 1;
    
    return 0;
}

/** Creates a new pcb_node which stores the value of PCB p. 
 *  Pushes the new pcb_node to the end of pcb_queue q. */
void enqueue(pcb_queue *q, PCB p)
{
    pcb_node *temp = (pcb_node*)malloc(sizeof(pcb_node));
    temp->element = p;
    temp->next = NULL;

    if (isEmpty(q))
    {
        q->head = temp;
    }
    else
    {
        q->tail->next = temp;
        
    }
    q->tail = temp;
    ++q->size;
    
}

/** Pops the first pcb_node from the pcb_queue q. Creates a PCB
 *  with the values from the popped node and returns a pointer
 *  to the newly created PCB. */
PCB* dequeue(pcb_queue *q)
{
    PCB *temp = (PCB*)malloc(sizeof(PCB));
    if (isEmpty(q))
    {
        perror("QUEUE IS EMPTY");
        return temp;
    }

    pcb_node *oldhead = q->head;
    *temp = q->head->element;
    q->head = oldhead->next;
    free(oldhead);
    --q->size;
    return temp;
    
}

/** Prints out the following details for a completed PCB: 
 *  *  PCB Arrival Time
 *  *  PCB End Time
 *  *  PCB Burst Time */
void printNewlyAllocatedPCB(PCB *p)
{
    printf("\n--------------------------\n");
    printf("PCB #%d => Ready Queue\n", p->pcbnumber);
    printf("PCB Arrived at time: %d\n", p->startTime);
    printf("PCB Burst Time: %d\n", (p->totalBurst));
    printMemoryBlock(p->pcb_memory_block);
    MemBlock* mb = p->pcb_memory_block;
    int fragmentation = (mb->page_size * mb->num_pages) - p->memoryNeeded;
    printf("Fragmented Memory: %d bytes\n", fragmentation);
    printf("--------------------------\n\n");
}

/** Prints out the following details for a completed PCB: 
 *  *  PCB Arrival Time
 *  *  PCB End Time
 *  *  PCB Burst Time */
void printCompletedPCB(PCB *p)
{
    printf("\n--------------------------\n");
    printf("PCB #%d Complete\n", p->pcbnumber);
    printf("PCB Arrived at time: %d\n", p->startTime);
    printf("PCB Ended at time: %d\n", p->endTime);
    printf("PCB Burst Time: %d\n", (p->totalBurst));
    printf("PCB Memory Returned.\n");
    printf("--------------------------\n\n");
}

#endif
