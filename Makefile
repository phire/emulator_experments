all: codegen codegen-clang

nocodegen-pgo: nocodegen.c
	gcc -g -O3 -Wall nocodegen.c -o nocodegen-pgo -fprofile-generate
	./nocodegen-pgo
	gcc -g -O3 -Wall nocodegen.c -o nocodegen-pgo -fprofile-use

nocodegen: nocodegen.c
	gcc -g -O3 -Wall nocodegen.c -o nocodegen

nocodegen-clang: nocodegen.c
	clang -g -O4 -Wall nocodegen.c -o nocodegen-clang

codegen.gen.c: codegen.m4.c
	m4 codegen.m4.c -s > codegen.gen.c

codegen: codegen.gen.c
	gcc -g -O3 -Wall codegen.gen.c -o codegen -std=gnu99 -Winline

codegen-clang: codegen.gen.c
	clang -g -O4 -Wall codegen.gen.c -o codegen-clang -Winline

armemu: arminterpeter.nasm
	nasm arminterpeter.nasm -E > tmp
	 nasm tmp -g -o arminterpeter.o -f elf64
	ld arminterpeter.o -g -o armemu

armdisasm: armdisasm.cc
	g++ -O3 armdisasm.cc -o armdisasm -std=gnu++11

clean:
	rm -f armemu armdisasm *.o
