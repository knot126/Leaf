/**
 * ============
 * Leaf Detours
 * ============
 * 
 * Leaf Detours is a simpler and more complete alternative to Leaf Hook, albeit
 * with some downsides. I plan to update Leaf Hook to be more complete one day, 
 * but Detours is a good alternative for my use case.
 * 
 * Unlike a proper hooking library, Leaf Detours does not let you use the
 * original function without uninstalling the hook. This does come with some
 * upsides:
 * 
 * - Detours can be really simple, because we don't have to worry about
 *   disassembly.
 * - We also don't have to worry about correctness on architectures that need
 *   us to put the full address in registers before jumping to it.
 * - Detours can more easily support fast unhooking in almost all cases.
 * - Detours does not need to worry about memory allocation very much; however,
 *   it is capable of detouring small functions when given a special memory
 *   block which is nearby the .text segment. (With this on ARM, even one
 *   instruction functions can be detoured.)
 * 
 * You can actually still call the original function, by first uninstalling the
 * detour, then calling the function, then reinstalling the detour.
 */

#ifndef LEAF_DETOURS_HEADER_
#define LEAF_DETOURS_HEADER_

#include <string.h>
#include <stdint.h>

#define LEAF_DETOUR_SUCCESS 0
#define LEAF_DETOUR_NO_SPACE (-1)
#define LEAF_DETOUR_ALLOC_FAILED (-2)
#define LEAF_DETOUR_OUT_OF_RANGE (-3)

#ifdef __arm__
#define LEAF_DETOUR_MAX_SIZE 8
#elif defined(__aarch64__)
#define LEAF_DETOUR_MAX_SIZE 16
#elif defined(__i386__)
#define LEAF_DETOUR_MAX_SIZE 6
#elif defined(__x86_64__)
#define LEAF_DETOUR_MAX_SIZE 12 // TODO: What is it?
#else
#error "Platform unsupported"
#endif

// Optional callback to get a small block of memory within near-jump distance
// of the .text segment.
typedef void *(*LeafDetourNearBlockFunction)(void *context, void *block, size_t size);

typedef struct LeafDetourAlloc {
	void *context;
	LeafDetourNearBlockFunction func;
} LeafDetourAlloc;

/**
 * Detour Metadata Object
 */
typedef struct LeafDetour {
	// Pointer to the call address of the function that is being detoured.
	void *function;
	
	// For small functions, there may be an extra trampoline that does the
	// actual jump. if NULL, there is none
	void *trampoline;
	
	// The actual size of the back buffer
	size_t buffer_size;
	
	// Contains either the jump instruction or the original instructions,
	// depending on what is currently active
	unsigned char buffer[LEAF_DETOUR_MAX_SIZE];
} LeafDetour;

// Generate the LeafDetour object, but don't actually install the detour. 
int LeafDetourPrepareEx(LeafDetour *self, void *function, size_t function_size, void *detour, size_t detour_size, LeafDetourAlloc *near);

// Install or uninstall the detour
void LeafDetourSwap(LeafDetour *self);

// Deallocate any memory stored with the detour block (for now, this is only the
// associated trampoline block if one exists). The allocator must be the same
// one passed to LeafDetourPrepareEx or LeafDetourCreateEx, or NULL.
void LeafDetourDestroyEx(LeafDetour *self, LeafDetourAlloc *near);

// Helper function to create a detour, then install it
int LeafDetourCreateEx(LeafDetour *self, void *function, size_t function_size, void *detour, size_t detour_size, LeafDetourAlloc *near);

#endif

#ifdef LEAF_DETOURS_IMPLEMENTATION
#undef LEAF_DETOURS_IMPLEMENTATION

#define IN_RANGE(A, VALUE, B) (((VALUE) >= (A)) && ((VALUE) <= (B)))

/// ARCH SPECIFIC HELPERS ///
#ifdef __arm__
#define LEAF_DETOURS_SMALL_JUMP_IN_RANGE(FROM, TO) IN_RANGE(-33554432, ((int32_t)(TO) - (int32_t)(FROM)), 33554428)

#define LEAF_DETOURS_LONG_JUMP_SIZE 8
static inline void LeafDetour_LongJump(unsigned char *buffer, void *from, void *to) {
	uint32_t *buf = (uint32_t *) buffer;
	// this doesn't seem to work well! why.
	buf[0] = 0xe51ff004; // ldr pc, [pc, #-4]; 1110 0101 0001 1111 1111 0000 0000 0100
	buf[1] = (uint32_t) to;
#if 0
	buf[0] = 0xe59fc000;
	buf[1] = 0xe12fff1c;
	buf[2] = (uint32_t) to;
#endif
}

#define LEAF_DETOURS_SMALL_JUMP_SIZE 4
static inline void LeafDetour_ShortJump(unsigned char *buffer, void *from, void *to) {
	uint32_t *buf = (uint32_t *) buffer;
	const size_t pcoffset = (((size_t)to - (size_t)from) - 8) >> 2;
	buf[0] = 0xea000000 | (pcoffset & 0xffffff); // b <imm24>
}
#elif defined(__aarch64__)
#define LEAF_DETOURS_SMALL_JUMP_IN_RANGE(FROM, TO) IN_RANGE(-134217728, ((int64_t)(TO) - (int64_t)(FROM)), 134217724)

#define LEAF_DETOURS_LONG_JUMP_SIZE 16
static inline void LeafDetour_LongJump(unsigned char *buffer, void *from, void *to) {
	uint32_t * const buf = (uint32_t *) buffer;
	size_t * const bufzz = (size_t *) buffer;
	buf[0] = 0x58000050; // ldr x16, #0x8
	buf[1] = 0xd61f0200; // br x16
	bufzz[1] = (size_t) to;
} 

#define LEAF_DETOURS_SMALL_JUMP_SIZE 4
static inline void LeafDetour_ShortJump(unsigned char *buffer, void *from, void *to) {
	uint32_t * const buf = (uint32_t *) buffer;
	const size_t pcoffset = ((size_t)to - (size_t)from) >> 2;
	buf[0] = 0x14000000 | (pcoffset & 0x3ffffff); // b <imm26>
}
#elif defined(__i386__)
#define LEAF_DETOURS_LONG_JUMP_SIZE 5
static inline void LeafDetour_LongJump(unsigned char *buffer, void *from, void *to) {
	// EIP is the instruction following jump
	const size_t pcoffset = ((size_t)to - (size_t)from) - 5;
	buffer[0] = 0xE9; // jmp #<offset>
	*(uint32_t *)(buffer + 1) = pcoffset;
}
#elif defined(__x86_64__)
#define LEAF_DETOURS_LONG_JUMP_SIZE 12
static inline void LeafDetour_LongJump(unsigned char *buffer, void *from, void *to) {
	// RAX seems(?) safe to modify between function calls, like the arm IP
	// register
	buffer[0] = 0x48; // mov rax, #<address>
	buffer[1] = 0xb8;
	*(size_t *)(buffer + 2) = (size_t) to;
	buffer[10] = 0xff; // jmp rax
	buffer[11] = 0xe0;
}
#else
#error "Platform unsupported"
#endif

int LeafDetourPrepareEx(LeafDetour *self, void *function, size_t function_size, void *detour, size_t detour_size, LeafDetourAlloc *near) {
	/**
	 * Initialise all of the instructions and metadata for a detour. Does not
	 * install the detour.
	 */
	
	self->function = function;
	self->trampoline = NULL;
	
#ifdef LEAF_DETOURS_SMALL_JUMP_IN_RANGE
	const uint8_t in_small_range = LEAF_DETOURS_SMALL_JUMP_IN_RANGE(function, detour);
	
	// Close enough detours functions get a short jump automatically
	if (in_small_range) {
		if (function_size >= LEAF_DETOURS_SMALL_JUMP_SIZE) {
			self->buffer_size = LEAF_DETOURS_SMALL_JUMP_SIZE;
			LeafDetour_ShortJump(self->buffer, function, detour);
			return LEAF_DETOUR_SUCCESS;
		}
		else {
			return LEAF_DETOUR_NO_SPACE;
		}
	}
#endif
	
	// Just long jump if the function is long enough
	if (function_size >= LEAF_DETOURS_LONG_JUMP_SIZE) {
		self->buffer_size = LEAF_DETOURS_LONG_JUMP_SIZE;
		LeafDetour_LongJump(self->buffer, function, detour);
		return LEAF_DETOUR_SUCCESS;
	}
#ifdef LEAF_DETOURS_SMALL_JUMP_IN_RANGE
	// Not long enough for a long jump, but still enough for a short jump to a
	// trampoline in a special area, if that's available. 
	else if (near && (function_size >= LEAF_DETOURS_SMALL_JUMP_SIZE)) {
		void *trampoline = (near->func)(near->context, NULL, LEAF_DETOURS_LONG_JUMP_SIZE);
		
		if (!trampoline) {
			return LEAF_DETOUR_ALLOC_FAILED;
		}
		
		// Check sanity of user's function
		if (!LEAF_DETOURS_SMALL_JUMP_IN_RANGE(function, trampoline)) {
			(near->func)(near->context, trampoline, 0);
			return LEAF_DETOUR_OUT_OF_RANGE;
		}
		
		self->buffer_size = LEAF_DETOURS_SMALL_JUMP_SIZE;
		LeafDetour_ShortJump(self->buffer, function, trampoline); // func -> trampoline
		LeafDetour_LongJump(trampoline, trampoline, detour); // trampoline -> detour
		
		return LEAF_DETOUR_SUCCESS;
	}
#endif
	else {
		return LEAF_DETOUR_NO_SPACE;
	}
}

void LeafDetourSwap(LeafDetour *self) {
	/**
	 * Toggle the detour on and off by swapping the buffered instructions and
	 * the instructions currently at the function address.
	 */
	
	unsigned char tempbuf[LEAF_DETOUR_MAX_SIZE];
	
	memcpy(tempbuf, self->function, self->buffer_size);
	memcpy(self->function, &self->buffer, self->buffer_size);
	memcpy(&self->buffer, tempbuf, self->buffer_size);
}

void LeafDetourDestroyEx(LeafDetour *self, LeafDetourAlloc *near) {
	/**
	 * Free any resources associated with a detour. The allocator must be the
	 * same one used to initialise the detour.
	 */
	
	if (self->trampoline && near) {
		(near->func)(near->context, self->trampoline, 0);
	}
}

int LeafDetourCreateEx(LeafDetour *self, void *function, size_t function_size, void *detour, size_t detour_size, LeafDetourAlloc *near) {
	/**
	 * Initialise all of the instructions and metadata for a detour and install
	 * it.
	 * 
	 * This is effectively the same as calling LeafDetourPrepareEx() then
	 * LeafDetourSwap().
	 */
	
	int result = LeafDetourPrepareEx(self, function, function_size, detour, detour_size, near);
	
	if (result) {
		return result;
	}
	
	LeafDetourSwap(self);
	
	return result;
}

#endif
