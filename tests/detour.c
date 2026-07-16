/**
 * Like Leafrun but for testing Leaf Detour.
 * 
 * Build:
 *     clang -g -o ldt32.bin -m32 detour.c
 *     clang -g -o ldt64.bin detour.c
 * 
 * ****************************************************************************
 * 
 * This file is part of Leaf. Copyright (C) 2024 - 2026 Knot126.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the “Software”), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>

#define LEAF_IMPLEMENTATION
#include "../leaf.h"

#define LEAF_DETOURS_IMPLEMENTATION
#include "../leaf_detour.h"

void print_usage(const char *name) {
	fprintf(stderr, "leafrun -- Load and run a function from a shared object using Leaf\n\nUsage:\n%s <function> <object> [ARGS ...]\n\nWhere:\n\t<function> is the symbol name of the function to execute\n\t<object> is the path to the library or executable\n\tARGS are main()-like arguments passed to the function\n", name);
	exit(127);
}

int print_with_name_hook(const char *a) {
	printf("This is print_with_name_hook()! Hello, %s.\n", a);
	return 0;
}

typedef int (*Main)(int argc, const char *argv[]);

int main(int argc, const char *argv[]) {
	if (argc < 3) {
		print_usage(argv[0]);
	}
	
	LeafParams params = {
		.pre_extra_size = 0x147,
	};
	
	Leaf *leaf = LeafInit(&params);
	
	if (!leaf) {
		fprintf(stderr, "Leaf init has failed\n");
		return 1;
	}
	
	const char *msg = LeafLoadFromFile(leaf, argv[2]);
	
	if (msg) {
		fprintf(stderr, "Leaf load error: %s\n", msg);
		return 2;
	}
	
	Main m = LeafSymbolAddr(leaf, argv[1]);
	
	if (!m) {
		m = LeafGetEntryPoint(leaf);
		
		if (!m) {
			fprintf(stderr, "Symbol not found: %s\n", argv[1]);
			return 3;
		}
		else {
			fprintf(stderr, "Symbol %s not found, use entry point instead\n", argv[1]);
		}
	}
	
	LeafSym *info = LeafSymbolInfo(leaf, "print_with_name");
	
	if (!info) {
		fprintf(stderr, "Could not find symbol print_with_name\n");
		return 5;
	}
	
	LeafDetour detour;
	int status = LeafDetourCreateEx(&detour, (void*)info->st_value, info->st_size, print_with_name_hook, 0, NULL);
	
	if (status) {
		fprintf(stderr, "Leaf detour create failed: %d\n", status);
		return 4;
	}
	
	int ret = m(argc - 2, argv + 2);
	
	LeafFree(leaf);
	
	return ret;
}
