/**************************    mem_structs.h    ***************************
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
* Purpose:          Header file which contains data types for PageFile, MemNode,
*                   MemBlock, and MemQueue and methods associated with each.
*
***********************************************************************/

#include <stdlib.h>
#include <stdio.h>

#ifndef MEM_STRUCTS
#define MEM_STRUCTS

typedef struct page PageFile;
typedef struct m_node MemNode;
typedef struct block MemBlock;
typedef struct m_queue MemQueue;
PageFile dequeuePageFile(MemQueue*);
void enqueueNode(MemQueue *, int);
int roundUpPower2(int);

struct page 
{
    int memory_start_address;
}; 

struct m_node
{
    PageFile memory;
    MemNode *next;
};

struct block
{
    int num_pages;
    int page_size;
    PageFile** memory_block;
};

struct m_queue
{
    MemNode *first;
    MemNode *last;
    int PageFile_size;
    int size;
};

MemNode* new_MemNode(int start_address)
{
    MemNode *N = (MemNode *)malloc(sizeof(MemNode));
    PageFile *S = (PageFile *)malloc(sizeof(PageFile));
    S->memory_start_address = start_address;
    N->memory = *S;
    return N;
}


/* Create a new MemQueue* Q with PageFile sized MemNodes which
*  account for all memory
*/
MemQueue* new_MemQueue(int total_size, int page_size)
{
    MemQueue *Q;
    int memory_address = 0; // Memory address for first MemNode
    if ( total_size <2 || page_size < 1 || total_size < page_size)
    {
        printf("Invalid values for Memory Size and Page File.\n");
        exit(1);
    }
    else
    {
        total_size = roundUpPower2(total_size);
        page_size = roundUpPower2(page_size);
    }

    // Allocate space for new MemQueue and set initial values
    Q = (MemQueue *)malloc(sizeof(MemQueue));
    Q->size = 0;
    Q->PageFile_size = page_size;
    Q->first = NULL;
    Q->last = NULL;

    // Create MemNodes for remaining available memory space
    while ((memory_address) < total_size)
    {
        enqueueNode(Q, memory_address);
        memory_address += page_size;
    }
    
    return Q;
}

// Returns 1 if Empty, Else 0
int isEmpty_Mem(MemQueue* Q)
{
    return (Q->size == 0);
}

/* Create a new MemNode N with the parameter address
*  add the new MemNode N to the end of MemQueue Q  
*/
void enqueueNode (MemQueue* Q, int address)
{
    MemNode *N = new_MemNode(address);
    N->next = NULL;
    if(Q->last)
    {
        Q->last->next = N;
    }
    Q->last = N;
    if(isEmpty_Mem(Q)){
        Q->first = N;
    }
    Q->size++;
}

MemBlock* requestBlockOfMemory(MemQueue* Q, int memory_requested)
{
    MemBlock* mb;
    mb = (MemBlock *)malloc(sizeof(MemBlock));
    int blocksRequired = (int)ceil(((double)memory_requested/ (double)Q->PageFile_size));

    if(Q->size >= blocksRequired)
    {
        mb->memory_block = (PageFile **)malloc(blocksRequired * sizeof(PageFile *));
        int i;
        for(i=0;i<blocksRequired;i++)
        {
            mb->memory_block[i] = (PageFile *)malloc(sizeof(PageFile));
            *mb->memory_block[i] = dequeuePageFile(Q);
        }
        mb->num_pages = blocksRequired;
    }
    return mb;
}



/* Returns the first MemNode in MemQueue Q. */
PageFile dequeuePageFile(MemQueue* Q)
{
    MemNode *N = Q->first;
    if(Q->first == Q->last)
    {
        Q->last=NULL;
    }
    Q->first = N->next;
    Q->size--;
    PageFile ms;
    ms = N->memory;
    free(N);
    return ms;
}

PageFile removeNode (MemNode* removedNode, MemNode* previousNode)
{
    previousNode->next = removedNode->next;
    PageFile memory = removedNode->memory;
    
}

void returnBlockOfMemory(MemQueue* Q, MemBlock* mb)
{
    int i;
    for(i=0;i<mb->num_pages;i++)
    {
        enqueueNode(Q, mb->memory_block[i]->memory_start_address);
    }
    free(mb);
}

// Round up number to the nearest power of 2. Return -1 on fail.
int roundUpPower2(int number)
{
    if (number <1)
    {
        return -1;
    }

    int temp=1;
    while (temp < number)
    {
        temp <<= 1;
    }
    return temp;
}

void printMemoryBlock(MemBlock *mb)
{
    int i;
    for(i=0;i<mb->num_pages;i++)
    {
        printf("Block #%d <= Pagefile #%d\n", i, mb->memory_block[i]->memory_start_address);
    }
}


#endif
