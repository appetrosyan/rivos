OUTPUT_ARCH( "riscv" )

ENTRY( _start )

MEMORY
{
    ram   (wxa!ri) : ORIGIN = 0x80000000, LENGTH = 512M
}

PHDRS
{
    text PT_LOAD;
    data PT_LOAD;
    bss PT_LOAD;
}

SECTIONS
{
    .text : {
    PROVIDE(_text_start = .);
    *(.text.init) *(.text .text.*)
    PROVIDE(_text_end = .);
    BYTE(0xAA)
    } >ram AT>ram :text
    PROVIDE(_global_pointer = .);
    .rodata : {
    . = ALIGN(4096);
    PROVIDE(_rodata_start = .);
    *(.rodata .rodata.*)
    PROVIDE(_rodata_end = .);
    BYTE(0xAA)
    } >ram AT>ram :text

    .data : {
    . = ALIGN(4096);
    PROVIDE(_data_start = .);
    *(.sdata .sdata.*) *(.data .data.*)
    PROVIDE(_data_end = .);
    BYTE(0xAA)
    } >ram AT>ram :data

    .bss :{
    . = ALIGN(4096);
    PROVIDE(_bss_start = .);
    *(.sbss .sbss.*) *(.bss .bss.*)
    PROVIDE(_bss_end = .);
    BYTE(0xAA)
    } >ram AT>ram :bss
    . = ALIGN(4096);

    PROVIDE(_memory_start = ORIGIN(ram));
    PROVIDE(_stack_end = .);
    PROVIDE(_stack_start = . + 0x80000);
    PROVIDE(_memory_end = ORIGIN(ram) + LENGTH(ram));
    PROVIDE(_heap_start = _stack_start);
    PROVIDE(_heap_size = _memory_end - _heap_start);
}
