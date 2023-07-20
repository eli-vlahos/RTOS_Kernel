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

/**************************************************************************//**
 * @file        k_task.c
 * @brief       task management C file
 *              l2
 * @version     V1.2021.01
 * @authors     Yiqing Huang
 * @date        2021 JAN
 *
 * @attention   assumes NO HARDWARE INTERRUPTS
 * @details     The starter code shows one way of implementing context switching.
 *              The code only has minimal sanity check.
 *              There is no stack overflow check.
 *              The implementation assumes only two simple privileged task and
 *              NO HARDWARE INTERRUPTS.
 *              The purpose is to show how context switch could be done
 *              under stated assumptions.
 *              These assumptions are not true in the required RTX Project!!!
 *              Understand the assumptions and the limitations of the code before
 *              using the code piece in your own project!!!
 *
 *****************************************************************************/

//#include "VE_A9_MP.h"
#include "Serial.h"
#include "k_task.h"
#include "k_rtx.h"

#ifdef DEBUG_0
#include "printf.h"
#endif /* DEBUG_0 */

/*
 *==========================================================================
 *                            GLOBAL VARIABLES
 *==========================================================================
 */

TCB             *gp_current_task = NULL;	// the current RUNNING task
TCB             g_tcbs[MAX_TASKS];			// an array of TCBs
RTX_TASK_INFO   g_null_task_info;			// The null task info
U32             g_num_active_tasks = 0;		// number of non-dormant tasks

TCB 			*task_q_head = NULL;		// Head for task queue

/*---------------------------------------------------------------------------
The memory map of the OS image may look like the following:

                       RAM_END+---------------------------+ High Address
                              |                           |
                              |                           |
                              |    Free memory space      |
                              |   (user space stacks      |
                              |         + heap            |
                              |                           |
                              |                           |
                              |                           |
 &Image$$ZI_DATA$$ZI$$Limit-->|---------------------------|-----+-----
                              |         ......            |     ^
                              |---------------------------|     |
                              |      U_STACK_SIZE         |     |
             g_p_stacks[15]-->|---------------------------|     |
                              |                           |     |
                              |  other kernel proc stacks |     |
                              |---------------------------|     |
                              |      U_STACK_SIZE         |  OS Image
              g_p_stacks[2]-->|---------------------------|     |
                              |      U_STACK_SIZE         |     |
              g_p_stacks[1]-->|---------------------------|     |
                              |      U_STACK_SIZE         |     |
              g_p_stacks[0]-->|---------------------------|     |
                              |   other  global vars      |     |
                              |                           |  OS Image
                              |---------------------------|     |
                              |      K_STACK_SIZE         |     |                
             g_k_stacks[15]-->|---------------------------|     |
                              |                           |     |
                              |     other kernel stacks   |     |                              
                              |---------------------------|     |
                              |      K_STACK_SIZE         |  OS Image
              g_k_stacks[2]-->|---------------------------|     |
                              |      K_STACK_SIZE         |     |                      
              g_k_stacks[1]-->|---------------------------|     |
                              |      K_STACK_SIZE         |     |
              g_k_stacks[0]-->|---------------------------|     |
                              |   other  global vars      |     |
                              |---------------------------|     |
                              |        TCBs               |  OS Image
                      g_tcbs->|---------------------------|     |
                              |        global vars        |     |
                              |---------------------------|     |
                              |                           |     |          
                              |                           |     |
                              |                           |     |
                              |                           |     V
                     RAM_START+---------------------------+ Low Address
    
---------------------------------------------------------------------------*/ 

/*
 *===========================================================================
 *                            FUNCTIONS
 *===========================================================================
 */

/**************************************************************************//**
 * @brief   scheduler, pick the TCB of the next to run task
 *
 * @return  TCB pointer of the next to run task
 * @post    gp_curret_task is updated
 *
 *****************************************************************************/

TCB *scheduler(void)
{
    TCB *temp_tcb = task_q_head;
    // Case 1: pop head
	if (temp_tcb->state == READY)
	{
		// make sure not to pop NULL task
		if (temp_tcb->next != NULL)
		{
			l_pop(temp_tcb);
		}
		return temp_tcb;
	}

	// Search for next non-dormant task
	while (temp_tcb->next != NULL)
	{
		// Case 2: pop from middle
		if (temp_tcb->state == READY)
		{
			l_pop(temp_tcb);
			return temp_tcb;
		}
		temp_tcb = temp_tcb->next;
	}

	// Case 3: return NULL task
	return temp_tcb;
}



/**************************************************************************//**
 * @brief       initialize all boot-time tasks in the system,
 *
 *
 * @return      RTX_OK on success; RTX_ERR on failure
 * @param       task_info   boot-time task information structure pointer
 * @param       num_tasks   boot-time number of tasks
 * @pre         memory has been properly initialized
 * @post        none
 *
 * @see         k_tsk_create_new
 *****************************************************************************/

int k_tsk_init(RTX_TASK_INFO *task_info, int num_tasks)
{
    extern U32 SVC_RESTORE;

    RTX_TASK_INFO *p_taskinfo = &g_null_task_info;
    g_num_active_tasks = 0;

    if (num_tasks > MAX_TASKS) {
    	return RTX_ERR;
    }

    // create the first task
    TCB *p_tcb = &g_tcbs[0];
    p_tcb->prio     = PRIO_NULL;
    p_tcb->priv     = 1;
    p_tcb->tid      = TID_NULL;
    p_tcb->state    = RUNNING;
    p_tcb->next 	= NULL;
    p_tcb->task_entry = task_null;
    g_num_active_tasks++;
    gp_current_task = p_tcb;
    task_q_head = p_tcb;


    // create the rest of the tasks
    p_taskinfo = task_info;
    for ( int i = 0; i < num_tasks; i++ ) {
        TCB *p_tcb = &g_tcbs[i+1];
        p_tcb->u_stack_lo = NULL;
        p_taskinfo->u_stack_hi = NULL;
        p_taskinfo->u_stack_size = 0;
        p_taskinfo->state = READY;
        if (k_tsk_create_new(p_taskinfo, p_tcb, i+1) == RTX_OK) {
        	g_num_active_tasks++;
        }
        p_taskinfo++;
    }
    for (int i = num_tasks; i < MAX_TASKS; i++) {
    	TCB *p_tcb = &g_tcbs[i+1];
    	p_tcb->state = DORMANT;
    }
    return RTX_OK;
}
/**************************************************************************//**
 * @brief       initialize a new task in the system,
 *              one dummy kernel stack frame, one dummy user stack frame
 *
 * @return      RTX_OK on success; RTX_ERR on failure
 * @param       p_taskinfo  task information structure pointer
 * @param       p_tcb       the tcb the task is assigned to
 * @param       tid         the tid the task is assigned to
 *
 * @details     From bottom of the stack,
 *              we have user initial context (xPSR, PC, SP_USR, uR0-uR12)
 *              then we stack up the kernel initial context (kLR, kR0-kR12)
 *              The PC is the entry point of the user task
 *              The kLR is set to SVC_RESTORE
 *              30 registers in total
 *
 *****************************************************************************/
int k_tsk_create_new(RTX_TASK_INFO *p_taskinfo, TCB *p_tcb, task_t tid)
{
    extern U32 SVC_RESTORE;

    U32 *sp;

    if (p_taskinfo == NULL || p_tcb == NULL)
    {
        return RTX_ERR;
    }
	initialize_tcb(p_tcb, NULL, NULL, p_taskinfo->prio, p_taskinfo->priv, p_taskinfo->state, p_taskinfo->ptask, tid, p_taskinfo->u_stack_size, p_taskinfo->u_stack_hi);
	l_insert(p_tcb);
    /*---------------------------------------------------------------
     *  Step1: allocate kernel stack for the task
     *         stacks grows down, stack base is at the high address
     * -------------------------------------------------------------*/

    ///////sp = g_k_stacks[tid] + (K_STACK_SIZE >> 2) ;
    sp = k_alloc_k_stack(tid);

    // 8B stack alignment adjustment
    if ((U32)sp & 0x04) {   // if sp not 8B aligned, then it must be 4B aligned
        sp--;               // adjust it to 8B aligned
    }

    /*-------------------------------------------------------------------
     *  Step2: create task's user/sys mode initial context on the kernel stack.
     *         fabricate the stack so that the stack looks like that
     *         task executed and entered kernel from the SVC handler
     *         hence had the user/sys mode context saved on the kernel stack.
     *         This fabrication allows the task to return
     *         to SVC_Handler before its execution.
     *
     *         16 registers listed in push order
     *         <xPSR, PC, uSP, uR12, uR11, ...., uR0>
     * -------------------------------------------------------------*/

    // if kernel task runs under SVC mode, then no need to create user context stack frame for SVC handler entering
    // since we never enter from SVC handler in this case
    // uSP: initial user stack
    if ( p_taskinfo->priv == 0 ) { // unprivileged task
        // xPSR: Initial Processor State
        *(--sp) = INIT_CPSR_USER;
        // PC contains the entry point of the user/privileged task
        *(--sp) = (U32) (p_taskinfo->ptask);

        //********************************************************************//
        //*** allocate user stack from the user space, not implemented yet ***//
        //********************************************************************//
        *(--sp) = p_taskinfo->u_stack_hi;

        // uR12, uR11, ..., uR0
        for ( int j = 0; j < 13; j++ ) {
            *(--sp) = 0x0;
        }
    }


    /*---------------------------------------------------------------
     *  Step3: create task kernel initial context on kernel stack
     *
     *         14 registers listed in push order
     *         <kLR, kR0-kR12>
     * -------------------------------------------------------------*/
    if ( p_taskinfo->priv == 0 ) {
        // user thread LR: return to the SVC handler
        *(--sp) = (U32) (&SVC_RESTORE);
    } else {
        // kernel thread LR: return to the entry point of the task
        *(--sp) = (U32) (p_taskinfo->ptask);
    }

    // kernel stack R0 - R12, 13 registers
    for ( int j = 0; j < 13; j++) {
        *(--sp) = 0x0;
    }

    // kernel stack CPSR
    *(--sp) = (U32) INIT_CPSR_SVC;
    p_tcb->ksp = sp;

    return RTX_OK;
}

/**************************************************************************//**
 * @brief       switching kernel stacks of two TCBs
 * @param:      p_tcb_old, the old tcb that was in RUNNING
 * @return:     RTX_OK upon success
 *              RTX_ERR upon failure
 * @pre:        gp_current_task is pointing to a valid TCB
 *              gp_current_task->state = RUNNING
 *              gp_crrent_task != p_tcb_old
 *              p_tcb_old == NULL or p_tcb_old->state updated
 * @note:       caller must ensure the pre-conditions are met before calling.
 *              the function does not check the pre-condition!
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * @attention   CRITICAL SECTION
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *
 *****************************************************************************/
__asm void k_tsk_switch(TCB *p_tcb_old)
{
        PUSH    {R0-R12, LR}
        MRS 	R1, CPSR
        PUSH 	{R1}
        STR     SP, [R0, #TCB_KSP_OFFSET]   ; save SP to p_old_tcb->ksp
        LDR     R1, =__cpp(&gp_current_task);
        LDR     R2, [R1]
        LDR     SP, [R2, #TCB_KSP_OFFSET]   ; restore ksp of the gp_current_task
        POP		{R0}
        MSR		CPSR_cxsf, R0
        POP     {R0-R12, PC}
}


/**************************************************************************//**
 * @brief       run a new thread. The caller becomes READY and
 *              the scheduler picks the next ready to run task.
 * @return      RTX_ERR on error and zero on success
 * @pre         gp_current_task != NULL && gp_current_task == RUNNING
 * @post        gp_current_task gets updated to next to run task
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * @attention   CRITICAL SECTION
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *****************************************************************************/
int k_tsk_run_new(void)
{
    TCB *p_tcb_old = NULL;
    
    if (gp_current_task == NULL) {
    	return RTX_ERR;
    }

    p_tcb_old = gp_current_task;
	gp_current_task = scheduler();

	if (gp_current_task != p_tcb_old) {
		gp_current_task->state = RUNNING;
		if (gp_current_task->tid == TID_NULL) {
			task_null();
		}
		if (p_tcb_old->state != DORMANT) {
			p_tcb_old->state = READY;			// change state of the to-be-switched-out tcb
			l_insert(p_tcb_old);
		}
		k_tsk_switch(p_tcb_old);
	}

	return RTX_OK;
}

/**************************************************************************//**
 * @brief       yield the cpu
 * @return:     RTX_OK upon success
 *              RTX_ERR upon failure
 * @pre:        gp_current_task != NULL &&
 *              gp_current_task->state = RUNNING
 * @post        gp_current_task gets updated to next to run task
 * @note:       caller must ensure the pre-conditions before calling.
 *****************************************************************************/
int k_tsk_yield(void)
{
	if (check_strict_prio() != RTX_OK) {
		return k_tsk_run_new();
	}
	return RTX_OK;
}


/*
 *===========================================================================
 *                             TO BE IMPLEMETED IN LAB2
 *===========================================================================
 */


int check_strict_prio() {
	TCB *p_tcb_current = gp_current_task;
	TCB *p_tcb_next = task_q_head;
	if (p_tcb_current->prio < p_tcb_next->prio) {
		return RTX_OK;
	}
	return RTX_ERR;
}

int check_prio() {
	TCB *p_tcb_current = gp_current_task;
	TCB *p_tcb_next = task_q_head;
	if (p_tcb_current->prio <= p_tcb_next->prio) {
		return RTX_OK;
	}
	return RTX_ERR;
}

task_t get_next_available_tid() {
	for (int tid=1; tid < MAX_TASKS; tid++){
		if (g_tcbs[tid].state == DORMANT) {
			return (task_t)tid;
		}
	}
	return NULL;
}

void initialize_rtx_task_info(RTX_TASK_INFO *buffer, U32 u_stack_hi, void (*task_entry)(void), U8 prio, task_t *task, U16 stack_size, U8 priv, U8 state){

	buffer->ptask = task_entry;
	buffer->k_stack_hi = (U32)(&g_k_stacks[*task + 1]);
	buffer->u_stack_hi = u_stack_hi;
	buffer->u_stack_size = stack_size;
	buffer->k_stack_size = K_STACK_SIZE;
	buffer->tid = *task;
	buffer->prio = prio;
	buffer->state = state;
	buffer->priv = priv;

}

void initialize_tcb(TCB *tcb, U32 *ksp, TCB *next, U8 prio, U8 priv, U8 state, void (*task_entry)(void), U8 tid, U16 stack_size, U32 u_stack_hi) {
	tcb->ksp = ksp;
	tcb->next = NULL;
	tcb->prio = prio;
	tcb->priv = priv;
	tcb->state = state;
	tcb->task_entry = task_entry;
	tcb->tid = tid;
	tcb->u_stack_size = stack_size;
	tcb->u_stack_hi = u_stack_hi;
}
int k_tsk_create(task_t *task, void (*task_entry)(void), U8 prio, U16 stack_size)
{
#ifdef DEBUG_0
    //printf("k_tsk_create: entering...\n\r");
    //printf("task = 0x%x, task_entry = 0x%x, prio=%d, stack_size = %d\n\r", task, task_entry, prio, stack_size);
#endif /* DEBUG_0 */

	if (prio == PRIO_NULL || prio == PRIO_RT || stack_size < U_STACK_SIZE || stack_size % 8  != 0 || task == NULL || task_entry == NULL || g_num_active_tasks >= MAX_TASKS) {
		return RTX_ERR;
	}
	*task = get_next_available_tid();

	TCB *tcb = &g_tcbs[*task];
	RTX_TASK_INFO rtx_task_info_temp;

	U32 user_stack_hi_addr = (U32) k_alloc_p_stack(tcb, *task, stack_size);
	initialize_rtx_task_info(&rtx_task_info_temp, user_stack_hi_addr, task_entry, prio, task, stack_size, 0, READY);

	k_tsk_create_new(&rtx_task_info_temp, tcb, *task);
	g_num_active_tasks++;

	if (check_prio() != RTX_OK) {
		k_tsk_run_new();
	}


	return RTX_OK;


}

void k_tsk_exit(void) 
{

    if (gp_current_task != NULL){
    	gp_current_task->state = DORMANT;
    }

    if (gp_current_task->priv == 0) {
    	k_mem_dealloc((void*)gp_current_task->u_stack_lo);
    }

    g_num_active_tasks--;
    k_tsk_run_new();

    return;
}

int k_tsk_set_prio(task_t task_id, U8 prio) 
{
#ifdef DEBUG_0
    //printf("k_tsk_set_prio: entering...\n\r");
    //printf("task_id = %d, prio = %d.\n\r", task_id, prio);
#endif /* DEBUG_0 */

	if (prio <= PRIO_RT || prio >= PRIO_NULL || task_id <= TID_NULL || task_id >= MAX_TASKS) {
		return RTX_ERR;
	}

	TCB* target_task = &g_tcbs[task_id];

	if ((gp_current_task->priv == 1 || target_task->priv == 0) && target_task->state != DORMANT){
		target_task->prio = prio;
		if ((gp_current_task->tid == target_task->tid && check_strict_prio()) ||
			(gp_current_task->tid != target_task->tid && check_prio())) {
			k_tsk_run_new();
		}

		return RTX_OK;
	}


    return RTX_ERR;
}

int k_tsk_get_info(task_t task_id, RTX_TASK_INFO *buffer)
{
#ifdef DEBUG_0
    //printf("k_tsk_get_info: entering...\n\r");
    //printf("task_id = %d, buffer = 0x%x.\n\r", task_id, buffer);
#endif /* DEBUG_0 */    
    if (buffer == NULL || task_id >= MAX_TASKS || task_id <= TID_NULL) {
        return RTX_ERR;
    }

    TCB* foundTCB = &g_tcbs[task_id];

    /* The code fills the buffer with some fake task information. 
       You should fill the buffer with correct information    */

    initialize_rtx_task_info(buffer, foundTCB->u_stack_hi, foundTCB->task_entry, foundTCB->prio, &task_id, foundTCB->u_stack_size, foundTCB->priv, foundTCB->state);

    return RTX_OK;     
}

task_t k_tsk_get_tid(void)
{
#ifdef DEBUG_0
    //printf("k_tsk_get_tid: %u entering...\n\r", gp_current_task->tid);
#endif /* DEBUG_0 */ 
    return gp_current_task->tid;
}

int k_tsk_ls(task_t *buf, int count){
#ifdef DEBUG_0
    //printf("k_tsk_ls: buf=0x%x, count=%d\r\n", buf, count);
#endif /* DEBUG_0 */
    return 0;
}

// Helper functions
int l_insert (TCB *new_tcb)
{
	//printf("DEBUG: new_tcb->prio : %d task_q_head->prio : %d\r\n", new_tcb->prio, task_q_head->prio);
	if (task_q_head == NULL || new_tcb == NULL || (g_num_active_tasks -1) >= MAX_TASKS){return RTX_ERR;} // Error
	if (task_q_head->prio > new_tcb->prio) // Schedule higher priority task to run first
	{
		new_tcb->next = task_q_head;
		task_q_head = new_tcb;
		return RTX_OK;
	}
	TCB *curr = task_q_head;
	TCB *next = task_q_head->next;
	TCB *prev = curr;
	while (next != NULL && next->prio < new_tcb->prio){prev = curr; curr = next; next = next->next;} // Find spot for new_tcb
	if (next == NULL)
	{
		if (curr->prio <= new_tcb->prio) // Lower priority task at end of list
		{
			curr->next = new_tcb;
			new_tcb->next = NULL;
			return RTX_OK;
		}
		// Higher priority task at end of list
		prev->next = new_tcb;
		new_tcb->next = curr;
		curr->next = NULL;
		return RTX_OK;
	}
	// Here: next->prio >= new_tcb->prio which implies we found a spot in the list for the new node
	if (curr->prio <= new_tcb->prio)
	{
		// Place new node at end of queue of same / next priority level
		while (next != NULL && next->prio == new_tcb->prio){prev = curr; curr = next; next = next->next;}
		if (next == NULL)
		{
			if (curr->prio <= new_tcb->prio) // Lower priority task at end of list
			{
				curr->next = new_tcb;
				new_tcb->next = NULL;
				return RTX_OK;
			}
			// Higher priority task at end of list
			prev->next = new_tcb;
			new_tcb->next = curr;
			curr->next = NULL;
			return RTX_OK;
		}
		// Lower priority insert in list
		curr->next = new_tcb;
		new_tcb->next = next;
		return RTX_OK;
	}
	// Higher priority task at end of list
	prev->next = new_tcb;
	new_tcb->next = curr;
	return RTX_OK;
}
TCB *l_pop(TCB *to_rm)
{
	TCB *temp = task_q_head;
	TCB *prev = task_q_head;
	while (temp != NULL && temp != to_rm){prev = temp; temp = temp->next;} // Linear search the queue for to_rm
	if (temp == NULL){return NULL;} // Error
	//Here: temp == to_rm
	if (temp == task_q_head)
	{
		// Update new head
		task_q_head = temp->next;
	}
	prev->next = temp->next;
	temp->next = NULL;
	return temp;
}
int l_update_priority(TCB *to_update, U8 new_prio)
{
	TCB *temp = NULL; // TODO: Antipattern with pop and insert
	temp = l_pop(to_update);			// Remove task from current queue
	if (temp == NULL){return RTX_ERR;} 	// Error
	temp->prio = new_prio;				// Set new priority
	return l_insert(to_update);			// Insert back into queue
}
void l_print(TCB *node)
{
#ifdef DEBUG_0
	TCB *temp = node;
	int list_length = 0;
	while (temp != NULL)
	{
		printf("l_print TCB address: 0x%x ", (U32)temp);
		printf("next: 0x%x ", (U32)temp->next);
		printf("priority: %d ", temp->prio);
		printf("state: %d", temp->state);
		printf("priv: %d\r\n", temp->priv);
		temp = temp->next;
		list_length++;
	}
	printf("LIST LENGTH: %d\r\n", list_length);
#endif
	return;
}
TCB *get_qhead(void)
{
	return task_q_head;
}
void set_qhead(TCB *new_head)
{
	task_q_head = new_head;
}
/*
 *===========================================================================
 *                             TO BE IMPLEMETED IN LAB4
 *===========================================================================
 */

int k_tsk_create_rt(task_t *tid, TASK_RT *task)
{
    return 0;
}

void k_tsk_done_rt(void) {
#ifdef DEBUG_0
    printf("k_tsk_done: Entering\r\n");
#endif /* DEBUG_0 */
    return;
}

void k_tsk_suspend(TIMEVAL *tv)
{
#ifdef DEBUG_0
    printf("k_tsk_suspend: Entering\r\n");
#endif /* DEBUG_0 */
    return;
}

/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
