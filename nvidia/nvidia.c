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

#include <xf86drm.h>
#include <libdrm_macros.h>
#include <stddef.h>
#include "nvidia_drm.h"

#include "nvidia.h"

drm_public
int nvidia_fence_supported(int fd)
{
	int ret =
		drmIoctl(fd, DRM_IOCTL_NVIDIA_FENCE_SUPPORTED, NULL);
	return ret;
}

drm_public
uint32_t nvidia_fence_context_create(int fd, uint32_t index, uint64_t size,
				uint64_t import_mem_nvkms_params_ptr,
				uint64_t import_mem_nvkms_params_size,
				uint64_t event_nvkms_params_ptr,
				uint64_t event_nvkms_params_size)
{
	int ret;

	struct drm_nvidia_fence_context_create_params fence;
	fence.index = index;
	fence.size = size;
	fence.import_mem_nvkms_params_ptr = import_mem_nvkms_params_ptr;
	fence.import_mem_nvkms_params_size = import_mem_nvkms_params_size;
	fence.event_nvkms_params_ptr = event_nvkms_params_ptr;
	fence.event_nvkms_params_size = event_nvkms_params_size;

	ret = drmIoctl(fd, DRM_IOCTL_NVIDIA_FENCE_CONTEXT_CREATE, &fence);

	if (ret == 0) {
		return 0;
	}

	return fence.handle;
}

drm_public
int nvidia_gem_fence_attach(int fd, uint32_t handle, uint32_t fence_context_handle,
					uint32_t sem_thresh)
{
	int ret;
	struct drm_nvidia_gem_fence_attach_params params;

	params.handle = handle;
	params.fence_context_handle = fence_context_handle;
	params.sem_thresh = sem_thresh;

	ret = drmIoctl(fd, DRM_IOCTL_NVIDIA_GEM_FENCE_ATTACH, &params);

	return ret;
}

drm_public
uint32_t nvidia_gem_import_nvkms_memory(int fd,
					uint64_t mem_size,
                                        uint64_t nvkms_params_ptr,
                                        uint64_t nvkms_params_size)
{
	int ret;

	struct drm_nvidia_gem_import_nvkms_memory_params params;

	params.mem_size = mem_size;
	params.nvkms_params_ptr = nvkms_params_ptr;
	params.nvkms_params_size = nvkms_params_size;

	ret = drmIoctl(fd, DRM_IOCTL_NVIDIA_GEM_IMPORT_NVKMS_MEMORY, &params);

	if (ret == 0) {
                return ret;
        }

	return params.handle;
}


drm_public
uint32_t nvidia_gem_import_userspace_memory(int fd, uint64_t size, uint64_t address)
{
	int ret;

	struct drm_nvidia_gem_import_userspace_memory_params params;

	params.size = size;
	params.address = address;

	ret = drmIoctl(fd, DRM_IOCTL_NVIDIA_GEM_IMPORT_USERSPACE_MEMORY, &params);

	if (ret == 0) {
		return ret;
	}

	return params.handle;
}

drm_public
int nvidia_get_dev_info_params(int fd, struct nvidia_get_dev_info_params params) 
{
	int ret = drmIoctl(fd, DRM_IOCTL_NVIDIA_GET_DEV_INFO, &params);

	return ret;
}

drm_public
int nvidia_gem_export_nvkms_memory(int fd, uint32_t handle, uint64_t nvkms_params_ptr,
					uint64_t nvkms_params_size)
{
	struct drm_nvidia_gem_export_nvkms_memory_params params;
	int ret;

	params.handle = handle;
	params.nvkms_params_ptr = nvkms_params_ptr;
	params.nvkms_params_size = nvkms_params_size;

	ret = drmIoctl(fd, DRM_IOCTL_NVIDIA_GEM_EXPORT_NVKMS_MEMORY, &params);

	return ret;
}

drm_public
uint64_t nvidia_get_gem_map_offset(int fd, uint32_t handle)
{
	int ret;
	struct drm_nvidia_gem_map_offset_params params;

	params.handle = handle;

	ret = drmIoctl(fd, DRM_IOCTL_NVIDIA_GEM_MAP_OFFSET, &params);

	if (ret == 0) {
		return ret;
	}

	return params.offset;
}

drm_public
int nvidia_gem_alloc_nvkms_memory(int fd, struct nvidia_gem_alloc_nvkms_memory_params params)
{
	int ret = drmIoctl(fd, DRM_IOCTL_NVIDIA_GEM_ALLOC_NVKMS_MEMORY, &params);

	return ret;
}

drm_public
int nvidia_gem_export_dmabuf_memory(int fd, uint32_t handle, uint64_t nvkms_params_ptr,
				uint64_t nvkms_params_size)
{
	int ret;
	struct drm_nvidia_gem_export_dmabuf_memory_params params;

	params.handle = handle;
	params.nvkms_params_ptr = nvkms_params_ptr;
	params.nvkms_params_size = nvkms_params_size;

	ret = drmIoctl(fd, DRM_IOCTL_NVIDIA_GEM_EXPORT_DMABUF_MEMORY, &params);

	return ret;
}

drm_public
int nvidia_gem_identify_object(int fd, uint32_t handle, int type)
{
	int ret;
	struct drm_nvidia_gem_identify_object_params params;

	params.handle = handle;

	ret = drmIoctl(fd, DRM_IOCTL_NVIDIA_GEM_IDENTIFY_OBJECT, &params);

	type = params.object_type;

	return ret;
}
