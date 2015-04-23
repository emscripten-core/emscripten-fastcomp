; RUN: not opt < %s -simplify-struct-reg-signatures -S

%struct = type { i32, i32 }

declare i32 @__hypothetical_personality_1(%struct)

declare void @something_to_invoke()

; landingpad with struct
define void @landingpad_is_struct() {
  invoke void @something_to_invoke()
    to label %OK unwind label %Err

OK:
  ret void

Err:
  %exn = landingpad i32 personality i32(%struct)* @__hypothetical_personality_1
      cleanup
  resume i32 %exn
}