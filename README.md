<img src="img/Leaf.png" style="width:100%"/>

# Leaf

Leaf is a collection of single header libraries meant for working with programs (mainly video games) at a low level.

* [Leaf](leaf.h) - The main project, a custom ELF loader, for AArch32 and AArch64 (with untested x86 and x86_64 support). Originally made for bypassing Android Q's restrictions on marking native code pages as RWX. Natrually all segments are loaded as RWX and it provides some replacement for dlsym() lookups.
* [LeafHook](leafhook.h) - Native function hooking library, for AArch32 and AArch64, works similarly to something like Cydia Substrate or comex's Substitute. Might support other hooking methods in the future.

## Future Plans

* Thread safe LeafHook with more hooking/detour methods (instruction copy, software break), unhooking support, and even mid-function hooking.
* LeafDebug for debugging programs without a "real" debugger.
