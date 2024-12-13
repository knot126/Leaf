#include <stdio.h>
#define LEAFHOOK_IMPLEMENTATION
#include "leafhook.h"

uint32_t gFakeInstrs[] = {
	0, 0, 0, 0, 0, 0, 0, 0,
};

uint32_t gCoolTest;

void dump_bytes(const char *title, uint8_t *data, size_t size) {
	printf("## %s <%p> (%zu) ##\n", title, data, size);
	
	for (size_t i = 0; i < size; i += 4) {
		printf("%02x %02x %02x %02x\n", data[i], data[i+1], data[i+2], data[i+3]);
	}
	
	printf("\n");
}

int main(int argc, const char *argv[]) {
	LHHooker *hooker = LHHookerCreate();
	void *orig;
	LHHookerHookFunction(hooker, gFakeInstrs, &gCoolTest, &orig);
	
	dump_bytes("function", (uint8_t *) gFakeInstrs, 16);
	dump_bytes("callback", orig, 28);
	
	LHHookerRelease(hooker);
}
