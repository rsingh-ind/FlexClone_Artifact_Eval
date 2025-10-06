#!/bin/bash

make -j4 fs/ext4-module modules
if [ $? != 0 ]
then
	exit -1
fi
exit 0
