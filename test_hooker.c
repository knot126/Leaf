#include <stdio.h>
#define LEAFHOOK_IMPLEMENTATION
#include "leafhook.h"

uint32_t gFakeInstrs[] = {
	0, 0, 0, 0,
};

uint32_t gCoolTest;

#define INSB(i) ((uint8_t *)&gFakeInstrs[i])

int main(int argc, const char *argv[]) {
	LHHooker *hooker = LHHookerCreate();
	LHHookerHookFunction(hooker, gFakeInstrs, &gCoolTest, NULL);
	
	for (size_t i = 0; i < 4; i++) {
		printf("[%zu] %02x %02x %02x %02x\n", i, INSB(i)[0], INSB(i)[1], INSB(i)[2], INSB(i)[3]);
	}
	
	LHHookerRelease(hooker);
}
