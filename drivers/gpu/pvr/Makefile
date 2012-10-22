ccflags-y := \
	-Wno-pointer-sign\
	-fno-strict-overflow\
	-fconserve-stack\
	-Wdeclaration-after-statement\
	-Wpointer-arith\
	-Wmissing-format-attribute\
	-Wno-format-zero-length\
	-Wmissing-prototypes\
	-Wstrict-prototypes\
	-Wno-unused-parameter\
	-Wno-sign-compare\
	-Werror\
	-fno-strict-aliasing\
	-Wno-pointer-arith\
	-Os

ccflags-y += -DLINUX -D__linux__ -DANDROID
ccflags-y += -Idrivers/gpu/pvr

ccflags-y += \
	-DSUPPORT_SGX_NEW_STATUS_VALS \
	-DSUPPORT_SGX_HWPERF \
	-DSYS_USING_INTERRUPTS \
	-DPVR_SECURE_HANDLES \
	-DSUPPORT_DBGDRV_EVENT_OBJECTS \
	-DTRANSFER_QUEUE \
	-DSUPPORT_SGX \
	-DLDM_PLATFORM \
	-DPVR_LINUX_USING_WORKQUEUES \
	-DPVR_LINUX_MISR_USING_PRIVATE_WORKQUEUE \
	-DPVR_LINUX_TIMERS_USING_WORKQUEUES \
	-DSYS_CUSTOM_POWERLOCK_WRAP \
	-DSUPPORT_MEMINFO_IDS \
	-DSYS_SGX_ACTIVE_POWER_LATENCY_MS=100 \
	-DPVR_BUILD_DATE="" \
	-DPVRSRV_MODNAME="\"pvrsrvkm\"" \
	-Idrivers/gpu/pvr/sgx \
 	-DSUPPORT_PERCONTEXT_PB \
 	-DSUPPORT_ACTIVE_POWER_MANAGEMENT \
 	-DSUPPORT_SGX_LOW_LATENCY_SCHEDULING \
 	-DPVR_NO_FULL_CACHE_OPS \
 	-DPVRSRV_NEED_PVR_DPF \
 	-DPVRSRV_NEED_PVR_ASSERT \
	-DSUPPORT_HW_RECOVERY \
	-DPVR_LDM_DRIVER_REGISTRATION_NAME="\"pvrsrvkm\""

#TODO : check this option
#       -DPVR_NO_FULL_CACHE_OPS - can we support this?
#	-DPVR2D_ALT_2DHW - this is defined in UM only
#	-DPVRSRV_RESET_ON_HWTIMEOUT
#       -DSUPPORT_LINUX_X86_WRITECOMBINE
#       -DSUPPORT_LINUX_X86_PAT
#       -DSGX_DYNAMIC_TIMING_INFO 

ccflags-$(CONFIG_PVR_BUILD_RELEASE) += \
	-DPVR_BUILD_TYPE="\"release\"" -DRELEASE

ccflags-$(CONFIG_PVR_BUILD_DEBUG) += \
	-DPVR_BUILD_TYPE="\"debug\"" -DDEBUG \
	-DDEBUG_LINUX_MEMORY_ALLOCATIONS 

pvrsrvkm-y := \
	osfunc.o \
	mutils.o \
	mmap.o \
	module.o \
	pdump.o \
	proc.o \
	pvr_bridge_k.o \
	pvr_debug.o \
	mm.o \
	mutex.o \
	event.o \
	osperproc.o \
	buffer_manager.o \
	devicemem.o \
	deviceclass.o \
	handle.o \
	hash.o \
	metrics.o \
	pvrsrv.o \
	queue.o \
	ra.o \
	resman.o \
	power.o \
	mem.o \
	pdump_common.o \
	bridged_support.o \
	bridged_pvr_bridge.o \
	perproc.o \
	lists.o \
	sgx/bridged_sgx_bridge.o \
	sgx/sgxinit.o \
	sgx/sgxpower.o \
	sgx/sgxreset.o \
	sgx/sgxutils.o \
	sgx/sgxkick.o \
	sgx/sgxtransfer.o \
	sgx/mmu.o \
	sgx/pb.o \
	osfunc_common.o \
	refcount.o

ccflags-$(CONFIG_ARCH_S5PV210) += \
	-DPVR_BUILD_DIR="\"smdkc110_android\"" \
	-Idrivers/gpu/pvr/s5pc110 \
	-DDISPLAY_CONTROLLER=s3c_lcd \
    -DSLSI_S5PC110

ccflags-$(CONFIG_ARCH_S5PV210) += \
	-DSGX540 -DSUPPORT_SGX540 \
	-DSGX_CORE_REV=120

pvrsrvkm-$(CONFIG_ARCH_S5PV210) += \
	s5pc110/sysconfig.o \
	s5pc110/sysutils.o

s3c_lcd-y := \
	s3c_lcd/s3c_displayclass.o \
	s3c_lcd/s3c_lcd.o

obj-$(CONFIG_PVR_PDUMP) += dbgdrv

obj-$(CONFIG_PVR_SGX) += pvrsrvkm.o

obj-$(CONFIG_ARCH_S5PV210) += s3c_lcd.o
