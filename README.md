
# FlexClone

This repository contains the artifact for the paper titled **"FlexClone: Efficient, Flexible and Pluggable File Cloning Support for Filesystems"**, to appear at the 26th ACM/IFIP International Middleware Conference (Middleware 2025)

We provide the implementation of FlexClone in the Linux kernel, along with relevant evaluation scripts and benchmarks, in this artifact, which can be used to reproduce the results from our paper. 

---
## Table of contents

- [Artifact Directory Structure](#artifact-directory-structure)
- [Two Approaches to Test Artifact](#two-approaches-to-test-artifact)
	- [Setup used for evaluation](#setup-used-for-evaluation)
	- [Compiling kernel from scratch](#approach-1-compiling-kernel-from-scratch)
	- [Using remote access to author's server](#approach-2-using-remote-access-to-authors-server)
- [Getting Started](#getting-started)	
- [Generating Figures and Tables](#generating-figures-and-tables)
	- [Figures](#figures)
	- [Tables](#tables)

---
## Artifact Directory Structure
```
FlexClone_Artifact_Eval
	|---linux_5.5.10	--- Contains our modified Linux Kernel source code
	|---getting_started --- Contains scripts to play around with FlexClone
	|---eval_scripts	--- Contains scripts for generating figures and tables
	|---README.md 		--- This document that you are reading
	|---auto_setup.sh	--- Partitions SSD and lays out filesystems
	|---vm_experiments_scripts --- Contains workload scripts for experiments requiring VM cloning

```

---
## Two Approaches to Test Artifact
In this section, we first discuss the setup used for evaluation reported in the paper and then specify the two approaches that can be used to evaluate this artifact.

---
### Setup used for evaluation

- We ran all experiments on a server with:
	- 32-core Intel(R) Xeon(R) CPU E5-2620 v4 CPU
	- 32GB DDR4 Synchronous 2400 MHz DRAM (Micron 36ASF4G72PZ-2G3B1)
	- Samsung 970 EVO Plus 512GiB NVMe-SSD
	- Ubuntu 18.04.3 LTS distribution with Linux kernel version 5.5.10.

---
### Approach 1: Compiling kernel from scratch

- *Prerequisites*
	- Minimum 60GB of free space in the root partition for kernel compilation and installation
	- Separate SSD, preferably, of size >=512GB.
	- FlexClone has been tested on Ubuntu 24.04.3 LTS. Please install this Ubuntu version.

- *Installing Linux kernel and required dependencies*
	- Run `auto_install.sh` script present inside `~/FlexClone_Artifact_Eval/linux_5.5.10` as a root user to automatically install the required dependencies and the Linux kernel containing FlexClone implementation.
```
	~/FlexClone_Artifact_Eval/linux_5.5.10# ./auto_install.sh
```
- 	- Reboot system after successful kernel installation
```
	~/FlexClone_Artifact_Eval/linux_5.5.10# reboot now
```
-	- After reboot, verify that that system is using Linux kernel 5.5.10. Please ignore character `9` at the start of the output.
```
	# uname -r
	95.5.10FlexClone
```
- *Creating partitions on SSD with filesystems*

	-  Run `auto_setup.sh` script present inside `FlexClone_Artifact_Eval/` directory that automatically creates partitions on SSD and lays out filesystems on these partitions
	- Note: This script sleeps internally while partitioning the disk.  No user input is required.
```
	FlexClone_Artifact_Eval/# ./auto_setup.sh <disk> <disk size in GB>
```
```
	//Eg: If 512GB SSD is attached as /dev/nvme0n1, then, run script as following:  
	//
	FlexClone_Artifact_Eval/ # ./auto_setup.sh /dev/nvme0n1 512
```

-	- Setup is done. Can move on to [Getting Started](#getting-started) or [Generating Figures and Tables](#generating-figures-and-tables) sections.

---
### Approach 2: Using remote access to author's server
- Login details are available upon request


---
## Getting Started
- This section aims to introduce the functionality of FlexClone using basic examples. Please refer to the [Generating Figures and Tables](#generating-figures-and-tables) section for reproducing experiments presented in the paper.
- This section dicusses the following:
	- [Preparing setup](#preparing-setup)
	- [Creating clones](#creating-clones)
	- [Printing parent-child relationship details](#printing-parent-child-relationship-details) 
	- [CoW and See-through functionality](#cow-and-see-through-functionality)
	- [Open-share functionality](#open-share-functionality)
	- [Some Limitations](#some-limitations)
- Note: Run all commands as root to avoid permission errors

---
### Preparing setup

*Step 1*

- Run the driver script (`auto_setup.sh`) present inside `FlexClone_Artifact_Eval/getting_started` directory to prepare the basic setup.
```
FlexClone_Artifact_Eval/getting_started # ./auto_setup
```
- This script copies the relevant helper scripts to the mount point and creates a 1MB file filled with random ASCII characters that can be used as a parent file in subsequent operations. 

---
*Step 2*

- Change current working directory to the mount point where Ext4 integrated with FlexClone is mounted.
```
FlexClone_Artifact_Eval/getting_started # cd /flexclone
```
- Inside mount point, the following binaries and scripts can be found:  `setxattr_generic`, `write`, `auto_clone.sh` and `auto_clean.sh`.

(Jump to: [Getting Started](#getting-started))

---
### Creating clones

- `setxattr_generic` utility can be used to create clone of a file.
- This utility takes four arguments:
	- name of parent file (-p par_name)
	- name of child file (-c child_name)
	- name of friend file (-f friend_name)
	- range of blocks and their clone behaviour i.e. CoW/See-through/Open-share (-r range) 
- Let's create two clones of `par` file
```
//create child file named "c1" and friend file named "f1" corresponding parent file "par" 
//No clone bheavior is specified, so, by default all blocks are cloned with CoW behaviour
//
/flexclone# ./setxattr_generic -c c1 -p par -f f1 

//create another clone with blocks 0 to 9 configured in see-through mode
//
/flexclone#./setxattr_generic -c c2 -p par -f f2 -r 0:9:3
```
- Note: 
	- `auto_clone.sh` script present within the mount point automatically performs the above specified clone operations.
	- `auto_clean.sh` script deletes the child and friend files
	
(Jump to: [Getting Started](#getting-started))

---
### Printing parent-child relationship details 
- FlexClone stores cloning metadata in the extended attributes of par, child, and friend files.
- `xattr` utility can be used to print the extended attributes of these files.
- Let us print the extended attributes stored in the par file
```
/flexclone# xattr -l par

Sample Output:
==============
user.c_0:
0000   0F 00 00 00 00 00 00 00                            ........

user.c_1:
0000   11 00 00 00 00 00 00 00                            ........
```
- In the above output, the parent has two key-value pairs.
	- `c_0` is the key corresponding to the first child, and `0F` is the inode number of the child file (in hexadecimal).
 	- `c_1` is the key corresponding to the second child, and `11` is the inode number of the child file (in hexadecimal).
 	- Note: Inode numbers of child files `c1` and `c2`can be found using `# ls -i c1` and `# ls -i c2` commands
 
 - Let us print the extended attributes stored in the first child file, `c1`
```
/flexclone# xattr -l c1

Sample Output:
==============
user.COPY_SIZE:
0000   00 00 10 00 00 00 00 00                            ........

user.BLOCKS_TO_COPY:
0000   00 01 00 00                                        ....

user.v:
0000   01 00 00 00 00 00 00 00                            ........

user.NUM_RANGES:
0000   00 00 00 00                                        ....

user.SCORW_FRIEND:
0000   12 00 00 00 00 00 00 00                            ........

user.SCORW_PARENT:
0000   0E 00 00 00 00 00 00 00                            ........

```
- In the above output, the child has multiple key-value pairs.
	- `COPY_SIZE` is the size of the parent file at the clone time (hexadecimal, little-endian)
	
		(00 00 10 00 00 00 00 00  --> 00 10 00 00 (Correct order, hexadecimal) --> 1048576 (decimal) --> 2^20 i.e. 1MB)
	
	- `v` is the version number for consistency
	- `SCORW_FRIEND` is the friend file's inode number
	- `SCORW_PARENT` is the parent file's inode number 

- Note:  `xattr` sometimes fails to parse extended attributes upon encountering some special characters. This does not imply that extended attributes are incorrect or not set.

(Jump to: [Getting Started](#getting-started))

---
### CoW and See-through functionality
- Note: 
	- Utilities like `head`, `cat`, or editors like `Vim` can be used to read parent and child files.
	- However, *do not modify* the parent/child files using editors such as Vim. When modifying the contents of a file, Vim actually writes the new contents to a new file, then deletes the original file, and renames the new file to match the original file's name. The manner in which Vim tries to preserve the extended attributes of the deleted file is currently incompatible with FlexClone. As a result, the newly written file will no longer have child status. We have left fixing this incompatibility as future work.
	- We have provided a custom utility, named `write`, that can be used to modify the contents of a file. This utility writes random data to specified file blocks.
	- Usage: `#./write <file to modify> <byte offset to start writing from> <length of write>`
	- Eg: `#./write c1 0 100`	//write 100 bytes of data from byte 0 (Block 0) onwards to file c1 


- *Reading child and parent files*
	- `/flexclone# vim c1`		//Reading child file c1
	- `/flexclone# vim c2`		//Reading child file c2
	- `/flexclone# vim par`		//Reading parent file par
	- Contents of all files should match because no modification has taken place yet 

- *Modifying child file*
	-  `/flexclone# ./write c1 0 100` //write 100 bytes of data from byte 0 onwards to file c1 
	- `/flexclone# vim c1`	//Reading child file c1
	- `/flexclone# vim par`	//Reading parent file par
	- First 100 bytes of c1 will differ from parent file due to CoW

- *Modifying par file*
	 -  `/flexclone# ./write par 4096 100` //write 100 bytes of data from byte 4096 (Block 1) onwards to parent file
	- `/flexclone# head -c 4110 par | tail -c 14`	//Read first 14 bytes of Block 1 of parent 
	- `/flexclone# head -c 4110 c1 | tail -c 14`	//Read first 14 bytes of Block 1 of c1
	- `/flexclone# head -c 4110 c2 | tail -c 14`	//Read first 14 bytes of Block 1 of c2
	- Parents' data is modified. 
	- Child c1 contains the original block contents due to CoW operation on write to parent.
	- Child c2 contains the same contents as the parent file because it is configured in See-through mode.  
	
(Jump to: [Getting Started](#getting-started))


---
### Open-share functionality
- *Cleanup existing child files*
```
/flexclone# ./auto_clean.sh
```

- *Create new child file in Open-share configuration*
```
//create child `c1` with blocks 0 to 9 configured in open-share mode
//
/flexclone# ./setxattr_generic -c c1 -p par -f f1 -r 0:9:4
```

- *Reading child and parent files*
	- `/flexclone# vim c1`		//Reading child file c1
	- `/flexclone# vim par`		//Reading parent file par
	- Contents of both files should match because no modification has taken place yet 

- *Modifying child file*
	-  `/flexclone# ./write c1 0 100` //write 100 bytes of data from byte 0 onwards to file c1 
	- `/flexclone# vim c1`	//Reading child file c1
	- `/flexclone# vim par`	//Reading parent file par
	- First 100 bytes of c1 have been modified
	- First 100 bytes of parent are the same as c1 because both child and parent
	  can modify a shared block in Open-share mode.

(Jump to: [Getting Started](#getting-started))

---
### Some Limitations
- The following functionalities are yet to be added in the prototype implementation:
	- Parent file should only be deleted after the shared parent blocks have been copied to the target child files
	- Truncation, Direct-IO and mmap() support is not yet implemented

- We have tried our best to test the implementation thoroughly; however, some bugs might still exist. **In the event of a bug/kernel crash during evaluation, please restart the system/VM**.
	
(Jump to: [Getting Started](#getting-started))

---
## Generating Figures and Tables
- Two driver scripts (`main_fig.sh` and `main_tab.sh`) present inside the `FlexClone_Artifact_Eval/eval_scripts` directory can be used to trigger the evaluation corresponding to various [figures](#figures) and [tables](#tables) presented in the paper.
- Note: 
	- The terms `ourExt4`, `flexclone`, `scorw`, and `dcopy` within scripts and source code refer to FlexClone itself.
	- The term `source file` used in this README implies parent file/base file.

---
### Figures
---
- `main_fig.sh` is the master script present inside `FlexClone_Artifact_Eval/eval_scripts` directory that can be used to trigger the evaluation corresponding to various figures presented in the paper.
- `main_fig.sh` takes two command-line arguments: 
	1. figure number 
	2. number of iterations (how many times to perform the experiment)
- Example: 
```
 		# ./main_fig.sh 8 10 		//Run experiment corresponding figure 8 ten times
        # ./main_fig.sh 13a 1  		//Run experiment corresponding figure 13a only once
```
- Note: Run `main_fig.sh` as root user, as shown in the above example 
- A summary of each figure's evaluation process is provided below.
	- Figures using microbenchmarks and macrobenchmarks, such as Fio, Filebench, SQLite, etc., workloads.
		- [Figure 8](#figure-8)
		- [Figure 9a](#figure-9a)
		- [Figure 9b](#figure-9b)
		- [Figure 10a](#figure-10a)
		- [Figure 10b](#figure-10b)
		- [Figure 11](#figure-11)
		- [Figure 12](#figure-12)
		- [Figure 13a](#figure-13a)
		- [Figure 13b](#figure-13b)
		- [Figure 14b](#figure-14b)
		- [Figure 15a](#figure-15a)
		- [Figure 15b](#figure-15b)
	- Following figures where workloads are run inside a VM (i.e., cloning VM image and running workload inside VM) require a VM to be setup before running each experiment:
		- [Figure 1](#figure-1)
		- [Figure 14a](#figure-14a)
		- [Figure 16a and 16b](#figure-16a-and-16b)

(Jump to: [Figures](#figures), [Tables](#tables))

---
### Figure 8

*Experiment Goal:*
```
Observe the time taken to clone files of various sizes (4KB to 16GB) on different filesystems.
```

*Command:*
```
FlexClone_Artifact_Eval/eval_scripts# ./main_fig.sh 8 1 	//Run single iteration of the experiment
```
*Estimated Runtime:*
```
30 mins of total time for the single iteration of the experiment (generation of source files for each filesystem takes time)
```

*Script actions:*
```
- For Ext4, XFS, Btrfs, FlexClone, Qcow2(on Ext4)
	- Mount filesystem
	- Copy helper scripts into the mount point
	- Generate files to act as the source files
	- Perform clone operation for each source file
```
*Results:*
```
- Results and corresponding plot are stored in "FlexClone_Artifact_Eval/eval_scripts/fig8_clone_time" directory
- Log file for the experiment is stored in "FlexClone_Artifact_Eval/eval_scripts/fig8_clone_time" directory with experiment timestamp being the name of the log file (Eg: log_20250926_181210).
```
(Jump to: [Figures](#figures), [Tables](#tables))

---
### Figure 9a

*Experiment Goal:*
```
Observe the read and write throughput corresponding the clone files on different filesystems
for block aligned I/O size (4KiB).
```

*Command:*
```
FlexClone_Artifact_Eval/eval_scripts# ./main_fig.sh 9a 1 	//Run single iteration of the experiment
```
*Estimated Runtime:*
```
20 mins of total time for the single iteration of the experiment
```

*Script actions:*
```
- For Ext4, XFS, Btrfs, FlexClone
	- Mount filesystem
	- Copy helper scripts into the mount point
	- Generate source file
	- Perform clone operation for source file
	- Perform read/write operation on the clone file using FIO benchmark
```
*Results:*
```
- Results and corresponding plot are stored in "FlexClone_Artifact_Eval/eval_scripts/fig9a_throughput_block_aligned/fio_output/ssd" directory
- For ease of access, plot is copied to "FlexClone_Artifact_Eval/eval_scripts/fig9a_throughput_block_aligned" directory
- Log file for the experiment is stored in  "FlexClone_Artifact_Eval/eval_scripts/fig9a_throughput_block_aligned" directory with experiment timestamp being the name of the log file (Eg: log_20250926_181210).
```
(Jump to: [Figures](#figures), [Tables](#tables))

---
### Figure 9b

*Experiment Goal:*
```
Observe the read and write throughput corresponding the clone files on different filesystems
for non-block aligned I/O size (3KiB).
```

*Command:*
```
FlexClone_Artifact_Eval/eval_scripts# ./main_fig.sh 9b 1 	//Run single iteration of the experiment
```
*Estimated Runtime:*
```
30 mins of total time for the single iteration of the experiment
```

*Script actions:*
```
- For Ext4, XFS, Btrfs, FlexClone
	- Mount filesystem
	- Copy helper scripts into the mount point
	- Generate source file
	- Perform clone operation for source file
	- Perform read/write operation on the clone file using FIO benchmark
```
*Results:*
```
- Results and corresponding plot are stored in "FlexClone_Artifact_Eval/eval_scripts/fig9b_throughput_non_block_aligned/fio_output/ssd" directory
- For ease of access, plot is copied to "FlexClone_Artifact_Eval/eval_scripts/fig9b_throughput_non_block_aligned" directory
- Log file for the experiment is stored in  "FlexClone_Artifact_Eval/eval_scripts/fig9b_throughput_non_block_aligned" directory with experiment timestamp being the name of the log file (Eg: log_20250926_181210).
```
(Jump to: [Figures](#figures), [Tables](#tables))


---
### Figure 10a

*Experiment Goal:*
```
Observe the scalability while performing multithreaded random read operation corresponding the clone files on different filesystems for block aligned I/O size (4KiB).
```

*Command:*
```
FlexClone_Artifact_Eval/eval_scripts# ./main_fig.sh 10a 1 	//Run single iteration of the experiment
```
*Estimated Runtime:*
```
30 mins of total time for the single iteration of the experiment
```

*Script actions:*
```
- For Ext4, XFS, Btrfs, FlexClone
	- Mount filesystem
	- Copy helper scripts into the mount point
	- Generate source file
	- Perform clone operation for source file
	- Perform rand-read operation on the clone file using FIO benchmark 
	  while varying the number of benchmark threads
```
*Results:*
```
- Results and corresponding plot are stored in "FlexClone_Artifact_Eval/eval_scripts/fig10a_multihreaded_randRead/fio_output/ssd" directory
- For ease of access, plot is copied to "FlexClone_Artifact_Eval/eval_scripts/fig10a_multihreaded_randRead" directory
- Log file for the experiment is stored in  "FlexClone_Artifact_Eval/eval_scripts/fig10a_multihreaded_randRead" directory with experiment timestamp being the name of the log file (Eg: log_20250926_181210).
```
(Jump to: [Figures](#figures), [Tables](#tables))


---
### Figure 10b

*Experiment Goal:*
```
Observe the scalability while performing multithreaded random write operation corresponding the clone files on different filesystems for block aligned I/O size (4KiB).
```

*Command:*
```
FlexClone_Artifact_Eval/eval_scripts# ./main_fig.sh 10b 1 	//Run single iteration of the experiment
```
*Estimated Runtime:*
```
15 mins of total time for the single iteration of the experiment
```

*Script actions:*
```
- For Ext4, XFS, Btrfs, FlexClone
	- Mount filesystem
	- Copy helper scripts into the mount point
	- Generate source file
	- Perform clone operation for source file
	- Perform rand-write operation on the clone file using FIO benchmark 
	  while varying the number of benchmark threads
```
*Results:*
```
- Results and corresponding plot are stored in "FlexClone_Artifact_Eval/eval_scripts/fig10b_multihreaded_randWrite/fio_output/ssd" directory
- For ease of access, plot is copied to "FlexClone_Artifact_Eval/eval_scripts/fig10b_multihreaded_randWrite" directory
- Log file for the experiment is stored in  "FlexClone_Artifact_Eval/eval_scripts/fig10b_multihreaded_randWrite" directory with experiment timestamp being the name of the log file (Eg: log_20250926_181210).
```
(Jump to: [Figures](#figures), [Tables](#tables))


---
### Figure 11

*Experiment Goal:*
```
Observe the write to parent file performance for different filesystems for block aligned I/O size (4KiB).
Note: Ext4-FC-No-DP configuration is not evaluated because it requires setup of a separate version of FlexClone.
```

*Command:*
```
FlexClone_Artifact_Eval/eval_scripts# ./main_fig.sh 11 1 	//Run single iteration of the experiment
```
*Estimated Runtime:*
```
20 mins of total time for the single iteration of the experiment
```

*Script actions:*
```
- For Ext4, XFS, Btrfs, FlexClone
	- Mount filesystem
	- Copy helper scripts into the mount point
	- Generate source file
	- Perform clone operation for creating varying number of clones of source file (1, 2, 4, 8)
	- Perform sequential write operation on the parent file using FIO benchmark 
```
*Results:*
```
- Results and corresponding plot are stored in "FlexClone_Artifact_Eval/eval_scripts/fig11_write_to_par/fio_output/ssd" directory
- For ease of access, plot is copied to "FlexClone_Artifact_Eval/eval_scripts/fig11_write_to_par" directory
- Log file for the experiment is stored in  "FlexClone_Artifact_Eval/eval_scripts/fig11_write_to_par" directory with experiment timestamp being the name of the log file (Eg: log_20250926_181210).
```
(Jump to: [Figures](#figures), [Tables](#tables))

---
### Figure 12

*Experiment Goal:*
```
Observe the time taken to recover friend files
```
*Command:*
```
FlexClone_Artifact_Eval/eval_scripts# ./main_fig.sh 12 1 	//Run single iteration of the experiment
```
*Estimated Runtime:*
```
60 mins of total time for the single iteration of the experiment
```

*Script actions:*
```
- For FlexClone
	- Mount filesystem
	- Copy helper scripts into the mount point
	- Generate 10,000 source files
	- Perform clone operation for all source files
	- For each child file, fill  amount of data equal to the 50% of the size of child file 
	- Partially corrupt version count in friend files
	- Perform recovery 
```
*Results:*
```
- Results and corresponding plot are stored in "FlexClone_Artifact_Eval/eval_scripts/fig12_recovery_using_version_count/fio_output/ssd" directory
- For ease of access, plot is copied to "FlexClone_Artifact_Eval/eval_scripts/fig12_recovery_using_version_count" directory
- Log file for the experiment is stored in  "FlexClone_Artifact_Eval/eval_scripts/fig12_recovery_using_version_count" directory with experiment timestamp being the name of the log file (Eg: log_20250926_181210).
```
(Jump to: [Figures](#figures), [Tables](#tables))

---
### Figure 13a

*Experiment Goal:*
```
Observe the cpu utilization during block aligned sequential write corresponding the clone files on different filesystems.
```

*Command:*
```
FlexClone_Artifact_Eval/eval_scripts# ./main_fig.sh 13a 1 	//Run single iteration of the experiment
```
*Estimated Runtime:*
```
10 mins of total time for the single iteration of the experiment
```

*Script actions:*
```
- For Ext4, XFS, Btrfs, FlexClone
	- Mount filesystem
	- Copy helper scripts into the mount point
	- Generate source file
	- Perform clone operation for source file
	- Perform sequential write operation on the clone file using FIO benchmark
	- Collect CPU utilization while the FIO benchmark runs
```
*Results:*
```
- Results and corresponding plot are stored in "FlexClone_Artifact_Eval/eval_scripts/fig13a_cpu_util/fio_output/ssd" directory
- For ease of access, plot is copied to "FlexClone_Artifact_Eval/eval_scripts/fig13a_cpu_util" directory
- Log file for the experiment is stored in  "FlexClone_Artifact_Eval/eval_scripts/fig13a_cpu_util" directory with experiment timestamp being the name of the log file (Eg: log_20250926_181210).
```
(Jump to: [Figures](#figures), [Tables](#tables))

---
### Figure 13b

*Experiment Goal:*
```
Observe the cpu utilization during block aligned random write corresponding the clone files on different filesystems.
```

*Command:*
```
FlexClone_Artifact_Eval/eval_scripts# ./main_fig.sh 13b 1 	//Run single iteration of the experiment
```
*Estimated Runtime:*
```
15 mins of total time for the single iteration of the experiment
```

*Script actions:*
```
- For Ext4, XFS, Btrfs, FlexClone
	- Mount filesystem
	- Copy helper scripts into the mount point
	- Generate source file
	- Perform clone operation for source file
	- Perform random write operation on the clone file using FIO benchmark
	- Collect CPU utilization while the FIO benchmark runs
```
*Results:*
```
- Results and corresponding plot are stored in "FlexClone_Artifact_Eval/eval_scripts/fig13b_cpu_util/fio_output/ssd" directory
- For ease of access, plot is copied to "FlexClone_Artifact_Eval/eval_scripts/fig13b_cpu_util" directory
- Log file for the experiment is stored in  "FlexClone_Artifact_Eval/eval_scripts/fig13b_cpu_util" directory with experiment timestamp being the name of the log file (Eg: log_20250926_181210).
```
(Jump to: [Figures](#figures), [Tables](#tables))

---
### Figure 14b

*Experiment Goal:*
```
Observe the performance of various filesystems with OverlayFS for Filebench workloads.
```

*Command:*
```
FlexClone_Artifact_Eval/eval_scripts# ./main_fig.sh 14b 1 	//Run single iteration of the experiment
```
*Estimated Runtime:*
```
120 mins of total time for the single iteration of the experiment
```

*Script actions:*
```
- For Ext4, XFS, Btrfs, FlexClone
	- Mount filesystem
	- Copy helper scripts into the mount point
	- Run Filebench workloads to generate files in lower layer
	- Mount OverlayFS
	- Run Filebench workloads on files visible in merged directory 
```
*Results:*
```
- Results and corresponding plot are stored in "FlexClone_Artifact_Eval/eval_scripts/ffig14b_filebench_overlayfsl/output" directory
- For ease of access, plot is copied to "FlexClone_Artifact_Eval/eval_scripts/fig14b_filebench_overlayfs" directory
- Log file for the experiment is stored in  "FlexClone_Artifact_Eval/eval_scripts/fig14b_filebench_overlayfs" directory with experiment timestamp being the name of the log file (Eg: log_20250926_181210).
```
(Jump to: [Figures](#figures), [Tables](#tables))

---
### Figure 15a

*Experiment Goal:*
```
Observe the performance of SQLite with FlexClone.
```

*Command:*
```
FlexClone_Artifact_Eval/eval_scripts# ./main_fig.sh 15a 1 	//Run single iteration of the experiment
```
*Estimated Runtime:*
```
100 mins of total time for the single iteration of the experiment
```

*Script actions:*
```
- For Ext4, FlexClone
	- Mount filesystem
	- Copy helper scripts into the mount point
	- Run benchbase with appropriate SQLite library
```
*Results:*
```
- Results and corresponding plot are stored in "FlexClone_Artifact_Eval/eval_scripts/fig15a_sqlite/output" directory
- For ease of access, plot is copied to "FlexClone_Artifact_Eval/eval_scripts/fig15a_sqlite" directory
- Log file for the experiment is stored in  "FlexClone_Artifact_Eval/eval_scripts/fig15a_sqlite" directory with experiment timestamp being the name of the log file (Eg: log_20250926_181210).
```
(Jump to: [Figures](#figures), [Tables](#tables))


---
### Figure 15b

*Experiment Goal:*
```
Observe the performance of SQLite with XFS.
```

*Command:*
```
FlexClone_Artifact_Eval/eval_scripts# ./main_fig.sh 15b 1 	//Run single iteration of the experiment
```
*Estimated Runtime:*
```
100 mins of total time for the single iteration of the experiment
```

*Script actions:*
```
- For XFS
	- Mount filesystem
	- Copy helper scripts into the mount point
	- Run benchbase with appropriate SQLite library (vanilla and modified)
```
*Results:*
```
- Results and corresponding plot are stored in "FlexClone_Artifact_Eval/eval_scripts/fig15b_sqlite/output" directory
- For ease of access, plot is copied to "FlexClone_Artifact_Eval/eval_scripts/fig15b_sqlite" directory
- Log file for the experiment is stored in  "FlexClone_Artifact_Eval/eval_scripts/fig15b_sqlite" directory with experiment timestamp being the name of the log file (Eg: log_20250926_181210).
```
(Jump to: [Figures](#figures), [Tables](#tables))


---
### Figure 1
*Experiment Goal:*
```
Observe the impact of duplicate caching.
```

*Setup:*

- This experiment runs a Webserver workload inside a VM, thus requiring a VM to be set up.
- To perform this experiment, set up a 32GB QEMU VM. This VM image will be used as the parent file/parent VM.
- Copy the Webserver scripts present inside `vm_experiments_scripts/fig1/` directory to the parent VM image.
- Run Filebench Webserver workload inside this parent VM to preallocate ~16GB files. Before running Filebench, disable ASLR inside the VM:
```
# echo 0 | sudo tee /proc/sys/kernel/randomize_va_space
# filebench -f webserver.f
```
- Convert the VM image format from `qcow2` to `raw`.
- For XFS, Btrfs, and FlexClone, clone the parent VM image to create two child VMs. Allocate 4GB DRAM and 8 CPU cores to each child VM. Set child VM caching mode to `writeback`.
- Inside child VMs run `webserver.f.reuse` workload
```
# echo 0 | sudo tee /proc/sys/kernel/randomize_va_space
# filebench -f webserver.f.reuse
```
- Memory utilization on the host can be captured using tools, such as `sar`.

(Jump to: [Figures](#figures), [Tables](#tables))

---
### Figure 14a
*Experiment Goal:*
```
Observe the performance of various FIO workloads inside VM.
```

*Setup:*

- This experiment runs FIO workloads inside a VM, thus requiring a VM to be set up.
- To perform this experiment, set up a 32GB QEMU VM. This VM image will be used as the parent file/parent VM.
- Copy the FIO scripts present inside `vm_experiments_scripts/fig14a/` directory to the parent VM image.
- Create an 8GB file inside parent VM using the following command:
```
# dd if=/dev/urandom of=8GB_child bs=1M count=8192
```
- Convert the VM image format from `qcow2` to `raw`.
- For XFS, Btrfs, and FlexClone, clone the parent VM image to create a child VM. Allocate 16GB DRAM and 1 CPU core to the child. Set child VM caching mode to `writeback`.
- Inside child VMs run FIO workload
```
# fio 8GB_seqWrite_coldCache_4096bs_Ext4_1thread_asynchOff
# fio 8GB_randWrite_coldCache_4096bs_Ext4_1thread_asynchOff
```

(Jump to: [Figures](#figures), [Tables](#tables))

---
### Figure 16a and 16b

*Experiment Goal:*
```
Observe the benefit of See-through clone configuration
```

*Setup:*

- This experiment performs kernel installation inside a VM, thus requiring a VM to be set up.
- To perform this experiment, set up a 32GB QEMU VM. This VM image will be used as the parent file/parent VM.
- Assign separate 4GB partition for /boot and separate 12GB partition for /lib/modules during kernel installation.
- Modify fstab to automatically mount /lib/modules partition on boot. 
- Make /boot/grub/grubenv in base image as immutable.
        $ sudo chattr +i /boot/grub/grubenv
- Disable journaling on /boot and /lib/modules filesystems.
- Add kernel installation dependencies in base VM
- Add 5.5.10 kernel source code in a separate disk attached to VM and compile this kernel with minimal .config file to reduce to the number of modules required to be installed.
- Convert the VM image format from `qcow2` to `raw`.
- Use `fdisk` to note down the range of blocks allocated to the /boot and /lib/modules partitions. This range will be used later during cloning.
- Shutdown parent VM.
- Create four child VM images by providing appropriate clone ranges during clone operation. For example: 
```
# ./setxattr_generic -c seethrough.copy1 -p seethrough.raw -f seethrough.frnd1 -r 258:999935:3,1000194:3999999:3
```
- Boot all child VMs and parent VM.
- Attach disk containing compiled Linux into base VM. 
- Trigger kernel installation commands inside base VM:
```
# make modules_install
# make install
```
- CPU and Memory utilization on the host can be captured using tools, such as `sar`.

(Jump to: [Figures](#figures), [Tables](#tables))

---
### Tables
---
- `main_tab.sh` is the master script present inside `FlexClone_Artifact_Eval/eval_scripts` directory that can be used to trigger the evaluation corresponding to various tables presented in the paper.
- `main_tab.sh` takes one command-line argument: 
	1. table number 
- Example: 
```
 		# ./main_tab.sh 1 		//Run experiment corresponding table 1 
        # ./main_tab.sh 5  		//Run experiment corresponding table 5 
```
- Note: 
	- Run `main_tab.sh` as root user, as shown in above example 
	- Experiments run for only one  iteration 

- A short summary corresponding each table's evaluation process is provided below:
	- [Table 1](#table-1)
	- [Table 3](#table-3)
	- [Table 4](#table-4)
	- [Table 5](#table-5)

(Jump to: [Figures](#figures), [Tables](#tables))

---
### Table 1

*Experiment Goal:*
```
Observe the lack of singleton caching in Btrfs and XFS.
```

*Command:*
```
FlexClone_Artifact_Eval/eval_scripts# ./main_tab.sh 1 	
```
*Estimated Runtime:*
```
5 mins of total time for the single iteration of the experiment
```

*Script actions:*
```
- For XFS, Btrfs
	- Mount filesystem
	- Generate source file
	- Perform clone operation for source file
	- Capture clone time and system memory usage after clone
	- Sequentially access source file
	- Capture access time and system memory usage after access
```
*Results:*
```
- Result summary  is stored in "result_btrfs" and "result_xfs" files within "FlexClone_Artifact_Eval/eval_scripts/tab1_duplicate_caching/" directory
- Log file for the experiment is stored in  "FlexClone_Artifact_Eval/eval_scripts/tab1_duplicate_caching" directory with experiment timestamp being the name of the log file (Eg: log_20250926_181210).
```
(Jump to: [Figures](#figures), [Tables](#tables))


---
### Table 3

*Experiment Goal:*
```
Observe the random write performance overhead due to sparseness.
```

*Command:*
```
FlexClone_Artifact_Eval/eval_scripts# ./main_tab.sh 3 	
```
*Estimated Runtime:*
```
3 mins of total time for the single iteration of the experiment
```

*Script actions:*
```
- For Ext4, FlexClone
	- Mount filesystem
	- Generate preallocated/sparse source file
	- Perform clone operation for source file (in case of FlexClone)
	- Run random write FIO workload on preallocated file(Ext4) / sparse file(Ext4)/ clone file(FlexClone)
```
*Results:*
```
- Result summary  is stored in "FlexClone_Artifact_Eval/eval_scripts/tab3_rand_write_overheads/summary"
- Detailed results are stored in "FlexClone_Artifact_Eval/eval_scripts/tab3_rand_write_overheads/fio_output/ssd"
- Log file for the experiment is stored in  "FlexClone_Artifact_Eval/eval_scripts/tab3_rand_write_overheads" directory with experiment timestamp being the name of the log file (Eg: log_20250926_181210).
```
(Jump to: [Figures](#figures), [Tables](#tables))


---
### Table 4

*Experiment Goal:*
```
Observe the performance of difference FlexClone modes (See-through, CoW and Open-share).
```

*Command:*
```
FlexClone_Artifact_Eval/eval_scripts# ./main_tab.sh 4 	
```
*Estimated Runtime:*
```
10 mins of total time for the single iteration of the experiment
```

*Script actions:*
```
- For Ext4, FlexClone
	- Mount filesystem
	- Generate source file
	- Perform clone operation to generate child files configured with different FlexClone modes.
	- Run random read and write FIO workloads on child files
```
*Results:*
```
- Result summary  is stored in "FlexClone_Artifact_Eval/eval_scripts/tab4_flexclone_modes/summary"
- Detailed results are stored in "FlexClone_Artifact_Eval/eval_scripts/tab4_flexclone_modes/fio_output/ssd"
- Log file for the experiment is stored in  "FlexClone_Artifact_Eval/eval_scripts/tab4_flexclone_modes" directory with experiment timestamp being the name of the log file (Eg: log_20250926_181210).
```
(Jump to: [Figures](#figures), [Tables](#tables))

---
### Table 5

*Experiment Goal:*
```
Observe the system memory utilization while operating on clone files on various filesystems.
```

*Command:*
```
FlexClone_Artifact_Eval/eval_scripts# ./main_tab.sh 5 	
```
*Estimated Runtime:*
```
30 mins of total time for the single iteration of the experiment
```

*Script actions:*
```
- For XFS, Btrfs
	- Mount filesystem
	- Generate source file
	- Perform clone operation for source file
	- Perform random write on the clone file using FIO benchmark
	- Capture the system memory usage while FIO runs
```
*Results:*
```
- Result summary  is stored in "FlexClone_Artifact_Eval/eval_scripts/tab5_mem_util/summary"
- Detailed results are stored in "FlexClone_Artifact_Eval/eval_scripts/tab5_mem_util/fio_output/ssd"
- Log file for the experiment is stored in  "FlexClone_Artifact_Eval/eval_scripts/tab5_mem_util" directory with experiment timestamp being the name of the log file (Eg: log_20250926_181210).
```
(Jump to: [Figures](#figures), [Tables](#tables))
