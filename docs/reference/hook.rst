=========
Leaf Hook
=========

Notes
=====

* Unlike other libraries that autodetect which platform to use, you must define
  either ``LH_AARCH64`` or ``LH_AARCH32`` before including the library.
* Leaf Hook only supports AArch64 and AArch32.
* Leaf Hook is not complete and will break in several edge cases.
* You are responsible for ensuring proper memory protection permissions when you hook functions.
* Please consider other hooking libraries before using Leaf Hook. They are probably much better.

Structures
==========

.. struct:: LHHooker
   
   Hooking context, mainly for allocation of rewritten code blocks

Functions
=========

.. function:: LHHooker *LHHookerCreate(void)
   
   Create a hooking context. This memory maps 10 pages for the rewritten
   function starts which is a "legacy" hardcoded number. If you want more or
   less use :func:`LHHookerCreateEx` now.

.. function:: LHHooker *LHHookerCreateEx(size_t npages)
   
   Create a hooking context, specifying the number of pages that should be
   allocated for rewritten code blocks.

.. function:: void LHHookerRelease(LHHooker *self)
   
   .. danger::
      
      Because hooks cannot be undone at this time, this will completely break
      any functions with hooks installed and cause them to crash the process.
      Never call this function.
   
   Release all resource associated with this hooking context.

.. function:: bool LHHookerHookFunction(LHHooker *self, void *function, void *hook, void **orig)
   
   Hook or replace a function
   
   .. caution::
      
      Leaf Hook does not preform size checks on the function. If the trampoline
      is larger than the function to be hooked, it may overwrite following
      functions or data.
   
   :param self: Hooking context, if :expr:`orig == NULL` then this can be :expr:`NULL` too
   :param function: Address of the function to hook
   :param hook: The function to call instead of the original
   :param orig: If not :expr:`NULL`, the address of a new function which is equivlent to the old one will be written here
   :returns: A boolean which is :expr:`true` on success or :expr:`false` on failure
   :retval false: When :expr:`orig != NULL` means there was an error rewriting the code block
   :retval false: Achitecture is not supported
