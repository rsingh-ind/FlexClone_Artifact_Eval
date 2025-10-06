#!/bin/bash

effective_uid=$(id -u)
if [ $effective_uid != 0 ]
then
	echo "Run me as root user"
	exit -1
fi

top_dir_name_wihtout_path=${PWD##*/}
if [ $top_dir_name_wihtout_path != "sqlite-jdbc" ]
then
	echo "Run this script inside 'sqlite-jdbc' directory"
	exit -1
fi


if [ $# != 1 ]
then
	echo "Usage: $0 <type of sqlite: 'orig' (or) 'modified' (or) 'Btrfs' (or) 'XFS' ?>"
        exit -1
fi

cur_dir=$(pwd)
sqlite_path="${cur_dir}/target/tmp/${1}_sqlite"

if [ ! -d $sqlite_path ]
then
        echo "Error: Invalid path '$sqlite_path'. Make sure '$1' is correct argument."
        exit -1
fi
echo "valid argument: $1"

cd "target/tmp/${1}_sqlite/sqlite-amalgamation-34600"
rm -r *

if [ $1 == "Btrfs" ]
then
	scp -r "rohit@172.27.21.216:/oldhome/rohit/ssdBtrfs/sqlite/${1}_sqlite/build/"* .
elif [ $1 == "XFS" ]
then
	scp -r "rohit@172.27.21.216:/oldhome/rohit/ssdXFS/sqlite/${1}_sqlite/build/"* .
elif [ $1 == "modified" ]
then
	scp -r "rohit@172.27.21.216:/oldhome/rohit/ssdOurExt4/sqlite/${1}_sqlite/build/"* .
else
	scp -r "rohit@172.27.21.216:/oldhome/rohit/ssdOrigExt4/sqlite/${1}_sqlite/build/"* .
fi


cd ..

rm sqlite-3.46-amal.zip
zip -r sqlite-3.46-amal.zip sqlite-amalgamation-34600/
cd ../..

rm sqlite-3.46-amal.zip
rm -r sqlite-3.46-Linux-x86_64
rm -r sqlite-amalgamation-34600
rm sqlite-unpack.log
cp "tmp/${1}_sqlite/sqlite-3.46-amal.zip" .
cd ..

export JAVA_HOME=/usr/lib/jvm/jdk-21-oracle-x64
make

if [ $1 == "Btrfs" ]
then
	scp ./src/main/resources/org/sqlite/native/Linux/x86_64/libsqlitejdbc.so rohit@172.27.21.216:"/oldhome/rohit/ssdBtrfs/benchbase_benchmark/target/benchbase-sqlite/lib/Btrfs_sqlite_lib"
elif [ $1 == "XFS" ]
then
	scp ./src/main/resources/org/sqlite/native/Linux/x86_64/libsqlitejdbc.so rohit@172.27.21.216:"/oldhome/rohit/ssdXFS/benchbase_benchmark/target/benchbase-sqlite/lib/XFS_sqlite_lib"
elif [ $1 == "modified" ]
then
	scp ./src/main/resources/org/sqlite/native/Linux/x86_64/libsqlitejdbc.so rohit@172.27.21.216:"/oldhome/rohit/ssdOurExt4/benchbase_benchmark/target/benchbase-sqlite/lib/modified_sqlite_lib"
else
	scp ./src/main/resources/org/sqlite/native/Linux/x86_64/libsqlitejdbc.so rohit@172.27.21.216:"/oldhome/rohit/ssdOrigExt4/benchbase_benchmark/target/benchbase-sqlite/lib/orig_sqlite_lib"
fi
