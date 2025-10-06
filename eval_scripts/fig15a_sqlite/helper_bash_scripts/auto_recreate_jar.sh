#!/bin/bash

lib_name="libsqlitejdbc.so"
dest="org/sqlite/native/Linux/x86_64/${lib_name}"

if [ $# != 1 ] 
then
	echo "Usage: $0 <directory containing $lib_name>"
	exit -1 
fi

lib_path="${1}/${lib_name}"
echo "Searching for library in $lib_path"

if [ ! -f $lib_path ]
then
	echo "Error: $1 doesn't contain the library $lib_name"
	exit -1 
fi
echo "library exists"

rm sqlite-jdbc-3.42.0.0.jar
cp $lib_path $dest
jar cvf sqlite-jdbc-3.42.0.0.jar org META-INF sqlite-jdbc.properties
ls -lah sqlite-jdbc-3.42.0.0.jar


