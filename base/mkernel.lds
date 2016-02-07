OUTPUT_FORMAT("binary")
OUTPUT_ARCH(i386)
STARTUP(initboot.o)
ENTRY(_main)
SECTIONS
{
    	. = 0x10800;
	.text : { *(.text) 
		  *(.fixup) 
		  }
	.kstrtab : { *(.kstrtab) }
	.rodata : { *(.rodata) }
	.data : { *(.data) CONSTRUCTORS}
	bss_start = . ; 
	.bss  : { *(.bss) *(COMMON) }
	bss_end = . ;
}
