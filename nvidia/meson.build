# Copyright © 2017-2018 Intel Corporation

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.


libdrm_nvidia = library(
  'drm_nvidia',
  [files( 'nvidia.c'), config_file],
  c_args : libdrm_c_args,
  gnu_symbol_visibility : 'hidden',
  include_directories : [inc_root, inc_drm],
  link_with : libdrm,
  dependencies : [dep_threads, dep_atomic_ops],
  version : '2.0.0',
  install : true,
)

ext_libdrm_nvidia = declare_dependency(
  link_with : [libdrm, libdrm_nvidia],
  include_directories : [inc_drm, include_directories('.')],
)

if meson.version().version_compare('>= 0.54.0')
  meson.override_dependency('libdrm_nvidia', ext_libdrm_nvidia)
endif

install_headers('nvidia.h', subdir : 'libdrm/nvidia')

pkg.generate(
  libdrm_nvidia,
  name : 'libdrm_nvidia',
  subdirs : ['.', 'libdrm', 'libdrm/nvidia'],
  description : 'Userspace interface to nvidia kernel DRM services',
)

test(
  'nvidia-symbols-check',
  symbols_check,
  args : [
    '--lib', libdrm_nvidia,
    '--symbols-file', files('nvidia-symbols.txt'),
    '--nm', prog_nm.path(),
  ],
)
