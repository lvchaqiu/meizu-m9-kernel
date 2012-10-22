zreladdr-y	:= 0x20008000
params_phys-y	:= 0x20000100

# override for Herring
zreladdr-$(CONFIG_MACH_HERRING)	:= 0x30008000
params_phys-$(CONFIG_MACH_HERRING)	:= 0x30000100
#override for meizu_m9w
zreladdr-$(CONFIG_MACH_MEIZU_M9W)	:= 0x30008000
params_phys-$(CONFIG_MACH_MEIZU_M9W)	:= 0x30000100
