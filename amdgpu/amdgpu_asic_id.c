/*
 * Copyright © 2017 Advanced Micro Devices, Inc.
 * All Rights Reserved.
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

#define _XOPEN_SOURCE 700
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64
#include <ftw.h>
#include <link.h>
#include <limits.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "xf86drm.h"
#include "amdgpu_drm.h"
#include "amdgpu_internal.h"

static char *amdgpuids_path = NULL;
static const char* amdgpuids_path_msg = NULL;

static int check_for_location_of_amdgpuids(const char *filepath, const struct stat *info, const int typeflag, struct FTW *pathinfo)
{
       if (typeflag == FTW_F && strstr(filepath, "amdgpu.ids")) {
              amdgpuids_path = strdup(filepath);
              return 1;
       }

       return 0;
}

static int parse_one_line(struct amdgpu_device *dev, const char *line)
{
       char *buf, *saveptr;
       char *s_did;
       uint32_t did;
       char *s_rid;
       uint32_t rid;
       char *s_name;
       char *endptr;
       int r = -EINVAL;

       /* ignore empty line and commented line */
       if (strlen(line) == 0 || line[0] == '#')
              return -EAGAIN;

       buf = strdup(line);
       if (!buf)
              return -ENOMEM;

       /* device id */
       s_did = strtok_r(buf, ",", &saveptr);
       if (!s_did)
              goto out;

       did = strtol(s_did, &endptr, 16);
       if (*endptr)
              goto out;

       if (did != dev->info.asic_id) {
              r = -EAGAIN;
              goto out;
       }

       /* revision id */
       s_rid = strtok_r(NULL, ",", &saveptr);
       if (!s_rid)
              goto out;

       rid = strtol(s_rid, &endptr, 16);
       if (*endptr)
              goto out;

       if (rid != dev->info.pci_rev_id) {
              r = -EAGAIN;
              goto out;
       }

       /* marketing name */
       s_name = strtok_r(NULL, ",", &saveptr);
       if (!s_name)
              goto out;

       /* trim leading whitespaces or tabs */
       while (isblank(*s_name))
              s_name++;
       if (strlen(s_name) == 0)
              goto out;

       dev->marketing_name = strdup(s_name);
       if (dev->marketing_name)
              r = 0;
       else
              r = -ENOMEM;

out:
       free(buf);

       return r;
}

void amdgpu_parse_asic_ids(struct amdgpu_device *dev)
{
       FILE *fp;
       char *line = NULL;
       size_t len = 0;
       ssize_t n;
       int line_num = 1;
       int r = 0;

       // attempt to find typical location for amdgpu.ids file
       fp = fopen(AMDGPU_ASIC_ID_TABLE, "r");

       // if it doesn't exist, search
       if (!fp) {

       char self_path[ PATH_MAX ];
       ssize_t count;
       ssize_t i;

       count = readlink( "/proc/self/exe", self_path, PATH_MAX );
       if (count > 0) {
              self_path[count] = '\0';

              // remove '/bin/python' from self_path
              for (i=count; i>0; --i) {
                     if (self_path[i] == '/') break;
                     self_path[i] = '\0';
              }
              self_path[i] = '\0';
              for (; i>0; --i) {
                     if (self_path[i] == '/') break;
                     self_path[i] = '\0';
              }
              self_path[i] = '\0';

              if (1 == nftw(self_path, check_for_location_of_amdgpuids, 5, FTW_PHYS)) {
                     fp = fopen(amdgpuids_path, "r");
                     amdgpuids_path_msg = amdgpuids_path;
              }
       }

       }
       else {
              amdgpuids_path_msg = AMDGPU_ASIC_ID_TABLE;
       }

       // both hard-coded location and search have failed
       if (!fp) {
              fprintf(stderr, "amdgpu.ids: No such file or directory\n");
              return;
       }

       /* 1st valid line is file version */
       while ((n = getline(&line, &len, fp)) != -1) {
              /* trim trailing newline */
              if (line[n - 1] == '\n')
                     line[n - 1] = '\0';

              /* ignore empty line and commented line */
              if (strlen(line) == 0 || line[0] == '#') {
                     line_num++;
                     continue;
              }

              drmMsg("%s version: %s\n", amdgpuids_path_msg, line);
              break;
       }

       while ((n = getline(&line, &len, fp)) != -1) {
              /* trim trailing newline */
              if (line[n - 1] == '\n')
                     line[n - 1] = '\0';

              r = parse_one_line(dev, line);
              if (r != -EAGAIN)
                     break;

              line_num++;
       }

       if (r == -EINVAL) {
              fprintf(stderr, "Invalid format: %s: line %d: %s\n",
                     amdgpuids_path_msg, line_num, line);
       } else if (r && r != -EAGAIN) {
              fprintf(stderr, "%s: Cannot parse ASIC IDs: %s\n",
                     __func__, strerror(-r));
       }

       free(line);
       fclose(fp);
}
