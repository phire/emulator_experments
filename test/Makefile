
all: fast.bin slow.bin

fast.bin: testfast.elf
	~/local/bin/arm-eabi-objcopy -O binary testfast.elf fast.bin

testfast.elf: t1.c start.S
	~/local/bin/arm-eabi-gcc -O2 start.S t1.c -o testfast.elf -nostdlib

slow.bin: testslow.elf
	~/local/bin/arm-eabi-objcopy -O binary testslow.elf slow.bin

testslow.elf: t1.c start.S
	~/local/bin/arm-eabi-gcc -O0 start.S t1.c -o testslow.elf -nostdlib

clean:
	rm -f *.elf *.bin
