================
Function Hooking
================

Conceptually, function hooking is about intercepting function calls and
using this interception to modify the behaviour of functions, and thus of
programs overall.

Usually, this takes the form of a special function called a *hook* which has
the same *interface* (e.g. same arguments and return values) as the original
function, but with different behaviour.

Preferably, hooks should also be able to call the original function (or one
behaviourially equivlent to it) to make simple modifications easier, although
for many use cases simply replacing the function is sufficent.

---------------------
Replacment by Jumping
---------------------

Most functions you want to hook won't be simple: they will be "thightly linked"
with the rest of the code, so your only real option is to modify the code
of the function itself to invoke your hook.

The fundemental unit of operation here is the *trampoline*. By replacing
the start of a function with a jump instruction to your hook, all calls of the
original function will instead land in your hook. It "trampolines" out of the
original and directly into the hook.

For example, take the start of ``Debug::log()`` from Smash Hit::

   ff c3 03 d1      sub     sp,sp,#0xf0
   f3 53 00 a9      stp     x19,x20,[sp]=>local_f0
   93 04 00 f0      adrp    x19,0x360000
   fe 23 00 f9      str     x30,[sp, #local_b0]
   f7 63 02 a9      stp     x23,x24,[sp, #local_d0]

We can just put a special jump here (assuming our hook is at ``0xCCCCCCCCCCCCCCCC``)::

   50 00 00 58      ldr     x16,#0x8  ; Load address into x16
   00 02 1f d6      br      x16       ; Branch to address in x16
   cc cc cc cc      ; Not valid instructions, instead this is the address of the
   cc cc cc cc      ; hook.
   f7 63 02 a9      stp     x23,x24,[sp, #local_d0]

.. note::

   You might immediately be asking: "What if the x16 register is used to pass an
   argument? Won't that corrupt it?"

   The simple answer: it just won't! The standard AArch64 calling
   convention defines ``x16`` and ``x17`` as registers that can be "corrupted"
   between invoking a function and control actually transfering to it. Functions
   simply don't use that register by convention unless the calling convention is
   really, really weird and non-standard.

Of course, this will only replace the function. We can't call the original
function again using this approach by itself.

This also doesn't properly work for smaller functions. For example, you can't
really hook a function that just returns zero since that function simpily isn't
big enough to fit the jump without corrupting the start of the next function
after it.

Both *Leaf Hook* and *Leaf Detours* support this method of hooking functions:

- In Deotours, this is implicit by just releasing the Detour data while the hook
  is installed.
- In Leaf Hook, there is an explicit option not to make the original
  callable and avoid the work needed to do that.

Calling the Original by Swapping Instructions
=============================================

The easiest way to call the original function is just to restore the original
instruction bytes when the call is needed, and to restore the hook bytes when
the original is done being called.

This is usually done by double buffering: the address of the original function
holds the front buffer with either the original bytes or trampoline, and an
internal "hook context" structure holds the back buffer.

This is a mostly clean way of doing things that doesn't require complex hacks.
It does, however, have pitfalls:

- When hooking a recursive function, the hook is only called for the first call
  because the hook will be uninstalled when the function calls itself again.
- It does not work well with multithreaded code, since the function instructions
  are global state that you (probably) cannot and do not want to write locks
  or atomics for.

This is the method of hooking used by *Leaf Detours*.

Calling the Original with a Reassembled Function
================================================

It is also possible to reassemble the first few bytes of a function into a
separate block so that it behaves like the original, and insert a jump back to
the rest of the instruction at the end.

This approach is more amenable to multithreaded programs and recursive functions
since no global state has to be changed after the initial hook. It's also nicer
for users, since they don't need to remember to uninstall and then reinstall
the hook when they want to call the original function.

It is, however, more complicated in nature:

* First, you need to disassemble the first few bytes of the original function.
  This on its own is not trivial.

  * Actually, you *should* disassemble the whole function, because a function
    could well contain jumps back those first few instructions, and those
    would need to be fixed up. It's not terribly common, but a hypothetical
    possiblity, that most hooking libraries bother to handle (AFAICT).

* Then you will need to handle any PC-relative loads, stores, and jumps, the
  first two of which are often used in position independent code. You should
  be careful not to load stale values or assume that something is constant when
  it might not be.
* Next you need to create the jump back to the original function, which might
  require finding which registers are used and which aren't if your arch doesn't
  support full range jump instructions.

A very limited version of this approach is implemented by *Leaf Hook* when
replacement mode isn't enabled.

It should be noted that *Leaf Hook* doesn't actually bother to fully
disassemble the function, instead only checking for PC-relative load/store
instructions and fixing those up if required, since they are most common for
it's single use case. This means it will break in a *lot* of edge cases, but for
Smash Hit it has worked fine. This also means that x86 support would be
complicated due to the instruction set not being as easy to decode or test for
relevant instructions.

Hooking Small Functions with Double Trampolines
===============================================

While small functions (usually a few instructions or less) are generally hard to
hook because they just aren't big enough to fit a full jump instruction, this
can be mitigated on some architectures when using smaller binaries and a custom
dynamic loader.

Essentially, when the loader loads the program, it should allocate some extra RWX
space before and after the binary so that large trampolines can be inserted
there. The hooking library can then create small trampolines (e.g. one short
range jump which will fit in the small functions) to the big trampolines,
forming a double trampoline which will call the function in the end.

*Leaf Detours* is able to integrate with the *Leaf ELF Loader* to make hooking
small functions possible in this manner. It does require the application to
write its own allocator to manage Leaf's block of memory.

---------------------------------
Hooking with Software Breakpoints
---------------------------------

Software breakpoints, while usually used for debugging purposes, can be used to
hook functions. On most architectures a software breakpoint could hook even
very small functions because they are small instructions. However, they're pretty
slow, and should be avoided if possible.

--------------------------
Hooking External Functions
--------------------------

If there is a function you want to hook that is from a library external to the
program you want to modify, it is usually simplest to replace the address of the
original function with your hook function in the symbol table.

For example, if you wish to hook :func:`malloc` using your function
:func:`malloc_hook`, then it would be easist to just tell the program that the
address of :func:`malloc` is :func:`malloc_hook`, instead of the regular system
:func:`malloc`.

Leaf doesn't provide a mechanism to do this at the moment, but it can be done
by manually updating the address of the function in the function table and then
preforming the relevant relocations again.

---------------
Further Reading
---------------

* https://en.wikipedia.org/wiki/Hooking
* https://www.codereversing.com/archives/592
