/*
 * linker script for STM32H743IIT6 (GNU ld)
 *
 *   FLASH      0x08000000  2048 KB  代码默认区
 *   ITCM       0x00000000    64 KB  内核 src+klibc .text（不含 components/libcpu）
 *   DTCM       0x20000000   128 KB  内核 src+klibc .data/.bss
 *   RAM        0x24000000   512 KB  默认数据 / 栈 / 堆
 *   RAM_D2_FS  0x30000000   256 KB  dfs_ramfs (SRAM1+SRAM2)
 *   RAM_D2_AUX 0x30040000    32 KB  其它 D2 用途 (SRAM3)
 *   RAM_D3     0x38000000    64 KB  USB/DMA noncacheable (SRAM4)
 *   BKPSRAM    0x38800000     4 KB  保留（备份域，需 VBAT）
 */

MEMORY
{
    FLASH     (rx)  : ORIGIN = 0x08000000, LENGTH = 2048K
    ITCM      (rwx) : ORIGIN = 0x00000000, LENGTH = 64K
    DTCM      (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
    RAM       (rwx) : ORIGIN = 0x24000000, LENGTH = 512K
    RAM_D2_FS (rwx) : ORIGIN = 0x30000000, LENGTH = 256K
    RAM_D2_AUX(rwx) : ORIGIN = 0x30040000, LENGTH = 32K
    RAM_D3    (rwx) : ORIGIN = 0x38000000, LENGTH = 64K
    BKPSRAM   (rwx) : ORIGIN = 0x38800000, LENGTH = 4K
}

ENTRY(Reset_Handler)
_system_stack_size = 0x400;

/*
 * Windows object paths look like build\kernel\src\xxx.o;
 * pattern *xxx.o cannot cross '\'. Use backslash and forward-slash
 * directory wildcards as exclusive OS-specific matches.
 */
SECTIONS
{
    /* 向量表必须在 Flash 起始 */
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
    } > FLASH

    /*
     * ITCM：内核 src + klibc（不含 components.o）。
     * components.o 含 entry/rtthread_startup，必须留在 Flash：
     * ITCM 拷贝在 board_init/startup 中完成，若 entry 也在 ITCM 会在拷贝前跳飞。
     */
    .itcm :
    {
        . = ALIGN(4);
        _sitcm = .;

        *\\kernel\\src\\clock.o(.text .text.*)
        */kernel/src/clock.o(.text .text.*)
        *\\kernel\\src\\cpu_up.o(.text .text.*)
        */kernel/src/cpu_up.o(.text .text.*)
        *\\kernel\\src\\cpu_mp.o(.text .text.*)
        */kernel/src/cpu_mp.o(.text .text.*)
        *\\kernel\\src\\defunct.o(.text .text.*)
        */kernel/src/defunct.o(.text .text.*)
        *\\kernel\\src\\idle.o(.text .text.*)
        */kernel/src/idle.o(.text .text.*)
        *\\kernel\\src\\ipc.o(.text .text.*)
        */kernel/src/ipc.o(.text .text.*)
        *\\kernel\\src\\irq.o(.text .text.*)
        */kernel/src/irq.o(.text .text.*)
        *\\kernel\\src\\kservice.o(.text .text.*)
        */kernel/src/kservice.o(.text .text.*)
        *\\kernel\\src\\mem.o(.text .text.*)
        */kernel/src/mem.o(.text .text.*)
        *\\kernel\\src\\memheap.o(.text .text.*)
        */kernel/src/memheap.o(.text .text.*)
        *\\kernel\\src\\mempool.o(.text .text.*)
        */kernel/src/mempool.o(.text .text.*)
        *\\kernel\\src\\object.o(.text .text.*)
        */kernel/src/object.o(.text .text.*)
        *\\kernel\\src\\scheduler_comm.o(.text .text.*)
        */kernel/src/scheduler_comm.o(.text .text.*)
        *\\kernel\\src\\scheduler_up.o(.text .text.*)
        */kernel/src/scheduler_up.o(.text .text.*)
        *\\kernel\\src\\scheduler_mp.o(.text .text.*)
        */kernel/src/scheduler_mp.o(.text .text.*)
        *\\kernel\\src\\signal.o(.text .text.*)
        */kernel/src/signal.o(.text .text.*)
        *\\kernel\\src\\thread.o(.text .text.*)
        */kernel/src/thread.o(.text .text.*)
        *\\kernel\\src\\timer.o(.text .text.*)
        */kernel/src/timer.o(.text .text.*)
        *\\kernel\\src\\klibc\\*.o(.text .text.*)
        */kernel/src/klibc/*.o(.text .text.*)

        . = ALIGN(4);
        _eitcm = .;
    } > ITCM AT > FLASH
    _sitcm_load = LOADADDR(.itcm);

    /* -------- FLASH：其余代码 -------- */
    .text :
    {
        . = ALIGN(4);
        _stext = .;

        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata*)
        *(.glue_7)
        *(.glue_7t)
        *(.gnu.linkonce.t*)

        . = ALIGN(4);
        __fsymtab_start = .;
        KEEP(*(FSymTab))
        __fsymtab_end = .;

        . = ALIGN(4);
        __vsymtab_start = .;
        KEEP(*(VSymTab))
        __vsymtab_end = .;

        . = ALIGN(4);
        __rtmsymtab_start = .;
        KEEP(*(RTMSymTab))
        __rtmsymtab_end = .;

        . = ALIGN(4);
        __rt_init_start = .;
        KEEP(*(SORT(.rti_fn*)))
        __rt_init_end = .;

        . = ALIGN(4);
        PROVIDE(__ctors_start__ = .);
        KEEP (*(SORT(.init_array.*)))
        KEEP (*(.init_array))
        PROVIDE(__ctors_end__ = .);

        . = ALIGN(4);
        _etext = .;
    } > FLASH = 0

    __exidx_start = .;
    .ARM.exidx :
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } > FLASH
    __exidx_end = .;

    /* -------- DTCM：内核 src + klibc（不含 components）-------- */
    .dtcm_data :
    {
        . = ALIGN(4);
        _sdtcm_data = .;

        *\\kernel\\src\\clock.o(.data .data.*)
        */kernel/src/clock.o(.data .data.*)
        *\\kernel\\src\\cpu_up.o(.data .data.*)
        */kernel/src/cpu_up.o(.data .data.*)
        *\\kernel\\src\\cpu_mp.o(.data .data.*)
        */kernel/src/cpu_mp.o(.data .data.*)
        *\\kernel\\src\\defunct.o(.data .data.*)
        */kernel/src/defunct.o(.data .data.*)
        *\\kernel\\src\\idle.o(.data .data.*)
        */kernel/src/idle.o(.data .data.*)
        *\\kernel\\src\\ipc.o(.data .data.*)
        */kernel/src/ipc.o(.data .data.*)
        *\\kernel\\src\\irq.o(.data .data.*)
        */kernel/src/irq.o(.data .data.*)
        *\\kernel\\src\\kservice.o(.data .data.*)
        */kernel/src/kservice.o(.data .data.*)
        *\\kernel\\src\\mem.o(.data .data.*)
        */kernel/src/mem.o(.data .data.*)
        *\\kernel\\src\\memheap.o(.data .data.*)
        */kernel/src/memheap.o(.data .data.*)
        *\\kernel\\src\\mempool.o(.data .data.*)
        */kernel/src/mempool.o(.data .data.*)
        *\\kernel\\src\\object.o(.data .data.*)
        */kernel/src/object.o(.data .data.*)
        *\\kernel\\src\\scheduler_comm.o(.data .data.*)
        */kernel/src/scheduler_comm.o(.data .data.*)
        *\\kernel\\src\\scheduler_up.o(.data .data.*)
        */kernel/src/scheduler_up.o(.data .data.*)
        *\\kernel\\src\\scheduler_mp.o(.data .data.*)
        */kernel/src/scheduler_mp.o(.data .data.*)
        *\\kernel\\src\\signal.o(.data .data.*)
        */kernel/src/signal.o(.data .data.*)
        *\\kernel\\src\\thread.o(.data .data.*)
        */kernel/src/thread.o(.data .data.*)
        *\\kernel\\src\\timer.o(.data .data.*)
        */kernel/src/timer.o(.data .data.*)
        *\\kernel\\src\\klibc\\*.o(.data .data.*)
        */kernel/src/klibc/*.o(.data .data.*)
        *(.dtcm)
        *(.dtcm.*)

        . = ALIGN(4);
        _edtcm_data = .;
    } > DTCM AT > FLASH
    _sdtcm_data_load = LOADADDR(.dtcm_data);

    _sidata = ALIGN(LOADADDR(.dtcm_data) + SIZEOF(.dtcm_data), 4);

    .dtcm_bss (NOLOAD) :
    {
        . = ALIGN(4);
        _sdtcm_bss = .;

        *\\kernel\\src\\clock.o(.bss .bss.* COMMON)
        */kernel/src/clock.o(.bss .bss.* COMMON)
        *\\kernel\\src\\cpu_up.o(.bss .bss.* COMMON)
        */kernel/src/cpu_up.o(.bss .bss.* COMMON)
        *\\kernel\\src\\cpu_mp.o(.bss .bss.* COMMON)
        */kernel/src/cpu_mp.o(.bss .bss.* COMMON)
        *\\kernel\\src\\defunct.o(.bss .bss.* COMMON)
        */kernel/src/defunct.o(.bss .bss.* COMMON)
        *\\kernel\\src\\idle.o(.bss .bss.* COMMON)
        */kernel/src/idle.o(.bss .bss.* COMMON)
        *\\kernel\\src\\ipc.o(.bss .bss.* COMMON)
        */kernel/src/ipc.o(.bss .bss.* COMMON)
        *\\kernel\\src\\irq.o(.bss .bss.* COMMON)
        */kernel/src/irq.o(.bss .bss.* COMMON)
        *\\kernel\\src\\kservice.o(.bss .bss.* COMMON)
        */kernel/src/kservice.o(.bss .bss.* COMMON)
        *\\kernel\\src\\mem.o(.bss .bss.* COMMON)
        */kernel/src/mem.o(.bss .bss.* COMMON)
        *\\kernel\\src\\memheap.o(.bss .bss.* COMMON)
        */kernel/src/memheap.o(.bss .bss.* COMMON)
        *\\kernel\\src\\mempool.o(.bss .bss.* COMMON)
        */kernel/src/mempool.o(.bss .bss.* COMMON)
        *\\kernel\\src\\object.o(.bss .bss.* COMMON)
        */kernel/src/object.o(.bss .bss.* COMMON)
        *\\kernel\\src\\scheduler_comm.o(.bss .bss.* COMMON)
        */kernel/src/scheduler_comm.o(.bss .bss.* COMMON)
        *\\kernel\\src\\scheduler_up.o(.bss .bss.* COMMON)
        */kernel/src/scheduler_up.o(.bss .bss.* COMMON)
        *\\kernel\\src\\scheduler_mp.o(.bss .bss.* COMMON)
        */kernel/src/scheduler_mp.o(.bss .bss.* COMMON)
        *\\kernel\\src\\signal.o(.bss .bss.* COMMON)
        */kernel/src/signal.o(.bss .bss.* COMMON)
        *\\kernel\\src\\thread.o(.bss .bss.* COMMON)
        */kernel/src/thread.o(.bss .bss.* COMMON)
        *\\kernel\\src\\timer.o(.bss .bss.* COMMON)
        */kernel/src/timer.o(.bss .bss.* COMMON)
        *\\kernel\\src\\klibc\\*.o(.bss .bss.* COMMON)
        */kernel/src/klibc/*.o(.bss .bss.* COMMON)
        *(.dtcm_bss)
        *(.dtcm_bss.*)

        . = ALIGN(4);
        _edtcm_bss = .;
    } > DTCM

    /* -------- AXI SRAM：默认数据 / 栈 / 堆 -------- */
    .data : AT (_sidata)
    {
        . = ALIGN(4);
        _sdata = .;

        *(.data)
        *(.data.*)
        *(.gnu.linkonce.d*)

        PROVIDE(__dtors_start__ = .);
        KEEP(*(SORT(.dtors.*)))
        KEEP(*(.dtors))
        PROVIDE(__dtors_end__ = .);

        . = ALIGN(4);
        _edata = .;
    } > RAM

    .stack :
    {
        . = ALIGN(4);
        _sstack = .;
        . = . + _system_stack_size;
        . = ALIGN(4);
        _estack = .;
    } > RAM

    __bss_start = .;
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;

        *(.bss)
        *(.bss.*)
        *(COMMON)

        . = ALIGN(4);
        _ebss = .;
        *(.bss.init)
    } > RAM
    __bss_end = .;

    _end = .;

    /* -------- D2：ramfs 池 (SRAM1+2 = 256KB) -------- */
    .ramfs_pool (NOLOAD) :
    {
        . = ALIGN(4);
        _sramfs_pool = .;
        *(.ramfs_pool)
        . = ORIGIN(RAM_D2_FS) + LENGTH(RAM_D2_FS);
        _eramfs_pool = .;
    } > RAM_D2_FS

    /* -------- D2 AUX：SRAM3 32KB，其它 D2 需求 -------- */
    .ram_d2_aux (NOLOAD) :
    {
        . = ALIGN(4);
        *(.ram_d2)
        *(.ram_d2.*)
        *(.ram_d2_aux)
        . = ALIGN(4);
    } > RAM_D2_AUX

    /* -------- D3：USB / DMA noncacheable -------- */
    .noncacheable (NOLOAD) :
    {
        . = ALIGN(32);
        _snoncacheable = .;
        *(.noncacheable)
        *(.noncacheable.*)
        . = ALIGN(32);
        _enoncacheable = .;
    } > RAM_D3

    /* -------- Backup SRAM：保留 -------- */
    .bkpsram (NOLOAD) :
    {
        . = ALIGN(4);
        *(.bkpsram)
        *(.bkpsram.*)
        . = ALIGN(4);
    } > BKPSRAM

    .stab          0 : { *(.stab) }
    .stabstr       0 : { *(.stabstr) }
    .stab.excl     0 : { *(.stab.excl) }
    .stab.exclstr  0 : { *(.stab.exclstr) }
    .stab.index    0 : { *(.stab.index) }
    .stab.indexstr 0 : { *(.stab.indexstr) }
    .comment       0 : { *(.comment) }
    .debug          0 : { *(.debug) }
    .line           0 : { *(.line) }
    .debug_srcinfo  0 : { *(.debug_srcinfo) }
    .debug_sfnames  0 : { *(.debug_sfnames) }
    .debug_aranges  0 : { *(.debug_aranges) }
    .debug_pubnames 0 : { *(.debug_pubnames) }
    .debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) }
    .debug_abbrev   0 : { *(.debug_abbrev) }
    .debug_line     0 : { *(.debug_line) }
    .debug_frame    0 : { *(.debug_frame) }
    .debug_str      0 : { *(.debug_str) }
    .debug_loc      0 : { *(.debug_loc) }
    .debug_macinfo  0 : { *(.debug_macinfo) }
    .debug_weaknames 0 : { *(.debug_weaknames) }
    .debug_funcnames 0 : { *(.debug_funcnames) }
    .debug_typenames 0 : { *(.debug_typenames) }
    .debug_varnames  0 : { *(.debug_varnames) }
}
