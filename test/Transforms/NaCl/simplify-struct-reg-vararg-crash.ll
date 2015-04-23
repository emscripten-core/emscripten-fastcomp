; RUN: not opt < %s -simplify-struct-reg-signatures -S

%struct = type { i32, i32 }

declare void @vararg_fct(...)

define void @vararg_caller_with_agg(%struct %str) {
  call void(...)* @vararg_fct(%struct %str)
  ret void
}