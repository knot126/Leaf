
typedef long long size_t;

size_t write(int fd, void *buf, size_t len) {
	asm(
		"mov $1, %rax;"
		"syscall"
	);
}

void exit(int code) {
	asm(
		"mov $60, %rax;"
		"syscall"
	);
}

void _start() {
	write(1, "Hello, world!\n", 14);
	exit(0);
}
