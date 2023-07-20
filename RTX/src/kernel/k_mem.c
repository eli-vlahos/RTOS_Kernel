/*
 ****************************************************************************
 *
 *                  UNIVERSITY OF WATERLOO ECE 350 RTOS LAB
 *
 *                     Copyright 2020-2021 Yiqing Huang
 *                          All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice and the following disclaimer.
 *
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS AND CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 ****************************************************************************
 */

/**************************************************************************/ /**
* @file        k_mem.c
* @brief       Kernel Memory Management API C Code
*
* @version     V1.2021.01.lab2
* @authors     Yiqing Huang
* @date        2021 JAN
*
* @note        skeleton code
*
*****************************************************************************/

/**
 * @brief:  k_mem.c kernel API implementations, this is only a skeleton.
 * @author: Yiqing Huang
 */

#include "k_mem.h"
#include "k_task.h"
#include "Serial.h"
#include "common_ext.h"
#ifdef DEBUG_0
#include "printf.h"
#endif /* DEBUG_0 */

/*
 *==========================================================================
 *                            GLOBAL VARIABLES
 *==========================================================================
 */
// kernel stack size, referred by startup_a9.s
const U32 g_k_stack_size = K_STACK_SIZE;
// task proc space stack size in bytes, referred by system_a9.c
const U32 g_p_stack_size = U_STACK_SIZE;

// task kernel stacks
U32 g_k_stacks[MAX_TASKS][K_STACK_SIZE >> 2] __attribute__((aligned(8)));

// process stack for tasks in SYS mode
U32 g_p_stacks[MAX_TASKS][U_STACK_SIZE >> 2] __attribute__((aligned(8)));

// Head of linked list of free memory segments
static header *head = NULL;

// int (treated as bool) that keeps track of if we're allocating memory that is owned by the genearl OS
// ex: when we call k_alloc_p_stack in k_mem.c
int mem_owned_by_os = 0;
/*
 *===========================================================================
 *                            FUNCTIONS
 *===========================================================================
 */

U32 *k_alloc_k_stack(task_t tid)
{
    return g_k_stacks[tid + 1];
}

U32 *get_hi_addr(TCB *tcb, header *header_pointer) {
	int head_padding = calc_padding((U32)header_pointer + sizeof(header));
	int end_padding = calc_padding((U32)header_pointer + sizeof(header) + head_padding + header_pointer->size);
	U32 *hi_addr = (U32 *)((U32)header_pointer + sizeof(header) + head_padding + header_pointer->size + end_padding);
	tcb->u_stack_size = header_pointer->size + end_padding;
	return hi_addr;
}

U32 *k_alloc_p_stack(TCB *p_tcb, task_t tid, size_t size)
{
	void *pointer = k_mem_alloc_os(size);
	p_tcb->u_stack_lo = (U32)pointer;
	header *header_pointer = ((header *)pointer) - 1;
	U32 padding = calc_reverse_padding((U32)header_pointer);
	header_pointer = (header*)((U32)header_pointer - padding);
	U32 *hi_addr = (U32*)((U32)pointer + header_pointer->size);
	hi_addr = (U32*)((U32)hi_addr + calc_padding((U32)hi_addr));
	p_tcb->u_stack_hi = (U32)hi_addr;
	return hi_addr;
}

int k_mem_init(void)
{
    unsigned int end_addr = (unsigned int)&Image$$ZI_DATA$$ZI$$Limit;
#ifdef DEBUG_0
    printf("k_mem_init: image ends at 0x%x\r\n", end_addr);
    printf("k_mem_init: RAM ends at 0x%x\r\n", RAM_END);
#endif /* DEBUG_0 */
    if (end_addr >= RAM_END)
    {
        return RTX_ERR;
    }
    header *starting_header = create_header((header *)end_addr, (header *)(U32)RAM_END, RAM_END - (end_addr + sizeof(header)), FREE); /* Starting metadata block */

    head = starting_header; /* Header metadata block that ALWAYS points to first metadata chunk that is free */
    return RTX_OK;
}

void* k_mem_alloc_os(size_t size) {
	mem_owned_by_os = 1;
	void* temp = k_mem_alloc(size);
	mem_owned_by_os = 0;
	return temp;
}

void* k_mem_alloc(size_t size) {
	#ifdef DEBUG_0
	printf("k_mem_alloc: requested memory size = %d\r\n", size);
	#endif /* DEBUG_0 */
    
    /* Check to make sure that k_mem_init has been called */
    if (head == NULL) {
        return NULL;
    }

	/* add exception for being the first block allocated */

	/* check to make sure size is not zero */
	if (size == 0 ) {
		return NULL;
	}

	header *seg_start = (header *)((U32)head); /* Go to start of header */
	header *seg_last = NULL;

	int current_seg_size = seg_start->size;
	int head_round = calc_padding( (U32)seg_start + sizeof(header) );
	int right_padding = calc_padding( (U32)seg_start + sizeof(header) + size + head_round);

	task_t temp_tid = k_tsk_get_tid();
	if (mem_owned_by_os) {
		temp_tid = 0;
		#ifdef DEBUG_0
		printf("k_mem_alloc: Allocating memory for OS and not task!!!\r\n", size);
		#endif /* DEBUG_0 */
	}

	/* run loop to get seg large enough, break if end or sufficient segment is reached */
	while ((current_seg_size < ( (U32)size + head_round + right_padding)) && (U32)seg_start != (U32)RAM_END){

		seg_last = seg_start;
		seg_start = seg_start->next;

		/* use temporary variable to prevent error when reaching RAM_END */
		if ((U32)seg_start != (U32)RAM_END && (U32)seg_start != NULL){
			current_seg_size = seg_start->size;
			head_round = calc_padding( (U32)seg_start + sizeof(header) );
			right_padding = calc_padding( (U32)seg_start + sizeof(header) + size + head_round);
		}

	}


	/* if at end return failure (NULL) */
	if ((U32)seg_start == (U32)RAM_END){
		return NULL; /* not enough memory */
	}

	if (seg_start->size > (size + sizeof(header) + head_round)) {
		/* if the size of the memory */
		/* change offset when you change size */
		U32 offset = size + sizeof(header) + head_round + right_padding;

		header* seg_new = (header *)((U32)seg_start + (U32)offset); /* creating new seg */

		/* initialize new segment to replace seg start */
		seg_new->next = seg_start->next;
		seg_new->size = seg_start->size - size - head_round - right_padding - sizeof(header);
		seg_new->is_allocated = 0;
		seg_new->tid = 0;

		/* initialize seg start to be allocated */
		seg_start->next = NULL;
		seg_start->size = size + head_round + right_padding;
		seg_start->is_allocated = ALLOCATED;
		seg_start->tid = temp_tid;
		#ifdef DEBUG_0
			printf("k_mem_alloc: current task running %u\r\n", temp_tid);
		#endif /* DEBUG_0 */
		

		/* connect two parts */
		if (head == seg_start){
			head = seg_new;
		}
		else {
			seg_last->next = seg_new;
		}
	}
	else{
		seg_start->size += seg_start->size - (size + head_round + right_padding);

		/*  if at start with more than one block, replace the header. Otherwise connect list */
		if (head == seg_start){

			head = seg_start->next;

		}
		else {
			seg_last->next = seg_start->next;
		}


		seg_start->next = NULL;
		seg_start->is_allocated = ALLOCATED;
		seg_start->tid = temp_tid;
		#ifdef DEBUG_0
			printf("k_mem_alloc: current task running %u\r\n", temp_tid);
		#endif /* DEBUG_0 */

	}

	void* temp = NULL;
	temp = (void*)((U32)seg_start + (U32)head_round + (U32)sizeof(header));

	return temp;

}

int k_mem_dealloc(void *ptr) {
#ifdef DEBUG_0
    printf("k_mem_dealloc: freeing 0x%x\r\n", (U32)ptr);
#endif /* DEBUG_0 */
    header *seg_start = (header *)((U32)ptr - sizeof(header)); /* Go to start of header */
    seg_start = (header *)((U32)seg_start - calc_padding((U32)seg_start));
    if ((header *)ptr == NULL)
	{
		/* Do nothing */
		return RTX_ERR;
	}
	task_t temp_tid = k_tsk_get_tid();
    if (temp_tid != seg_start->tid && seg_start->tid != 0)
	{
		/*current task isn't task freeing memory*/
		#ifdef DEBUG_0
			printf("k_mem_dealloc: current task running %d trying to delete memory owned by %u\r\n", temp_tid, seg_start->tid);
		#endif /* DEBUG_0 */
		return RTX_ERR;
	}
	#ifdef DEBUG_0
	if (temp_tid == 0) {
		printf("k_mem_dealloc: freeing memory owned by null task/OS\r\n");
	}
	#endif /* DEBUG_0 */
    if (seg_start->is_allocated == FREE)
    {
    	/* Cannot free unallocated memory, fail */
    	return RTX_ERR;
    }
    /* ptr is allocated memory */
    if ((U32)seg_start < (U32)head)
    {
    	/* Reassign head to lower address */
    	seg_start->next = head;
    	head = seg_start;
    }
    else
    {
    	/* Insert ptr into list of free memory */
    	header *temp_prev = find_prev_free_mem(head, seg_start);
    	seg_start->next = temp_prev->next;
    	temp_prev->next = seg_start;
    }
    seg_start->is_allocated = FREE;
    return coalesce(head, head->next);
}


int k_mem_count_extfrag(size_t size)
{
#ifdef DEBUG_0
    printf("k_mem_extfrag: size = %d\r\n", size);
#endif /* DEBUG_0 */
    header *temp = head;
    int counter = 0;
    while (temp != (header *)(U32)RAM_END)
    {
        if ((temp->size + sizeof(header) < size) && (temp->is_allocated == FREE))
        {
            counter++;
        }
        temp = temp->next;
    }
    return counter;
}

void *create_header(header* address, header* next, U32 size, U8 is_allocated)
{
    header* header_node = address;
    header_node->next = next;
    header_node->is_allocated = is_allocated;
    header_node->size = size;
    return header_node;
}
header *find_prev_free_mem( header *head, header *a)
{
	if (a == head)
	{
		return a;
	}

	header *temp_prev = NULL;
	header *temp_next = head;

	/* Find previous address */
	while ((U32)temp_next < (U32)a)
	{
		temp_prev = temp_next;
		temp_next = temp_next->next;
	}

	return temp_prev;
}
int coalesce(header *a, header *b)
{
	if ((U32)b == RAM_END)
	{
		/* Done */
		return RTX_OK;
	}
	if ( a->is_allocated == ALLOCATED || b->is_allocated == ALLOCATED)
	{
		/* Cannot coalesce allocated memory */
		return RTX_ERR;
	}
	/* a and b are unallocated memory */
	if (((U32)a + sizeof(header) + a->size) == (U32)b)
	{
		/* a and b are next to each other, combine */
		a->size = a->size + sizeof(header) + b->size; /* Reclaim segment */
		a->next = b->next;
		b->next = NULL;
		return coalesce(a, a->next); /* Continue to next free segment */
	}
	return coalesce(b, b->next); /* Not beside each other, go to next free segment */
}
void print_header_seg(header *a)
{
#ifdef DEBUG_0
    printf("\nprint_header_seg: header starts at 0x%x\r\n", (U32)a);
    printf("print_header_seg: header next is 0x%x\r\n", (U32)a->next);
    printf("print_header_seg: header size is %u\r\n", (U32)a->size);
    printf("print_header_seg: header allocated status is %u\r\n", (U8)a->is_allocated);
#endif  /* DEBUG_0 */
}
void print_header_list(header *a)
{
#ifdef DEBUG_0
	header *temp = a;

	if (temp->next == NULL)
	{
		printf("\nprint_header_seg: next is NULL\r\n");
		return;
	}

    while ((U32)temp->next != RAM_END)
    {
    	print_header_seg(temp);
    	temp = temp->next;
    }
    print_header_seg(temp);
    printf("\nprint_header_seg: END OF LIST\r\n");

#endif  /* DEBUG_0 */
}
int calc_reverse_padding(U32 address) {
	if (address % 8 == 0)
	{
		return 0;
	}
	return address % 8;
}
int calc_padding(U32 address){
	if (address % 8 == 0)
	{
		return 0;
	}
#ifdef DEBUG_0
	printf("calc_padding: padding of %d added\r\n", (8 -address % 8));
#endif

	return (8 -address % 8);
}
header *get_head() {
    return head;
}

/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
