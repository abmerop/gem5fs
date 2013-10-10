# -*- mode:python -*-
#
# The license below extends only to copyright in the software and shall
# not be construed as granting a license to any other intellectual
# property including but not limited to intellectual property relating
# to a hardware implementation of the functionality of the software
# licensed hereunder.  You may use the software subject to the license
# terms below provided that you ensure that this notice is replicated
# unmodified and in its entirety in all distributions of the software,
# modified or unmodified, in source code or in binary form.
#
# Copyright (c) 2012-2013, The Microsystems Design Laboratory (MDL)
# Department of Computer Science and Engineering, The Pennsylvania State University
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Authors: Matt Poremba

import os
import sys

from os.path import join as joinpath

#
#  The implementation requires FUSE API version 26, so 
#  these options can supply the path to a newer version
#  for systems with older version of FUSE.
#
AddOption('--with-fuse-include', type="string",
          help='Provide path to custom FUSE headers')
AddOption('--with-fuse-lib', type="string",
          help='Provide path to custom FUSE libraries')
AddOption('--arch', type="string", default="x86",
          help='Architecture to compile with')

#
# Setup the enironment, mirroring some of gem5's variable
# names so we can share the same SConscript between the
# gem5 EXTRAS code and the FUSE standalone build.
#
env = Environment()
env['BUILDROOT'] = "build"      # Name of the output folder for FUSE build
env.srcdir = Dir(".")           
env.Append(CPPPATH=Dir('.'))    # Add cwd to includes..
env['ARCH'] = GetOption("arch")
env['BUILD'] = "gem5fs"         # Set so SConscript can tell the build type
base_dir = env.srcdir.abspath

#
# We need the path to gem5 to find the m5op_ARCH.S files.
#
try:
    env.root = os.environ['M5_PATH']
except KeyError:
    print "M5_PATH is not set. Please set the path to gem5 in your environment variables."
    sys.exit(1)


#
#  Set fuse flags.
#  @TODO: Maybe call pkg-config and replace the -I/-L options somehow instead of
#         hard coding the pkg-config output, incase this changes in the future.
#
if GetOption("with_fuse_include") == None:
    env.ParseConfig('pkg-config --cflags fuse')
else:
    env.Append(CFLAGS=("-I%s -D_FILE_OFFSET_BITS=64" % GetOption("with_fuse_include")))

if GetOption("with_fuse_lib") == None:
    env.ParseConfig('pkg-config --libs fuse')
else:
    env.Append(LINKFLAGS=("-pthread -L%s -lfuse -lrt -ldl" % GetOption("with_fuse_lib")))

#
#  Set the include path for gem5 to find util/m5/m5op_ARCH.S.
#
env.Append(CFLAGS=("-I%s" % env.root))

Export('env')

#
# Define gem5's SConscript Source/DebugFlag function for the FUSE build.
#
def Source(src):
    print "Ignoring gem5 source %s" % src
Export('Source')

def DebugFlag(flg):
    print "Ignoring gem5 debug flag %s" % flg
Export('DebugFlag')

#
# List of sources for the FUSE build and the executable's name.
#
fuse_src_list = []
fuse_prog_name = "gem5fs"

#
# Sources added with FuseSource will not build with gem5, but will
# be built with the FUSE executable
#
def FuseSource(src):
    fuse_src_list.append(File(src))
Export('FuseSource')

#
# Setup our variant directory
#
build_dir = joinpath(base_dir, env['BUILDROOT'])
SConscript('SConscript', variant_dir=build_dir)

#
# It's tool time.
#
env.Program(fuse_prog_name, fuse_src_list)

