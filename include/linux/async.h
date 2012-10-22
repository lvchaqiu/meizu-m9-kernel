/*
 * async.h: Asynchronous function calls for boot performance
 *
 * (C) Copyright 2009 Intel Corporation
 * Author: Arjan van de Ven <arjan@linux.intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/types.h>
#include <linux/list.h>
#include <linux/stringify.h>

typedef u64 async_cookie_t;
typedef void (async_func_ptr) (void *data, async_cookie_t cookie);

extern async_cookie_t async_named_schedule(async_func_ptr *ptr, void *data, const char *name);
extern async_cookie_t async_named_schedule_domain(async_func_ptr *ptr, void *data,
					    struct list_head *list, const char *name);

#define async_schedule(ptr, data) \
	async_named_schedule(ptr, data, __stringify(ptr))
#define async_schedule_domain(ptr, data, list) \
	async_named_schedule_domain(ptr, data, list, __stringify(ptr))

extern void async_synchronize_full(void);
extern void async_synchronize_full_domain(struct list_head *list);
extern unsigned long async_synchronize_cookie(async_cookie_t cookie);
extern unsigned long async_synchronize_cookie_domain(async_cookie_t cookie,
					    struct list_head *list);

