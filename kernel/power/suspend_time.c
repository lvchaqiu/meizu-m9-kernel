/*
 * debugfs file to track time spent in suspend
 *
 * Copyright (c) 2011, Google, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/seq_file.h>
#include <linux/syscore_ops.h>
#include <linux/time.h>
#include <linux/autotest.h>
#include <linux/rtc.h>

static struct timespec suspend_time_before;
static unsigned long long time_in_suspend_bins[32];
static unsigned long long times_in_suspend;
static struct timespec time_in_suspend;

#ifdef CONFIG_DEBUG_FS
static int suspend_time_debug_show(struct seq_file *s, void *data)
{
	int bin;
	seq_printf(s, "time (secs)  count\n");
	seq_printf(s, "------------------\n");
	for (bin = 0; bin < 32; bin++) {
		if (time_in_suspend_bins[bin] == 0)
			continue;
		seq_printf(s, "%4d - %4d %8llu\n",
			bin ? 1 << (bin - 1) : 0, 1 << bin,
				time_in_suspend_bins[bin]);
	}
	seq_printf(s, "total: %lu.%03lu seconds, %llu times\n", time_in_suspend.tv_sec,
		time_in_suspend.tv_nsec / NSEC_PER_MSEC, times_in_suspend);
	return 0;
}

static int suspend_time_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, suspend_time_debug_show, NULL);
}

static const struct file_operations suspend_time_debug_fops = {
	.open		= suspend_time_debug_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init suspend_time_debug_init(void)
{
	struct dentry *d;

	d = debugfs_create_file("suspend_time", 0755, NULL, NULL,
		&suspend_time_debug_fops);
	if (!d) {
		pr_err("Failed to create suspend_time debug file\n");
		return -ENOMEM;
	}

	return 0;
}

late_initcall(suspend_time_debug_init);
#endif

static int suspend_time_syscore_suspend(void)
{
	read_persistent_clock(&suspend_time_before);

	return 0;
}

static void suspend_time_syscore_resume(void)
{
	struct timespec after;
	struct rtc_time tm;

	read_persistent_clock(&after);
	rtc_time_to_tm(suspend_time_before.tv_sec, &tm);

	pr_info("\nSuspend @ %d-%02d-%02d %02d:%02d:%02d\n",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour + 8, tm.tm_min, tm.tm_sec);
	rtc_time_to_tm(after.tv_sec, &tm);
	pr_info("Resume @ %d-%02d-%02d %02d:%02d:%02d\n",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour + 8, tm.tm_min, tm.tm_sec);

	after = timespec_sub(after, suspend_time_before);

	time_in_suspend_bins[fls(after.tv_sec)]++;
	time_in_suspend = timespec_add(time_in_suspend, after);
	times_in_suspend++;

	pr_info("Suspended for %lu.%03lu seconds, total %lu.%03lu seconds, total %llu times\n", after.tv_sec,
		after.tv_nsec / NSEC_PER_MSEC, time_in_suspend.tv_sec, time_in_suspend.tv_nsec / NSEC_PER_MSEC,
		times_in_suspend);
	show_suspend_statistic();
}

static struct syscore_ops suspend_time_syscore_ops = {
	.suspend = suspend_time_syscore_suspend,
	.resume = suspend_time_syscore_resume,
};

static int suspend_time_syscore_init(void)
{
	register_syscore_ops(&suspend_time_syscore_ops);

	return 0;
}

static void suspend_time_syscore_exit(void)
{
	unregister_syscore_ops(&suspend_time_syscore_ops);
}
module_init(suspend_time_syscore_init);
module_exit(suspend_time_syscore_exit);
