# Leaf

A collection of single header libraries working with low-level stuff; currently, an ELF loading library and function hooking library.

* [Leaf](leaf.h) - The main project, a custom ELF loader. Made for bypassing Android Q's restrictions on marking native code pages as RWX. Natrually all segments are loaded as RWX and it provides some replacement for dlsym() lookups.
* [LeafHook](leafhook.h) - Native function hooking library, for AArch32 and AArch64, works similarly to something like Cydia Substrate or comex's Substitute. Might support other hooking methods in the future.
