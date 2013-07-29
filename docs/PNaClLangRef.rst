==============================
PNaCl Bitcode Reference Manual
==============================

.. contents::
   :local:
   :depth: 3

Introduction
============

This document is a reference manual for the PNaCl bitcode format. It describes
the bitcode on a *semantic* level; the physical encoding level will be described
elsewhere. For the purpose of this document, the textual form of LLVM IR is
used to describe instructions and other bitcode constructs.

Since the PNaCl bitcode is based to a large extent on LLVM IR, many sections
in this document point to a relevant section of the LLVM language reference
manual. Only the changes, restrictions and variations specific to PNaCl are
described - full semantic descriptions are not duplicated from the LLVM
reference manual.

*[TODO(eliben): this may gradually change in the future, as we move more
contents into this document; also, the physical encoding will also be described
here in the future, once we know what it's going to be]*

High Level Structure
====================

A PNaCl portable executable ("pexe" in short) is a single LLVM IR module.

.. _linkagetypes:

Linkage Types
-------------

`LLVM LangRef: Linkage Types <LangRef.html#linkage>`_

The linkage types supported by PNaCl bitcode are ``internal`` and ``external``.
A single function in the pexe, named ``_start``, has the linkage type
``external``. All the other functions and globals have the linkage type
``internal``.

Calling Conventions
-------------------

`LLVM LangRef: Calling Conventions <LangRef.html#callingconv>`_

The only calling convention supported by PNaCl bitcode is ``ccc`` - the C
calling convention.

Visibility Styles
-----------------

`LLVM LangRef: Visibility Styles <LangRef.html#visibilitystyles>`_

PNaCl bitcode does not support visibility styles.

Global Values
-------------

The following restrictions apply to both global variables and functions.

These attributes are disallowed:

* ``addrspace``, ``section``, ``unnamed_addr``.

.. _globalvariables:

Global Variables
----------------

`LLVM LangRef: Global Variables <LangRef.html#globalvars>`_

Restrictions on global variables:

* PNaCl bitcode does not support TLS models.
* Restrictions on :ref:`linkage types <linkagetypes>`.
* ``externally_initialized``.

Every global variable must have an initializer. Each initializer must be
either a *SimpleElement* or a *CompoundElement*, defined as follows.

A *SimpleElement* is one of the following:

1) An i8 array literal or ``zeroinitializer``:

     [SIZE x i8] c"DATA"
     [SIZE x i8] zeroinitializer

2) A reference to a *GlobalValue* (a function or global variable) with an
   optional 32-bit byte offset added to it (the addend, which may be
   negative):

     ptrtoint (TYPE* @GLOBAL to i32)
     add (i32 ptrtoint (TYPE* @GLOBAL to i32), i32 ADDEND)

A *CompoundElement* is a unnamed, packed struct containing more than one
*SimpleElement*.

Functions
---------

`LLVM LangRef: Functions <LangRef.html#functionstructure>`_

The restrictions on :ref:`linkage types <linkagetypes>`, calling conventions
and visibility styles apply to functions. In addition, the following are
not supported for functions:

* Function attributes (either for the the function itself, its parameters or its
  return type).
* Garbage collector name (``gc``).
* Functions with a variable number of arguments (*vararg*).
* Alignment (``align``).

Aliases
-------

`LLVM LangRef: Aliases <LangRef.html#langref_aliases>`_

PNaCl bitcode does not support aliases.

Named Metadata
--------------

`LLVM LangRef: Named Metadata <LangRef.html#namedmetadatastructure>`_

While PNaCl bitcode has provisions for debugging metadata, it is not considered
part of the stable ABI. It exists for tool support and should not appear in
distributed pexes.

Other kinds of LLVM metadata are not supported.

Module-Level Inline Assembly
----------------------------

`LLVM LangRef: Module-Level Inline Assembly <LangRef.html#moduleasm>`_

PNaCl bitcode does not support inline assembly.

Volatile Memory Accesses
------------------------

`LLVM LangRef: Volatile Memory Accesses <LangRef.html#volatile>`_

PNaCl bitcode does not support volatile memory accesses. The ``volatile``
attribute on loads and stores is not supported.

.. note::

    The C11/C++11 standards mandate that ``volatile`` accesses execute
    in program order (but are not fences, so other memory operations can
    reorder around them), are not necessarily atomic, and can’t be
    elided. They can be separated into smaller width accesses.

    The PNaCl toolchain applies regular LLVM optimizations along these
    guidelines, and it further prevents any load/store (even
    non-``volatile`` and non-atomic ones) from moving above or below a
    volatile operations: they act as compiler barriers before
    optimizations occur. The PNaCl toolchain freezes ``volatile``
    accesses after optimizations into atomic accesses with sequentially
    consistent memory ordering. This eases the support of legacy
    (i.e. non-C11/C++11) code, and combined with builtin fences these
    programs can do meaningful cross-thread communication without
    changing code. It also reflects the original code's intent and
    guarantees better portability.

    Relaxed ordering could be used instead, but for the first release it
    is more conservative to apply sequential consistency. Future
    releases may change what happens at compile-time, but
    already-released pexes will continue using sequential consistency.

    The PNaCl toolchain also requires that ``volatile`` accesses be at
    least naturally aligned, and tries to guarantee this alignment.

Memory Model for Concurrent Operations
--------------------------------------

`LLVM LangRef: Memory Model for Concurrent Operations <LangRef.html#memmodel>`_

The memory model offered by PNaCl relies on the same coding guidelines
as the C11/C++11 one: concurrent accesses must always occur through
atomic primitives (offered by `atomic intrinsics`_), and these accesses
must always occur with the same size for the same memory
location. Visibility of stores is provided on a happens-before basis
that relates memory locations to each other as the C11/C++11 standards
do.

.. note::

    As in C11/C++11 some atomic accesses may be implemented with locks
    on certain platforms. The ``ATOMIC_*_LOCK_FREE`` macros will always
    be ``1``, signifying that all types are sometimes lock-free. The
    ``is_lock_free`` methods will return the current platform's
    implementation at runtime.

    The PNaCl toolchain supports concurrent memory accesses through
    legacy GCC-style ``__sync_*`` builtins, as well as through C11/C++11
    atomic primitives. ``volatile`` memory accesses can also be used,
    though these are discouraged, and aren't present in bitcode.

    PNaCl supports concurrency and parallelism with some restrictions:

    * Threading is explicitly supported.
    * Inter-process communication through shared memory is limited to
      operations which are lock-free on the current platform
      (``is_lock_free`` methods). This may change at a later date.
    * Direct interaction with device memory isn't supported.
    * Signal handling isn't supported, PNaCl therefore promotes all
      primitives to cross-thread (instead of single-thread). This may
      change at a later date. Note that using atomic operations which
      aren't lock-free may lead to deadlocks when handling asynchronous
      signals.
    * ``volatile`` and atomic operations are address-free (operations on
      the same memory location via two different addresses work
      atomically), as intended by the C11/C++11 standards. This is
      critical for inter-process communication as well as synchronous
      "external modifications" such as mapping underlying memory at
      multiple locations.

    Setting up the above mechanisms requires assistance from the
    embedding sandbox's runtime (e.g. NaCl's Pepper APIs), but using
    them once setup can be done through regular C/C++ code.

    The PNaCl toolchain currently optimizes for memory ordering as LLVM
    normally does, but at pexe creation time it promotes all
    ``volatile`` accesses as well as all atomic accesses to be
    sequentially consistent. Other memory orderings will be supported in
    a future release, but pexes generated with the current toolchain
    will continue functioning with sequential consistency. Using
    sequential consistency provides a total ordering for all
    sequentially-consistent operations on all addresses.

    This means that ``volatile`` and atomic memory accesses can only be
    re-ordered in some limited way before the pexe is created, and will
    act as fences for all memory accesses (even non-atomic and
    non-``volatile``) after pexe creation. Non-atomic and
    non-``volatile`` memory accesses may be reordered (unless a fence
    intervenes), separated, elided or fused according to C and C++'s
    memory model before the pexe is created as well as after its
    creation.

Atomic Memory Ordering Constraints
----------------------------------

`LLVM LangRef: Atomic Memory Ordering Constraints <LangRef.html#ordering>`_

PNaCl bitcode currently supports sequential consistency only, through
its `atomic intrinsics`_.

.. note::

    Atomics follow the same ordering constraints as in regular LLVM, but
    all accesses are promoted to sequential consistency (the strongest
    memory ordering) at pexe creation time. As more C11/C++11 code
    allows us to understand performance and portability needs we intend
    to support the full gamut of C11/C++11 memory orderings:

       - Relaxed: no operation orders memory.
       - Consume: a load operation performs a consume operation on the
         affected memory location (currently unsupported by LLVM).
       - Acquire: a load operation performs an acquire operation on the
         affected memory location.
       - Release: a store operation performs a release operation on the
         affected memory location.
       - Acquire-release: load and store operations perform acquire and
         release operations on the affected memory.
       - Sequentially consistent: same as acquire-release, but providing
         a global total ordering for all affected locations.

    As in C11/C++11:

      - Atomic accesses must at least be naturally aligned.
      - Some accesses may not actually be atomic on certain platforms,
        requiring an implementation that uses a global lock.
      - An atomic memory location must always be accessed with atomic
        primitives, and these primitives must always be of the same bit
        size for that location.
      - Not all memory orderings are valid for all atomic operations.

Fast-Math Flags
---------------

`LLVM LangRef: Fast-Math Flags <LangRef.html#fastmath>`_

Fast-math mode is not currently supported by the PNaCl bitcode.

Type System
===========

`LLVM LangRef: Type System <LangRef.html#typesystem>`_

The LLVM types allowed in PNaCl bitcode are restricted, as follows:

Scalar types
------------

* The only scalar types allowed are integer, float, double and void.

  * The only integer sizes allowed are i1, i8, i16, i32 and i64.
  * The only integer sizes allowed for function arguments and function return
    values are i32 and i64.

Array and struct types
----------------------

Array and struct types are only allowed in
:ref:`global variable initializers <globalvariables>`.

.. _pointertypes:

Pointer types
-------------

Only the following pointer types are allowed:

* Pointers to valid PNaCl bitcode scalar types, as specified above.
* Pointers to functions.

In addition, the address space for all pointers must be 0.

A pointer is *inherent* when it represents the return value of an ``alloca``
instruction, or is an address of a global value.

A pointer is *normalized* if it's either:

* *inherent*
* Is the return value of a ``bitcast`` instruction.
* Is the return value of a ``inttoptr`` instruction.

Note: the size of a pointer in PNaCl is 32 bits.

Undefined Values
----------------

`LLVM LangRef: Undefined Values <LangRef.html#undefvalues>`_

``undef`` is only allowed within functions, not in global variable initializers.

Constant Expressions
--------------------

`LLVM LangRef: Constant Expressions <LangRef.html#constantexprs>`_

Constant expressions are only allowed in
:ref:`global variable initializers <globalvariables>`.

Other Values
============

Metadata Nodes and Metadata Strings
-----------------------------------

`LLVM LangRef: Metadata Nodes and Metadata Strings <LangRef.html#metadata>`_

While PNaCl bitcode has provisions for debugging metadata, it is not considered
part of the stable ABI. It exists for tool support and should not appear in
distributed pexes.

Other kinds of LLVM metadata are not supported.

Intrinsic Global Variables
==========================

`LLVM LangRef: Intrinsic Global Variables <LangRef.html#intrinsicglobalvariables>`_

PNaCl bitcode does not support intrinsic global variables.

Instruction Reference
=====================

This is a list of LLVM instructions supported by PNaCl bitcode. Where
applicable, PNaCl-specific restrictions are provided.

The following attributes are disallowed for all instructions:

* ``nsw`` and ``nuw``
* ``exact``

Only the LLVM instructions listed here are supported by PNaCl bitcode.

* ``ret``
* ``br``
* ``switch``

  i1 values are disallowed for ``switch``.

* ``add``, ``sub``, ``mul``, ``shl``,  ``udiv``, ``sdiv``, ``urem``, ``srem``,
  ``lshr``, ``ashr``

  These arithmetic operations are disallowed i1.

  Integer division (``udiv``, ``sdiv``, ``urem``, ``srem``) by zero is
  guaranteed to trap in PNaCl bitcode.

* ``and``
* ``or``
* ``xor``
* ``fadd``
* ``fsub``
* ``fmul``
* ``fdiv``
* ``frem``
* ``alloca``

  The only allowed type for ``alloca`` instructions in PNaCl bitcode
  is i8. The size argument must be an i32. For example:

.. code-block:: llvm

    %buf = alloca i8, i32 8, align 4

* ``load``, ``store``

  The pointer argument of these instructions must be a *normalized* pointer
  (see :ref:`pointer types <pointertypes>`). The ``volatile`` and ``atomic``
  attributes are not supported. Loads and stores of the type ``i1`` are not
  supported.

  These instructions must use ``align 1`` on integer memory accesses.

* ``trunc``
* ``zext``
* ``sext``
* ``fptrunc``
* ``fpext``
* ``fptoui``
* ``fptosi``
* ``uitofp``
* ``sitofp``

* ``ptrtoint``

  The pointer argument of a ``ptrtoint`` instruction must be a *normalized*
  pointer (see :ref:`pointer types <pointertypes>`) and the integer argument
  must be an i32.

* ``inttoptr``

  The integer argument of a ``inttoptr`` instruction must be an i32.

* ``bitcast``

  The pointer argument of a ``bitcast`` instruction must be a *inherent* pointer
  (see :ref:`pointer types <pointertypes>`).

* ``icmp``
* ``fcmp``
* ``phi``
* ``select``
* ``call``

Intrinsic Functions
===================

`LLVM LangRef: Intrinsic Functions <LangRef.html#intrinsics>`_

The only intrinsics supported by PNaCl bitcode are the following.

* ``llvm.memcpy``
* ``llvm.memmove``
* ``llvm.memset``

  These intrinsics are only supported with an i32 ``len`` argument.

* ``llvm.bswap``

  The overloaded ``llvm.bswap`` intrinsic is only supported with the following
  argument types: i16, i32, i64 (the types supported by C-style GCC builtins).

* ``llvm.ctlz``
* ``llvm.cttz``
* ``llvm.ctpop``

  The overloaded llvm.ctlz, llvm.cttz, and llvm.ctpop intrinsics are only
  supported with the i32 and i64 argument types (the types supported by
  C-style GCC builtins).

* ``llvm.sqrt``

  The overloaded ``llvm.sqrt`` intrinsic is only supported for float
  and double arguments types. Unlike the standard LLVM intrinsic,
  PNaCl guarantees that llvm.sqrt returns a QNaN for values less than -0.0.

* ``llvm.stacksave``
* ``llvm.stackrestore``
* ``llvm.trap``
* ``llvm.nacl.read.tp``

  TODO: describe

* ``llvm.nacl.longjmp``

  TODO: describe

* ``llvm.nacl.setjmp``

  TODO: describe

.. _atomic intrinsics:

* ``llvm.nacl.atomic.store``
* ``llvm.nacl.atomic.load``
* ``llvm.nacl.atomic.rmw``
* ``llvm.nacl.atomic.cmpxchg``
* ``llvm.nacl.atomic.fence``

  .. code-block:: llvm

    declare iN @llvm.nacl.atomic.load.<size>(
            iN* <source>, i32 <memory_order>)
    declare void @llvm.nacl.atomic.store.<size>(
            iN <operand>, iN* <destination>, i32 <memory_order>)
    declare iN @llvm.nacl.atomic.rmw.<size>(
            i32 <computation>, iN* <object>, iN <operand>, i32 <memory_order>)
    declare iN @llvm.nacl.atomic.cmpxchg.<size>(
            iN* <object>, iN <expected>, iN <desired>,
            i32 <memory_order_success>, i32 <memory_order_failure>)
    declare void @llvm.nacl.atomic.fence(i32 <memory_order>)

  Each of these intrinsics is overloaded on the ``iN`` argument, which
  is reflected through ``<size>`` in the overload's name. Integral types
  of 8, 16, 32 and 64-bit width are supported for these arguments.

  The ``@llvm.nacl.atomic.rmw`` intrinsic implements the following
  read-modify-write operations, from the general and arithmetic sections
  of the C11/C++11 standards:

   - ``add``
   - ``sub``
   - ``or``
   - ``and``
   - ``xor``
   - ``exchange``

  For all of these read-modify-write operations, the returned value is
  that at ``object`` before the computation. The ``computation``
  argument must be a compile-time constant.

  All atomic intrinsics also support C11/C++11 memory orderings, which
  must be compile-time constants. Those are detailed in `Atomic Memory
  Ordering Constraints`_.

  Integer values for these computations and memory orderings are defined
  in ``"llvm/IR/NaClAtomicIntrinsics.h"``.

  .. note::

      These intrinsics allow PNaCl to support C11/C++11 style atomic
      operations as well as some legacy GCC-style ``__sync_*`` builtins
      while remaining stable as the LLVM codebase changes. The user
      isn't expected to use these intrinsics directly.
