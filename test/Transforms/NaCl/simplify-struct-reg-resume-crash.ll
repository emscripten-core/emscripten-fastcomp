; RUN: not opt < %s -simplify-struct-reg-signatures -S

%struct = type { i8*, void(%struct)* }

declare i32 @__gxx_personality_v0(...)
declare void @something_to_invoke()

; landingpad with struct
define void @landingpad_is_struct(%struct %str) {
  invoke void @something_to_invoke()
    to label %OK unwind label %Err

OK:
  ret void

Err:
  %exn = landingpad {i8*, i32} personality i32 (...)* @__gxx_personality_v0
    cleanup
  resume %struct %str
}