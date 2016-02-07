OUTPUT_ARCH(i386)
ENTRY(main)
SECTIONS
{
	. = 0x20000000;
	.text : {
/* pthread_exit code */
		LONG(0x0047b853)
		LONG(0xe3890000)
		LONG(0x0008c381)
		LONG(0x30cd0000) 
		*(.text) 
		*(.fixup) 
		*(.kstrtab) 
		*(.rodata) 
	}
	. = 0x24000000;
	.data : { 
		*(.data) CONSTRUCTORS 
	}

	.bss : { 
		*(.bss) *(COMMON) 
	}
}

