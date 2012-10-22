/*
 * Built-in autotest framework
 *
 * Author: Wu Zhangjin <falcon@meizu.com> or <wuzhangjin@gmail.com>
 * Update: Thu Dec  8 10:04:48 CST 2011
 */

#include <linux/wakelock.h>
#include <linux/reboot.h>
#include <linux/delay.h>
#include <linux/syscalls.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include <linux/rtc.h>
#include <linux/suspend.h>
#include <linux/autotest.h>

static struct wake_lock autotest_wake_lock;
/* The other tests can only be started after the dump thread finish */
struct completion dump;

#ifdef CONFIG_AUTOREBOOT_TEST
/* unit: seconds */
#ifdef CONFIG_AUTOREBOOT_CYCLE
#define AUTOREBOOT_CYCLE CONFIG_AUTOREBOOT_CYCLE
#else
#define AUTOREBOOT_CYCLE 60
#endif
static int autoreboot_thread(void *data)
{
	unsigned int autoreboot_cycle;

	autoreboot_cycle = get_random_secs(10, AUTOREBOOT_CYCLE);

	pr_at_info("%s: Enter into autoreboot_thread\n", __func__);

	/* Only allow to start the autoreboot test when normally boot:
	 * This allows users to dump out the previous kernel log
	 * from /proc/last_kmsg when RAM_CONSOLE is enabled.
	 */
	pr_at_info("%s: Sleep %d ms for reboot\n", __func__, autoreboot_cycle);
	msleep_interruptible(autoreboot_cycle);

	pr_at_info("%s: Sync all changes to disk\n", __func__);
	/* Sync the changes from cache to disk */
	sys_sync();

	/* Tell uboot to start a normal reboot to avoid it come to
	 * the charging mode when a USB is inserted
	 */
	pr_at_info("%s: Should start a normal reboot\n", __func__);

	pr_at_info("%s: Stop suspend just before sending out restart command\n", __func__);
#ifdef CONFIG_AUTOSUSPEND_TEST
	wake_lock(&autotest_wake_lock);
#endif
	pr_at_info("%s: Send out the restart command ...\n", __func__);

	kernel_restart(NULL);

	return 0;
}

static void start_autoreboot_thread(void)
{
	struct task_struct *autoreboot_task;

	/* autosuspend have the wakealarm, so, reboot thread will be executed eventually */
#ifndef CONFIG_AUTOSUSPEND_TEST
	/* stop suspend when doing autoreboot test */
	wake_lock(&autotest_wake_lock);
#endif

	pr_at_info("%s: Start autoreboot test thread\n", __func__);
	autoreboot_task = kthread_run(autoreboot_thread, NULL, "autoreboottest/daemon");
	if (IS_ERR(autoreboot_task))
		pr_err(ATP "%s: Fail to create autoreboot_thread\n", __func__);
}
#else
#define start_autoreboot_thread()	do { } while (0)
#endif

#ifdef CONFIG_AUTOSUSPEND_TEST

#ifdef CONFIG_AUTOSUSPEND_CYCLE
#define AUTOSUSPEND_CYCLE CONFIG_AUTOSUSPEND_CYCLE
#else
#define AUTOSUSPEND_CYCLE 60
#endif

#ifdef CONFIG_AUTOSUSPEND_TIME
#define AUTOSUSPEND_TIME CONFIG_AUTOSUSPEND_TIME
#else
#define AUTOSUSPEND_TIME 60
#endif

extern int setup_test_suspend(unsigned long suspend_time);

static int autosuspend_thread(void *data)
{
	unsigned int autosuspend_cycle, autosuspend_time;
	pr_at_info("%s: Enter into autosuspend_thread\n", __func__);

	while (1) {
		autosuspend_cycle = get_random_secs(1, AUTOSUSPEND_CYCLE) / 2;
		/* stop suspend for AUTOSUSPEND_CYCLE / 2 seconds */
		wake_lock(&autotest_wake_lock);
		pr_at_info("%s: Stop suspend for %d ms\n", __func__, autosuspend_cycle);
		msleep_interruptible(autosuspend_cycle);
		wake_unlock(&autotest_wake_lock);

		/* The setup_test_suspend() function accept secs as unit */
		autosuspend_time = get_random_secs(10, AUTOSUSPEND_TIME) / MSEC_PER_SEC;
		pr_at_info("%s: Allow go to suspend, suspend time = %u s\n", __func__, autosuspend_time);
		setup_test_suspend(autosuspend_time);

		autosuspend_cycle = get_random_secs(3, AUTOSUSPEND_CYCLE);
		pr_at_info("%s: Sleep %d ms for suspend\n", __func__, autosuspend_cycle);
		/* allow suspend for AUTOSUSPEND_CYCLE / 2 seconds */
		msleep_interruptible(autosuspend_cycle);
	}

	return 0;
}

static void start_autosuspend_thread(void)
{
	struct task_struct *autosuspend_task;

	pr_at_info("%s: Start autosuspend test thread\n", __func__);
	autosuspend_task = kthread_run(autosuspend_thread, NULL, "autosuspendtest/daemon");
	if (IS_ERR(autosuspend_task))
		pr_err(ATP "%s: Fail to create autosuspend_thread\n", __func__);
}

#else
#define start_autosuspend_thread()	do { } while (0)
#endif

#ifdef CONFIG_DUMP_LAST_KMSG

#define DISK_MOUNT_TIME	CONFIG_DISK_MOUNT_TIME	/* secs */

static int dump_thread(void *data)
{
	struct rtc_device *rtc;
	struct rtc_time tm;
	int err = 0;
	char log_file_name[100];
	char backup_log_file_name[100];

	pr_at_info("%s: Enter into dump_thread\n", __func__);

	/* Dump the last kernel log */
	/* MX have two rtc devices */
	rtc = rtc_class_open("rtc0");
	if (rtc == NULL) {
		rtc = rtc_class_open("rtc1");
		if (rtc == NULL) {
			pr_err(ATP "%s: can not open rtc devices\n", __func__);
			err = -ENODEV;
		}
	}
	if (!err) {
		err = rtc_read_time(rtc, &tm);
		if (err)
			pr_err(ATP "%s: unable to read the hardware clock\n", __func__);
	}

	sprintf(log_file_name, "%s-%d%02d%02d_%02d%02d%02d", CONFIG_LAST_KMSG_LOG_FILE,
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour + 8, tm.tm_min, tm.tm_sec);
	pr_at_info("%s: Get log file name: %s\n", __func__, log_file_name);
	err = dump_last_kmsg(log_file_name);
	if (err) {
		pr_err(ATP "%s: Failed dump kernel log to %s\n", __func__, log_file_name);
		sprintf(backup_log_file_name, "%s-%d%02d%02d_%02d%02d%02d", CONFIG_BACKUP_LAST_KMSG_LOG_FILE,
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour + 8, tm.tm_min, tm.tm_sec);
		pr_at_info("%s: Get backup log file name: %s\n", __func__, backup_log_file_name);
		err = dump_last_kmsg(backup_log_file_name);
		if (err) {
			pr_err(ATP "%s: Failed dump kernel log to %s\n", __func__, backup_log_file_name);
			goto out;
		} else {
			pr_at_info("%s: kernel log file dumped to %s\n", __func__, backup_log_file_name);
		}
	} else {
		pr_at_info("%s: kernel log file dumped to %s\n", __func__, log_file_name);
	}

out:
	complete_and_exit(&dump, 0);

	return err;
}

static void start_dump_thread(void)
{
	struct task_struct *dump_task;

	pr_at_info("%s: Wating for the disk being mounted\n", __func__);
	msleep_interruptible(DISK_MOUNT_TIME * MSEC_PER_SEC);

	pr_at_info("%s: Start dump thread\n", __func__);
	dump_task = kthread_run(dump_thread, NULL, "dumptest/daemon");
	if (IS_ERR(dump_task))
		pr_err(ATP "%s: Fail to create dump_thread\n", __func__);
}
#else
#define start_dump_thread()	do { } while (0)
#endif

static int autotest_thread(void *data)
{
	/* Init a wake lock for autotest */
	wake_lock_init(&autotest_wake_lock, WAKE_LOCK_SUSPEND, "autotest");

	pr_at_info("%s: start autotest\n", __func__);
	if (boot_from_crash()) {
		init_completion(&dump);

		/* stop suspend when start from an unormal boot, which allows the admin to track the logs */
		wake_lock(&autotest_wake_lock);

		/* Start a thread to dump out the last kernel message */
		start_dump_thread();

		wake_unlock(&autotest_wake_lock);

		/* Wait for the dump thread finish */
		wait_for_completion_interruptible(&dump);
	}

	start_autosuspend_thread();
	start_autoreboot_thread();

	return 0;
}

void __init start_autotest(void)
{
	struct task_struct *autotest_task;

	pr_at_info("%s: Start autotest thread\n", __func__);
	autotest_task = kthread_run(autotest_thread, NULL, "autotesttest/daemon");
	if (IS_ERR(autotest_task))
		pr_err(ATP "%s: Fail to create autotest_thread\n", __func__);
}
