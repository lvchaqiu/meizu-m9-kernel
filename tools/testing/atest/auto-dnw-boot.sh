#!/bin/bash
# auto-dnw-boot.sh -- wrapper for auto-dnw-boot.py

LOG=logs/auto-dnw-boot
REPORT_EMAIL=1
#BSP_BUILD_ML=bsp-build@meizu.com
BSP_BUILD_ML=falcon@meizu.com
BSP_BUILD_ROBOT="Meizu Boot Robot"

mkdir -p ${LOG}
boot_log=${LOG}/boot.log
echo "LOG: Run tools/testing/atest/auto-dnw-boot.py $@ > ${boot_log} 2>&1"
tools/testing/atest/auto-dnw-boot.py $@ > ${boot_log} 2>&1
if [ $? -ne 0 ]; then
	echo "LOG: Boot Test: FAIL, please check log: ${boot_log}"
else
	echo "LOG: Boot Test: PASS, full log saved in ${boot_log}"
fi
