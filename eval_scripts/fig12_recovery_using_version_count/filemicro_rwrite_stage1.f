#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#
#
# Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"%Z%%M%	%I%	%E% SMI"

# Single threaded asynchronous ($sync) random writes (2KB I/Os) on a 1GB file.
# Stops when 128MB ($bytes) has been written.

set $dir="."
set $nfiles=10000
set $meandirwidth=100000
set $filesize=cvar(type=cvar-gamma,parameters=mean:4194304;gamma:1.5)
set $nthreads=1
set $iosize=4k

define fileset name=bigfileset,path=$dir,size=$filesize,entries=$nfiles,dirwidth=$meandirwidth,prealloc

define process name=filewriter,instances=1
{
  thread name=filewriterthread,memsize=10m,instances=$nthreads
  {
        flowop openfile name=openfile1,filesetname=bigfileset,fd=1
        #flowop write name=write-file,fd=1,random,iosize=$iosize
        flowop closefile name=closefile1,fd=1
  }
}

echo  "FileMicro-WriteRand Version 2.1 personality successfully loaded"
run 10
