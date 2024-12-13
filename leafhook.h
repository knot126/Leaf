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
#define MAKE_AARCH64_ADR(imm, Rd) ((((Rd) & 0x1f) << 0) | (((imm >> 2) & 0x7ffff) << 5) | (0b10000 << 24) | (((imm) & 0x3) << 29) | (0b0 << 31))
#define AARCH64_ADR_DECODE_IMM(input) ((((input >> 29) & 0x3) << 0) | (((input >> 5) & 0x7ffff) << 2))
#define AARCH64_ADR_DECODE_RD(input) ((((input >> 0) & 0x1f) << 0))

#define IS_AARCH64_ADR(input) ((input & 0x9f000000) == 0x10000000)
#define MAKE_AARCH64_ADRP(imm, Rd) ((((Rd) & 0x1f) << 0) | (((imm >> 2) & 0x7ffff) << 5) | (0b10000 << 24) | (((imm) & 0x3) << 29) | (0b1 << 31))
#define AARCH64_ADRP_DECODE_IMM(input) ((((input >> 29) & 0x3) << 0) | (((input >> 5) & 0x7ffff) << 2))
#define AARCH64_ADRP_DECODE_RD(input) ((((input >> 0) & 0x1f) << 0))

#define IS_AARCH64_ADRP(input) ((input & 0x9f000000) == 0x90000000)
#define MAKE_AARCH64_LDR_LITERAL(x, imm, Rt) ((((Rt) & 0x1f) << 0) | (((imm) & 0x7ffff) << 5) | (0b011000 << 24) | (((x) & 0x1) << 30) | (0b0 << 31))
#define AARCH64_LDR_LITERAL_DECODE_X(input) ((((input >> 30) & 0x1) << 0))
#define AARCH64_LDR_LITERAL_DECODE_IMM(input) ((((input >> 5) & 0x7ffff) << 0))
#define AARCH64_LDR_LITERAL_DECODE_RT(input) ((((input >> 0) & 0x1f) << 0))

#define IS_AARCH64_LDR_LITERAL(input) ((input & 0xbf000000) == 0x18000000)
// END AUTO GENERATED MACROS

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

#ifdef LH_AARCH64
static uint32_t *LHRewriteAArch64Block(LHHooker *self, uint32_t *old_block, size_t block_size) {
	/**
	 * Rewrite a block of instructions located at `old_block` to be position
	 * indepedent, also inserting a jump back to (old_block + block_size) at the
	 * end.
	 */
	
	LHStream code; LHStreamInit(&code);
	LHStream data; LHStreamInit(&data);
	
	for (size_t i = 0; i < block_size; i++) {
		uint32_t ins = old_block[i];
		
		if (IS_AARCH64_ADR(ins)) {
			uint32_t Rd = AARCH64_ADR_DECODE_RD(ins);
			size_t imm = LH_SEXT64(AARCH64_ADR_DECODE_IMM(ins));
			
			size_t result = (size_t) (((void *)&old_block[i]) + imm);
			size_t offset = LH_INS_OFFSET;
			
			LHStreamWrite32(&code, MAKE_AARCH64_LDR_LITERAL(1, offset, Rd));
			LHStreamWrite64(&data, result);
		}
		else if (IS_AARCH64_ADRP(ins)) {
			// similar to adr but works with respect to pages
			uint32_t Rd = AARCH64_ADR_DECODE_RD(ins);
			size_t imm = LH_SEXT64(AARCH64_ADR_DECODE_IMM(ins)) << 12;
			
			size_t result = (size_t) ((void *)&old_block[i]);
			result &= 0xfffffffffffff000;
			result += imm;
			size_t offset = LH_INS_OFFSET;
			
			LHStreamWrite32(&code, MAKE_AARCH64_LDR_LITERAL(1, offset, Rd));
			LHStreamWrite64(&data, result);
		}
		else if (IS_AARCH64_LDR_LITERAL(ins)) {
			bool x = AARCH64_LDR_LITERAL_DECODE_X(ins);
			uint32_t Rt = AARCH64_LDR_LITERAL_DECODE_RT(ins);
			size_t imm = LH_SEXT64(AARCH64_LDR_LITERAL_DECODE_IMM(ins)) << 2;
			
			LHStreamWrite32(&code, MAKE_AARCH64_LDR_LITERAL(x, LH_INS_OFFSET, Rt));
			
			if (x) {
				uint64_t d = ((uint64_t *)(((void *)&old_block[i]) + imm))[i];
				LHStreamWrite64(&data, d);
			}
			else {
				uint32_t d = ((uint32_t *)(((void *)&old_block[i]) + imm))[i];
				LHStreamWrite32(&data, d);
			}
		}
		else {
			LHStreamWrite32(&code, old_block[i]);
		}
	}
	
	// Insert jump back to end
	// TODO: Actually figure out which registers are available to use instead of
	// just using x17
	LHStreamWrite32(&code, MAKE_AARCH64_LDR_LITERAL(1, LH_INS_OFFSET, 17));
	LHStreamWrite64(&data, (size_t)(old_block + block_size));
	
	// Copy to rwx block
	void *new_block = LHHookerAllocRwx(self, LHStreamTell(&code) + LHStreamTell(&data));
	memcpy(new_block, code.data, LHStreamTell(&code));
	memcpy(new_block + LHStreamTell(&code), data.data, LHStreamTell(&data));
	return new_block;
}
#endif // LH_AARCH64

#undef LH_INS_OFFSET

#endif // LEAFHOOK_IMPLEMENTATION
#endif // _LEAFHOOK_HEADER
