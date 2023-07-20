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
 * @file        ae_mem.c
 * @brief       memory lab auto-tester
 *
 * @version     V1.2021.01
 * @authors     Yiqing Huang
 * @date        2021 JAN
 *
 *****************************************************************************/

#include "rtx.h"
#include "Serial.h"
#include "printf.h"
#include "ae_priv_tasks.h"
#include "ae.h"

#if TEST == -1

int test_mem(void) {
	unsigned int start = timer_get_current_val(2);
	printf("NOTHING TO TEST.\r\n");
	unsigned int end = timer_get_current_val(2);

	// Clock counts down
	printf("This took %u us\r\n", start - end);
}

#endif
#if TEST == 1
BOOL test_coalescing_free_regions_using_count_extfrag() {
	void * p1 = k_mem_alloc(32);
	void * p2 = k_mem_alloc(32);

	unsigned int header_size = (unsigned int)p2 - (unsigned int)p1 - 32;

	void * p3 = k_mem_alloc(32);
	void * p4 = k_mem_alloc(32);
	void * p5 = k_mem_alloc(32);
	void * p6 = k_mem_alloc(32);
	void * p7 = k_mem_alloc(32);
	void * p8 = k_mem_alloc(32);
	void * p9 = k_mem_alloc(32);

	int size_33_plus_one_header = k_mem_count_extfrag(32 + header_size + 1);
	int size_97_plus_three_header = k_mem_count_extfrag(96 + 3*header_size + 1);

	if((size_33_plus_one_header!=0) || (size_97_plus_three_header!=0)) {
		printf("test_coalescing_free_regions_using_count_extfrag: 1. Either mem_alloc or mem_count_extfrag has failed.\r\n");
		k_mem_dealloc(p1);
		k_mem_dealloc(p2);
		k_mem_dealloc(p3);
		k_mem_dealloc(p4);
		k_mem_dealloc(p5);
		k_mem_dealloc(p6);
		k_mem_dealloc(p7);
		k_mem_dealloc(p8);
		k_mem_dealloc(p9);
		return FALSE;
	}

	k_mem_dealloc(p2);
	k_mem_dealloc(p4);
	k_mem_dealloc(p6);
	k_mem_dealloc(p8);

	size_33_plus_one_header = k_mem_count_extfrag(32 + header_size + 1);
	size_97_plus_three_header = k_mem_count_extfrag(96 + 3*header_size + 1);

	if((size_33_plus_one_header!=4) || (size_97_plus_three_header!=4)) {
		printf("test_coalescing_free_regions_using_count_extfrag: 2. Either mem_dealloc or coalescing has failed.\r\n");
		k_mem_dealloc(p1);
		k_mem_dealloc(p3);
		k_mem_dealloc(p5);
		k_mem_dealloc(p7);
		k_mem_dealloc(p9);
		return FALSE;
	}

	k_mem_dealloc(p3);
	k_mem_dealloc(p7);

	size_33_plus_one_header = k_mem_count_extfrag(32 + header_size + 1);
	size_97_plus_three_header = k_mem_count_extfrag(96 + 3*header_size + 1);

	if((size_33_plus_one_header!=0) || (size_97_plus_three_header!=2)) {
		printf("test_coalescing_free_regions_using_count_extfrag: 3. Either mem_dealloc or coalescing has failed.\r\n");
		k_mem_dealloc(p1);
		k_mem_dealloc(p5);
		k_mem_dealloc(p9);
		return FALSE;
	}

	k_mem_dealloc(p1);
	k_mem_dealloc(p5);
	k_mem_dealloc(p9);

	int size_289_plus_nine_header = k_mem_count_extfrag(288 + 9*header_size + 1);

	if(size_289_plus_nine_header!=0) {
		printf("test_coalescing_free_regions_using_count_extfrag: 4. Either mem_dealloc or coalescing has failed.\r\n");
		return FALSE;
	}

	return TRUE;
}


int test_mem(void) {

	int test_coalescing_free_regions_result = test_coalescing_free_regions_using_count_extfrag();

	return test_coalescing_free_regions_result;
}
#endif

#if TEST == 2

#define N 10

#define CODE_MEM_INIT -1
#define CODE_MEM_ALLOC -2
#define CODE_MEM_DEALLOC -3
#define CODE_HEAP_LEAKAGE_1 -4
#define CODE_HEAP_LEAKAGE_2 -5
#define CODE_SUCCESS 0

int heap_leakage_test() {

	char *p_old[N], *p_new[N];

	// Step 1: Allocate memory
	for (int i = 0; i < N; i++) {
		p_old[i] = (char*) k_mem_alloc(i * 256 + 255);

		// pointer to allocated memory should not be null
		// starting address of allocated memory should be four-byte aligned
		if (p_old[i] == NULL || ((unsigned int) p_old[0] & 3))
			return CODE_MEM_ALLOC;

		if (i > 0) {
			// adjacent allocated memory should not conflict
			if (p_old[i - 1] + 256 * i >= p_old[i])
				return CODE_MEM_ALLOC;
		}
	}

	// Step 2: De-allocate memory
	for (int i = 0; i < N; i++) {
		if (k_mem_dealloc(p_old[i]) == -1) {
			return CODE_MEM_DEALLOC;
		}
	}

	// Step 3: Memory Leakage
	for (int i = 0; i < N; i++) {
		p_new[i] = (char*) k_mem_alloc((N - i) * 256 - 1);

		// pointer to allocated memory should not be null
		// starting address of allocated memory should be four-byte aligned
		if (p_new[i] == NULL || ((unsigned int) p_new[0] & 3))
			return CODE_HEAP_LEAKAGE_1;

		if (i > 0) {
			// adjacent allocated memory should not conflict
			if (p_new[i - 1] + 256 * (N - i + 1) >= p_new[i])
				return CODE_HEAP_LEAKAGE_1;
		}
	}

	// the total occupied area in the re-allocation
	// should be the same as in the initial allocation
	if ((p_old[0] != p_new[0])
			|| (p_old[N - 1] + N * 256 != p_new[N - 1] + 256)) {
		return CODE_HEAP_LEAKAGE_2;
	}

	for (int i = 0; i < N; i++) {
		k_mem_dealloc(p_new[i]);
	}

	return CODE_SUCCESS;
}

int test_mem(void) {

	int result = heap_leakage_test();
	switch (result) {
	case CODE_MEM_INIT:
	case CODE_MEM_ALLOC:
		printf("Err: Basic allocation fails.\r\n");
		break;
	case CODE_MEM_DEALLOC:
		printf("Err: Basic deallocation fails.\r\n");
		break;
	case CODE_HEAP_LEAKAGE_1:
		printf("Err: Reallocation fails.\r\n");
		break;
	case CODE_HEAP_LEAKAGE_2:
		printf("Err: Heap memory is leaked.\r\n");
		break;
	case CODE_SUCCESS:
		printf("No heap leakage.\r\n");
		break;
	default:
	}

	return result == CODE_SUCCESS;
}
#endif

#if TEST == 3

#define CODE_MEM_INIT -1
#define CODE_ALLOC_FAILED -2
#define CODE_NORMAL_ALLOC_FAILED -3
#define CODE_OVER_ALLOC_FAILED -4
#define CODE_SUCCESS 0

int heap_allocation_test(int size)
{
	void *pointer = k_mem_alloc(size);
	k_mem_dealloc(pointer);
	if (pointer == NULL)
	{
		return CODE_ALLOC_FAILED;
	}
	return CODE_SUCCESS;
}

int double_allocation_test()
{
	// init test
	int mem_init_result = k_mem_init();
	if (mem_init_result == -1)
	{
		return CODE_MEM_INIT;
	}

	int result1 = heap_allocation_test(2000000000);
	int result2 = heap_allocation_test(900000000);

	if (result1 == CODE_ALLOC_FAILED && result2 == CODE_SUCCESS)
	{
		return CODE_SUCCESS;
	}
	else if (result1 == CODE_SUCCESS)
	{
		return CODE_OVER_ALLOC_FAILED;
	}
	return CODE_NORMAL_ALLOC_FAILED;
}

int test_mem(void)
{
	int result = double_allocation_test();
	switch (result)
	{
	case CODE_MEM_INIT:
	case CODE_NORMAL_ALLOC_FAILED:
		printf("Err: Basic allocation fails.\r\n");
		break;
	case CODE_ALLOC_FAILED:
		printf("Err: Basic allocation fails.\r\n");
		break;
	case CODE_OVER_ALLOC_FAILED:
		printf("Err: Basic allocation fails.\r\n");
		break;
	case CODE_SUCCESS:
		printf("OS handled allocation requests correctly.\r\n");
		break;
	default:
		break;
	}

	return result == CODE_SUCCESS;
}

#endif
#if TEST == 4

#define CODE_ALIGN_FAILED -1
#define CODE_SUCCESS 0
#define LOOP_COUNT 10

int align_test()
{
	// init test
	int mem_init_result = k_mem_init();

	// alloc memory
	for (int i = 0; i < LOOP_COUNT; i++) {
		void *pointer = k_mem_alloc(i + 1);
		int val = (U32)pointer % 4;
		k_mem_dealloc(pointer);
		if (val != 0) {
			return CODE_ALIGN_FAILED;
		}
	}
	return CODE_SUCCESS;
}

int test_mem(void)
{
	int result = align_test();
	switch (result)
	{
	case CODE_ALIGN_FAILED:
		printf("Memory did not four byte align properly.\r\n");
		break;
	case CODE_SUCCESS:
		printf("Memory was 4 byte aligned correctly.\r\n");
		break;
	default:
		break;
	}

	return result == CODE_SUCCESS;
}

#endif

#if TEST == 5

#define FREE_LIST_LENGTH 2

#define CODE_MEM_INIT -1
#define CODE_MEMORY_LOSS_ERROR -2
#define CODE_SUCCESS 0
#define ALLOC_SIZE 10

int memory_loss_test()
{
	// init test
	int mem_init_result = k_mem_init();
	if (mem_init_result == -1)
	{
		return CODE_MEM_INIT;
	}

	header *head = get_head();
	U32 pre_alloc_size = head->size;

	void *pointer_list[ALLOC_SIZE];

	for (int i = 0; i < ALLOC_SIZE; i++) {
		pointer_list[i] = k_mem_alloc(10);
	}

	for (int i = 0; i < ALLOC_SIZE; i++) {
		k_mem_dealloc(pointer_list[i]);
	}

	head = get_head();
	U32 post_alloc_size = head->size;

	if (post_alloc_size == pre_alloc_size) {
		return CODE_SUCCESS;
	}
	printf("Memory lost amount: %d\r\n", pre_alloc_size - post_alloc_size);
	return CODE_MEMORY_LOSS_ERROR;



}

int test_mem(void)
{
	int result = memory_loss_test();
	switch (result)
	{
	case CODE_MEM_INIT:
		printf("Err: Basic allocation fails.\r\n");
		break;
	case CODE_SUCCESS:
		printf("Success. No memory is lost.\r\n");
		break;
	case CODE_MEMORY_LOSS_ERROR:
		printf("Failure. Memory has been lost!.\r\n");
		break;
	default:
		break;
	}

	return result == CODE_SUCCESS;
}
#endif

#if TEST == 6

#define CODE_MEM_INIT -1
#define CODE_HEAD_LOCATION_ERROR -2
#define CODE_SUCCESS 0
#define ALLOC_AMT 200

int head_location_test() {
	int mem_init_result = k_mem_init();
	if (mem_init_result == -1)
	{
		return CODE_MEM_INIT;
	}

	header *ptr_header = get_head();
	void *ptr = k_mem_alloc(ALLOC_AMT);
	header *shifted_head = get_head();

	if ((U32)ptr + ptr_header->size - calc_padding((U32)ptr + ptr_header->size) == (U32)shifted_head) {
		return CODE_SUCCESS;
	}
	return CODE_HEAD_LOCATION_ERROR;

}

int test_mem(void) {
	int result = head_location_test();
	switch (result)
	{
	case CODE_MEM_INIT:
		printf("Failure.  Basic allocation failed.\r\n");
	case CODE_HEAD_LOCATION_ERROR:
		printf("Failure. Head is not in the correct location.\r\n");
		break;
	case CODE_SUCCESS:
		printf("Success. Head is in the correct location.\r\n");
		break;
	default:
		break;
	}
	return result == CODE_SUCCESS;
}
#endif

#if TEST == 7

#define CODE_MEM_INIT -1
#define CODE_DEALLOC_ERR -2
#define CODE_ALLOC_ERR -3
#define CODE_SUCCESS 0
#define ALLOC_SIZE 123 /* Must be >> sizeof(header) */

int alloc_no_split_test() {
	int mem_init_result = k_mem_init();
	if (mem_init_result == -1)
	{
		return CODE_MEM_INIT;
	}

	header *p_head = get_head();

	/* Allocate two blocks and free first block */
	void *first = k_mem_alloc(ALLOC_SIZE);
	void *second = k_mem_alloc(ALLOC_SIZE);
	int result = k_mem_dealloc(first);
	if (result == RTX_ERR)
	{
		return CODE_DEALLOC_ERR;
	}
	/* Attempt to allocate a block of size equal to available space then free it */
	first = k_mem_alloc(ALLOC_SIZE);
	if (first == NULL)
	{
		return CODE_ALLOC_ERR;
	}
	result = k_mem_dealloc(first);
	if (result == RTX_ERR)
	{
		return CODE_DEALLOC_ERR;
	}

	/* Attempt to allocate a block sizeof(header) less than available space then free it */
	first = k_mem_alloc(ALLOC_SIZE - sizeof(header));
	if (first == NULL)
	{
		return CODE_ALLOC_ERR;
	}
	result = k_mem_dealloc(first);
	if (result == RTX_ERR)
	{
		return CODE_DEALLOC_ERR;
	}

	/* Attempt to allocate a block sizeof(header) - 4 less than available space then free it */
	first = k_mem_alloc(ALLOC_SIZE - sizeof(header) - 4);
	if (first == NULL)
	{
		return CODE_ALLOC_ERR;
	}
	result = k_mem_dealloc(first);
	if (result == RTX_ERR)
	{
		return CODE_DEALLOC_ERR;
	}

	/* Attempt to allocate a block sizeof(header) - 1 less than available space then free it */
	first = k_mem_alloc(ALLOC_SIZE - sizeof(header) - 1);
	if (first == NULL)
	{
		return CODE_ALLOC_ERR;
	}
	result = k_mem_dealloc(first);
	if (result == RTX_ERR)
	{
		return CODE_DEALLOC_ERR;
	}
	print_header_list(get_head());
	return CODE_SUCCESS;

}

int test_mem(void) {
	int result = alloc_no_split_test();
	switch (result)
	{
	case CODE_MEM_INIT:
		printf("Failure.  Basic allocation failed.\r\n");
	case CODE_DEALLOC_ERR:
		printf("Failure. k_mem_dealloc returned RTX_ERR.\r\n");
		break;
	case CODE_ALLOC_ERR:
		printf("Failure. k_mem_alloc returned RTX_ERR.\r\n");
		break;
	case CODE_SUCCESS:
		printf("Success. Program handles not splitting when allocating memory.\r\n");
		break;
	default:
		break;
	}
	return result == CODE_SUCCESS;
}
#endif

#if TEST == 8

#define CODE_MEM_INIT -1
#define MEMORY_OOB_ERROR -2
#define FINAL_ALLOC_ERROR -3
#define CODE_SUCCESS 0
#define ALLOC_AMT 1000

U32 calc_padded_amt(header *head) {
	U32 header_padding = calc_padding((U32)head + sizeof(header));
	U32 data_padding = calc_padding((U32)head + sizeof(header) + header_padding + ALLOC_AMT);
	return (header_padding + ALLOC_AMT + data_padding);
}

int fill_memory_test() {
	int mem_init_result = k_mem_init();
	if (mem_init_result == -1)
	{
		return CODE_MEM_INIT;
	}

	void *pointer = (void *)(U32)1;
	while (pointer != NULL) {
		pointer = k_mem_alloc(ALLOC_AMT);
		if ((U32)pointer > RAM_END) {
			return MEMORY_OOB_ERROR;
		}
	}
	header *head = get_head();
	U32 padded_alloc_amt = calc_padded_amt(head);
	if (padded_alloc_amt > head->size) {
		return CODE_SUCCESS;
	}
	return FINAL_ALLOC_ERROR;
}

int test_mem() {
	int result = fill_memory_test();
	switch(result) {
	case MEMORY_OOB_ERROR:
		printf("Failure. Memory went out of bounds!\r\n");
		break;
	case FINAL_ALLOC_ERROR:
		printf("Failure. The final allocation failed, but there was enough free memory!\r\n");
		break;
	case CODE_SUCCESS:
		printf("Success. Memory did not go out of bounds!\r\n");
		break;
	default:
		break;
	}
	return result == CODE_SUCCESS;
}

#endif


/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
