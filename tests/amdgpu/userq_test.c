/*
 * Copyright 2023 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "CUnit/Basic.h"

#include "amdgpu_drm.h"
#include "amdgpu_internal.h"
#include "amdgpu_test.h"
#include "util_math.h"
#include "xf86drm.h"

#define PAGE_SIZE			4096
#define USERMODE_QUEUE_SIZE		256
#define ALIGNMENT			256

#define GFX_COMPUTE_NOP			0xffff1000

#define PACKET_TYPE3			3
#define PACKET3(op, n)			((PACKET_TYPE3 << 30) |  \
					(((op) & 0xFF) << 8)  |  \
					((n) & 0x3FFF) << 16)

#define PACKET3_PROTECTED_FENCE_SIGNAL	0xd0
#define PACKET3_WRITE_DATA		0x37
#define WR_CONFIRM			(1 << 20)
#define WRITE_DATA_DST_SEL(x)		((x) << 8)
#define WRITE_DATA_ENGINE_SEL(x)	((x) << 30)
#define  WRITE_DATA_CACHE_POLICY(x)	((x) << 25)

#define DOORBELL_INDEX			4
#define AMDGPU_USERQ_BO_WRITE		1

struct amdgpu_userq_bo {
	amdgpu_bo_handle handle;
	amdgpu_va_handle va_handle;
	uint64_t mc_addr;
	uint64_t size;
	void *ptr;
};

static void amdgpu_userqueue(void);
static void amdgpu_userqueue_synchronize_test(void);

pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static amdgpu_device_handle device_handle;
static struct amdgpu_userq_bo shared_userq_bo;
static amdgpu_bo_handle shared_bo;
static int shared_syncobj_fd;
static uint32_t major_version;
static uint32_t minor_version;

CU_TestInfo userq_tests[] = {
	{"Create UserQueue Test", amdgpu_userqueue},
	{"Userqueue Synchronize Test", amdgpu_userqueue_synchronize_test},
	CU_TEST_INFO_NULL,
};

CU_BOOL suite_userq_tests_enable(void)
{
	return CU_TRUE;
}

int suite_userq_tests_init(void)
{
	struct amdgpu_bo_alloc_request req = {0};
	static amdgpu_va_handle va_handle;
	amdgpu_bo_handle buf_handle;
	uint32_t *ptr;
	uint64_t va;
	int r;

	r = amdgpu_device_initialize(drm_amdgpu[0], &major_version,
				     &minor_version, &device_handle);
	if (r) {
		if ((r == -EACCES) && (errno == EACCES))
			printf("\n\nError:%s. "
			       "Hint:Try to run this test program as root.",
				strerror(errno));

		return CUE_SINIT_FAILED;
	}

	req.alloc_size = USERMODE_QUEUE_SIZE;
	req.phys_alignment = PAGE_SIZE;
	req.preferred_heap = AMDGPU_GEM_DOMAIN_GTT;

	r = amdgpu_bo_alloc(device_handle, &req, &buf_handle);
	if (r)
		return CUE_SINIT_FAILED;

	r = amdgpu_va_range_alloc(device_handle,
				  amdgpu_gpu_va_range_general,
				  USERMODE_QUEUE_SIZE, PAGE_SIZE, 0,
				  &va, &va_handle, 0);
	if (r)
		goto error_va_alloc;

	r = amdgpu_bo_va_op(buf_handle, 0, USERMODE_QUEUE_SIZE, va, 0, AMDGPU_VA_OP_MAP);
	if (r)
		goto error_va_map;

	r = amdgpu_bo_cpu_map(buf_handle, (void **)&shared_userq_bo.ptr);
	if (r)
		goto error_va_map;

	ptr = (uint32_t *)shared_userq_bo.ptr;
	memset(ptr, 0, sizeof(*ptr));

	shared_userq_bo.mc_addr = va;
	shared_userq_bo.handle = buf_handle;
	shared_userq_bo.size = req.alloc_size;
	shared_userq_bo.va_handle = va_handle;
	shared_bo = buf_handle;

	return CUE_SUCCESS;

error_va_map:
	amdgpu_va_range_free(va_handle);

error_va_alloc:
	amdgpu_bo_free(buf_handle);
	return CUE_SINIT_FAILED;
}

int suite_userq_tests_clean(void)
{
	int r = amdgpu_device_deinitialize(device_handle);

	if (r == 0)
		return CUE_SUCCESS;
	else
		return CUE_SCLEAN_FAILED;
}

void delay_micro(int micro_seconds)
{
	// Storing start time
	clock_t start_time = clock();

	// looping till required time is not achieved
	while (clock() < start_time + micro_seconds)
	;
}

static void alloc_doorbell(struct amdgpu_userq_bo *userq_bo,
			   unsigned size, unsigned domain)
{
	struct amdgpu_bo_alloc_request req = {0};
	amdgpu_bo_handle buf_handle;
	int r;

	req.alloc_size = ALIGN(size, PAGE_SIZE);
	req.preferred_heap = domain;
	r = amdgpu_bo_alloc(device_handle, &req, &buf_handle);
	CU_ASSERT_EQUAL(r, 0);

	userq_bo->handle = buf_handle;
	userq_bo->size = req.alloc_size;

	r = amdgpu_bo_cpu_map(userq_bo->handle, (void **)&userq_bo->ptr);
	CU_ASSERT_EQUAL(r, 0);

	return;
}

static void amdgpu_userqueue(void)
{
	int r, i = 0;
	struct drm_amdgpu_userq_mqd_gfx mqd;
	uint32_t *ptr, *newptr;
	uint32_t q_id;
	struct  amdgpu_userq_bo queue, dstptr, shadow, doorbell;
	uint64_t gtt_flags = 0, *doorbell_ptr;

	amdgpu_bo_alloc_and_map(device_handle, USERMODE_QUEUE_SIZE + 8 + 8,
				ALIGNMENT,
				AMDGPU_GEM_DOMAIN_GTT,
				gtt_flags,
				&queue.handle, &queue.ptr,
				&queue.mc_addr, &queue.va_handle);
	CU_ASSERT_EQUAL(r, 0);

	amdgpu_bo_alloc_and_map(device_handle, USERMODE_QUEUE_SIZE,
				ALIGNMENT,
				AMDGPU_GEM_DOMAIN_VRAM,
				gtt_flags,
				&dstptr.handle, &dstptr.ptr,
				&dstptr.mc_addr, &dstptr.va_handle);
	CU_ASSERT_EQUAL(r, 0);

	amdgpu_bo_alloc_and_map(device_handle, PAGE_SIZE * 4, PAGE_SIZE,
				AMDGPU_GEM_DOMAIN_VRAM,
				gtt_flags,
				&shadow.handle, &shadow.ptr,
				&shadow.mc_addr, &shadow.va_handle);
	CU_ASSERT_EQUAL(r, 0);

	alloc_doorbell(&doorbell, PAGE_SIZE, AMDGPU_GEM_DOMAIN_DOORBELL);

	mqd.queue_va = queue.mc_addr;
	mqd.rptr_va = queue.mc_addr + USERMODE_QUEUE_SIZE;
	mqd.wptr_va = queue.mc_addr + USERMODE_QUEUE_SIZE + 8;
	mqd.shadow_va = shadow.mc_addr;
	mqd.queue_size = USERMODE_QUEUE_SIZE;

	mqd.doorbell_handle = doorbell.handle->handle;
	mqd.doorbell_offset = DOORBELL_INDEX;

	doorbell_ptr = (uint64_t *)doorbell.ptr;

	newptr = (uint32_t *)dstptr.ptr;
	memset(newptr, 0, sizeof(*newptr));

	ptr = (uint32_t *)queue.ptr;
	memset(ptr, 0, sizeof(*ptr));

	/* Create the Usermode Queue */
	r = amdgpu_create_userq_gfx(device_handle, &mqd, AMDGPU_HW_IP_GFX, &q_id);
	CU_ASSERT_EQUAL(r, 0);
	if (r)
		goto err_free_queue;

	ptr[0] = PACKET3(PACKET3_WRITE_DATA, 7);
	ptr[1] = WRITE_DATA_DST_SEL(5) | WR_CONFIRM | WRITE_DATA_CACHE_POLICY(3);
	ptr[2] = 0xfffffffc & (dstptr.mc_addr);
	ptr[3] = (0xffffffff00000000 & (dstptr.mc_addr)) >> 32;
	ptr[4] = 0xdeadbeaf;
	ptr[5] = 0xdeadbeaf;
	ptr[6] = 0xdeadbeaf;
	ptr[7] = 0xdeadbeaf;
	ptr[8] = 0xdeadbeaf;

	/* firmware needs 300us to 500us time to map the user queue */
	delay_micro(300);

	doorbell_ptr[DOORBELL_INDEX]  = 9;

	while (!newptr[0]) {
		//busy-loop untill destination in not updated.
		printf("Destination is still not updated newptr[0] = %x\n",
			newptr[0]);
	}

	i = 0;
	while (i < 5) {
		printf(" => newptr[%d] = %x\n", i, newptr[i]);
		CU_ASSERT_EQUAL(newptr[i++], 0xdeadbeaf);
	}

	/* Free the Usermode Queue */
	r = amdgpu_free_userq_gfx(device_handle, q_id);
	CU_ASSERT_EQUAL(r, 0);

err_free_queue:
	r = amdgpu_bo_unmap_and_free(shadow.handle, shadow.va_handle,
				     shadow.mc_addr, PAGE_SIZE*4);
	CU_ASSERT_EQUAL(r, 0);

	r = amdgpu_bo_unmap_and_free(dstptr.handle, dstptr.va_handle,
				     dstptr.mc_addr, PAGE_SIZE);
	CU_ASSERT_EQUAL(r, 0);

	r = amdgpu_bo_cpu_unmap(doorbell.handle);
	CU_ASSERT_EQUAL(r, 0);

	r = amdgpu_bo_free(doorbell.handle);
	CU_ASSERT_EQUAL(r, 0);

	r = amdgpu_bo_unmap_and_free(queue.handle, queue.va_handle,
				     queue.mc_addr, PAGE_SIZE);
	CU_ASSERT_EQUAL(r, 0);

	return;
}

static void *userq_signal(void *data)
{
	struct  amdgpu_userq_bo queue, shadow, doorbell, wptr_bo, rptr;
	uint64_t gtt_flags = 0, *doorbell_ptr;
	struct drm_amdgpu_userq_mqd_gfx mqd;
	uint32_t q_id, syncobj_handle;
	uint32_t *ptr, *wptr;
	int r, i;

	amdgpu_bo_alloc_and_map(device_handle, USERMODE_QUEUE_SIZE,
				ALIGNMENT,
				AMDGPU_GEM_DOMAIN_GTT,
				gtt_flags,
				&queue.handle, &queue.ptr,
				&queue.mc_addr, &queue.va_handle);
	CU_ASSERT_EQUAL(r, 0);

	amdgpu_bo_alloc_and_map(device_handle, PAGE_SIZE,
                                PAGE_SIZE,
                                AMDGPU_GEM_DOMAIN_GTT,
                                gtt_flags,
                                &wptr_bo.handle, &wptr_bo.ptr,
                                &wptr_bo.mc_addr, &wptr_bo.va_handle);
        CU_ASSERT_EQUAL(r, 0);

	amdgpu_bo_alloc_and_map(device_handle, PAGE_SIZE,
                                PAGE_SIZE,
                                AMDGPU_GEM_DOMAIN_GTT,
                                gtt_flags,
                                &rptr.handle, &rptr.ptr,
                                &rptr.mc_addr, &rptr.va_handle);
        CU_ASSERT_EQUAL(r, 0);

	amdgpu_bo_alloc_and_map(device_handle, PAGE_SIZE * 4, PAGE_SIZE,
				AMDGPU_GEM_DOMAIN_VRAM,
				gtt_flags,
				&shadow.handle, &shadow.ptr,
				&shadow.mc_addr, &shadow.va_handle);
	CU_ASSERT_EQUAL(r, 0);

	alloc_doorbell(&doorbell, PAGE_SIZE, AMDGPU_GEM_DOMAIN_DOORBELL);

	mqd.queue_va = queue.mc_addr;
	mqd.rptr_va = rptr.mc_addr;
	mqd.wptr_va = wptr_bo.mc_addr;
	mqd.shadow_va = shadow.mc_addr;
	mqd.queue_size = USERMODE_QUEUE_SIZE;

	mqd.doorbell_handle = doorbell.handle->handle;
	mqd.doorbell_offset = DOORBELL_INDEX;

	doorbell_ptr = (uint64_t *)doorbell.ptr;

	ptr = (uint32_t *)queue.ptr;
	memset(ptr, 0, sizeof(*ptr));

	wptr = (uint32_t *)wptr_bo.ptr;
	memset(wptr, 0, sizeof(*wptr));

	/* Create the Usermode Queue */
	r = amdgpu_create_userq_gfx(device_handle, &mqd, AMDGPU_HW_IP_GFX, &q_id);
	CU_ASSERT_EQUAL(r, 0);
	if (r)
		goto err_free_queue;

	r = drmSyncobjCreate(device_handle->fd, 0, &syncobj_handle);
	CU_ASSERT_EQUAL(r, 0);

	r = drmSyncobjHandleToFD(device_handle->fd, syncobj_handle, &shared_syncobj_fd);
	CU_ASSERT_EQUAL(r, 0);

	ptr[0] = PACKET3(PACKET3_WRITE_DATA, 7);
	ptr[1] = WRITE_DATA_DST_SEL(5) | WR_CONFIRM | WRITE_DATA_CACHE_POLICY(3);
	ptr[2] = 0xfffffffc & (shared_userq_bo.mc_addr);
	ptr[3] = (0xffffffff00000000 & (shared_userq_bo.mc_addr)) >> 32;
	ptr[4] = 0xdeadbeaf;
	ptr[5] = 0xdeadbeaf;
	ptr[6] = 0xdeadbeaf;
	ptr[7] = 0xdeadbeaf;
	ptr[8] = 0xdeadbeaf;

	for (i = 9; i < 1000; i++)
		ptr[i] = GFX_COMPUTE_NOP;

	ptr[i++] = PACKET3(PACKET3_PROTECTED_FENCE_SIGNAL, 0);

	*wptr = i;

	r = amdgpu_userq_signal(device_handle, q_id, syncobj_handle,
				(uint64_t)&shared_bo->handle, 1, AMDGPU_USERQ_BO_WRITE);
	CU_ASSERT_EQUAL(r, 0);
	if (!r)
		pthread_cond_signal(&cond);

	/* firmware needs 300us to 500us time to map the user queue */
	delay_micro(300);

	doorbell_ptr[DOORBELL_INDEX]  = 1002;

	/* Free the Usermode Queue */
	r = amdgpu_free_userq_gfx(device_handle, q_id);
	CU_ASSERT_EQUAL(r, 0);

err_free_queue:
	r = amdgpu_bo_unmap_and_free(shadow.handle, shadow.va_handle,
				     shadow.mc_addr, PAGE_SIZE * 4);
	CU_ASSERT_EQUAL(r, 0);

	r = amdgpu_bo_cpu_unmap(doorbell.handle);
	CU_ASSERT_EQUAL(r, 0);

	r = amdgpu_bo_free(doorbell.handle);
	CU_ASSERT_EQUAL(r, 0);

	r = amdgpu_bo_unmap_and_free(queue.handle, queue.va_handle,
				     queue.mc_addr, PAGE_SIZE);
	CU_ASSERT_EQUAL(r, 0);

	return (void *)(long)r;
}

static void *userq_wait(void *data)
{
	struct drm_amdgpu_userq_fence_info *fence_info = NULL;
	uint64_t bo_handles_array, num_fences, s_handle;
	uint32_t syncobj_handle, *wait_ptr;
	int r, i;

	pthread_mutex_lock(&lock);
	pthread_cond_wait(&cond, &lock);
	pthread_mutex_unlock(&lock);

	r = drmSyncobjFDToHandle(device_handle->fd, shared_syncobj_fd, &syncobj_handle);
	CU_ASSERT_EQUAL(r, 0);

	s_handle = syncobj_handle;
	bo_handles_array = (uint64_t)&shared_bo->handle;

	num_fences = 0;
	r = amdgpu_userq_wait(device_handle, (uint64_t)&s_handle, 1, bo_handles_array, 1,
			      (uint64_t *)fence_info, &num_fences, AMDGPU_USERQ_BO_WRITE);
	CU_ASSERT_EQUAL(r, 0);

	fence_info = malloc(num_fences * sizeof(struct drm_amdgpu_userq_fence_info));
	r = amdgpu_userq_wait(device_handle, (uint64_t)&s_handle, 1, bo_handles_array, 1,
			      (uint64_t *)fence_info, &num_fences, AMDGPU_USERQ_BO_WRITE);
	CU_ASSERT_EQUAL(r, 0);

	wait_ptr = (uint32_t *)shared_userq_bo.ptr;

	while (!wait_ptr[0]) {
		/*
		 * busy-loop until hardware updates the shared buffer with a specified value
		 * and this should be replaced with WAIT MEM command using address/value pair
		 * retrieved in fence_info.
		 */
		printf("Waiting for hardware write to the shared buffer = %x\n",
			wait_ptr[0]);
	}

	i = 0;
	while (i < 5) {
		printf(" => wait_ptr[%d] = %x\n", i, wait_ptr[i]);
		CU_ASSERT_EQUAL(wait_ptr[i++], 0xdeadbeaf);
	}

	return (void *)(long)r;
}

static void amdgpu_userqueue_synchronize_test(void)
{
	static pthread_t signal_thread, wait_thread;
	int r;

	r = pthread_create(&signal_thread, NULL, userq_signal, NULL);
	CU_ASSERT_EQUAL(r, 0);

	r = pthread_create(&wait_thread, NULL, userq_wait, NULL);
	CU_ASSERT_EQUAL(r, 0);

	r = pthread_join(signal_thread, NULL);
	CU_ASSERT_EQUAL(r, 0);

	r = pthread_join(wait_thread, NULL);
	CU_ASSERT_EQUAL(r, 0);
}
