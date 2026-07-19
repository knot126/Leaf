<img src="img/Leaf.png" style="width:100%"/>

# Leaf

Leaf is a collection of single header libraries meant for working with programs (mainly video games) at a low level.

The following libraries are actively developed:

* [Leaf ELF Loader](leaf.h) - Custom ELF loader with AArch32, AArch64, x86, and x86_64 support. Originally made for bypassing Android Q's restrictions on marking native code pages as RWX. Natrually all segments are loaded as RWX and it provides some replacement for dlsym() lookups among other useful functions.
* [Leaf Detours](leaf_detours.h) - Native function detouring<sup>1</sup> library supporting AArch32, AArch64, x86, and x86_64. Works by placing jumps to detours, which can then uninstall the detour to call the original function if wanted. When integrated properly with Leaf, it supports hooking even very small functions which many normal hooking libraries could not do.

We also have a different function hooking library that *Leaf Detours* is replacing:

* [Leaf Hook](leafhook.h) - Native function hooking library, for AArch32 and AArch64 only. Works by replacing the start of the function with a "big" jump to the hook, and giving the user a pointer to a reassembled block of code that will execute something equivlent to the original function again when called. It's similar to Cydia Subtrate or Substitute from the iOS world.

## Future Plans and TODO

* A better function hooking library. *Leaf Detours* improves things somewhat with a more reliable techinque and broader platform support, but it doesn't have a clean interface (puts requirements on the user to install/uninstall the hook) and has drawbacks of its own (like not being able to handle detour uninstall to call the original function when multiple threads call that function).
  * I want a thread safe hooking library with more hooking/detour methods for worst cases (instruction copying, software break), unhooking support, and even mid-function hooking, that doesn't require undos.
* "*Leaf Debug*" for self-debugging programs without a "real" debugger.

## Notes

1. The reason I say "detouring" and not "hooking" is because Detours does not rewrite the assembly of the function to be hooked in order to give a proper method to call the hooked function again. Instead, it relies on the undo support to allow the user to undo the hook, call the original function, then redo the hook. Since internally this only requires replacing the function, I feel detour is a better term for what is happening.
    a. *Also*, Since this changes program code constantly during execution instead of just once, it's not really multithreading compatible like Leaf Hook is.
