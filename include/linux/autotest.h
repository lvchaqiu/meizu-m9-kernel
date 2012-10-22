#ifndef _LINUX_AUTOTEST_H
#define _LINUX_AUTOTEST_H

#include <linux/random.h>
#include <linux/printk.h>
#include <linux/kmsg_dump.h>
#include <linux/kernel.h>

#define AUTOTEST_PREFIX "AUTOTEST: "
#define ATP	AUTOTEST_PREFIX
#define pr_at_info(fmt, ...) \
	pr_info(ATP pr_fmt(fmt), ##__VA_ARGS__)

/* From 1 to max, max <= 255 */
static inline int get_random(unsigned int max)
{
	unsigned short random = 0;

#ifdef CONFIG_AUTOTEST_RANDOM
	get_random_bytes(&random, 2);
#endif
	return 1 + (random & max);
}

static inline int get_random_secs(unsigned int max, unsigned int msecs)
{
	return get_random(max) * msecs * MSEC_PER_SEC;
}

#ifdef CONFIG_AUTOTEST
extern void start_autotest(void);
#else
#define start_autotest()	do { } while (0)
#endif

/* provided by ANDROID_RAM_CONSOLE */
#ifdef CONFIG_ANDROID_RAM_CONSOLE
extern int dump_last_kmsg(char *log_file);
extern void record_boot_reason(enum kmsg_dump_reason reason);
extern int boot_from_crash(void);
#else
#define dump_last_kmsg(f)	do { } while (0)
#define record_boot_reason(r)	do { } while (0)
#define boot_from_crash()	(0)
#endif

#ifdef CONFIG_PANIC_ON_WARN
#define panic_on_warn() \
	panic("Please check this warning! if it is okay, disable CONFIG_PANIC_ON_WARN and ignore it!\n")
#else
#define panic_on_warn()			do {} while (0)
#endif

#ifdef CONFIG_PANIC_ON_BAD_IRQ
#define panic_on_bad_irq() \
	panic("Please check bad irq! if it is okay, disable CONFIG_PANIC_ON_BAD_IRQ and ignore it!\n")
#else
#define panic_on_bad_irq()			do {} while (0)
#endif

#ifdef CONFIG_PANIC_ON_KMEMLEAK
#define panic_on_kmemleak() \
	panic("Please check the above kernel memory leak! if it is okay, disable CONFIG_PANIC_ON_KMEMLEAK and ignore it through kmemleak_not_leak()!\n")
#else
#define panic_on_kmemleak()			do {} while (0)
#endif

#ifdef CONFIG_PANIC_ON_LOCK_BUG
#define panic_on_lock_bug() \
	panic("Please check this lock detection report! after fix it, disable CONFIG_PANIC_ON_LOCK_BUG!\n");
#else
#define panic_on_lock_bug()		do {} while (0)
#endif

static inline void dump_stack_and_panic(void)
{
	dump_stack();
	panic_on_lock_bug();
}

/* for testing autosuspend */
#ifdef CONFIG_PM_TEST_SUSPEND
extern struct timespec suspend_return_time;
extern void show_suspend_statistic(void);
#else
#define show_suspend_statistic()
#endif

#endif /* _LINUX_AUTOTEST_H */
