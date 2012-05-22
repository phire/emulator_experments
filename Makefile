all: nocodegen nocodegen-clang

nocodegen-pgo: nocodegen.c
	gcc -g -O3 -Wall nocodegen.c -o nocodegen-pgo -fprofile-generate
	./nocodegen-pgo
	gcc -g -O3 -Wall nocodegen.c -o nocodegen-pgo -fprofile-use


nocodegen: nocodegen.c
	gcc -g -O3 -Wall nocodegen.c -o nocodegen -march=native #-fno-tree-tail-merge -fno-crossjumping

nocodegen-clang: nocodegen.c
	clang -g -O4 -Wall nocodegen.c -o nocodegen-clang

armemu: arminterpeter.nasm
	nasm arminterpeter.nasm -E > tmp
	 nasm tmp -g -o arminterpeter.o -f elf64
	ld arminterpeter.o -g -o armemu

armdisasm: armdisasm.cc
	g++ -O3 armdisasm.cc -o armdisasm -std=gnu++11

clean:
	rm -f armemu armdisasm *.o
