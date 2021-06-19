/* Memory layout for esp-open-rtos when using OTA second stage bootloader */
MEMORY
{
  dport0_0_seg :                      	org = 0x3FF00000, len = 0x10
  dram0_0_seg :                       	org = 0x3FFE8000, len = 0x14000
  iram1_0_seg :                       	org = 0x40100000, len = 0x08000
/* irom0 section, mapped from SPI flash
  - Origin is offset by 0x2010 to create spacer for second stage bootloader image,
    header.

  - Length is max 8Mbit of mappable flash, minus start offset
*/
  irom0_0_seg :                       	org = 0x40202010, len = (1M - 0x2010)
}

/* FreeRTOS memory management functions

   We link these directly to newlib functions (have to do it at link
   time as binary libraries use these symbols too.)
*/
pvPortMalloc = malloc;
vPortFree = free;

/* SDK compatibility */
ets_printf = printf;

PHDRS
{
  dport0_0_phdr PT_LOAD;
  dram0_0_phdr PT_LOAD;
  dram0_0_bss_phdr PT_LOAD;
  iram1_0_phdr PT_LOAD;
  irom0_0_phdr PT_LOAD;
}


/*  Default entry point:  */
ENTRY(call_user_start)
PROVIDE(_memmap_vecbase_reset = 0x40000000);
/* Various memory-map dependent cache attribute settings: */
_memmap_cacheattr_wb_base = 0x00000110;
_memmap_cacheattr_wt_base = 0x00000110;
_memmap_cacheattr_bp_base = 0x00000220;
_memmap_cacheattr_unused_mask = 0xFFFFF00F;
_memmap_cacheattr_wb_trapnull = 0x2222211F;
_memmap_cacheattr_wba_trapnull = 0x2222211F;
_memmap_cacheattr_wbna_trapnull = 0x2222211F;
_memmap_cacheattr_wt_trapnull = 0x2222211F;
_memmap_cacheattr_bp_trapnull = 0x2222222F;
_memmap_cacheattr_wb_strict = 0xFFFFF11F;
_memmap_cacheattr_wt_strict = 0xFFFFF11F;
_memmap_cacheattr_bp_strict = 0xFFFFF22F;
_memmap_cacheattr_wb_allvalid = 0x22222112;
_memmap_cacheattr_wt_allvalid = 0x22222112;
_memmap_cacheattr_bp_allvalid = 0x22222222;
PROVIDE(_memmap_cacheattr_reset = _memmap_cacheattr_wb_trapnull);

SECTIONS
{

  .dport0.rodata : ALIGN(4)
  {
    _dport0_rodata_start = ABSOLUTE(.);
    *(.dport0.rodata)
    *(.dport.rodata)
    _dport0_rodata_end = ABSOLUTE(.);
  } >dport0_0_seg :dport0_0_phdr

  .dport0.literal : ALIGN(4)
  {
    _dport0_literal_start = ABSOLUTE(.);
    *(.dport0.literal)
    *(.dport.literal)
    _dport0_literal_end = ABSOLUTE(.);
  } >dport0_0_seg :dport0_0_phdr

  .dport0.data : ALIGN(4)
  {
    _dport0_data_start = ABSOLUTE(.);
    *(.dport0.data)
    *(.dport.data)
    _dport0_data_end = ABSOLUTE(.);
  } >dport0_0_seg :dport0_0_phdr

  .text : ALIGN(4) /* IRAM */
  {
    _stext = .;
    _text_start = ABSOLUTE(.);
    . = ALIGN (16);
    *(.vecbase.text)
    *(.entry.text)
    *(.init.literal)
    *(.init)
    /* esp-open-rtos compiled source files use the .iram1.* section names for IRAM
       functions, etc. */
    *(.iram1.*)
    /* SDK libraries expect their .text sections to link to iram, not irom */
    *sdklib*:*(.literal .text .literal.* .text.*)
    /* libgcc integer functions also need to be in .text, as some are called before
       flash is mapped (also performance)
    */
    *libgcc.a:*i3.o(.literal .text .literal.* .text.*)

    /* libc also in IRAM */
    *libc.a:*malloc.o(.literal .text .literal.* .text.*)
    *libc.a:*mallocr.o(.literal .text .literal.* .text.*)
    *libc.a:*freer.o(.literal .text .literal.* .text.*)
    *libc.a:*memcpy.o(.literal .text .literal.* .text.*)
    *libc.a:*memset.o(.literal .text .literal.* .text.*)
    *libc.a:*memcmp.o(.literal .text .literal.* .text.*)
    *libc.a:*memmove.o(.literal .text .literal.* .text.*)
    *libc.a:*rand.o(.literal .text .literal.* .text.*)
    *libc.a:*bzero.o(.literal .text .literal.* .text.*)
    *libc.a:*lock.o(.literal .text .literal.* .text.*)

    *libc.a:*-printf.o(.literal .text .literal.* .text.*)
    *libc.a:*-sprintf.o(.literal .text .literal.* .text.*)
    *libc.a:*-fprintf.o(.literal .text .literal.* .text.*)
    *libc.a:*-svfprintf.o(.literal .text .literal.* .text.*)
    *libc.a:*-vfprintf.o(.literal .text .literal.* .text.*)
    *libc.a:*findfp.o(.literal .text .literal.* .text.*)
    *libc.a:*fputwc.o(.literal .text .literal.* .text.*)

    *(.stub .gnu.warning .gnu.linkonce.literal.* .gnu.linkonce.t.*.literal .gnu.linkonce.t.*)
    *(.fini.literal)
    *(.fini)
    *(.gnu.version)
    _text_end = ABSOLUTE(.);
    _etext = .;
  } >iram1_0_seg :iram1_0_phdr

  .data : ALIGN(4)
  {
    _data_start = ABSOLUTE(.);
    *(.data)
    *(.data.*)
    *(.gnu.linkonce.d.*)
    *(.data1)
    *(.sdata)
    *(.sdata.*)
    *(.gnu.linkonce.s.*)
    *(.sdata2)
    *(.sdata2.*)
    *(.gnu.linkonce.s2.*)
    *(.jcr)
    _data_end = ABSOLUTE(.);
  } >dram0_0_seg :dram0_0_phdr

  /* rodata in DRAM

     cherry-picked compilation units that need rodata
     to be in DRAM - anything that may be run while
     SPI flash is unmapped (ie IRAM functions that are
     called from interrupt context or spi flash management
     functions) need their compilation units listed here.

     If you have constant data that is performance-critical,
     list the compilation unit(s) here as well.
  */
  .rodata : ALIGN(4)
  {
    _rodata_start = ABSOLUTE(.);

    /* Store all of core, libc, freertos .rodata in RAM by default
       (some parts are necessary, some parts for performance reasons.)
    */
    *core.a:*(.rodata.* .rodata) *libc.a:*.o(.rodata.* .rodata)
    *freertos.a:*(.rodata.* .rodata)

    /* spi flash management rodata needs to be accessed
       while flash is unmapped. */
    *libmain.a:spi_flash.o(.rodata.* .rodata)

    /* libpp wdev.o has the NMI handler (sdk_wDev_ProcessFiq)
       which runs at all times, flash mapped or not. */
    *libpp.a:wdev.o(.rodata.* .rodata)

    _rodata_end = ABSOLUTE(.);
  } > dram0_0_seg :dram0_0_phdr

  .bss ALIGN(8) (NOLOAD) : ALIGN(4)
  {
    . = ALIGN (8);
    _bss_start = ABSOLUTE(.);
    *(.dynsbss)
    *(.sbss)
    *(.sbss.*)
    *(.gnu.linkonce.sb.*)
    *(.scommon)
    *(.sbss2)
    *(.sbss2.*)
    *(.gnu.linkonce.sb2.*)
    *(.dynbss)
    *(.bss)
    *(.bss.*)
    *(.gnu.linkonce.b.*)
    *(COMMON)
    . = ALIGN (8);
    _bss_end = ABSOLUTE(.);
    _heap_start = ABSOLUTE(.);
/*    _stack_sentry = ALIGN(0x8); */
  } >dram0_0_seg :dram0_0_bss_phdr
/* __stack = 0x3ffc8000; <-- this value seems a bit odd, stack on sdk_user_start is ~0x3ffffce9 */


   /* All data that goes to flash (IROM) ends up in this section */
  .irom0.text : ALIGN(4)
  {
    /*****************************
     * Actual irom0 text section *
     *****************************/

    _irom0_text_start = ABSOLUTE(.);
    /* esp-open-rtos compiled code goes into IROM by default
       (except for libgcc which is matched above.)

       We also link .rodata here in the hope that data is stored near
       its code on the flash (in practice this doesn't quite happen. :/)
    */
    *(.literal .text .literal.* .text.* .rodata .rodata.*)
    /* Anything explicitly marked as "irom" or "irom0" should go here */
    *(.irom.* .irom.*.* .irom0.*)
    _irom0_text_end = ABSOLUTE(.);

    /**************************************************************
      C++ constructor and destructor tables, properly ordered:
     **************************************************************/
    __init_array_start = ABSOLUTE(.);
    KEEP (*crtbegin.o(.ctors))
    KEEP (*(EXCLUDE_FILE (*crtend.o) .ctors))
    KEEP (*(SORT(.ctors.*)))
    KEEP (*(.ctors))
    __init_array_end = ABSOLUTE(.);
    KEEP (*crtbegin.o(.dtors))
    KEEP (*(EXCLUDE_FILE (*crtend.o) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))

    /***********************************
       C++ exception handlers table:  *
     **********************************/
    __XT_EXCEPTION_DESCS__ = ABSOLUTE(.);
    *(.xt_except_desc)
    *(.gnu.linkonce.h.*)
    __XT_EXCEPTION_DESCS_END__ = ABSOLUTE(.);
    *(.xt_except_desc_end)

    /***********************************
     Additional .rodata special sections
     stored in flash
     ************************************/
    . = ALIGN(4);
    *(.gnu.linkonce.r.*)
    __XT_EXCEPTION_TABLE__ = ABSOLUTE(.);
    *(.xt_except_table)
    *(.gcc_except_table .gcc_except_table.*)
    *(.gnu.linkonce.e.*)
    *(.gnu.version_r)
    . = (. + 3) & ~ 3;
    __eh_frame = ABSOLUTE(.);
    KEEP(*(.eh_frame))
    . = (. + 7) & ~ 3;
    . = ALIGN(4);
    *(.dynamic)
    *(.gnu.version_d)
    . = ALIGN(4);       /* this table MUST be 4-byte aligned */
    _bss_table_start = ABSOLUTE(.);
    LONG(_bss_start)
    LONG(_bss_end)
    _bss_table_end = ABSOLUTE(.);
  } > irom0_0_seg :irom0_0_phdr

  .lit4 : ALIGN(4)
  {
    _lit4_start = ABSOLUTE(.);
    *(*.lit4)
    *(.lit4.*)
    *(.gnu.linkonce.lit4.*)
    _lit4_end = ABSOLUTE(.);
  } >iram1_0_seg :iram1_0_phdr
}
