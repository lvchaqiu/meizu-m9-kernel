#!/bin/bash
# auto-build.sh -- build the kernel and report warnings/errors
# Usage: bash -x -v auto-build.sh

# Requirements
# exim4, svn, git ...

# Notes
# git related parts are not tested yet ...

# Configuration
ARCH=arm
SCM=git
LOG=logs/auto-build
FAIL_IF_WARN=1
REPORT_WARN=1
REPORT_EMAIL=0
GIT_COMMIT_LEN=10
# Specify the start date of the project to filter the old logs
FROM_TIME=20120106
# Clean: 0: don't clean; 1: clean; 2: distclean
MAKE_CLEAN=2
LOG_BEFORE=5
LOG_AFTER=5
CPUS=`grep processor /proc/cpuinfo | wc -l`
BSP_BUILD_ML=bsp-build@meizu.com
BSP_BUILD_ROBOT="Meizu Build Robot"
BSP_AUTHOR_FILE=tools/testing/atest/svn.users.txt
CFGS="meizum9_ics_eng_defconfig"

# Preparing
mkdir -p ${LOG}

# Get the latest revision
function svn_revision
{
	svn info | grep "Last Changed Rev:" | cut -d':' -f2 | tr -d ' '
}

function git_revision
{
	long=`git show HEAD | grep commit | cut -d' ' -f2`
	echo ${long:0:$GIT_COMMIT_LEN}
}

function update_kernel
{
	if [ ${SCM} == "svn" ]; then
		echo "LOG: Record the old revision"
		OLD_REV=`svn_revision`
		[ $? -ne 0 ] && echo "ERR: Fail to record the old revision" && exit -1

		echo "LOG: Update the kernel souce code"
		svn up
		[ $? -ne 0 ] && echo "ERR: Fail to update the kernel source code" && exit -1

		echo "LOG: Record the new revision"
		NEW_REV=`svn_revision`
		[ $? -ne 0 ] && echo "ERR: Fail to record the new revision" && exit -1
	else
		echo "LOG: Record the old revision"
		OLD_REV=`git_revision`
		[ $? -ne 0 ] && echo "ERR: Fail to record the old revision" && exit -1

		echo "LOG: Update the kernel souce code"
		git pull
		[ $? -ne 0 ] && echo "ERR: Fail to update the kernel source code" && exit -1

		echo "LOG: Record the new revision"
		NEW_REV=`git_revision`
		[ $? -ne 0 ] && echo "ERR: Fail to record the new revision" && exit -1
	fi
	echo "LOG: Update from ${OLD_REV} to ${NEW_REV}"
}

# Warning/errors handling
function report_log
{
	LOG_NAME=$1
	LOG_FILE=$2
	LOG_LEVEL=$3

	# Parse log and create an email
	LOG_EMAIL=`mktemp`
	echo -e "LOG: Create an email: $LOG_EMAIL\n"

	SUBJECT="[${LOG_NAME}]: ${LOG_LEVEL} : Build test for update from ${OLD_REV} to ${NEW_REV}"

	echo "" | tee ${LOG_EMAIL}
	echo "${LOG_LEVEL}: " | tee -a ${LOG_EMAIL}
	echo "======= " | tee -a ${LOG_EMAIL}
	egrep -i "${LOG_LEVEL}:| ${LOG_LEVEL}" \
		-A ${LOG_AFTER} -B ${LOG_BEFORE} ${LOG_FILE} \
		| tee -a ${LOG_EMAIL}

	echo -e "\nChanges: " | tee -a ${LOG_EMAIL}
	echo "======= " | tee -a ${LOG_EMAIL}
	TO_BSP_AUTHORS=""
	for FILE_LINE in `grep ${LOG_LEVEL} ${LOG_FILE} | cut -d':' -f1,2`
	do
		FILE=`echo $FILE_LINE | cut -d':' -f1`
		LINE=`echo $FILE_LINE | cut -d':' -f2`
		# Only for SVN
		if [ ${SCM} == "svn" ]; then
			#CHANGE=`svn blame -r ${OLD_REV}:${NEW_REV} $FILE | cat -n | tr '\t' ' ' | tr -s ' ' | grep -v '\- \-' | grep "^ $LINE"`
			CHANGE=`svn blame $FILE | cat -n | tr '\t' ' ' | tr -s ' ' | grep -v '\- \-' | grep "^ $LINE"`
			AUTHOR=`echo $CHANGE | cut -d' ' -f3`
			[ -z "$AUTHOR" ] && echo "ERR: Author information is empty" && continue
			EMAIL_ADDRESS=`grep $AUTHOR ${BSP_AUTHOR_FILE} | cut -d' ' -f4`
		else
			CHANGE=`git blame --since=$FROM_TIME -L$LINE,$LINE -e init/main.c`
			AUTHOR=`git blame --since=$FROM_TIME -L$LINE,$LINE init/main.c | cut -d' ' -f2 | tr -d '('`
			[ -z "$AUTHOR" ] && echo "ERR: Author information is empty" && continue
			EMAIL_ADDRESS=`git blame --since=$FROM_TIME -L$LINE,$LINE -e init/main.c | cut -d' ' -f2 | tr -d '('`
		fi
		echo -e "$CHANGE\n----" | tee -a ${LOG_EMAIL}
		TO_BSP_AUTHORS="${TO_BSP_AUTHORS} -c ${EMAIL_ADDRESS}"
	done
	TO_BSP_AUTHORS=`echo ${TO_BSP_AUTHORS} | sed -e "s/-c /\\n-c /g" | sort -u | uniq`
	echo -e "\n\n---\nBest Regards,\n$BSP_BUILD_ROBOT" | tee -a ${LOG_EMAIL}

	# Send out the log pieces to mailing list and the authors
	[ ${REPORT_EMAIL} -eq 1 ] && mail ${BSP_BUILD_ML} -a "From: ${BSP_BUILD_ML}" -s "${SUBJECT}" ${TO_BSP_AUTHORS} < ${LOG_EMAIL}

	# Remove the email content
	[ ${REPORT_EMAIL} -eq 1 ] && rm ${LOG_EMAIL}
}

function report_errors
{
	echo "ERR: $1: FAIL, please check log: $2"
	report_log $1 $2 "error"
	exit -1
}

function check_and_report_warnings
{
	grep -q error: $2
	[ $? -eq 0 ] && report_errors $1 $2
	grep -q warning: $2
	if [ $? -eq 0 ]; then
		echo "WARN: $1, please check log: $2"
		[ $REPORT_WARN -eq 1 ] && report_log $1 $2 "warning"
		[ $FAIL_IF_WARN -eq 1 ] && echo "LOG: Treat warnings as errors" && exit -1
	fi
	echo
}

# Build the kernel with an indicated config
function build_kernel_cfg
{
	echo "LOG: Checking the config file"

	# Check argument
	[ -z "$1" ] && echo "ERR: Should indicate a default kernel config file under arch/${ARCH}/configs/" && exit -1

	cfg=$1
	cfg_with_path=arch/${ARCH}/configs/${cfg}
	[ ! -f $cfg_with_path ] && echo "ERR: $cfg_with_path doesn't exist" && exit -1

	echo "LOG: Preparing a clean build environment"
	# Prepare a clean environment
	if [ ${MAKE_CLEAN} -eq 2 ]; then
		make ARCH=${ARCH} distclean
	elif [ ${MAKE_CLEAN} -eq 1 ]; then
		make ARCH=${ARCH} clean
	fi

	echo "LOG: Configuring the kernel"
	# Config
	cfg_log=${LOG}/config.log
	echo "LOG: Run make ARCH=${ARCH} ${cfg} > ${cfg_log} 2>&1"
	make ARCH=${ARCH} ${cfg} > ${cfg_log} 2>&1
	[ $? -ne 0 ] && report_errors CONFIG ${cfg_log}
	check_and_report_warnings CONFIG ${cfg_log}

	echo "LOG: Building the kernel"
	# Build (with necessary check)
	build_log=${LOG}/build.log
	echo "LOG: Run make ARCH=${ARCH} -j${CPUS} CONFIG_DEBUG_SECTION_MISMATCH=y > ${build_log} 2>&1"
	make ARCH=${ARCH} -j${CPUS} CONFIG_DEBUG_SECTION_MISMATCH=y > ${build_log} 2>&1
	RET=$?
	[ $RET -eq 130 ] && echo "LOG: Exit by user" && exit 0
	[ $RET -ne 0 ] && report_errors BUILD ${build_log}
	check_and_report_warnings BUILD ${build_log}

	echo
}

function build_kernel
{
	echo "LOG: Building kernels for all configs"
	echo "LOG: $CFGS"
	for cfg in $CFGS
	do
		echo "LOG: Building kernel for $cfg"
		build_kernel_cfg $cfg
		[ $RET -ne 0 ] && echo "ERR: BUILD: $cfg: FAIL" && exit -1
		echo "LOG: BUILD: $cfg: PASS"
	done
	echo "LOG: BUILD: PASS"
}

# main entry
update_kernel
build_kernel
