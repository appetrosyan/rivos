CFLAGS= -g -mcmodel=medany -Wall -Ofast
CFLAGS+=-static -ffreestanding -nostdlib
CFLAGS+=-march=rv64gc -mabi=lp64
LDFLAGS=
DRIVE=hdd.dsk

kernel.bin: virt.lds
		riscv64-unknown-elf-gcc $(CFLAGS) $(LDFLAGS) -T $< -o $@ $(wildcard src/*.s) $(wildcard src/*.c)

hdd:
	./make_hdd.sh

run: clean kernel.bin hdd
	mkfifo pipe.in pipe.out
	qemu-system-riscv64 -machine virt -cpu sifive-u54 -smp 4 -m 512M -nographic -serial pipe:./pipe -bios none -kernel kernel.bin -drive if=none,format=raw,file=$(DRIVE),id=foo -device virtio-blk-device,scsi=off,drive=foo
	rm -drf pipe.in pipe.out

debug: clean kernel.bin hdd
	mkfifo pipe.in pipe.out
	qemu-system-riscv64 -machine virt -cpu sifive-u54 -smp 4 -m 512M -nographic -serial pipe:./pipe -bios none -kernel kernel.bin -drive if=none,format=raw,file=$(DRIVE),id=foo -device virtio-blk-device,scsi=off,drive=foo -s -S
	rm -drf pipe.in pipe.out
clean:
	rm -drf pipe.in pipe.out
	rm -drf kernel.bin hdd.dsk
	make -C viewer clean
