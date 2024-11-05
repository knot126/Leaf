#include <stdio.h>

#define LEAF_IMPLEMENTATION
#include "leaf.h"

int main(int argc, const char *argv[]) {
	Leaf *leaf = LeafInit();
	printf("leaf instance: <%p>\n", leaf);
	const char *error = LeafLoadFromFile(leaf, argv[1]);
	if (error) {
		printf("Leaf error: %s\n", error);
	}
	printf("Address of android_main: <%p>\n", LeafSymbolAddr(leaf, "android_main"));
	printf("Address of gGame: <%p>\n", LeafSymbolAddr(leaf, "gGame"));
	LeafFree(leaf);
	
	return 0;
}
