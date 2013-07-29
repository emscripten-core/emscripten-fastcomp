=======================
PNaCl Developer's Guide
=======================

.. contents::
   :local:
   :depth: 3

Introduction
============

TODO

Memory Model and Atomics
========================

Volatile Memory Accesses
------------------------

The C11/C++11 standards mandate that ``volatile`` accesses execute in program
order (but are not fences, so other memory operations can reorder around them),
are not necessarily atomic, and can’t be elided. They can be separated into
smaller width accesses.

The PNaCl toolchain applies regular LLVM optimizations along these guidelines,
and it further prevents any load/store (even non-``volatile`` and non-atomic
ones) from moving above or below a volatile operations: they act as compiler
barriers before optimizations occur. The PNaCl toolchain freezes ``volatile``
accesses after optimizations into atomic accesses with sequentially consistent
memory ordering. This eases the support of legacy (i.e. non-C11/C++11) code, and
combined with builtin fences these programs can do meaningful cross-thread
communication without changing code. It also reflects the original code's intent
and guarantees better portability.

Relaxed ordering could be used instead, but for the first release it is more
conservative to apply sequential consistency. Future releases may change what
happens at compile-time, but already-released pexes will continue using
sequential consistency.

The PNaCl toolchain also requires that ``volatile`` accesses be at least
naturally aligned, and tries to guarantee this alignment.

Memory Model for Concurrent Operations
--------------------------------------

The memory model offered by PNaCl relies on the same coding guidelines as the
C11/C++11 one: concurrent accesses must always occur through atomic primitives
(offered by `atomic intrinsics <PNaClLangRef.html#atomicintrinsics>`_), and
these accesses must always occur with the same size for the same memory
location. Visibility of stores is provided on a happens-before basis that
relates memory locations to each other as the C11/C++11 standards do.

As in C11/C++11 some atomic accesses may be implemented with locks on certain
platforms. The ``ATOMIC_*_LOCK_FREE`` macros will always be ``1``, signifying
that all types are sometimes lock-free. The ``is_lock_free`` methods will return
the current platform's implementation at runtime.

The PNaCl toolchain supports concurrent memory accesses through legacy GCC-style
``__sync_*`` builtins, as well as through C11/C++11 atomic primitives.
``volatile`` memory accesses can also be used, though these are discouraged, and
aren't present in bitcode.

PNaCl supports concurrency and parallelism with some restrictions:

* Threading is explicitly supported.

* Inter-process communication through shared memory is limited to operations
  which are lock-free on the current platform (``is_lock_free`` methods). This
  may change at a later date.

* Direct interaction with device memory isn't supported.

* Signal handling isn't supported, PNaCl therefore promotes all primitives to
  cross-thread (instead of single-thread). This may change at a later date. Note
  that using atomic operations which aren't lock-free may lead to deadlocks when
  handling asynchronous signals.
  
* ``volatile`` and atomic operations are address-free (operations on the same
  memory location via two different addresses work atomically), as intended by
  the C11/C++11 standards. This is critical for inter-process communication as
  well as synchronous "external modifications" such as mapping underlying memory
  at multiple locations.

Setting up the above mechanisms requires assistance from the embedding sandbox's
runtime (e.g. NaCl's Pepper APIs), but using them once setup can be done through
regular C/C++ code.

The PNaCl toolchain currently optimizes for memory ordering as LLVM normally
does, but at pexe creation time it promotes all ``volatile`` accesses as well as
all atomic accesses to be sequentially consistent. Other memory orderings will
be supported in a future release, but pexes generated with the current toolchain
will continue functioning with sequential consistency. Using sequential
consistency provides a total ordering for all sequentially-consistent operations
on all addresses.

This means that ``volatile`` and atomic memory accesses can only be re-ordered
in some limited way before the pexe is created, and will act as fences for all
memory accesses (even non-atomic and non-``volatile``) after pexe creation.
Non-atomic and non-``volatile`` memory accesses may be reordered (unless a fence
intervenes), separated, elided or fused according to C and C++'s memory model
before the pexe is created as well as after its creation.

Atomic Memory Ordering Constraints
----------------------------------

Atomics follow the same ordering constraints as in regular LLVM, but
all accesses are promoted to sequential consistency (the strongest
memory ordering) at pexe creation time. As more C11/C++11 code
allows us to understand performance and portability needs we intend
to support the full gamut of C11/C++11 memory orderings:

- Relaxed: no operation orders memory.
- Consume: a load operation performs a consume operation on the affected memory
  location (currently unsupported by LLVM).
- Acquire: a load operation performs an acquire operation on the affected memory
  location.
- Release: a store operation performs a release operation on the affected memory
  location.
- Acquire-release: load and store operations perform acquire and release
  operations on the affected memory.
- Sequentially consistent: same as acquire-release, but providing a global total
  ordering for all affected locations.

As in C11/C++11:

- Atomic accesses must at least be naturally aligned.
- Some accesses may not actually be atomic on certain platforms, requiring an
  implementation that uses a global lock.
- An atomic memory location must always be accessed with atomic primitives, and
  these primitives must always be of the same bit size for that location.
- Not all memory orderings are valid for all atomic operations.

