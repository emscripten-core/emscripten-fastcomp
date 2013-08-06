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

Memory Model for Concurrent Operations
--------------------------------------

The memory model offered by PNaCl relies on the same coding guidelines
as the C11/C++11 one: concurrent accesses must always occur through
atomic primitives (offered by `atomic intrinsics
<PNaClLangRef.html#atomicintrinsics>`_), and these accesses must always
occur with the same size for the same memory location. Visibility of
stores is provided on a happens-before basis that relates memory
locations to each other as the C11/C++11 standards do.

Non-atomic memory accesses may be reordered, separated, elided or fused
according to C and C++'s memory model before the pexe is created as well
as after its creation.

As in C11/C++11 some atomic accesses may be implemented with locks on
certain platforms. The ``ATOMIC_*_LOCK_FREE`` macros will always be
``1``, signifying that all types are sometimes lock-free. The
``is_lock_free`` methods and ``atomic_is_lock_free`` will return the
current platform's implementation at translation time. These macros,
methods and functions are in the C11 header ``<stdatomic.h>`` and the
C++11 header ``<atomic>``.

The PNaCl toolchain supports concurrent memory accesses through legacy
GCC-style ``__sync_*`` builtins, as well as through C11/C++11 atomic
primitives.  ``volatile`` memory accesses can also be used, though these
are discouraged. See `Volatile Memory Accesses`_.

PNaCl supports concurrency and parallelism with some restrictions:

* Threading is explicitly supported and has no restrictions over what
  prevalent implementations offer. See `Threading`_.
  
* ``volatile`` and atomic operations are address-free (operations on the
  same memory location via two different addresses work atomically), as
  intended by the C11/C++11 standards. This is critical in supporting
  synchronous "external modifications" such as mapping underlying memory
  at multiple locations.

* Inter-process communication through shared memory is currently not
  supported. See `Future Directions`_.

* Signal handling isn't supported, PNaCl therefore promotes all
  primitives to cross-thread (instead of single-thread). This may change
  at a later date. Note that using atomic operations which aren't
  lock-free may lead to deadlocks when handling asynchronous
  signals. See `Future Directions`_.

* Direct interaction with device memory isn't supported, and there is no
  intent to support it. The embedding sandbox's runtime can offer APIs
  to indirectly access devices.

Setting up the above mechanisms requires assistance from the embedding
sandbox's runtime (e.g. NaCl's Pepper APIs), but using them once setup
can be done through regular C/C++ code.

Atomic Memory Ordering Constraints
----------------------------------

Atomics follow the same ordering constraints as in regular C11/C++11,
but all accesses are promoted to sequential consistency (the strongest
memory ordering) at pexe creation time. As more C11/C++11 code allows us
to understand performance and portability needs we intend to support the
full gamut of C11/C++11 memory orderings:

- Relaxed: no operation orders memory.
- Consume: a load operation performs a consume operation on the affected
  memory location (note: currently unsupported by LLVM).
- Acquire: a load operation performs an acquire operation on the
  affected memory location.
- Release: a store operation performs a release operation on the
  affected memory location.
- Acquire-release: load and store operations perform acquire and release
  operations on the affected memory.
- Sequentially consistent: same as acquire-release, but providing a
  global total ordering for all affected locations.

As in C11/C++11:

- Atomic accesses must at least be naturally aligned.
- Some accesses may not actually be atomic on certain platforms,
  requiring an implementation that uses global lock(s).
- An atomic memory location must always be accessed with atomic
  primitives, and these primitives must always be of the same bit size
  for that location.
- Not all memory orderings are valid for all atomic operations.

Volatile Memory Accesses
------------------------

The C11/C++11 standards mandate that ``volatile`` accesses execute in
program order (but are not fences, so other memory operations can
reorder around them), are not necessarily atomic, and can’t be
elided. They can be separated into smaller width accesses.

Before any optimizations occur the PNaCl toolchain transforms
``volatile`` loads and stores into sequentially consistent ``volatile``
atomic loads and stores, and applies regular compiler optimizations
along the above guidelines. This orders ``volatiles`` according to the
atomic rules, and means that fences (including ``__sync_synchronize``)
act in a better-defined manner. Regular memory accesses still do not
have ordering guarantees with ``volatile`` and atomic accesses, though
the internal representation of ``__sync_synchronize`` attempts to
prevent reordering of memory accesses to objects which may escape.

Relaxed ordering could be used instead, but for the first release it is
more conservative to apply sequential consistency. Future releases may
change what happens at compile-time, but already-released pexes will
continue using sequential consistency.

The PNaCl toolchain also requires that ``volatile`` accesses be at least
naturally aligned, and tries to guarantee this alignment.

The above guarantees ease the support of legacy (i.e. non-C11/C++11)
code, and combined with builtin fences these programs can do meaningful
cross-thread communication without changing code. They also better
reflect the original code's intent and guarantee better portability.

Threading
=========

Threading is explicitly supported through C11/C++11's threading
libraries as well as POSIX threads.

Communication between threads should use atomic primitives as described
in `Memory Model and Atomics`_.

Inline Assembly
===============

Inline assembly isn't supported by PNaCl because it isn't portable. The
one current exception is the common compiler barrier idiom
``asm("":::"memory")``, which gets transformed to a sequentially
consistent memory barrier (equivalent to ``__sync_synchronize()``). In
PNaCl this barrier is only guaranteed to order ``volatile`` and atomic
memory accesses, though in practice the implementation attempts to also
prevent reordering of memory accesses to objects which may escape.

Future Directions
=================

Inter-Process Communication
---------------------------

Inter-process communication through shared memory is currently not
supported by PNaCl.  When implemented, it may be limited to operations
which are lock-free on the current platform (``is_lock_free``
methods). It will rely on the address-free properly discussed in `Memory
Model for Concurrent Operations`_.

Signal Handling
---------------

Untrusted signal handling currently isn't supported by PNaCl. When
supported, the impact of ``volatile`` and atomics for same-thread signal
handling will need to be carefully detailed.
