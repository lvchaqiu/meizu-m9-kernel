#!/usr/bin/env python
#
# auto-dnw-boot.py - program the serial port to download images for android devices

# Usage:
#	python /path/to/auto-dnw-boot.py -B
#	Enter 'CTRL+C' to exit

# Copyright 2006 Sony Corporation
# Copyright 2012 Meizu Co., Ltd
#
# This program is provided under the Gnu General Public License (GPL)
# version 2 ONLY.
#
# 2006-09-07 by Tim Bird <tim.bird@am.sony.com>
# 2011-09-24 Constantine Shulyupin <const@makelinux.com> better time output and time delta
# 2012-02-13 by Wu Zhangjin <wuzhangjin@gmail.com> auto-download images and boot
#
# To do:
#  * buffer output chars??
#

MAJOR_VERSION=1
MINOR_VERSION=0
REVISION=0

import os, sys
import getopt
import serial
import thread
import time
import re

cmd = os.path.basename(sys.argv[0])
verbose = 0
quiet_serial_output = 0

def vprint(message):
	if verbose:
		print message

def usage(rcode):
	print """%s : Serial line reader
	Usage: %s [options] <config_file>
options:
    -h, --help             Print this message
    -d, --device=<devpath> Set the device to read (default '/dev/ttyS0')
    -b, --baudrate=<val>   Set the baudrate (default 115200)
    -w, --width=<val>      Set the data bit width (default 8)
    -p, --parity=<val>     Set the parity (default N)
    -s, --stopbits=<val>   Set the stopbits (default 1)
    -x, --xonxoff          Enable software flow control (default off)
    -r, --rtscts           Enable RTC/CTS flow control (default off)
    -e, --endtime=<secs>   End the program after the specified seconds have
                           elapsed.
    -t, --time             Print time for each line received.  The time is
                           when the first character of each line is
                           received by %s
    -m, --match=<pat>      Specify a regular expression pattern to match to
                           set a base time.  Time values for lines after the
                           line matching the pattern will be relative to
                           this base time.

    -K, --kernel=<kernelpath>
    -D, --ramdisk=<ramdiskpath>
    -S, --system=<systempath>
    -R, --recovery=<recoverypath>
    -U, --uboot=<ubootpath>
    -I, --imgpath=<imagespath>
    -N, --nodnw            Don't download anything, useful for boot time
                           measurement or reboot test
    -n, --nobreak          Don't break the autoboot of uboot
    -a,
    -B, --reboot=<val>	   Reboot android val times
    -W, --reboot_wait=<secs>Wait for another reboot after secs, default: 35
    -u, --utest=<val>	   Reboot uboot val times
    -P, --phone=<M9|MX>    Specify the phone type
    -L, --logalways        Specify if log the serial output always, default: 0
    -C, --compile          Specify if compilie the kernel automatically
    -c, --command=<cmd>    Send a uboot command

    -v, --verbose          Show verbose runtime messages
    -V, --version          Show version number and exit

Ex: %s -e 30 -t -m "^Linux version.*"
This will grab serial input for 30 seconds, displaying the time for
each line, and re-setting the base time when the line starting with
"Linux version" is seen.
""" % (cmd, cmd, cmd, cmd)
	sys.exit(rcode)

def log(test, total, current):
	print "\n==================================="
	print "LOG: %s Test: PASS: %d, LEFT: %d" % (test, total - current, current)
	print "===================================\n"

def myexit(s, err, errstr):
	print errstr
	if s.isOpen():
		s.close()
	sys.exit(err)

def dnw(s, image):
	insdnw = "dnw "+ image
	ret = os.system(insdnw)
	if ret:
		exit(s, ret, "ERR: %s: FAIL" % insdnw)

	# Interaction
def interaction(s):
	# Quiet serial output or not
	global quiet_serial_output
	global nobreak
	# Promput for input
	prompt = ""
	while (1):
		input = raw_input(prompt)
		if input == "exit" or input == "quit":
			myexit(s, 0, "LOG: Exit by user")
		elif input == "quiet":
			quiet_serial_output = 1
			prompt = ">> "
		elif input == "verbose":
			quiet_serial_output = 0
			prompt = ""
		elif input == "nobreak":
			nobreak = 1
		elif input == "break":
			nobreak = 0
		else:
			# send the character to the device
			s.write(input + '\n')

def main():
	global verbose
	global quiet_serial_output
	global nobreak

	# parse the command line options
	try:
		opts, args = getopt.getopt(sys.argv[1:],
			 "hd:b:w:p:s:xrtm:e:K:D:S:R:U:I:u:a:B:W:P:c:nNLCvV", ["help", "device=",
			"baudrate=", "width=","parity=","stopbits=",
			"xonxoff", "rtscts","time", "match=", "endtime=",
			"kernel=", "ramdisk=", "system=", "recovery", "uboot=", "imgpath=",
			"utest=", "reboot=", "reboot_wait=", "nobreak", "nodnw", "phone=", "command=",
			"logalways", "compile", "verbose", "version"])
	except:
		# print help info and exit
		print "Error parsing command line options"
		usage(2)

	device="/dev/ttyUSB0"
	MIN_SERIAL_WAIT = 240
	serial_timeout = MIN_SERIAL_WAIT
	baudrate=115200
	width=8
	parity='N'
	stopbits=1
	xon=0
	rtc=0
	show_time = 0
	endtime = 0
	# Default MX
	phone="M9"
	uboot_img = "uboot_fuse.bin"
	# Supported phones
	phones=["M9", "MX"]
	m9pat = "MEIZU M9"
	mxpat = "MEIZU MX"
	# Prompts for U-Boot is staring...
	basepat = "U-Boot"
	# path
	kernel = ""
	system = ""
	ramdisk = ""
	recovery = ""
	uboot = ""
	imgpath = ""
	# images
	imgtotal = 0
	# uboot test times
	utest = 0
	MIN_UTEST_WAIT = 15
	utest_wait = MIN_UTEST_WAIT
	utest_timeout = 0;
	# reboot the android devices automatically, wait for 1 minutes by default
	reboot = 1
	MIN_REBOOT_WAIT = 60
	reboot_wait = MIN_REBOOT_WAIT
	reboot_next_time = 0
	# Don't download images
	nodnw = 0
	# Don't break autoboot
	nobreak = 0
	# Don't log after checking the booted pattern
	log_always = 0
	# Compile the kernel automatically
	auto_compile = 0
	compile_command = "make ARCH=arm -j8"
	# Uboot/Kernel command
	command = ""
	# Unicode of ctrl+c
	ctrl_c_key = unichr(int('03', 16))

	# get a dummy instance for error checking
	sd = serial.Serial()

	for opt, arg in opts:
		if opt in ["-h", "--help"]:
			usage(0)
                if opt in ["-d", "--device"]:
                        device = arg
			if not os.path.exists(device):
				print "ERR: serial device '%s' does not exist" % device
				usage(2)
		if opt in ["-b", "--baudrate"]:
			baudrate = int(arg)
			if baudrate not in sd.BAUDRATES:
				print "ERR: invalid baud rate '%d' specified" % baudrate
				print "Valid baud rates are: %s" % str(sd.BAUDRATES)
				sys.exit(3)
                if opt in ["-p", "--parity"]:
			parity = arg.upper()
			if parity not in sd.PARITIES:
				print "ERR: invalid parity '%s' specified" % parity
				print "Valid parities are: %s" % str(sd.PARITIES)
				sys.exit(3)
		if opt in ["-w", "--width"]:
			width = int(arg)
			if width not in sd.BYTESIZES:
				print "ERR: invalid data bit width '%d' specified" % width
				print "Valid data bit widths are: %s" % str(sd.BYTESIZES)
				sys.exit(3)
		if opt in ["-s", "--stopbits"]:
			stopbits = int(arg)
			if stopbits not in sd.STOPBITS:
				print "ERR: invalid stopbits '%d' specified" % stopbits
				print "Valid stopbits are: %s" % str(sd.STOPBITS)
				sys.exit(3)
		if opt in ["-x", "--xonxoff"]:
			xon = 1
		if opt in ["-r", "--rtcdtc"]:
			rtc = 1
		if opt in ["-t", "--time"]:
			show_time=1
		if opt in ["-m", "--match"]:
			basepat=arg
		if opt in ["-e", "--endtime"]:
			endstr=arg
			try:
				endtime = time.time()+float(endstr)
			except:
				print "ERR: invalid endtime %s specified" % arg
				sys.exit(3)
		if opt in ["-c", "--command"]:
			command = arg
		if opt in ["-C", "--compile"]:
			auto_compile = 1
		if opt in ["-P", "--phone"]:
			phone = arg;
			if not phone in phones:
				print "ERR: %s is not supported" % phone
				usage(2)
			if phone == "M9":
				uboot_img = "u-boot-dev.signed"
		if opt in ["-u", "--utest"]:
			utest = int(arg)
		if opt in ["-K", "--kernel"]:
			kernel=arg
			if not os.path.exists(kernel):
				print "ERR: kernel image '%s' does not exist" % kernel
				usage(2)
			reboot_wait += 10
			imgtotal += 1

		if opt in ["-D", "--ramdisk"]:
			ramdisk=arg
			if not os.path.exists(ramdisk):
				print "ERR: ramdisk image '%s' does not exist" % ramdisk
				usage(2)
			reboot_wait += 5
			imgtotal += 1

		if opt in ["-S", "--system"]:
			system=arg
			if not os.path.exists(system):
				print "ERR: system image '%s' does not exist" % system
				usage(2)
			# System image is very big, which requires enough time to be downloaded
			reboot_wait += 150
			imgtotal += 1

		if opt in ["-R", "--recovery"]:
			recovery=arg
			if not os.path.exists(recovery):
				print "ERR: recovery image '%s' does not exist" % recovery
				usage(2)
			reboot_wait += 5
			imgtotal += 1

		if opt in ["-U", "--uboot"]:
			uboot=arg
			if not os.path.exists(uboot):
				print "ERR: uboot image '%s' does not exist" % uboot
				usage(2)
			reboot_wait += 10
			imgtotal += 1

		if opt in ["-I", "--imgpath"]:
			imgpath=arg
			if not os.path.exists(imgpath):
				print "ERR: images path '%s' does not exist" % imgpath
				usage(2)
			print "LOG: imgpath %s is set, ignore the -[KDSRU] options" % imgpath
			imgtotal = 0
			kernel = imgpath + "/zImage"
			if os.path.exists(kernel):
				print "LOG: kernel image found: %s" % kernel
				reboot_wait += 10
				imgtotal+=1
			system = imgpath + "/system.img"
			if os.path.exists(system):
				# System image is very big, which requires enough time to be downloaded
				reboot_wait += 150
				print "LOG: system image found: %s" % system
				imgtotal+=1
			ramdisk = imgpath + "/ramdisk-uboot.img"
			if os.path.exists(ramdisk):
				print "LOG: ramdisk image found: %s" % ramdisk
				reboot_wait += 5
				imgtotal+=1
			recovery = imgpath + "/recovery-uboot.img"
			if os.path.exists(recovery):
				print "LOG: recovery image found: %s" % recovery
				reboot_wait += 5
				imgtotal+=1
			uboot = imgpath + "/" + uboot_img
			if os.path.exists(uboot):
				print "LOG: uboot image found: %s" % uboot
				reboot_wait += 10
				imgtotal+=1
		if opt in ["-a", "-B", "--reboot"]:
			reboot = int(arg)
		if opt in ["-W", "--reboot_wait"]:
			reboot_wait = int(arg)
			if reboot_wait<MIN_REBOOT_WAIT:
				reboot_wait=MIN_REBOOT_WAIT
			print "LOG: Set reboot wait time to %d seconds" % reboot_wait
		if opt in ["-N", "--nodnw"]:
			nodnw = 1
			imgtotal = 0
		if opt in ["-n", "--nobreak"]:
			nobreak = 1
		if opt in ["-L", "--logalways"]:
			log_always = 1
			# Never timeout if log always...
			reboot_wait = 2147483647
		if opt in ["-v", "--verbose"]:
			verbose=1
		if opt in ["-V", "--version"]:
			print "grabserial version %d.%d.%d" % (MAJOR_VERSION, MINOR_VERSION, REVISION)
			sys.exit(0)

	# Configure for different phones
	if phone == "M9":
		# M9
		breakpat = "Press ctrl\+c to stop autoboot:  "
		breakkey = ctrl_c_key
		chargingpat = "battery_charge:charege state:"
		switchkey = breakkey
		consolepat = "MEIZU_M9 # "
		dnwpat = "Press the 'USB Port => Download' button."
		dnwedpat = " installation."
		dnwedpat_prefix = "Completed "
		ubootpat = "uboot"
		bootedpat = "sh: "
		enterfiqpat = "hit enter to activate fiq debugger"
		fiqpat = "debug> "
	else:
		# MX
		breakpat = "Hit any key to stop autoboot:"
		breakkey = " "
		chargingpat = "CPU: "
		switchkey = ctrl_c_key
		consolepat = "MEIZUMX # "
		dnwpat = "Press the 'USB Port => Download' button."
		dnwedpat = " complete!"
		dnwedpat_prefix = ""
		ubootpat = "u-boot"
		bootedpat = "sh: "

	# Compile before openning serial port
	if auto_compile:
		print "LOG: Compile the kernel"
		ret = os.system(compile_command)
		if ret:
			print "ERR: Compile fail"
			sys.exit(ret)

	# if verbose, show what our settings are
	vprint("Opening serial port %s" % device)
	vprint("%d:%d%s%s:xonxoff=%d:rtcdtc=%d" % (baudrate, width,
		 parity, stopbits, xon, rtc))
	if endtime:
		vprint("Program will end in %s seconds" % endstr)
	if show_time:
		vprint("Printing timing information for each line")
	if basepat:
		vprint("Matching pattern '%s' to set base time" % basepat)
	if not imgtotal and not nodnw:
		kernel = "arch/arm/boot/zImage"
		if os.path.exists(kernel):
			print "LOG: kerne image found: %s" % kernel
			imgtotal += 1

	# now actually open and configure the requested serial port
	# specify a read timeout of 1 second
	s = serial.Serial(device, baudrate, width, parity, stopbits, 1,
		xon, rtc)
	s.open()
	s.isOpen()

	# Start the interaction thread
	thread.start_new_thread(interaction, (s, ))

	# Handle serial commands
	if command:
		reboot = 0
		s.write(command + "\n")
		print "LOG: Sent '%s' to serial(uboot or kernel)" % command

	basetime = 0
	prev1 = 0
	linetime = 0
	newline = 1
	curline = ""
	outline = ""
	writing = 0
	dnwing = 0
	issued_boot = 0
	uboot_booted = 0
	utest_total = utest
	running_utest = utest
	reboot_total = reboot
	running_btest = reboot_total
	reboot_timeout = reboot_wait
	first_serial_access = 1
	real_phone = ""
	ret = 0
	retstr = ""
	vprint("Use Control-C to stop...")

	# Handle the other options
	while(1):
		try:
			# Loop for reboot test
			if running_btest and ((reboot == reboot_total) or (reboot_next_time and time.time()>reboot_next_time)):
				if reboot<=0:
					myexit(s, ret, "ERR: Timeout, please check log and try 'echo boot > %s'" % device)
					break
				else:
					while (1):
						ret = os.system("adb devices | grep device$")
						if not ret:
							break;
						else:
							print "LOG: wait for usb device being availabe... %d" % reboot_timeout
							print "LOG: the android system may not boot, please try the option: -B0"
							time.sleep(1)
							reboot_timeout -= 1
							if reboot_timeout<=0:
								if reboot>1:
									log("Reboot", reboot_total, reboot)
								time.sleep(1)
								myexit(s, ret, "ERR: No suitable adb device found")
					ret = os.system("adb shell sync; adb shell reboot")
					if ret:
						if reboot>1:
							log("Reboot", reboot_total, reboot)
						myexit(s, ret, "ERR: Fail to reboot android device!\n")
					else:
						utest_timeout = time.time() + utest_wait
						reboot_next_time = time.time() + reboot_wait
						uboot_booted = 0
						# serial port should work after any reboot
						first_serial_access = 1
						serial_timeout = MIN_SERIAL_WAIT
						if reboot>1:
							log("Reboot", reboot_total, reboot)
						reboot -= 1

			# see if we're supposed to stop yet
			if endtime and time.time()>endtime:
				retstr = "LOG: End the program after the specified seconds"
				break

			# read for up to 1 second
			while (1):
				try:
					x = s.read()
				except:
					myexit(s, -1, "LOG: exit serial port.")

				# if we didn't read anything, loop with timeout
				if len(x)!=0:
					first_serial_access = 0
					break
				else:
					if not first_serial_access:
						# The device may be already booted or in sleep, so, no serial output...
						break
					else:
						print "LOG: wait for serial port being availabe... %d" % serial_timeout
						serial_timeout -= 1
						if serial_timeout<=0:
							time.sleep(1)
							myexit(s, -1, "ERR: serial port may be not available")

			# Check if need reboot
			if len(x)==0:
				continue
			# ignore carriage returns
			if x=="\r":
				continue

			# set basetime, by default, to when first char
			# is received
			if not basetime:
				basetime = time.time()

			if show_time and newline:
				linetime = time.time()
				elapsed = linetime-basetime
				if not prev1:
					prev1 = elapsed
				outline += "%4.2f\t" % elapsed
				#if (elapsed-prev1)>0.0005:
				outline += "%4d\t" % ((elapsed-prev1)*100)
				#else:
				#	sys.stdout.write("    0\t" )
				prev1 = elapsed
				newline = 0

			# FIXTHIS - should I buffer the output here??
			outline += x
			curline += x

			# Mark Uboot start booting
			if re.search(basepat, curline):
				# Specify uboot is not yet booted,this is very useful to indicate a
				# long-press-power-button-reboot or a kernel-self-auto-reboot
				uboot_booted = 0

			# Check the real phone
			if re.search(m9pat, curline):
				real_phone = "M9"
			if re.search(mxpat, curline):
				real_phone = "MX"
			if real_phone != "" and phone != real_phone:
				print "\n\nERR: real phone is %s, but your current setting is %s, please append: -P %s\n" % (real_phone, phone, real_phone)
				usage(2)

			# Interaction
			if not nobreak and not uboot_booted and (running_utest or running_btest or (imgtotal>0 and not nodnw)):
				# Switch from charging mode to normal mode, MX doesn't support this
				# Which requires extra hacked uboot image
				if re.search(chargingpat, curline):
					s.write(switchkey)
					if phone == "MX":
						time.sleep(0.1)
						s.write(switchkey)
					curline = ""

				if re.match(breakpat, curline):
					uboot_booted = 1
					s.write(breakkey)
					if phone == "M9":
						time.sleep(0.1)
						s.write(breakkey)
						time.sleep(0.1)
					curline = ""

			if running_utest:
				if utest_timeout and time.time()>utest_timeout and not uboot_booted:
					log("Uboot", utest_total, utest)
					myexit(s, utest, "ERR: Uboot Test: FAIL")

			if not nobreak and re.match(consolepat, curline):
				# If all images are downloaded, boot it
				if imgtotal<=0:
					# Do we want to do uboot test
					if utest<=0:
						# Boot it
						s.write("boot\r\n")
						issued_boot = 1
					else:
						utest_timeout = time.time() + utest_wait
						log("Uboot", utest_total, utest)
						utest -= 1
						uboot_booted = 0
						s.write("reset\r\n")

				else:
					if kernel and not writing:
						writing = 1
						s.write("ins zimage\r\n")
					if system and not writing:
						writing = 1
						s.write("ins system\r\n")
					if ramdisk and not writing:
						writing = 1
						s.write("ins ramdisk\r\n")
					if recovery and not writing:
						writing = 1
						s.write("ins recovery\r\n")
					if uboot and not writing:
						writing = 1
						s.write("ins uboot\r\n")
					# Wait for the usb connection being ready
					time.sleep(1)

				curline = ""

			if writing and re.match(dnwpat, curline):
				print "\n"
				# In order to ensure the usb is probed in uboot, this is
				# required for insert the usb after issueing ins command
				time.sleep(1)
				if kernel and not dnwing:
					dnwing = 1
					dnw(s, kernel)
				if system and not dnwing:
					dnwing = 1
					dnw(s, system)
				if ramdisk and not dnwing:
					dnwing = 1
					dnw(s, ramdisk)
				if recovery and not dnwing:
					dnwing = 1
					dnw(s, recovery)
				if uboot and not dnwing:
					dnwing = 1
					dnw(s, uboot)
				# Wait for the usb communication being finished
				time.sleep(1)
				curline = ""

			if dnwing and re.search(dnwedpat, curline):
				imgtotal -= 1
				if re.search(dnwedpat_prefix + "kernel", curline):
					kernel = ""
				if re.search(dnwedpat_prefix + "system", curline):
					system = ""
				if re.search(dnwedpat_prefix + "ramdisk", curline):
					ramdisk = ""
				if re.search(dnwedpat_prefix + "recovery", curline):
					recovery = ""
				if re.search(dnwedpat_prefix + ubootpat, curline):
					# using the new uboot after a reboot
					uboot = ""
					s.write("reset\r\n")
				curline = ""
				writing = 0
				dnwing = 0

			if issued_boot:
				# Get serial log through entering into the kernel console
				# Press Enter and input "console"
				if phone == "M9":
					if re.search(enterfiqpat, curline):
						s.write("\n")
					if re.match(fiqpat, curline):
						s.write("console\n")
				# The last sucessful boot is specified a user-space "sh:" prompt
				if re.match(bootedpat, curline):
					issued_boot = 0
					# If finished reboot and uboot reset test, exit the whole test
					if (running_utest and utest<=0):
						log("Uboot", utest_total, utest)
						if not log_always:
							break
					if (running_btest and reboot<=0):
						log("Reboot", reboot_total, reboot)
						if not log_always:
							break

			if x=="\n":
				newline = 1
				if basepat and re.search(basepat, curline):
					basetime = linetime
				curline = ""

			if not quiet_serial_output:
				sys.stdout.write(outline)
				sys.stdout.flush()
			outline = ""

		except:
			break

	myexit(s, ret, retstr)

main()
