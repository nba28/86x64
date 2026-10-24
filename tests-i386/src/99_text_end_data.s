## 99_text_end_data — data at the very end of __text that decodes as an
## instruction running past the section (`b8` = mov $imm32,%eax) aborted the
## whole translation ("blob overruns section end"). Exit 42.
## OFF arm: M64_NO_TEXT_END_DATA=1 at translate time.
	.text
	.globl _main
_main:
	pushl	$42
	calll	_exit
	.byte	0xb8, 0x01
