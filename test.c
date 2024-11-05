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
	LeafFree(leaf);
	
	return 0;
}
