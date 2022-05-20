/*
 * Copyright 2022 Yusuf Khan.
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
 * Authors: Yusuf Khan
 */

#ifndef __NVIDIA_H__
#define __NVIDIA_H__

#include "nvidia_drm.h"

#include <stdint.h>
#include <stdbool.h>

struct nvidia_get_dev_info_params {
    uint32_t gpu_id;             /* OUT */
    uint32_t primary_index;      /* OUT; the "card%d" value */

    /* See DRM_FORMAT_MOD_NVIDIA_BLOCK_LINEAR_2D definitions of these */
    uint32_t generic_page_kind;    /* OUT */
    uint32_t page_kind_generation; /* OUT */
    uint32_t sector_layout;        /* OUT */
};

struct nvidia_gem_alloc_nvkms_memory_params {
    uint32_t handle;              /* OUT */
    uint8_t  block_linear;        /* IN */
    uint8_t  compressible;        /* IN/OUT */
    uint16_t __pad;

    uint64_t memory_size;         /* IN */
};

int nvidia_fence_supported(int fd);

uint32_t nvidia_fence_context_create(int fd, uint32_t index, uint64_t size,
                                uint64_t import_mem_nvkms_params_ptr,
                                uint64_t import_mem_nvkms_params_size,
                                uint64_t event_nvkms_params_ptr,
                                uint64_t event_nvkms_params_size);

int nvidia_gem_fence_attach(int fd, uint32_t handle, uint32_t fence_context_handle,
                                        uint32_t sem_thresh);

uint32_t nvidia_gem_import_nvkms_memory(int fd,
					uint64_t mem_size,
                                        uint64_t nvkms_params_ptr,
                                        uint64_t nvkms_params_size);

uint32_t nvidia_gem_import_userspace_memory(int fd, uint64_t size, uint64_t address);

int nvidia_get_dev_info_params(int fd, struct nvidia_get_dev_info_params params);

int nvidia_gem_export_nvkms_memory(int fd, uint32_t handle, uint64_t nvkms_params_ptr,
					uint64_t nvkms_params_size);

uint64_t nvidia_get_gem_map_offset(int fd, uint32_t handle);

int nvidia_gem_alloc_nvkms_memory(int fd, struct nvidia_gem_alloc_nvkms_memory_params params);

int nvidia_gem_identify_object(int fd, uint32_t handle, int type);

int nvidia_gem_export_dmabuf_memory(int fd, uint32_t handle, uint64_t nvkms_params_ptr,
                                uint64_t nvkms_params_size);

#endif
