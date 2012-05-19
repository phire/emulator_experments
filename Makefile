
armemu: arminterpeter.nasm
	nasm arminterpeter.nasm  -g -o arminterpeter.o -f elf64
	ld arminterpeter.o -g -o armemu

armdisasm: armdisasm.cc
	g++ -O3 armdisasm.cc -o armdisasm -std=gnu++11

clean:
	rm -f armemu armdisasm *.o
