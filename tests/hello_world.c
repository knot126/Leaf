// clang -o hello32.bin -rdynamic -m32 ./hello_world.c

#include <stdio.h>
#include <string.h>

int print_with_name(const char *name) {
	if (!strcmp(name, "test")) {
		printf("Just testing? Hello!\n");
		return 1;
	}
	else if (strlen(name) > 30) {
		printf("Woah, you have a reallly long name! I don't think I can say it.\n");
		return 2;
	}
	else {
		printf("Hello, %s!\n", name);
		return 0;
	}
}

int main(int argc, const char *argv[]) {
	if (argc < 2) {
		printf("Hello, world!\n");
	}
	else {
		print_with_name(argv[1]);
	}
	
	return 0;
}
