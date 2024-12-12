/**
 * LeafHook - single header hooking library usable with Leaf
 */

#ifndef _LEAFHOOK_HEADER
#define _LEAFHOOK_HEADER
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>

typedef struct LHHooker {
	void *rwx_block;
	size_t rwx_block_size;
	size_t rwx_block_used;
} LHHooker;

LHHooker *LHHookerCreate(void);
void LHHookerRelease(LHHooker *self);

#ifdef LEAFHOOK_IMPLEMENTATION

// Test if input is negative for sign extend
#define LH_SEXT_ISNEG(input, nbits) (input & (1 << (nbits - 1)))

// Get bits to OR to sign extend if negative
#define LH_SEXT64_NB(nbits) (0xffffffffffffffff - ( (1 << (nbits - 1)) - 1 ))

// Fill the last (64 - nbits) bits with 1's if most significant is 1 or zero if
// most significant is zero.
#define LH_SEXT64(input, nbits) (LH_SEXT_ISNEG(input, nbits) ? (LH_SEXT64_NB(nbits) | input) : input)

// Automatically generated macros for working with ARM instructions

void *LHHookerMapRwxPages(size_t size) {
	return mmap(NULL, size, PROT_EXEC | PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

LHHooker *LHHookerCreate(void) {
	/**
	 * Create a new hook manager
	 */
	
	LHHooker *self = malloc(sizeof *self);
	
	if (!self) {
		return NULL;
	}
	
	memset(self, 0, sizeof *self);
	
	self->rwx_block_size = 10 * getpagesize();
	self->rwx_block = LHHookerMapRwxPages(self->rwx_block_size);
	
	if (!self->rwx_block) {
		LHHookerRelease(self);
		return NULL;
	}
	
	return self;
}

void LHHookerRelease(LHHooker *self) {
	/**
	 * Release a hook manager and any related resources. Don't call if any
	 * hooked functions might still be used.
	 */
	
	if (!self) {
		return;
	}
	
	if (self->rwx_block) {
		munmap(self->rwx_block, self->rwx_block_size);
	}
	
	free(self);
}

static void *LHHookerAllocRwx(LHHooker *self, size_t size) {
	/**
	 * Allocate some RWX memory from self->rwx_block. Should be 4 byte aligned.
	 */
	
	// Ensure 4 byte alignment
	if (size & 3) {
		size += 4 - (size & 3);
	}
	
	// Calc pointer
	void *ptr = self->rwx_block + self->rwx_block_used;
	
	// Add this allocation to used size
	self->rwx_block_used += size;
	
	return ptr;
}

typedef struct LHStream {
	uint8_t data[0x100];
	size_t head;
} LHStream;

static void LHStreamInit(LHStream *self) {
	memset(self, 0, sizeof *self);
}

static void LHStreamWrite(LHStream *self, size_t size, void *data) {
	if (self->head + size > sizeof(self->data)) {
		// fail
		return;
	}
	
	memcpy(self->data + self->head, data, size);
	self->head += size;
}

static size_t LHStreamWrite32(LHStream *self, uint32_t data) {
	LHStreamWrite(self, sizeof data, &data);
	return self->head - sizeof data;
}

static size_t LHStreamWrite64(LHStream *self, uint64_t data) {
	LHStreamWrite(self, sizeof data, &data);
	return self->head - sizeof data;
}

static size_t LHStreamTell(LHStream *self) {
	return self->head;
}

// Offset from next instruction to next available data region
#define LH_INS_OFFSET (((block_size + 1) * sizeof(uint32_t)) - LHStreamTell(&code) + LHStreamTell(&data))

static uint32_t *LHRewriteAArch64Block(LHHooker *self, uint32_t *old_block, size_t block_size) {
	/**
	 * Rewrite a block of instructions located at `old_block` to be position
	 * indepedent, also inserting a jump back to (old_block + block_size) at the
	 * end.
	 */
	
	LHStream code; LHStreamInit(&code);
	LHStream data; LHStreamInit(&data);
	
	for (size_t i = 0; i < block_size; i++) {
		if (0) {
			// ... handle various instruction types ...
		}
		else {
			LHStreamWrite32(&code, old_block[i]);
		}
	}
	
	// Insert jump back to end
	// TODO: Actually figure out which registers are available to use instead of
	// just using x17
	LHStreamWrite();
	
	// Copy to rwx block
	void *new_block = LHHookerAllocRwx(self, LHStreamTell(&code) + LHStreamTell(&data));
	memcpy(new_block, code.data, LHStreamTell(&code));
	memcpy(new_block + LHStreamTell(&code), data.data, LHStreamTell(&data));
	return new_block;
}

#undef LH_INS_OFFSET

#endif // LEAFHOOK_IMPLEMENTATION
#endif // _LEAFHOOK_HEADER
