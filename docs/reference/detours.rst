============
Leaf Detours
============

Notes
=====

* Please remember that you are responsible for ensuring that any affected pages
  are readable, writable, and executable when they need to be. When using with
  an ELF loader like Leaf, that always loads all pages as RWX, this is not a
  problem.

Structures
==========

.. struct:: LeafDetour

   State for a single detour
   
   .. member:: void *function
   
   A pointer to the detoured function's address.
   
   .. member:: void *trampoline
   
   If using a double trampoline, this a pointer to the allocated trampoline.
   It is NULL if there was no need for a second trampoline.
   
   .. member:: size_t buffer_size
   
   Amount of real data in the back buffer
   
   .. member:: unsigned char buffer[LEAF_DETOUR_MAX_SIZE]
   
   A back buffer which is large enough to hold the biggest jump instruction
   size available. This holds either the replaced bytes of the instruction
   (when the detour is installed) or the trampoline instructions (when the
   hook is uninstalled).

User Segment Allocator
----------------------

.. type:: void *(*LeafDetourNearBlockFunction)(void *context, void *block, size_t size)
   
   A function that:
   
   * When ``block`` is ``NULL`` and :expr:`size > 0`, allocates a block of memory of
     at least ``size`` bytes nearby the ``.text`` segment of the binary
     containing the function to hook. On success it returns a pointer to the
     start of the block, and on failure returns ``NULL``.
   * When ``block`` is not ``NULL`` and :expr:`size == 0`, *may* deallocate a
     previously allocated block for future use. Return value is ignored, but
     should probably be :expr:`NULL`.
   * Otherwise, is undefined.
   
   You can optionally provide a ``context``.

.. struct:: LeafDetourAlloc
   
   A near block allocation function with an associated context.
   
   .. member:: void *context
   
      An opaque context object which is passed to the allocation function
      whenever required
   
   .. member:: LeafDetourNearBlockFunction func
      
      A pointer to the allocation function

Constants
=========

.. macro:: LEAF_DETOUR_MAX_SIZE
   
   The size of the largest trampoline construct supported with the architecture
   that Leaf Detours is being compiled for.

.. macro:: LEAF_DETOUR_SUCCESS
   
   For operations that return integer status codes, indicates success.

.. macro:: LEAF_DETOUR_NO_SPACE
   
   Indicates that you tried hooking a function that is too small to be hooked
   given the current constraints.

.. macro:: LEAF_DETOUR_ALLOC_FAILED
   
   Indicates that your allocator function failed to allocate memory (e.g. it
   returned :expr:`NULL` when :expr:`size > 0`).

.. macro:: LEAF_DETOUR_OUT_OF_RANGE
   
   Indicates that your allocator returned a block which is too far to be jumped
   to from the function to be hooked. This should only occur with big binaries.

Functions
=========

.. function:: int LeafDetourPrepareEx(LeafDetour *self, void *function, size_t function_size, void *detour, size_t detour_size, LeafDetourAlloc *near)
   
   Takes an uninitialised detour object and prepares it for use.
   
   If this function fails, the contents of the :expr:`LeafDetour` are undefined.
   
   :param self: A pointer to an uninitialised :expr:`LeafDetour` which will hold the detour data
   :param function: Address of the function to be hooked.
   :param function_size: The size, in bytes, of the function to be hooked.
   :param detour: The hook to be called instead of the original function whenever the detour is installed.
   :param detour_size: The size of the detour. Allowed to be 0 if that is unknown.
   :param near: :expr:`LeafDetourAlloc` describing a near-``.text`` allocator to use for small function hooking. This can be NULL to disable the small function hooking feature.
   :returns: Signed integer error code which is negative on failure and zero on success
   :retval LEAF_DETOUR_SUCCESS: When the function has been successfully hooked
   :retval LEAF_DETOUR_NO_SPACE: When the function is not large enough to be hooked
   :retval LEAF_DETOUR_ALLOC_FAILED: When :expr:`near != NULL` and allocation failed
   :retval LEAF_DETOUR_OUT_OF_RANGE: When :expr:`near != NULL` and the returned block was outside of short jump range

.. function:: int LeafDetourCreateEx(LeafDetour *self, void *function, size_t function_size, void *detour, size_t detour_size, LeafDetourAlloc *near)
   
   This is the same as :func:`LeafDetourPrepareEx`, but it immediately calls :func:`LeafDetourSwap` to install the detour if preparing the detour is successful.
   
   If you have no intent to ever uninstall the hook, you can safely discard the resulting :expr:`LeafDetour` struct.

.. function:: void LeafDetourSwap(LeafDetour *self)
   
   Swaps the front and back buffers in the :expr:`LeafDetour` object,
   effectively installing or uninstalling the detour.
   
   Note that there is currently no way to query if the hook is currently
   installed or uninstalled.

.. function:: void LeafDetourDestroyEx(LeafDetour *self, LeafDetourAlloc *near)
   
   Release the trampoline associated with the given detour. If :expr:`near == NULL` then this is a no-op.
