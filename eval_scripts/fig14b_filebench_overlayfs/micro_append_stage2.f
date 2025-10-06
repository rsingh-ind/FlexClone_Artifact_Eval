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

set $dir="."
set $meandirwidth=20000000
set $nfiles=256
set $filesize=33554432
set $nthreads=1
set $iosize=1m
set $meanappendsize=16k

####################################################################################
#Rohit:
#       prealloc=80 has been carefully set.
#       createfile/deletefile flowops have also been carefully commented out.
#       Do not modify these changes
###################################################################################
define fileset name=bigfileset,path=$dir,size=$filesize,entries=$nfiles,dirwidth=$meandirwidth,prealloc,reuse,trusttree

define process name=filereader,instances=1
{
  thread name=filereaderthread,memsize=10m,instances=$nthreads
  {
        flowop openfile name=openfile1,filesetname=bigfileset,fd=1
        flowop appendfilerand name=appendfilerand1,iosize=$meanappendsize,fd=1
        flowop finishoncount name=count,value=$nfiles,target=appendfilerand1
        #flowop readwholefile name=readfile1,fd=1,iosize=$iosize
        #flowop finishoncount name=count,value=$nfiles,target=readfile1
        flowop closefile name=closefile2,fd=1
  }
}

echo  "Microbenchmark personality successfully loaded"

run 60
