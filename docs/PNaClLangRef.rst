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

Linkage Types
-------------

`LLVM LangRef: Linkage Types <LangRef.html#linkage>`_

The linkage types supported by PNaCl bitcode are ``internal`` and ``external``.
A single function in the pexe, named ``_start``, has the linkage type
``external``. All the other functions have the linkage type ``internal``.

Calling Conventions
-------------------

`LLVM LangRef: Calling Conventions <LangRef.html#callingconv>`_

The only calling convention supported by PNaCl bitcode is ``ccc`` - the C
calling convention.

Visibility Styles
-----------------

`LLVM LangRef: Visibility Styles <LangRef.html#visibilitystyles>`_

PNaCl bitcode does not support visibility styles.

Global Variables
----------------

`LLVM LangRef: Global Variables <LangRef.html#globalvars>`_

PNaCl bitcode does not support TLS models.

TODO: describe other restrictions on global variables

Functions
---------

`LLVM LangRef: Functions <LangRef.html#functionstructure>`_

The restrictions on linkage types, calling conventions and visibility styles
apply to functions. In addition, the following are not supported for functions:

* Function attributes.
* Section specification.
* Garbage collector name.
* Parameter attributes for the return type.

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

Parameter Attributes
--------------------

`LLVM LangRef: Parameter Attributes <LangRef.html#paramattrs>`_

TODO: what parameter attributes are supported.

Function Attributes
-------------------

`LLVM LangRef: Function Attributes <LangRef.html#fnattrs>`_

PNaCl bitcode does not support function attributes.

Module-Level Inline Assembly
----------------------------

`LLVM LangRef: Module-Level Inline Assembly <LangRef.html#moduleasm>`_

PNaCl bitcode does not support inline assembly.

Volatile Memory Accesses
------------------------

`LLVM LangRef: Volatile Memory Accesses <LangRef.html#volatile>`_

TODO: are we going to promote volatile to atomic?

Memory Model for Concurrent Operations
--------------------------------------

`LLVM LangRef: Memory Model for Concurrent Operations <LangRef.html#memmodel>`_

TODO.

Atomic Memory Ordering Constraints
----------------------------------

`LLVM LangRef: Atomic Memory Ordering Constraints <LangRef.html#ordering>`_

TODO.

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
  * The only integer sizes allowed for function arguments are i32 and i64.

Arrays and structs are only allowed in TODO.

.. _pointertypes:

Pointer types
-------------

Pointer types are allowed with the following restrictions:

* Pointers to valid PNaCl bitcode scalar types, as specified above.
* Pointers to functions (but not intrinsics).
* The address space for all pointers must be 0.

A pointer is *inherent* when it represents the return value of an ``alloca``
instruction, or is an address of a global value.

A pointer is *normalized* if it's either:

* *inherent*
* Is the return value of a ``bitcast`` instruction.
* Is the return value of a ``inttoptr`` instruction.

Note: the size of a pointer in PNaCl is 32 bits.

Global Variable and Function Addresses
--------------------------------------

Undefined Values
----------------

`LLVM LangRef: Undefined Values <LangRef.html#undefvalues>`_

Poison Values
-------------

`LLVM LangRef: Poison Values <LangRef.html#poisonvalues>`_

PNaCl bitcode does not support poison values; consequently, the ``nsw`` and
``nuw`` are not supported.

Constant Expressions
--------------------

`LLVM LangRef: Constant Expressions <LangRef.html#constantexprs>`_

In the general sense, PNaCl bitcode does not support constant expressions.
There is a single, restricted, use case permitted in global initializers,
where the ``add`` and ``ptrtoint`` constant expressions are allowed.

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

Only the LLVM instructions listed here are supported by PNaCl bitcode.

* ``ret``
* ``br``
* ``switch``
* ``add``

  The ``nsw`` and ``nuw`` modes are not supported.

* ``sub``

  The ``nsw`` and ``nuw`` modes are not supported.

* ``mul``

  The ``nsw`` and ``nuw`` modes are not supported.

* ``shl``

  The ``nsw`` and ``nuw`` modes are not supported.

* ``udiv``, ``sdiv``, ``urem``, ``srem``

  Integer division is guaranteed to trap in PNaCl bitcode. This trap can
  not be intercepted.

* ``lshr``
* ``ashr``
* ``and``
* ``or``
* ``xor``
* ``fadd``
* ``fsub``
* ``fmul``
* ``fdiv``
* ``frem``
* ``alloca``

  The only allowed type for ``alloca`` instructions in PNaCl bitcode is an
  array of i8. For example:

.. code-block:: llvm

    %buf = alloca [4 x i8], align 1

* ``load``, ``store``

  The pointer argument of these instructions must be a *normalized* pointer
  (see :ref:`pointer types <pointertypes>`).

* ``fence``
* ``cmpxchg``, ``atomicrmw``

  The pointer argument of these instructions must be a *normalized* pointer
  (see :ref:`pointer types <pointertypes>`).

  TODO(jfb): this may change

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

TODO(jfb): atomics

* ``llvm.memcpy``
* ``llvm.memmove``
* ``llvm.memset``
* ``llvm.bswap``

  The llvm.bswap intrinsic is only supported with the following argument types:
  i16, i32, i64.

* ``llvm.trap``
* ``llvm.nacl.read.tp``

  TODO: describe

* ``llvm.nacl.longjmp``

  TODO: describe

* ``llvm.nacl.setjmp``

  TODO: describe

