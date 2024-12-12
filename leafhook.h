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
	
} LHHooker;

#ifdef LEAFHOOK_IMPLEMENTATION

// Test if input is negative for sign extend
#define LH_SEXT_ISNEG(input, nbits) (input & (1 << (nbits - 1)))

// Get bits to OR to sign extend if negative
#define LH_SEXT64_NB(nbits) (0xffffffffffffffff - ( (1 << (nbits - 1)) - 1 ))

// Fill the last (64 - nbits) bits with 1's if most significant is 1 or zero if
// most significant is zero.
#define LH_SEXT64(input, nbits) (LH_SEXT_ISNEG(input, nbits) ? (LH_SEXT64_NB(nbits) | input) : input)

#endif // LEAFHOOK_IMPLEMENTATION
#endif // _LEAFHOOK_HEADER
