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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "libdrm_macros.h"
#include "xf86drm.h"
#include "amdgpu_drm.h"
#include "amdgpu_internal.h"

drm_public int amdgpu_create_userq_gfx(amdgpu_device_handle dev,
				       struct drm_amdgpu_userq_mqd_gfx *mqd,
				       uint32_t ip_type,
				       uint32_t *queue_id)
{
	union drm_amdgpu_userq userq;
	int r;

	memset(&userq, 0, sizeof(userq));
	userq.in.op = AMDGPU_USERQ_OP_CREATE;
	userq.in.ip_type = ip_type;
	memcpy(&userq.in.mqd.gfx, mqd, sizeof(*mqd));

	r = drmCommandWriteRead(dev->fd, DRM_AMDGPU_USERQ,
				&userq, sizeof(userq));
	*queue_id = userq.out.queue_id;

	return r;
}

drm_public int amdgpu_free_userq_gfx(amdgpu_device_handle dev, uint32_t queue_id)
{
	union drm_amdgpu_userq userq;

	memset(&userq, 0, sizeof(userq));
	userq.in.op = AMDGPU_USERQ_OP_FREE;
	userq.in.queue_id = queue_id;

	return drmCommandWriteRead(dev->fd, DRM_AMDGPU_USERQ,
				   &userq, sizeof(userq));
}
