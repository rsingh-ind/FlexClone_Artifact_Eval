#!/bin/bash

#############################################################################################
#Note: 
#       1) Run this script as root and as a background process i.e. "$./script &"
#
#################################################################################################


benchbase_root="benchbase_benchmark"
benchbase_lib="${benchbase_root}/target/benchbase-sqlite/lib/"
benchbase_main="${benchbase_root}/target/benchbase-sqlite/"		#main directory where workloads are triggered
curdir=$(pwd)
outPath="${curdir}/output/"

dev="ssd"
fs=("ext4" "dcopy")
#fs=("btrfs" "btrfs" "xfs" "xfs")	#We want to run btrfs and xfs twice. One for generating o/p for vanilla sqlite and once for our modified sqlite
fs_sub_case=1				#If fs_sub_case=1, means generating o/p for vanilla sqlite for btrfs and xfs
					#If fs_sub_case=2, means generating o/p for our modified sqlite for btrfs and xfs
workload=("twitter" "tpcc"  "epinions")
#workload=("twitter" "epinions")
module_path=$(../module_info.sh)


if [ "$(id -u)" -ne 0 ]
then
        echo "Error: Run this script ($0) as root user only!"
        exit -1
fi

if [ $# != 1 ]
then
        echo "Usage: $0 <iterations>"
        echo ""
        echo "Iterations indicates number of times the experiment should be rerun before"
        echo "generating the mean and std. dev. results."
        echo ""
        exit -1
fi

#########################################
#Disable sync-mode in FlexClone module
#########################################
function disable_sync_mode ()
{
	MODULE_FILE="${module_path}/fs/ext4-module/corw_sparse.c"
	OLD_LINE='scorw_inode_write_and_wait_range(c_scorw_inode->i_vfs_inode, 0, c_scorw_inode->i_copy_size);'
	NEW_LINE='//scorw_inode_write_and_wait_range(c_scorw_inode->i_vfs_inode, 0, c_scorw_inode->i_copy_size);'

	sed -i "s|$OLD_LINE|$NEW_LINE|" $MODULE_FILE

	$(${module_path}make_ext4_module.sh)
	if [ $? != 0 ]
	then
		echo "Failed to disable sync-mode in FlexClone module"
		exit -1
	fi
}

#########################################
#Enable sync-mode in FlexClone module
#########################################
function enable_sync_mode ()
{
	MODULE_FILE="${module_path}/fs/ext4-module/corw_sparse.c"
	OLD_LINE='//scorw_inode_write_and_wait_range(c_scorw_inode->i_vfs_inode, 0, c_scorw_inode->i_copy_size);'
	NEW_LINE='scorw_inode_write_and_wait_range(c_scorw_inode->i_vfs_inode, 0, c_scorw_inode->i_copy_size);'

	sed -i "s|$OLD_LINE|$NEW_LINE|" $MODULE_FILE

	$(${module_path}make_ext4_module.sh)
	if [ $? != 0 ]
	then
		echo "Failed to enable sync-mode in FlexClone module"
		disable_sync_mode
		exit -1
	fi
}

enable_sync_mode
exit

for i in ${fs[@]}
do
	cd $curdir

        #mount point
        mount=$(../mount_info.sh $i $dev 0)
	if [ "$mount" == "/" ]; then
		echo "Error: $0 Aborting..Somehow mount point is '/' instead of the desired mount point"
		disable_sync_mode
		exit -1
	fi
        dest="${mount}/"

	#unmount
	mountpoint -q "$mount"
	status=$?
	if [ $status -eq 0 ]; then
	    umount "$mount"
	    umount_status=$?
	    if [ $umount_status -ne 0 ]; then
		echo "Error: $0 unmounting $mount failed..skipping this device.."
		disable_sync_mode
		exit -1
	    fi
	fi

	#fresh mount
        mount_cmd=$(../mount_info.sh $i $dev 1)
        $($mount_cmd)
	if [ $? != 0 ]
	then
		echo "Error: $0 mounting failed..skipping this device.."
		disable_sync_mode
		exit -1
	fi

        #cleanup old setup
        rm -r "${mount}/"*

        #copy relevant files to target filesystem
        echo "copying benchbase to $i filesystem"
	cp -r $benchbase_root $dest

	if [ $i == "dcopy" ]
	then
		echo "copying zip file containing sqlite library to $i filesystem"
		cp "sqlite_libraries/modified/sqlite-jdbc-3.42.0.0.jar" "${dest}/${benchbase_lib}"
	elif [ $i == "ext4" ]
	then
		echo "copying zip file containing sqlite library to $i filesystem"
		cp "sqlite_libraries/orig/sqlite-jdbc-3.42.0.0.jar" "${dest}/${benchbase_lib}"
	elif [ $i == "btrfs" ]
	then
		if [ $fs_sub_case == 1 ]
		then
			echo "copying zip file containing VANILLA sqlite library to $i filesystem"
			cp "sqlite_libraries/orig/sqlite-jdbc-3.42.0.0.jar" "${dest}/${benchbase_lib}"
		else
			echo "copying zip file containing MODIFIED sqlite library to $i filesystem"
			cp "sqlite_libraries/btrfs_xfs/sqlite-jdbc-3.42.0.0.jar" "${dest}/${benchbase_lib}"
		fi
	elif [ $i == "xfs" ]
	then
		if [ $fs_sub_case == 1 ]
		then
			echo "copying zip file containing VANILLA sqlite library to $i filesystem"
			cp "sqlite_libraries/orig/sqlite-jdbc-3.42.0.0.jar" "${dest}/${benchbase_lib}"
		else
			echo "copying zip file containing MODIFIED sqlite library to $i filesystem"
			cp "sqlite_libraries/btrfs_xfs/sqlite-jdbc-3.42.0.0.jar" "${dest}/${benchbase_lib}"
		fi
	else
		echo "Error: Invalid filesystem: $i"
		disable_sync_mode
		exit -1
	fi

        for wl in ${workload[@]}
        do
		end=$1
		for ((k=0; k<end; k++))
		do
			cd $curdir

			#prepare filesystem
			echo "Remounting $i filesystem"
			mountpoint -q "$mount"
			status=$?
			if [ $status -eq 0 ]; then
			    umount "$mount"
			    umount_status=$?
			    if [ $umount_status -ne 0 ]; then
				echo "Error: $0 unmounting $mount failed..skipping this device.."
				disable_sync_mode
				exit -1
			    fi
			fi

			if [ $i == "dcopy" ]
			then
				echo "Re-inserting ext4-module"
				cd $module_path
				sudo ./remove_ext4_module.sh
				sudo ./insert_ext4_module.sh
				cd $curdir
			fi
			mount_cmd=$(../mount_info.sh $i $dev 1)
			$($mount_cmd)
			if [ $? != 0 ]
			then
				echo "Error: $0 mounting failed..skipping this device.."
				disable_sync_mode
				exit -1
			fi

			dest="${mount}/${benchbase_main}"
			cd $dest

			db_file="${wl}.db"
			db_file_bak="${wl}.db.bak"
			config_file="config/sqlite/sample_${wl}_config.xml"

			rm $db_file
			cp $db_file_bak $db_file
			sync; echo 3 > /proc/sys/vm/drop_caches
	
			#Trigger benchbase on workload
			echo "Triggering benchbase on workload: ${wl}"
			if [ $i == "btrfs" ]
			then
				if [ $fs_sub_case == 1 ]
				then
					java -jar benchbase.jar -b $wl -c $config_file --execute=true >> "${outPath}/out_${i}_${wl}_vanillaSqlite"
					if [ $? != 0 ]
					then
						echo "Error: Failed to run benchbase.."
					fi
					echo "======================== $i $wl $k VANILLA SQLite========================" >> "${outPath}/out_${i}_${wl}_vanillaSqlite"
				elif [ $fs_sub_case == 2 ]
				then
					java -jar benchbase.jar -b $wl -c $config_file --execute=true >> "${outPath}/out_${i}_${wl}_modifiedSqlite"
					if [ $? != 0 ]
					then
						echo "Error: Failed to run benchbase.."
					fi
					echo "======================== $i $wl $k MODIFIED SQLite========================" >> "${outPath}/out_${i}_${wl}_modifiedSqlite"
				fi
			elif [ $i == "xfs" ]
			then
				if [ $fs_sub_case == 1 ]
				then
					java -jar benchbase.jar -b $wl -c $config_file --execute=true >> "${outPath}/out_${i}_${wl}_vanillaSqlite"
					if [ $? != 0 ]
					then
						echo "Error: Failed to run benchbase.."
					fi
					echo "======================== $i $wl $k VANILLA SQLite========================" >> "${outPath}/out_${i}_${wl}_vanillaSqlite"
				elif [ $fs_sub_case == 2 ]
				then
					java -jar benchbase.jar -b $wl -c $config_file --execute=true >> "${outPath}/out_${i}_${wl}_modifiedSqlite"
					if [ $? != 0 ]
					then
						echo "Error: Failed to run benchbase.."
					fi
					echo "======================== $i $wl $k MODIFIED SQLite========================" >> "${outPath}/out_${i}_${wl}_modifiedSqlite"
				fi
			else
				java -jar benchbase.jar -b $wl -c $config_file --execute=true >> "${outPath}/out_${i}_${wl}"
				if [ $? != 0 ]
				then
					echo "Error: Failed to run benchbase.."
				fi
				echo "======================== $i $wl $k ========================" >> "${outPath}/out_${i}_${wl}"
			fi

			echo "======================== $i $wl $k ========================"
			echo ""
			echo ""
			cd $curdir
		done
	done
	#unmount
	mountpoint -q "$mount"
	status=$?
	if [ $status -eq 0 ]; then
	    umount "$mount"
	    umount_status=$?
	    if [ $umount_status -ne 0 ]; then
		echo "Error: $0 unmounting $mount failed..skipping this device.."
		disable_sync_mode
		exit -1
	    fi
	fi

	if [ $i == "btrfs" ]
	then
		if [ $fs_sub_case == 1 ]
		then
			fs_sub_case=2
		else
			fs_sub_case=1
		fi
	elif [ $i == "xfs" ]
	then
		if [ $fs_sub_case == 1 ]
		then
			fs_sub_case=2
		else
			fs_sub_case=1
		fi
	else
		#do nothing
		echo ""
	fi
done

cd output
./gen_output.sh
gnuplot plot_sqlite_commit.p
epstopdf sqlite-commit-flexsnap.eps
cp sqlite-commit-flexsnap.pdf ..
disable_sync_mode
exit 0

