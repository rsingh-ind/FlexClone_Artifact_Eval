#!/bin/bash

id=$(id -u)
if [ $id != 0 ]
then
	echo "Run as root user"
	exit -1
fi

if [ $# != 1 ]
then
	echo "Usage: $0 <database name. Eg: tpcc/twitter/epinions>"
	exit -1
fi

db_file_full_name="${1}.db"			#Eg: tpcc.db
db_file_untouched="${db_file_full_name}.bak"	#Eg: tpcc.db.bak
config_file="config/sqlite/sample_${1}_config.xml"	#Eg: config/sqlite/sample_tpcc_config.xml

if [ ! -f $db_file_untouched ]
then
	echo "Error: untouched db file '$db_file_untouched' doesn't exists"
	exit -1
fi

rm $db_file_full_name
cp $db_file_untouched $db_file_full_name
echo 3 > /proc/sys/vm/drop_caches
java -jar benchbase.jar -b $1 -c $config_file --execute=true

