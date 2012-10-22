/*
 * ARM Specific module linker script, this will be merged to the
 * common linker script in scripts/module-common.lds
 */

#include <asm-generic/vmlinux.lds.h>
#include <asm/thread_info.h>
#include <asm/memory.h>
#include <asm/page.h>

SECTIONS {
	/DISCARD/ : {
#ifndef CONFIG_MMU
		*(.fixup)
		*(__ex_table)
#endif
	}
}
