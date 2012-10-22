/*
 * async.c: Asynchronous function calls for boot performance
 *
 * (C) Copyright 2009 Intel Corporation
 * Author: Arjan van de Ven <arjan@linux.intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */


/*

Goals and Theory of Operation

The primary goal of this feature is to reduce the kernel boot time,
by doing various independent hardware delays and discovery operations
decoupled and not strictly serialized.

More specifically, the asynchronous function call concept allows
certain operations (primarily during system boot) to happen
asynchronously, out of order, while these operations still
have their externally visible parts happen sequentially and in-order.
(not unlike how out-of-order CPUs retire their instructions in order)

Key to the asynchronous function call implementation is the concept of
a "sequence cookie" (which, although it has an abstracted type, can be
thought of as a monotonically incrementing number).

The async core will assign each scheduled event such a sequence cookie and
pass this to the called functions.

The asynchronously called function should before doing a globally visible
operation, such as registering device numbers, call the
async_synchronize_cookie() function and pass in its own cookie. The
async_synchronize_cookie() function will make sure that all asynchronous
operations that were scheduled prior to the operation corresponding with the
cookie have completed.

Subsystem/driver initialization code that scheduled asynchronous probe
functions, but which shares global resources with other drivers/subsystems
that do not use the asynchronous call feature, need to do a full
synchronization with the async_synchronize_full() function, before returning
from their init function. This is to maintain strict ordering between the
asynchronous and synchronous parts of the kernel.

*/

#include <linux/async.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <asm/atomic.h>

static async_cookie_t next_cookie = 1;

#define MAX_WORK	32768

static LIST_HEAD(async_pending);
static LIST_HEAD(async_running);
static DEFINE_SPINLOCK(async_lock);

struct async_entry {
	char			*name;
	struct list_head	list;
	struct work_struct	work;
	async_cookie_t		cookie;
	async_func_ptr		*func;
	void			*data;
	struct list_head	*running;
};

static DECLARE_WAIT_QUEUE_HEAD(async_done);

static atomic_t entry_count;

extern int initcall_debug;


/*
 * MUST be called with the lock held!
 */
static async_cookie_t  __lowest_in_progress(struct list_head *running)
{
	struct async_entry *entry;

	if (!list_empty(running)) {
		entry = list_first_entry(running,
			struct async_entry, list);
		return entry->cookie;
	}

	list_for_each_entry(entry, &async_pending, list)
		if (entry->running == running)
			return entry->cookie;

	return next_cookie;	/* "infinity" value */
}

static async_cookie_t  lowest_in_progress(struct list_head *running)
{
	unsigned long flags;
	async_cookie_t ret;

	spin_lock_irqsave(&async_lock, flags);
	ret = __lowest_in_progress(running);
	spin_unlock_irqrestore(&async_lock, flags);
	return ret;
}

static void async_run_entry_fn_debug(struct async_entry *entry)
{
	ktime_t calltime, delta, rettime;

	printk("calling  %lli_%pF @ %i\n", (long long)entry->cookie,
		entry->func, task_pid_nr(current));
	calltime = ktime_get();

	entry->func(entry->data, entry->cookie);

	rettime = ktime_get();
	delta = ktime_sub(rettime, calltime);
	printk("initcall %lli_%pF returned 0 after %lld usecs\n",
		(long long)entry->cookie,
		entry->func,
		(long long)ktime_to_ns(delta) >> 10);
}

static char  *__lowest_in_progress_name(struct list_head *running)
{
	struct async_entry *entry;

	if (!list_empty(running)) {
		entry = list_first_entry(running,
			struct async_entry, list);
		return entry->name;
	}

	list_for_each_entry(entry, &async_pending, list)
		if (entry->running == running)
			return entry->name;

	return NULL;	/* "infinity" value */
}

static char  *lowest_in_progress_name(struct list_head *running)
{
	unsigned long flags;
	char *name;

	spin_lock_irqsave(&async_lock, flags);
	name = __lowest_in_progress_name(running);
	spin_unlock_irqrestore(&async_lock, flags);
	return name;
}

/*
 * pick the first pending entry and run it
 */
static void async_run_entry_fn(struct work_struct *work)
{
	struct async_entry *entry =
		container_of(work, struct async_entry, work);
	unsigned long flags;

	/* 1) move self to the running queue */
	spin_lock_irqsave(&async_lock, flags);
	list_move_tail(&entry->list, entry->running);
	spin_unlock_irqrestore(&async_lock, flags);

	/* 2) run (and print duration) */
	if (initcall_debug && system_state == SYSTEM_BOOTING)
		async_run_entry_fn_debug(entry);
	else {
		pr_debug("%s: async_runs @ cookie = %llu, %s\n", __func__, entry->cookie, entry->name);
		entry->func(entry->data, entry->cookie);
	}

	/* 3) remove self from the running queue */
	spin_lock_irqsave(&async_lock, flags);
	list_del(&entry->list);

	/* 4) record the entry and will free it later */
	pr_debug("%s: async_finishs @ cookie = %llu, %s\n", __func__, entry->cookie, entry->name);
	if (entry->name)
		kfree(entry->name);
	kfree(entry);
	atomic_dec(&entry_count);

	spin_unlock_irqrestore(&async_lock, flags);

	/* 5) wake up any waiters */
	wake_up(&async_done);
}

static async_cookie_t __async_schedule(async_func_ptr *ptr, void *data, const char *name, struct list_head *running)
{
	struct async_entry *entry;
	unsigned long flags;
	async_cookie_t newcookie;

	/* allow irq-off callers */
	entry = kzalloc(sizeof(struct async_entry), GFP_ATOMIC);

	/*
	 * If we're out of memory or if there's too much work
	 * pending already, we execute synchronously.
	 */
	if (!entry || atomic_read(&entry_count) > MAX_WORK) {
		kfree(entry);
		spin_lock_irqsave(&async_lock, flags);
		newcookie = next_cookie++;
		spin_unlock_irqrestore(&async_lock, flags);

		/* low on memory.. run synchronously */
		ptr(data, newcookie);
		return newcookie;
	}
	INIT_WORK(&entry->work, async_run_entry_fn);
	entry->func = ptr;
	entry->data = data;
	entry->running = running;
	if (name) {
		entry->name = kzalloc(strlen(name) + 1, GFP_ATOMIC);
		strncpy(entry->name, name, strlen(name) + 1);
	}

	spin_lock_irqsave(&async_lock, flags);
	newcookie = entry->cookie = next_cookie++;
	list_add_tail(&entry->list, &async_pending);
	atomic_inc(&entry_count);
	spin_unlock_irqrestore(&async_lock, flags);

	/* schedule for execution */
	queue_work(system_unbound_wq, &entry->work);

	return newcookie;
}

/**
 * async_schedule - schedule a function for asynchronous execution
 * @ptr: function to execute asynchronously
 * @data: data pointer to pass to the function
 * @name: name pointer to the asynced function name or any unique identity
 *
 * Returns an async_cookie_t that may be used for checkpointing later.
 * Note: This function may be called from atomic or non-atomic contexts.
 */
async_cookie_t async_named_schedule(async_func_ptr *ptr, void *data, const char *name)
{
	async_cookie_t cookie;

	cookie = __async_schedule(ptr, data, name, &async_running);

	pr_debug("%s: async_schedules @ cookie = %llu, %s\n", __func__, cookie, name);

	return cookie;
}
EXPORT_SYMBOL_GPL(async_named_schedule);

/**
 * async_schedule_domain - schedule a function for asynchronous execution within a certain domain
 * @ptr: function to execute asynchronously
 * @data: data pointer to pass to the function
 * @running: running list for the domain
 * @name: name pointer to the asynced function name or any unique identity
 *
 * Returns an async_cookie_t that may be used for checkpointing later.
 * @running may be used in the async_synchronize_*_domain() functions
 * to wait within a certain synchronization domain rather than globally.
 * A synchronization domain is specified via the running queue @running to use.
 * Note: This function may be called from atomic or non-atomic contexts.
 */
async_cookie_t async_named_schedule_domain(async_func_ptr *ptr, void *data,
				     struct list_head *running, const char *name)
{
	return __async_schedule(ptr, data, name, running);
}
EXPORT_SYMBOL_GPL(async_schedule_domain);

/**
 * async_synchronize_full - synchronize all asynchronous function calls
 *
 * This function waits until all asynchronous function calls have been done.
 */
void async_synchronize_full(void)
{
	long ret;
	do {
		ret = async_synchronize_cookie(next_cookie);
		if (!ret) {
			WARN(1, "Detected timeout of async-scheduled functions\n");
			break;
		}
	} while (!list_empty(&async_running) || !list_empty(&async_pending));
}
EXPORT_SYMBOL_GPL(async_synchronize_full);

/**
 * async_synchronize_full_domain - synchronize all asynchronous function within a certain domain
 * @list: running list to synchronize on
 *
 * This function waits until all asynchronous function calls for the
 * synchronization domain specified by the running list @list have been done.
 */
void async_synchronize_full_domain(struct list_head *list)
{
	async_synchronize_cookie_domain(next_cookie, list);
}
EXPORT_SYMBOL_GPL(async_synchronize_full_domain);


unsigned long async_synchronize_cookie_domain_debug(async_cookie_t cookie,
				     struct list_head *running, unsigned long ret)
{
	ktime_t starttime, delta, endtime;

	printk("async_waiting @ %i\n", task_pid_nr(current));
	starttime = ktime_get();

	ret = wait_event_timeout(async_done, lowest_in_progress(running) >= cookie, ret);

	endtime = ktime_get();
	delta = ktime_sub(endtime, starttime);

	printk("async_continuing @ %i after %lli usec\n",
		task_pid_nr(current),
		(long long)ktime_to_ns(delta) >> 10);

	return ret;
}

/**
 * async_synchronize_cookie_domain - synchronize asynchronous function calls within a certain domain with cookie checkpointing
 * @cookie: async_cookie_t to use as checkpoint
 * @running: running list to synchronize on
 *
 * This function waits until all asynchronous function calls for the
 * synchronization domain specified by the running list @list submitted
 * prior to @cookie have been done.
 *
 * The function returns 0 if the @timeout elapsed, and the remaining
 * jiffies if the condition evaluated to true before the timeout elapsed.
 */
unsigned long async_synchronize_cookie_domain(async_cookie_t cookie,
				     struct list_head *running)
{
	unsigned long ret = msecs_to_jiffies(CONFIG_DEFAULT_ASYNC_SCHED_TIMEOUT * MSEC_PER_SEC);
	async_cookie_t lipr_cookie;
	char *lipr_name;

	if (initcall_debug && system_state == SYSTEM_BOOTING)
		ret = async_synchronize_cookie_domain_debug(cookie, running, ret);
	else
		ret = wait_event_timeout(async_done, lowest_in_progress(running) >= cookie, ret);

	if (!ret) {
		lipr_cookie = lowest_in_progress(running);
		lipr_name = lowest_in_progress_name(running);

		/* If we can reboot, panic here instead of warning */
		if (panic_timeout > 0)
			panic("%s: wait_event_timeout @ cookie = %llu, %s\n", __func__, \
				lipr_cookie, lipr_name);
		else
			WARN(1, "%s: wait_event_timeout @ cookie = %llu, %s\n", __func__, \
				lipr_cookie, lipr_name);
	} else {
		/* Note: lowest_in_progress_name() can not be called in this branch for
		the entry would have been released for a non-blocked event. */
	}

	return ret;
}
EXPORT_SYMBOL_GPL(async_synchronize_cookie_domain);

/**
 * async_synchronize_cookie - synchronize asynchronous function calls with cookie checkpointing
 * @cookie: async_cookie_t to use as checkpoint
 *
 * This function waits until all asynchronous function calls prior to @cookie
 * have been done.
 */
unsigned long async_synchronize_cookie(async_cookie_t cookie)
{
	return async_synchronize_cookie_domain(cookie, &async_running);
}
EXPORT_SYMBOL_GPL(async_synchronize_cookie);
