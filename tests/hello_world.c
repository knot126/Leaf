// clang -o hello32.bin -rdynamic -m32 ./hello_world.c

#include <stdio.h>

int main(int argc, const char *argv[]) {
	if (argc < 2) {
		printf("Hello, world!\n");
	}
	else {
		printf("Hello, %s!\n", argv[1]);
	}
	
	return 0;
}
