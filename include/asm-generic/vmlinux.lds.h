/*
 * Helper macros to support writing architecture specific
 * linker scripts.
 *
 * A minimal linker scripts has following content:
 * [This is a sample, architectures may have special requiriements]
 *
 * OUTPUT_FORMAT(...)
 * OUTPUT_ARCH(...)
 * ENTRY(...)
 * SECTIONS
 * {
 *	. = START;
 *	__init_begin = .;
 *	HEAD_TEXT_SECTION
 *	INIT_TEXT_SECTION(PAGE_SIZE)
 *	INIT_DATA_SECTION(...)
 *	PERCPU_SECTION(CACHELINE_SIZE)
 *	__init_end = .;
 *
 *	_stext = .;
 *	TEXT_SECTION = 0
 *	_etext = .;
 *
 *      _sdata = .;
 *	RO_DATA_SECTION(PAGE_SIZE)
 *	RW_DATA_SECTION(...)
 *	_edata = .;
 *
 *	EXCEPTION_TABLE(...)
 *	NOTES
 *
 *	BSS_SECTION(0, 0, 0)
 *	_end = .;
 *
 *	STABS_DEBUG
 *	DWARF_DEBUG
 *
 *	DISCARDS		// must be the last
 * }
 *
 * [__init_begin, __init_end] is the init section that may be freed after init
 * [_stext, _etext] is the text section
 * [_sdata, _edata] is the data section
 *
 * Some of the included output section have their own set of constants.
 * Examples are: [__initramfs_start, __initramfs_end] for initramfs and
 *               [__nosave_begin, __nosave_end] for the nosave data
 */

#ifdef CONFIG_FIXED_SECTIONS
# define __all(s)	s.*
#else	/* Don't enable sections support */
# define __all(s)
#endif

#define ALL(s)	*(s __all(s))
#define KEEP_ALL(s)	KEEP(ALL(s))

#ifndef LOAD_OFFSET
#define LOAD_OFFSET 0
#endif

#ifndef SYMBOL_PREFIX
#define VMLINUX_SYMBOL(sym) sym
#else
#define PASTE2(x,y) x##y
#define PASTE(x,y) PASTE2(x,y)
#define VMLINUX_SYMBOL(sym) PASTE(SYMBOL_PREFIX, sym)
#endif

/* Align . to a 8 byte boundary equals to maximum function alignment. */
#define ALIGN_FUNCTION()  . = ALIGN(8)

/*
 * Align to a 32 byte boundary equal to the
 * alignment gcc 4.5 uses for a struct
 */
#define STRUCT_ALIGNMENT 32
#define STRUCT_ALIGN() . = ALIGN(STRUCT_ALIGNMENT)

/* The actual configuration determine if the init/exit sections
 * are handled as text/data or they can be discarded (which
 * often happens at runtime)
 */
#ifdef CONFIG_HOTPLUG
#define DEV_KEEP(sec)    *(.dev##sec)
#define DEV_DISCARD(sec)
#else
#define DEV_KEEP(sec)
#define DEV_DISCARD(sec) *(.dev##sec)
#endif

#ifdef CONFIG_HOTPLUG_CPU
#define CPU_KEEP(sec)    *(.cpu##sec)
#define CPU_DISCARD(sec)
#else
#define CPU_KEEP(sec)
#define CPU_DISCARD(sec) *(.cpu##sec)
#endif

#if defined(CONFIG_MEMORY_HOTPLUG)
#define MEM_KEEP(sec)    *(.mem##sec)
#define MEM_DISCARD(sec)
#else
#define MEM_KEEP(sec)
#define MEM_DISCARD(sec) *(.mem##sec)
#endif

#ifdef CONFIG_FTRACE_MCOUNT_RECORD
#define MCOUNT_REC()	. = ALIGN(8);				\
			VMLINUX_SYMBOL(__start_mcount_loc) = .; \
			ALL(__mcount_loc)			\
			VMLINUX_SYMBOL(__stop_mcount_loc) = .;
#else
#define MCOUNT_REC()
#endif

#ifdef CONFIG_TRACE_BRANCH_PROFILING
#define LIKELY_PROFILE()	VMLINUX_SYMBOL(__start_annotated_branch_profile) = .; \
				ALL(_ftrace_annotated_branch)			      \
				VMLINUX_SYMBOL(__stop_annotated_branch_profile) = .;
#else
#define LIKELY_PROFILE()
#endif

#ifdef CONFIG_PROFILE_ALL_BRANCHES
#define BRANCH_PROFILE()	VMLINUX_SYMBOL(__start_branch_profile) = .;   \
				ALL(_ftrace_branch)			      \
				VMLINUX_SYMBOL(__stop_branch_profile) = .;
#else
#define BRANCH_PROFILE()
#endif

#ifdef CONFIG_EVENT_TRACING
#define FTRACE_EVENTS()	. = ALIGN(8);					\
			VMLINUX_SYMBOL(__start_ftrace_events) = .;	\
			ALL(_ftrace_events)				\
			VMLINUX_SYMBOL(__stop_ftrace_events) = .;
#else
#define FTRACE_EVENTS()
#endif

#ifdef CONFIG_TRACING
#define TRACE_PRINTKS() VMLINUX_SYMBOL(__start___trace_bprintk_fmt) = .;      \
			 ALL(__trace_printk_fmt) /* Trace_printk fmt' pointer */ \
			 VMLINUX_SYMBOL(__stop___trace_bprintk_fmt) = .;
#else
#define TRACE_PRINTKS()
#endif

#ifdef CONFIG_FTRACE_SYSCALLS
#define TRACE_SYSCALLS() . = ALIGN(8);					\
			 VMLINUX_SYMBOL(__start_syscalls_metadata) = .;	\
			 ALL(__syscalls_metadata)				\
			 VMLINUX_SYMBOL(__stop_syscalls_metadata) = .;
#else
#define TRACE_SYSCALLS()
#endif


#define KERNEL_DTB()							\
	STRUCT_ALIGN();							\
	VMLINUX_SYMBOL(__dtb_start) = .;				\
	ALL(.dtb.init.rodata)						\
	VMLINUX_SYMBOL(__dtb_end) = .;

#ifdef CONFIG_DYNAMIC_DEBUG
#define DEBUG_VERBOSE()						\
	VMLINUX_SYMBOL(__start___verbose) = .;				\
	ALL(__verbose)							\
	VMLINUX_SYMBOL(__stop___verbose) = .;
#else
#define DEBUG_VERBOSE()
#endif

#ifdef CONFIG_MARKERS
#define TRACE_MARKERS()							\
	VMLINUX_SYMBOL(__start___markers) = .;				\
	ALL(__markers)							\
	VMLINUX_SYMBOL(__stop___markers) = .;
#define TRACE_MARKERS_STRINGS()						\
	ALL(__markers_strings)
#else
#define TRACE_MARKERS()
#define TRACE_MARKERS_STRINGS()
#endif

#ifdef CONFIG_TRACEPOINTS
#define TRACEPOINTS()							\
	ALL(*__tracepoints)
#define TRACEPOINTS_STRINGS()						\
	ALL(__tracepoints_strings)
#define TRACEPOINTS_PTRS()						\
	VMLINUX_SYMBOL(__start___tracepoints_ptrs) = .;			\
	ALL(__tracepoints_ptrs)	/* Tracepoints: pointer array */	\
	VMLINUX_SYMBOL(__stop___tracepoints_ptrs) = .;
#else
#define TRACEPOINTS()
#define TRACEPOINTS_PTRS()
#define TRACEPOINTS_STRINGS()
#endif

/* Text section helpers */
#define PAGE_ALIGNED_TEXT(page_align)					\
	. = ALIGN(page_align);						\
	ALL(.text..page_aligned)

/* .data.foo are generated by gcc itself with -fdata-sections,
 * whereas double-dot sections (like .data..percpu) are generated
 * by kernel's magic macros.
 *
 * arch/.../vmlinux.lds.S decides where to place various double-dot sections
 * as needed by its arch, here DATA_DATA needs to be careful and collect
 * only .data and .data.foo sections, skipping .data..foo
 *
 * Same goes for .text, .bss and .rodata. In case of .rodata, various
 * .rodata.foo sections are generated by gcc even without -fdata-sections
 */

/* .data section */
#define DATA_DATA							\
	ALL(.data..shared_aligned) /* percpu related */			\
	ALL(.data)							\
	ALL(.ref.data)							\
	DEV_KEEP(init.data*)						\
	DEV_KEEP(exit.data*)						\
	CPU_KEEP(init.data*)						\
	CPU_KEEP(exit.data*)						\
	MEM_KEEP(init.data*)						\
	MEM_KEEP(exit.data*)						\
	STRUCT_ALIGN();							\
	TRACEPOINTS()							\
	/* implement dynamic printk debug */				\
	. = ALIGN(8);                                                   \
	VMLINUX_SYMBOL(__start___jump_table) = .;                       \
	ALL(__jump_table)						\
	VMLINUX_SYMBOL(__stop___jump_table) = .;                        \
	. = ALIGN(8);							\
	DEBUG_VERBOSE()							\
	LIKELY_PROFILE()		       				\
	BRANCH_PROFILE()						\
	TRACE_PRINTKS()

/*
 * Data section helpers
 */
#define NOSAVE_DATA							\
	. = ALIGN(PAGE_SIZE);						\
	VMLINUX_SYMBOL(__nosave_begin) = .;				\
	ALL(.data..nosave)						\
	. = ALIGN(PAGE_SIZE);						\
	VMLINUX_SYMBOL(__nosave_end) = .;

#define PAGE_ALIGNED_DATA(page_align)					\
	. = ALIGN(page_align);						\
	ALL(.data..page_aligned)

#define READ_MOSTLY_DATA(align)						\
	. = ALIGN(align);						\
	ALL(.data..read_mostly)						\
	. = ALIGN(align);

#define CACHELINE_ALIGNED_DATA(align)					\
	. = ALIGN(align);						\
	ALL(.data..cacheline_aligned)

#define INIT_TASK_DATA(align)						\
	. = ALIGN(align);						\
	ALL(.data..init_task)

#ifdef CONFIG_MODULES
#ifdef CONFIG_MODVERSIONS
#define KCRC_DATA							\
	/* Kernel symbol table: Normal symbols */			\
	__kcrctab         : AT(ADDR(__kcrctab) - LOAD_OFFSET) {		\
		VMLINUX_SYMBOL(__start___kcrctab) = .;			\
		KEEP_ALL(SORT(__kcrctab+*))				\
		VMLINUX_SYMBOL(__stop___kcrctab) = .;			\
	}								\
	/* Kernel symbol table: GPL-only symbols */			\
	__kcrctab_gpl     : AT(ADDR(__kcrctab_gpl) - LOAD_OFFSET) {	\
		VMLINUX_SYMBOL(__start___kcrctab_gpl) = .;		\
		KEEP_ALL(SORT(__kcrctab_gpl+*))				\
		VMLINUX_SYMBOL(__stop___kcrctab_gpl) = .;		\
	}								\
	/* Kernel symbol table: GPL-only unused symbols */		\
	__kcrctab_unused_gpl : AT(ADDR(__kcrctab_unused_gpl) - LOAD_OFFSET) { \
		VMLINUX_SYMBOL(__start___kcrctab_unused_gpl) = .;	\
		ALL(SORT(__kcrctab_unused_gpl+*))			\
		VMLINUX_SYMBOL(__stop___kcrctab_unused_gpl) = .;	\
	}								\
	/* Kernel symbol table: GPL-future-only symbols */		\
	__kcrctab_gpl_future : AT(ADDR(__kcrctab_gpl_future) - LOAD_OFFSET) { \
		VMLINUX_SYMBOL(__start___kcrctab_gpl_future) = .;	\
		ALL(SORT(__kcrctab_gpl_future+*))			\
		VMLINUX_SYMBOL(__stop___kcrctab_gpl_future) = .;	\
	}
#else /* CONFIG_MODVERSIONS */
#define KCRC_DATA
#endif

#define KSYM_DATA							\
	/* Kernel symbol table: Normal symbols */			\
	__ksymtab         : AT(ADDR(__ksymtab) - LOAD_OFFSET) {		\
		VMLINUX_SYMBOL(__start___ksymtab) = .;			\
		KEEP_ALL(SORT(___ksymtab+*))				\
		VMLINUX_SYMBOL(__stop___ksymtab) = .;			\
	}								\
									\
	/* Kernel symbol table: GPL-only symbols */			\
	__ksymtab_gpl     : AT(ADDR(__ksymtab_gpl) - LOAD_OFFSET) {	\
		VMLINUX_SYMBOL(__start___ksymtab_gpl) = .;		\
		KEEP_ALL(SORT(___ksymtab_gpl+*))			\
		VMLINUX_SYMBOL(__stop___ksymtab_gpl) = .;		\
	}								\
									\
	/* Kernel symbol table: Normal unused symbols */		\
	__ksymtab_unused  : AT(ADDR(__ksymtab_unused) - LOAD_OFFSET) {	\
		VMLINUX_SYMBOL(__start___ksymtab_unused) = .;		\
		KEEP_ALL(SORT(___ksymtab_unused+*))			\
		VMLINUX_SYMBOL(__stop___ksymtab_unused) = .;		\
	}								\
									\
	/* Kernel symbol table: GPL-only unused symbols */		\
	__ksymtab_unused_gpl : AT(ADDR(__ksymtab_unused_gpl) - LOAD_OFFSET) { \
		VMLINUX_SYMBOL(__start___ksymtab_unused_gpl) = .;	\
		KEEP_ALL(SORT(___ksymtab_unused_gpl+*))			\
		VMLINUX_SYMBOL(__stop___ksymtab_unused_gpl) = .;	\
	}								\
									\
	/* Kernel symbol table: GPL-future-only symbols */		\
	__ksymtab_gpl_future : AT(ADDR(__ksymtab_gpl_future) - LOAD_OFFSET) { \
		VMLINUX_SYMBOL(__start___ksymtab_gpl_future) = .;	\
		KEEP_ALL(SORT(___ksymtab_gpl_future+*))			\
		VMLINUX_SYMBOL(__stop___ksymtab_gpl_future) = .;	\
	}								\
	/* Kernel symbol table: strings */				\
        __ksymtab_strings : AT(ADDR(__ksymtab_strings) - LOAD_OFFSET) {	\
		KEEP_ALL(__ksymtab_strings)				\
	}								\
	KCRC_DATA

#ifdef CONFIG_FIXED_SECTIONS
#define MOD_MAGIC_TEXT							\
	.text..page_aligned 0 : AT(0) {	ALL(.text..page_aligned) }

#define MOD_MAGIC_DATA							\
	.data..percpu 0: AT(0) {					\
		ALL(.data..percpu..first)				\
		ALL(.data..percpu..page_aligned)			\
		ALL(.data..percpu..shared_aligned)			\
		ALL(.data..percpu)					\
	}
#else
#define MOD_MAGIC_TEXT
#define MOD_MAGIC_DATA
#endif

#else /* !CONFIG_MODULES */
#define KSYM_DATA
#endif

#ifdef CONFIG_PCI_QUIRKS
#define PCI_QUIRKS							\
	/* PCI quirks */						\
	.pci_fixup        : AT(ADDR(.pci_fixup) - LOAD_OFFSET) {	\
		VMLINUX_SYMBOL(__start_pci_fixups_early) = .;		\
		KEEP_ALL(.pci_fixup_early)				\
		VMLINUX_SYMBOL(__end_pci_fixups_early) = .;		\
		VMLINUX_SYMBOL(__start_pci_fixups_header) = .;		\
		KEEP_ALL(.pci_fixup_header)				\
		VMLINUX_SYMBOL(__end_pci_fixups_header) = .;		\
		VMLINUX_SYMBOL(__start_pci_fixups_final) = .;		\
		KEEP_ALL(.pci_fixup_final)				\
		VMLINUX_SYMBOL(__end_pci_fixups_final) = .;		\
		VMLINUX_SYMBOL(__start_pci_fixups_enable) = .;		\
		KEEP_ALL(.pci_fixup_enable)				\
		VMLINUX_SYMBOL(__end_pci_fixups_enable) = .;		\
		VMLINUX_SYMBOL(__start_pci_fixups_resume) = .;		\
		KEEP_ALL(.pci_fixup_resume)				\
		VMLINUX_SYMBOL(__end_pci_fixups_resume) = .;		\
		VMLINUX_SYMBOL(__start_pci_fixups_resume_early) = .;	\
		KEEP_ALL(.pci_fixup_resume_early)			\
		VMLINUX_SYMBOL(__end_pci_fixups_resume_early) = .;	\
		VMLINUX_SYMBOL(__start_pci_fixups_suspend) = .;		\
		KEEP_ALL(.pci_fixup_suspend)				\
		VMLINUX_SYMBOL(__end_pci_fixups_suspend) = .;		\
	}
#else /* !CONFIG_PCI_QUIRKS */
#define PCI_QUIRKS
#endif

/*
 * Read only Data
 */
#define RO_DATA_SECTION(align)						\
	. = ALIGN((align));						\
	.rodata           : AT(ADDR(.rodata) - LOAD_OFFSET) {		\
		VMLINUX_SYMBOL(__start_rodata) = .;			\
		ALL(.rodata)						\
		*(__vermagic)		/* Kernel version magic */	\
		. = ALIGN(8);						\
		TRACEPOINTS_PTRS()					\
		TRACE_MARKERS()						\
		TRACEPOINTS_STRINGS()					\
	}								\
									\
	.rodata1          : AT(ADDR(.rodata1) - LOAD_OFFSET) {		\
		ALL(.rodata1)						\
	}								\
									\
	BUG_TABLE							\
									\
	PCI_QUIRKS							\
									\
	/* Built-in firmware blobs */					\
	.builtin_fw        : AT(ADDR(.builtin_fw) - LOAD_OFFSET) {	\
		VMLINUX_SYMBOL(__start_builtin_fw) = .;			\
		KEEP_ALL(.builtin_fw)					\
		VMLINUX_SYMBOL(__end_builtin_fw) = .;			\
	}								\
									\
	/* RapidIO route ops */						\
	.rio_ops        : AT(ADDR(.rio_ops) - LOAD_OFFSET) {		\
		VMLINUX_SYMBOL(__start_rio_switch_ops) = .;		\
		ALL(.rio_switch_ops)					\
		VMLINUX_SYMBOL(__end_rio_switch_ops) = .;		\
	}								\
									\
	TRACEDATA							\
									\
	KSYM_DATA							\
									\
	/* __*init sections */						\
	__init_rodata : AT(ADDR(__init_rodata) - LOAD_OFFSET) {		\
		ALL(.ref.rodata)					\
		DEV_KEEP(init.rodata*)					\
		DEV_KEEP(exit.rodata*)					\
		CPU_KEEP(init.rodata*)					\
		CPU_KEEP(exit.rodata*)					\
		MEM_KEEP(init.rodata*)					\
		MEM_KEEP(exit.rodata*)					\
	}								\
									\
	/* Built-in module parameters. */				\
	__param : AT(ADDR(__param) - LOAD_OFFSET) {			\
		VMLINUX_SYMBOL(__start___param) = .;			\
		KEEP_ALL(__param)					\
		VMLINUX_SYMBOL(__stop___param) = .;			\
	}								\
									\
	/* Built-in module versions. */					\
	__modver : AT(ADDR(__modver) - LOAD_OFFSET) {			\
		VMLINUX_SYMBOL(__start___modver) = .;			\
		*(__modver)						\
		VMLINUX_SYMBOL(__stop___modver) = .;			\
		. = ALIGN((align));					\
		VMLINUX_SYMBOL(__end_rodata) = .;			\
	}								\
	. = ALIGN((align));

/* RODATA & RO_DATA provided for backward compatibility.
 * All archs are supposed to use RO_DATA() */
#define RODATA          RO_DATA_SECTION(4096)
#define RO_DATA(align)  RO_DATA_SECTION(align)

#define SECURITY_INIT							\
	.security_initcall.init : AT(ADDR(.security_initcall.init) - LOAD_OFFSET) { \
		VMLINUX_SYMBOL(__security_initcall_start) = .;		\
		ALL(.security_initcall.init)				\
		VMLINUX_SYMBOL(__security_initcall_end) = .;		\
	}

/* .text section. Map to function alignment to avoid address changes
 * during second ld run in second ld pass when generating System.map */
#define TEXT_TEXT_SECTION(section)					\
		ALIGN_FUNCTION();					\
		*(.text.hot)						\
		section							\
		ALL(.text)						\
		ALL(.ref.text)						\
	DEV_KEEP(init.text*)					\
	DEV_KEEP(exit.text*)					\
	CPU_KEEP(init.text*)					\
	CPU_KEEP(exit.text*)					\
	MEM_KEEP(init.text*)					\
	MEM_KEEP(exit.text*)					\
		ALL(.text.unlikely)

#define TEXT_TEXT TEXT_TEXT_SECTION()

/* sched.text is aling to function alignment to secure we have same
 * address even at second ld pass when generating System.map */
#define SCHED_TEXT							\
		ALIGN_FUNCTION();					\
		VMLINUX_SYMBOL(__sched_text_start) = .;			\
		ALL(.sched.text)					\
		VMLINUX_SYMBOL(__sched_text_end) = .;

/* spinlock.text is aling to function alignment to secure we have same
 * address even at second ld pass when generating System.map */
#define LOCK_TEXT							\
		ALIGN_FUNCTION();					\
		VMLINUX_SYMBOL(__lock_text_start) = .;			\
		ALL(.spinlock.text)					\
		VMLINUX_SYMBOL(__lock_text_end) = .;

#define KPROBES_TEXT							\
		ALIGN_FUNCTION();					\
		VMLINUX_SYMBOL(__kprobes_text_start) = .;		\
		ALL(.kprobes.text)					\
		VMLINUX_SYMBOL(__kprobes_text_end) = .;

#define ENTRY_TEXT							\
		ALIGN_FUNCTION();					\
		VMLINUX_SYMBOL(__entry_text_start) = .;			\
		ALL(.entry.text)						\
		VMLINUX_SYMBOL(__entry_text_end) = .;

#ifdef CONFIG_FUNCTION_GRAPH_TRACER
#define IRQENTRY_TEXT							\
		ALIGN_FUNCTION();					\
		VMLINUX_SYMBOL(__irqentry_text_start) = .;		\
		ALL(.irqentry.text)					\
		VMLINUX_SYMBOL(__irqentry_text_end) = .;
#else
#define IRQENTRY_TEXT
#endif

/* Section used for early init (in .S files) */
#define HEAD_TEXT  KEEP_ALL(.head.text)

#define HEAD_TEXT_SECTION							\
	.head.text : AT(ADDR(.head.text) - LOAD_OFFSET) {		\
		HEAD_TEXT						\
	}

/*
 * Exception table
 */
#define EXCEPTION_TABLE(align)						\
	. = ALIGN(align);						\
	__ex_table : AT(ADDR(__ex_table) - LOAD_OFFSET) {		\
		VMLINUX_SYMBOL(__start___ex_table) = .;			\
		KEEP(*(__ex_table*))					\
		VMLINUX_SYMBOL(__stop___ex_table) = .;			\
	}

/*
 * Init task
 */
#define INIT_TASK_DATA_SECTION(align)					\
	. = ALIGN(align);						\
	.data..init_task :  AT(ADDR(.data..init_task) - LOAD_OFFSET) {	\
		INIT_TASK_DATA(align)					\
	}

#ifdef CONFIG_CONSTRUCTORS
#define KERNEL_CTORS()	. = ALIGN(8);			   \
			VMLINUX_SYMBOL(__ctors_start) = .; \
			KEEP(*(.ctors))			   \
			VMLINUX_SYMBOL(__ctors_end) = .;
#else
#define KERNEL_CTORS()
#endif

/* init and exit section handling */
#define INIT_DATA							\
	ALL(.init.data)							\
	DEV_DISCARD(init.data*)						\
	CPU_DISCARD(init.data*)						\
	MEM_DISCARD(init.data*)						\
	KERNEL_CTORS()							\
	ALL(.init.rodata)						\
	MCOUNT_REC()							\
	FTRACE_EVENTS()							\
	TRACE_SYSCALLS()						\
	DEV_DISCARD(init.rodata*)					\
	CPU_DISCARD(init.rodata*)					\
	MEM_DISCARD(init.rodata*)					\
	KERNEL_DTB()

#define INIT_TEXT							\
	ALL(.init.text)							\
	DEV_DISCARD(init.text*)						\
	CPU_DISCARD(init.text*)						\
	MEM_DISCARD(init.text*)

#define EXIT_DATA							\
	ALL(.exit.data)							\
	DEV_DISCARD(exit.data*)						\
	DEV_DISCARD(exit.rodata*)					\
	CPU_DISCARD(exit.data*)						\
	CPU_DISCARD(exit.rodata*)					\
	MEM_DISCARD(exit.data*)						\
	MEM_DISCARD(exit.rodata*)

#define EXIT_TEXT							\
	ALL(.exit.text)							\
	DEV_DISCARD(exit.text*)						\
	CPU_DISCARD(exit.text*)						\
	MEM_DISCARD(exit.text*)

#define EXIT_CALL							\
	ALL(.exitcall.exit)

/*
 * bss (Block Started by Symbol) - uninitialized data
 * zeroed during startup
 */
#define SBSS(sbss_align)						\
	. = ALIGN(sbss_align);						\
	.sbss : AT(ADDR(.sbss) - LOAD_OFFSET) {				\
		ALL(.sbss)						\
		*(.scommon)						\
	}

#define BSS(bss_align)							\
	. = ALIGN(bss_align);						\
	.bss : AT(ADDR(.bss) - LOAD_OFFSET) {				\
		ALL(.bss..page_aligned)					\
		*(.dynbss)						\
		ALL(.bss)						\
		*(COMMON)						\
	}

/*
 * DWARF debug sections.
 * Symbols in the DWARF debugging sections are relative to
 * the beginning of the section so we begin them at 0.
 */
#define DWARF_DEBUG							\
		/* DWARF 1 */						\
		.debug          0 : { *(.debug) }			\
		.line           0 : { *(.line) }			\
		/* GNU DWARF 1 extensions */				\
		.debug_srcinfo  0 : { *(.debug_srcinfo) }		\
		.debug_sfnames  0 : { *(.debug_sfnames) }		\
		/* DWARF 1.1 and DWARF 2 */				\
		.debug_aranges  0 : { *(.debug_aranges) }		\
		.debug_pubnames 0 : { *(.debug_pubnames) }		\
		/* DWARF 2 */						\
		.debug_info     0 : { *(.debug_info			\
				.gnu.linkonce.wi.*) }			\
		.debug_abbrev   0 : { *(.debug_abbrev) }		\
		.debug_line     0 : { *(.debug_line) }			\
		.debug_frame    0 : { *(.debug_frame) }			\
		.debug_str      0 : { *(.debug_str) }			\
		.debug_loc      0 : { *(.debug_loc) }			\
		.debug_macinfo  0 : { *(.debug_macinfo) }		\
		/* SGI/MIPS DWARF 2 extensions */			\
		.debug_weaknames 0 : { *(.debug_weaknames) }		\
		.debug_funcnames 0 : { *(.debug_funcnames) }		\
		.debug_typenames 0 : { *(.debug_typenames) }		\
		.debug_varnames  0 : { *(.debug_varnames) }		\

		/* Stabs debugging sections.  */
#define STABS_DEBUG							\
		.stab 0 : { *(.stab) }					\
		.stabstr 0 : { *(.stabstr) }				\
		.stab.excl 0 : { *(.stab.excl) }			\
		.stab.exclstr 0 : { *(.stab.exclstr) }			\
		.stab.index 0 : { *(.stab.index) }			\
		.stab.indexstr 0 : { *(.stab.indexstr) }		\
		.comment 0 : { *(.comment) }

#ifdef CONFIG_GENERIC_BUG
#define BUG_TABLE							\
	. = ALIGN(8);							\
	__bug_table : AT(ADDR(__bug_table) - LOAD_OFFSET) {		\
		VMLINUX_SYMBOL(__start___bug_table) = .;		\
		ALL(__bug_table)					\
		VMLINUX_SYMBOL(__stop___bug_table) = .;			\
	}
#else
#define BUG_TABLE
#endif

#ifdef CONFIG_PM_TRACE
#define TRACEDATA							\
	. = ALIGN(4);							\
	.tracedata : AT(ADDR(.tracedata) - LOAD_OFFSET) {		\
		VMLINUX_SYMBOL(__tracedata_start) = .;			\
		ALL(.tracedata)						\
		VMLINUX_SYMBOL(__tracedata_end) = .;			\
	}
#else
#define TRACEDATA
#endif

#define NOTES								\
	.notes : AT(ADDR(.notes) - LOAD_OFFSET) {			\
		VMLINUX_SYMBOL(__start_notes) = .;			\
		ALL(.note)						\
		VMLINUX_SYMBOL(__stop_notes) = .;			\
	}

#define INIT_SETUP(initsetup_align)					\
		. = ALIGN(initsetup_align);				\
		VMLINUX_SYMBOL(__setup_start) = .;			\
		KEEP_ALL(.init.setup)					\
		VMLINUX_SYMBOL(__setup_end) = .;

#define INITCALLS							\
	KEEP_ALL(.initcallearly.init)					\
	VMLINUX_SYMBOL(__early_initcall_end) = .;			\
	KEEP_ALL(.initcall0.init)					\
	KEEP_ALL(.initcall0s.init)					\
	KEEP_ALL(.initcall1.init)					\
	KEEP_ALL(.initcall1s.init)					\
	KEEP_ALL(.initcall2.init)					\
	KEEP_ALL(.initcall2s.init)					\
	KEEP_ALL(.initcall3.init)					\
	KEEP_ALL(.initcall3s.init)					\
	KEEP_ALL(.initcall4.init)					\
	KEEP_ALL(.initcall4s.init)					\
	KEEP_ALL(.initcall5.init)					\
	KEEP_ALL(.initcall5s.init)					\
	KEEP_ALL(.initcallrootfs.init)					\
	KEEP_ALL(.initcall6.init)					\
	KEEP_ALL(.initcall6s.init)					\
	KEEP_ALL(.initcall7.init)					\
	KEEP_ALL(.initcall7s.init)

#define INIT_CALLS							\
		VMLINUX_SYMBOL(__initcall_start) = .;			\
		INITCALLS						\
		VMLINUX_SYMBOL(__initcall_end) = .;

#define CON_INITCALL							\
		VMLINUX_SYMBOL(__con_initcall_start) = .;		\
		KEEP_ALL(.con_initcall.init)				\
		VMLINUX_SYMBOL(__con_initcall_end) = .;

#define SECURITY_INITCALL						\
		VMLINUX_SYMBOL(__security_initcall_start) = .;		\
		ALL(.security_initcall.init)				\
		VMLINUX_SYMBOL(__security_initcall_end) = .;

#ifdef CONFIG_BLK_DEV_INITRD
#define INIT_RAM_FS							\
	. = ALIGN(4);							\
	VMLINUX_SYMBOL(__initramfs_start) = .;				\
	KEEP(*(.init.ramfs))						\
	. = ALIGN(8);							\
	KEEP(*(.init.ramfs.info))
#else
#define INIT_RAM_FS
#endif

/*
 * Default discarded sections.
 *
 * Some archs want to discard exit text/data at runtime rather than
 * link time due to cross-section references such as alt instructions,
 * bug table, eh_frame, etc.  DISCARDS must be the last of output
 * section definitions so that such archs put those in earlier section
 * definitions.
 */
#define DISCARDS							\
	/DISCARD/ : {							\
	EXIT_TEXT							\
	EXIT_DATA							\
	EXIT_CALL							\
	ALL(.discard)							\
	}

/**
 * PERCPU_INPUT - the percpu input sections
 * @cacheline: cacheline size
 *
 * The core percpu section names and core symbols which do not rely
 * directly upon load addresses.
 *
 * @cacheline is used to align subsections to avoid false cacheline
 * sharing between subsections for different purposes.
 */
#define PERCPU_INPUT(cacheline)						\
	VMLINUX_SYMBOL(__per_cpu_start) = .;				\
	ALL(.data..percpu..first)					\
	. = ALIGN(PAGE_SIZE);						\
	ALL(.data..percpu..page_aligned)				\
	. = ALIGN(cacheline);						\
	ALL(.data..percpu..readmostly)					\
	. = ALIGN(cacheline);						\
	ALL(.data..percpu..shared_aligned)				\
	ALL(.data..percpu)						\
	VMLINUX_SYMBOL(__per_cpu_end) = .;

/**
 * PERCPU_VADDR - define output section for percpu area
 * @cacheline: cacheline size
 * @vaddr: explicit base address (optional)
 * @phdr: destination PHDR (optional)
 *
 * Macro which expands to output section for percpu area.
 *
 * @cacheline is used to align subsections to avoid false cacheline
 * sharing between subsections for different purposes.
 *
 * If @vaddr is not blank, it specifies explicit base address and all
 * percpu symbols will be offset from the given address.  If blank,
 * @vaddr always equals @laddr + LOAD_OFFSET.
 *
 * @phdr defines the output PHDR to use if not blank.  Be warned that
 * output PHDR is sticky.  If @phdr is specified, the next output
 * section in the linker script will go there too.  @phdr should have
 * a leading colon.
 *
 * Note that this macros defines __per_cpu_load as an absolute symbol.
 * If there is no need to put the percpu section at a predetermined
 * address, use PERCPU_SECTION.
 */
#define PERCPU_VADDR(cacheline, vaddr, phdr)				\
	VMLINUX_SYMBOL(__per_cpu_load) = .;				\
	.data..percpu vaddr : AT(VMLINUX_SYMBOL(__per_cpu_load)		\
				- LOAD_OFFSET) {			\
		PERCPU_INPUT(cacheline)					\
	} phdr								\
	. = VMLINUX_SYMBOL(__per_cpu_load) + SIZEOF(.data..percpu);

/**
 * PERCPU_SECTION - define output section for percpu area, simple version
 * @cacheline: cacheline size
 *
 * Align to PAGE_SIZE and outputs output section for percpu area.  This
 * macro doesn't manipulate @vaddr or @phdr and __per_cpu_load and
 * __per_cpu_start will be identical.
 *
 * This macro is equivalent to ALIGN(PAGE_SIZE); PERCPU_VADDR(@cacheline,,)
 * except that __per_cpu_load is defined as a relative symbol against
 * .data..percpu which is required for relocatable x86_32 configuration.
 */
#define PERCPU_SECTION(cacheline)					\
	. = ALIGN(PAGE_SIZE);						\
	.data..percpu	: AT(ADDR(.data..percpu) - LOAD_OFFSET) {	\
		VMLINUX_SYMBOL(__per_cpu_load) = .;			\
		PERCPU_INPUT(cacheline)					\
	}


/*
 * Definition of the high level *_SECTION macros
 * They will fit only a subset of the architectures
 */


/*
 * Writeable data.
 * All sections are combined in a single .data section.
 * The sections following CONSTRUCTORS are arranged so their
 * typical alignment matches.
 * A cacheline is typical/always less than a PAGE_SIZE so
 * the sections that has this restriction (or similar)
 * is located before the ones requiring PAGE_SIZE alignment.
 * NOSAVE_DATA starts and ends with a PAGE_SIZE alignment which
 * matches the requirement of PAGE_ALIGNED_DATA.
 *
 * use 0 as page_align if page_aligned data is not used */
#define RW_DATA_SECTION(cacheline, pagealigned, inittask)		\
	. = ALIGN(PAGE_SIZE);						\
	.data : AT(ADDR(.data) - LOAD_OFFSET) {				\
		INIT_TASK_DATA(inittask)				\
		NOSAVE_DATA						\
		PAGE_ALIGNED_DATA(pagealigned)				\
		CACHELINE_ALIGNED_DATA(cacheline)			\
		READ_MOSTLY_DATA(cacheline)				\
		DATA_DATA						\
		CONSTRUCTORS						\
	}

#define INIT_TEXT_SECTION(inittext_align)				\
	. = ALIGN(inittext_align);					\
	.init.text : AT(ADDR(.init.text) - LOAD_OFFSET) {		\
		VMLINUX_SYMBOL(_sinittext) = .;				\
		INIT_TEXT						\
		VMLINUX_SYMBOL(_einittext) = .;				\
	}

#define INIT_DATA_SECTION(initsetup_align)				\
	.init.data : AT(ADDR(.init.data) - LOAD_OFFSET) {		\
		INIT_DATA						\
		INIT_SETUP(initsetup_align)				\
		INIT_CALLS						\
		CON_INITCALL						\
		SECURITY_INITCALL					\
		INIT_RAM_FS						\
	}

#define BSS_SECTION(sbss_align, bss_align, stop_align)			\
	. = ALIGN(sbss_align);						\
	VMLINUX_SYMBOL(__bss_start) = .;				\
	SBSS(sbss_align)						\
	BSS(bss_align)							\
	. = ALIGN(stop_align);						\
	VMLINUX_SYMBOL(__bss_stop) = .;
