// Define support macros for defining classes, etc.

#ifndef LLVM_SUPPORT_SUPPORT_MACROS_H__
#define LLVM_SUPPORT_SUPPORT_MACROS_H__

// Define macro, to use within a class declaration,  to disallow constructor
// copy. Defines copy constructor declaration under the assumption that it
// is never defined.
#define DISALLOW_CLASS_COPY(class_name) \
  class_name(class_name& arg)  // Do not implement

// Define macro, to use within a class declaration,  to disallow assignment.
// Defines assignment operation declaration under the assumption that it
// is never defined.
#define DISALLOW_CLASS_ASSIGN(class_name) \
  void operator=(class_name& arg)  // Do not implement

// Define macro to add copy and assignment declarations to a class file,
// for which no bodies will be defined, effectively disallowing these from
// being defined in the class.
#define DISALLOW_CLASS_COPY_AND_ASSIGN(class_name) \
  DISALLOW_CLASS_COPY(class_name); \
  DISALLOW_CLASS_ASSIGN(class_name)

#endif  // LLVM_SUPPORT_SUPPORT_MACROS_H__
