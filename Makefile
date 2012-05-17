
armemu: arminterpeter.nasm
	nasm arminterpeter.nasm -E | sed -e "s/^\%line/;\%line/" > arminterpeter.s  
	nasm -a arminterpeter.s -o arminterpeter.o -f elf64
	ld arminterpeter.o -o armemu

armdisasm: armdisasm.cc
	g++ -O3 armdisasm.cc -o armdisasm -std=gnu++11

clean:
	rm -f armemu armdisasm *.o
