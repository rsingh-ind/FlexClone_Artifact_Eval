#!/bin/bash

#Note: Run by changing asynch thread on/off	
if [ "$(id -u)" -ne 0 ]
then
        echo "Error: Run this script ($0) as root user only!"
        exit 1
fi

#
fileSize=("8GB")
fs="8g"
blockSize=("4096" "3072")
#cache=("coldCache" "hotCache")
cache=("coldCache")
#threads=("2thread" "4thread" "8thread" "16thread" "32thread")
threads=("1thread")
coldCacheFilter="coldCache"

function genFioJob {
	local fioJobFile="$1"
	local j="$2"
	local nThreads="$3"
	local childFile="$4"
	local bs="$5"	#block size

	#echo $fioJobFile
	#echo $j
	#echo $nThreads
	#echo "========="
	echo "size=$fs" >> "$fioJobFile"
	echo "ioengine=sync" >> "$fioJobFile"
	: '
	printf "%s" "$fioJobFile" | grep "$coldCacheFilter" >/dev/null
        hotCache=$?
        echo "hotCache: "$hotCache
        if [ "$hotCache" == "1" ];then
		echo "invalidate=0"
		echo "invalidate=0" >> "$fioJobFile"
       	else
		echo "invalidate=1"
		echo "invalidate=1" >> "$fioJobFile"
	fi	
	'
	echo "invalidate=0" >> "$fioJobFile"
	echo "allow_file_create=0" >> "$fioJobFile"
	echo "thread" >> "$fioJobFile"
	echo "numjobs=$nThreads" >> "$fioJobFile"
	#echo "thinktime=1us" >> "$fioJobFile"
	echo "bs=$bs" >> "$fioJobFile"
	echo -n  "filename=$childFile" >> "$fioJobFile"
}


if [ $# != '3' ];then 
	echo -n "Usage: "
	echo "$./script <deprecated (enter something dummy)>  <operation (read, write)>  <threadUsage (for ourExt4 Read/Write): (asynchOn, asynchOff, 0)>"
	exit -1
fi


asynchThread=("$3")
readOp=("seqRead" "randRead")
if [ "$2" == "read" ];then
	for i in "${fileSize[@]}"
	do
		for j in "${readOp[@]}"
		do
			for k in "${cache[@]}"
			do
				for l in "${blockSize[@]}"
				do
					for n in "${threads[@]}"
					do
						for p in "${asynchThread[@]}"
						do
							fioJobFile="${i}_${j}_${k}_${l}bs_${n}_${p}"
							touch "$fioJobFile"
							echo "[$j]" > "$fioJobFile"
							if [ $j == "seqRead" ]; then
								echo "rw=read" >> "$fioJobFile"
							else
								echo "rw=randread" >> "$fioJobFile"
							fi
							nThreads=$(echo $n | egrep -o '^[0-9]+' 2> /dev/null)
							childFile="${i}_child"
							genFioJob $fioJobFile $j $nThreads $childFile $l

						done

					done
				done
			done
		done
	done
fi


asynchThread=("$3")
writeOp=("seqWrite" "randWrite")
if [ "$2" == "write" ];then
	for i in "${fileSize[@]}"
	do
		for j in "${writeOp[@]}"
		do
			for k in "${cache[@]}"
			do
				for l in "${blockSize[@]}"
				do
					for n in "${threads[@]}"
					do
						for p in "${asynchThread[@]}"
						do
							fioJobFile="${i}_${j}_${k}_${l}bs_${n}_${p}"
							touch "$fioJobFile"
							echo "[$j]" > "$fioJobFile"
							if [ $j == "seqWrite" ]; then
								echo "rw=write" >> "$fioJobFile"
							else
								echo "rw=randwrite" >> "$fioJobFile"
							fi
							nThreads=$(echo $n | egrep -o '^[0-9]+' 2> /dev/null)
							childFile="${i}_child"
							genFioJob $fioJobFile $j $nThreads $childFile $l
						done

					done
				done
			done
		done
	done
fi

