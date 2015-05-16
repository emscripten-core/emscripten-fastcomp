; RUN: llc < %s

; regression check for emscripten #3088 - we were not clearing BlockChanges in i64 lowering

; ModuleID = 'waka.bc'
target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:128-n32-S128"
target triple = "asmjs-unknown-emscripten"

%"class.ZenLib::uint128" = type <{ i64, i64 }>

@.str = private unnamed_addr constant [15 x i8] c"hello, world!\0A\00", align 1

@.str368164 = external hidden unnamed_addr constant [10 x i8], align 1
@.str398167 = external hidden unnamed_addr constant [6 x i8], align 1
@.str718199 = external hidden unnamed_addr constant [9 x i8], align 1
@.str738201 = external hidden unnamed_addr constant [21 x i8], align 1
@.str748202 = external hidden unnamed_addr constant [26 x i8], align 1
@.str758203 = external hidden unnamed_addr constant [21 x i8], align 1
@.str768204 = external hidden unnamed_addr constant [8 x i8], align 1
@.str778205 = external hidden unnamed_addr constant [14 x i8], align 1
@.str788206 = external hidden unnamed_addr constant [22 x i8], align 1
@.str798207 = external hidden unnamed_addr constant [25 x i8], align 1
@.str808208 = external hidden unnamed_addr constant [24 x i8], align 1
@.str818209 = external hidden unnamed_addr constant [20 x i8], align 1
@.str828210 = external hidden unnamed_addr constant [34 x i8], align 1
@.str838211 = external hidden unnamed_addr constant [31 x i8], align 1
@.str848212 = external hidden unnamed_addr constant [29 x i8], align 1
@.str858213 = external hidden unnamed_addr constant [44 x i8], align 1
@.str868214 = external hidden unnamed_addr constant [12 x i8], align 1
@.str908218 = external hidden unnamed_addr constant [21 x i8], align 1
@.str918219 = external hidden unnamed_addr constant [8 x i8], align 1
@.str928220 = external hidden unnamed_addr constant [6 x i8], align 1
@.str9210864 = external hidden unnamed_addr constant [5 x i8], align 1
@.str514367 = external hidden unnamed_addr constant [5 x i8], align 1
@.str214409 = external hidden unnamed_addr constant [4 x i8], align 1
@.str20216493 = external hidden unnamed_addr constant [3 x i8], align 1
@.str2017231 = external hidden unnamed_addr constant [11 x i8], align 1
@.str2317234 = external hidden unnamed_addr constant [14 x i8], align 1
@.str2417235 = external hidden unnamed_addr constant [4 x i8], align 1
@.str2717238 = external hidden unnamed_addr constant [5 x i8], align 1
@.str3217243 = external hidden unnamed_addr constant [4 x i8], align 1
@.str1717689 = external hidden unnamed_addr constant [5 x i8], align 1
@.str2104 = external hidden unnamed_addr constant [1 x i8], align 1

; Function Attrs: nounwind readonly
define hidden i8* @_ZN12MediaInfoLib22Mxf_EssenceCompressionEN6ZenLib7uint128E(%"class.ZenLib::uint128"* nocapture readonly %EssenceCompression) #0 {
entry:
  %hi = getelementptr inbounds %"class.ZenLib::uint128", %"class.ZenLib::uint128"* %EssenceCompression, i32 0, i32 1
  %0 = load i64, i64* %hi, align 1
  %and = and i64 %0, -256
  %cmp = icmp eq i64 %and, 436333716306985216
  br i1 %cmp, label %lor.lhs.false, label %return

lor.lhs.false:                                    ; preds = %entry
  %lo = getelementptr inbounds %"class.ZenLib::uint128", %"class.ZenLib::uint128"* %EssenceCompression, i32 0, i32 0
  %1 = load i64, i64* %lo, align 1
  %and1 = and i64 %1, -72057594037927936
  switch i64 %and1, label %return [
    i64 288230376151711744, label %if.end
    i64 1008806316530991104, label %if.end
  ]

if.end:                                           ; preds = %lor.lhs.false, %lor.lhs.false
  %shr = lshr i64 %1, 56
  %conv = trunc i64 %shr to i32
  %and10 = lshr i64 %1, 48
  %and14 = lshr i64 %1, 40
  %and18 = lshr i64 %1, 32
  %conv20 = trunc i64 %and18 to i32
  %and22 = lshr i64 %1, 24
  %and26 = lshr i64 %1, 16
  %conv28 = trunc i64 %and26 to i32
  %and30 = lshr i64 %1, 8
  %conv32 = trunc i64 %and30 to i32
  switch i32 %conv, label %return [
    i32 4, label %sw.bb
    i32 14, label %sw.bb112
  ]

sw.bb:                                            ; preds = %if.end
  %conv12 = trunc i64 %and10 to i32
  %conv34 = and i32 %conv12, 255
  switch i32 %conv34, label %return [
    i32 1, label %sw.bb35
    i32 2, label %sw.bb64
  ]

sw.bb35:                                          ; preds = %sw.bb
  %conv36 = and i64 %and14, 255
  %cond12 = icmp eq i64 %conv36, 2
  br i1 %cond12, label %sw.bb37, label %return

sw.bb37:                                          ; preds = %sw.bb35
  %conv38 = and i32 %conv20, 255
  switch i32 %conv38, label %return [
    i32 1, label %sw.bb39
    i32 2, label %sw.bb42
  ]

sw.bb39:                                          ; preds = %sw.bb37
  %conv40 = and i64 %and22, 255
  %cond14 = icmp eq i64 %conv40, 1
  %. = select i1 %cond14, i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str214409, i32 0, i32 0), i8* getelementptr inbounds ([1 x i8], [1 x i8]* @.str2104, i32 0, i32 0)
  br label %return

sw.bb42:                                          ; preds = %sw.bb37
  %2 = trunc i64 %and22 to i32
  %conv43 = and i32 %2, 255
  switch i32 %conv43, label %sw.default61 [
    i32 1, label %sw.bb44
    i32 2, label %return
    i32 3, label %sw.bb56
    i32 113, label %sw.bb60
  ]

sw.bb44:                                          ; preds = %sw.bb42
  %conv45 = and i32 %conv28, 255
  switch i32 %conv45, label %sw.default54 [
    i32 0, label %return
    i32 1, label %return
    i32 2, label %return
    i32 3, label %return
    i32 4, label %return
    i32 17, label %return
    i32 32, label %sw.bb52
    i32 48, label %sw.bb53
    i32 49, label %sw.bb53
    i32 50, label %sw.bb53
    i32 51, label %sw.bb53
    i32 52, label %sw.bb53
    i32 53, label %sw.bb53
    i32 54, label %sw.bb53
    i32 55, label %sw.bb53
    i32 56, label %sw.bb53
    i32 57, label %sw.bb53
    i32 58, label %sw.bb53
    i32 59, label %sw.bb53
    i32 60, label %sw.bb53
    i32 61, label %sw.bb53
    i32 62, label %sw.bb53
    i32 63, label %sw.bb53
  ]

sw.bb52:                                          ; preds = %sw.bb44
  br label %return

sw.bb53:                                          ; preds = %sw.bb44, %sw.bb44, %sw.bb44, %sw.bb44, %sw.bb44, %sw.bb44, %sw.bb44, %sw.bb44, %sw.bb44, %sw.bb44, %sw.bb44, %sw.bb44, %sw.bb44, %sw.bb44, %sw.bb44, %sw.bb44
  br label %return

sw.default54:                                     ; preds = %sw.bb44
  br label %return

sw.bb56:                                          ; preds = %sw.bb42
  %conv57 = and i64 %and26, 255
  %cond13 = icmp eq i64 %conv57, 1
  %.35 = select i1 %cond13, i8* getelementptr inbounds ([10 x i8], [10 x i8]* @.str368164, i32 0, i32 0), i8* getelementptr inbounds ([1 x i8], [1 x i8]* @.str2104, i32 0, i32 0)
  br label %return

sw.bb60:                                          ; preds = %sw.bb42
  br label %return

sw.default61:                                     ; preds = %sw.bb42
  br label %return

sw.bb64:                                          ; preds = %sw.bb
  %conv65 = and i64 %and14, 255
  %cond9 = icmp eq i64 %conv65, 2
  br i1 %cond9, label %sw.bb66, label %return

sw.bb66:                                          ; preds = %sw.bb64
  %conv67 = and i32 %conv20, 255
  switch i32 %conv67, label %return [
    i32 1, label %sw.bb68
    i32 2, label %sw.bb75
  ]

sw.bb68:                                          ; preds = %sw.bb66
  %3 = trunc i64 %and22 to i32
  %conv69 = and i32 %3, 255
  switch i32 %conv69, label %sw.default74 [
    i32 0, label %return
    i32 1, label %return
    i32 126, label %return
    i32 127, label %return
  ]

sw.default74:                                     ; preds = %sw.bb68
  br label %return

sw.bb75:                                          ; preds = %sw.bb66
  %conv76 = and i64 %and22, 255
  %cond10 = icmp eq i64 %conv76, 3
  br i1 %cond10, label %sw.bb77, label %return

sw.bb77:                                          ; preds = %sw.bb75
  %conv78 = and i32 %conv28, 255
  switch i32 %conv78, label %return [
    i32 1, label %sw.bb79
    i32 2, label %sw.bb84
    i32 3, label %sw.bb92
    i32 4, label %sw.bb96
  ]

sw.bb79:                                          ; preds = %sw.bb77
  %conv80 = and i32 %conv32, 255
  switch i32 %conv80, label %sw.default83 [
    i32 1, label %return
    i32 16, label %sw.bb82
  ]

sw.bb82:                                          ; preds = %sw.bb79
  br label %return

sw.default83:                                     ; preds = %sw.bb79
  br label %return

sw.bb84:                                          ; preds = %sw.bb77
  %conv85 = and i32 %conv32, 255
  switch i32 %conv85, label %sw.default91 [
    i32 1, label %return
    i32 4, label %sw.bb87
    i32 5, label %sw.bb88
    i32 6, label %sw.bb89
    i32 28, label %sw.bb90
  ]

sw.bb87:                                          ; preds = %sw.bb84
  br label %return

sw.bb88:                                          ; preds = %sw.bb84
  br label %return

sw.bb89:                                          ; preds = %sw.bb84
  br label %return

sw.bb90:                                          ; preds = %sw.bb84
  br label %return

sw.default91:                                     ; preds = %sw.bb84
  br label %return

sw.bb92:                                          ; preds = %sw.bb77
  %conv93 = and i64 %and30, 255
  %cond11 = icmp eq i64 %conv93, 1
  %.36 = select i1 %cond11, i8* getelementptr inbounds ([14 x i8], [14 x i8]* @.str778205, i32 0, i32 0), i8* getelementptr inbounds ([1 x i8], [1 x i8]* @.str2104, i32 0, i32 0)
  br label %return

sw.bb96:                                          ; preds = %sw.bb77
  %conv97 = and i32 %conv32, 255
  switch i32 %conv97, label %sw.default106 [
    i32 1, label %return
    i32 2, label %sw.bb99
    i32 3, label %sw.bb100
    i32 4, label %sw.bb101
    i32 5, label %sw.bb102
    i32 6, label %sw.bb103
    i32 7, label %sw.bb104
    i32 8, label %sw.bb105
  ]

sw.bb99:                                          ; preds = %sw.bb96
  br label %return

sw.bb100:                                         ; preds = %sw.bb96
  br label %return

sw.bb101:                                         ; preds = %sw.bb96
  br label %return

sw.bb102:                                         ; preds = %sw.bb96
  br label %return

sw.bb103:                                         ; preds = %sw.bb96
  br label %return

sw.bb104:                                         ; preds = %sw.bb96
  br label %return

sw.bb105:                                         ; preds = %sw.bb96
  br label %return

sw.default106:                                    ; preds = %sw.bb96
  br label %return

sw.bb112:                                         ; preds = %if.end
  %4 = trunc i64 %and10 to i32
  %conv113 = and i32 %4, 255
  switch i32 %conv113, label %return [
    i32 4, label %sw.bb114
    i32 6, label %sw.bb127
  ]

sw.bb114:                                         ; preds = %sw.bb112
  %conv115 = and i64 %and14, 255
  %cond5 = icmp eq i64 %conv115, 2
  %conv117 = and i64 %and18, 255
  %cond6 = icmp eq i64 %conv117, 1
  %or.cond = and i1 %cond5, %cond6
  %conv119 = and i64 %and22, 255
  %cond7 = icmp eq i64 %conv119, 2
  %or.cond39 = and i1 %or.cond, %cond7
  br i1 %or.cond39, label %sw.bb120, label %return

sw.bb120:                                         ; preds = %sw.bb114
  %conv121 = and i64 %and26, 255
  %cond8 = icmp eq i64 %conv121, 4
  %.37 = select i1 %cond8, i8* getelementptr inbounds ([5 x i8], [5 x i8]* @.str514367, i32 0, i32 0), i8* getelementptr inbounds ([1 x i8], [1 x i8]* @.str2104, i32 0, i32 0)
  br label %return

sw.bb127:                                         ; preds = %sw.bb112
  %conv128 = and i64 %and14, 255
  %cond = icmp eq i64 %conv128, 4
  %conv130 = and i64 %and18, 255
  %cond1 = icmp eq i64 %conv130, 1
  %or.cond40 = and i1 %cond, %cond1
  %conv132 = and i64 %and22, 255
  %cond2 = icmp eq i64 %conv132, 2
  %or.cond41 = and i1 %or.cond40, %cond2
  %conv134 = and i64 %and26, 255
  %cond3 = icmp eq i64 %conv134, 4
  %or.cond42 = and i1 %or.cond41, %cond3
  br i1 %or.cond42, label %sw.bb135, label %return

sw.bb135:                                         ; preds = %sw.bb127
  %conv136 = and i64 %and30, 255
  %cond4 = icmp eq i64 %conv136, 2
  %.38 = select i1 %cond4, i8* getelementptr inbounds ([12 x i8], [12 x i8]* @.str868214, i32 0, i32 0), i8* getelementptr inbounds ([1 x i8], [1 x i8]* @.str2104, i32 0, i32 0)
  br label %return

return:                                           ; preds = %sw.bb135, %sw.bb127, %sw.bb120, %sw.bb114, %sw.bb112, %sw.default106, %sw.bb105, %sw.bb104, %sw.bb103, %sw.bb102, %sw.bb101, %sw.bb100, %sw.bb99, %sw.bb96, %sw.bb92, %sw.default91, %sw.bb90, %sw.bb89, %sw.bb88, %sw.bb87, %sw.bb84, %sw.default83, %sw.bb82, %sw.bb79, %sw.bb77, %sw.bb75, %sw.default74, %sw.bb68, %sw.bb68, %sw.bb68, %sw.bb68, %sw.bb66, %sw.bb64, %sw.default61, %sw.bb60, %sw.bb56, %sw.default54, %sw.bb53, %sw.bb52, %sw.bb44, %sw.bb44, %sw.bb44, %sw.bb44, %sw.bb44, %sw.bb44, %sw.bb42, %sw.bb39, %sw.bb37, %sw.bb35, %sw.bb, %if.end, %lor.lhs.false, %entry
  %retval.0 = phi i8* [ getelementptr inbounds ([1 x i8], [1 x i8]* @.str2104, i32 0, i32 0), %sw.default106 ], [ getelementptr inbounds ([44 x i8], [44 x i8]* @.str858213, i32 0, i32 0), %sw.bb105 ], [ getelementptr inbounds ([29 x i8], [29 x i8]* @.str848212, i32 0, i32 0), %sw.bb104 ], [ getelementptr inbounds ([31 x i8], [31 x i8]* @.str838211, i32 0, i32 0), %sw.bb103 ], [ getelementptr inbounds ([34 x i8], [34 x i8]* @.str828210, i32 0, i32 0), %sw.bb102 ], [ getelementptr inbounds ([20 x i8], [20 x i8]* @.str818209, i32 0, i32 0), %sw.bb101 ], [ getelementptr inbounds ([24 x i8], [24 x i8]* @.str808208, i32 0, i32 0), %sw.bb100 ], [ getelementptr inbounds ([25 x i8], [25 x i8]* @.str798207, i32 0, i32 0), %sw.bb99 ], [ getelementptr inbounds ([1 x i8], [1 x i8]* @.str2104, i32 0, i32 0), %sw.default91 ], [ getelementptr inbounds ([8 x i8], [8 x i8]* @.str768204, i32 0, i32 0), %sw.bb90 ], [ getelementptr inbounds ([21 x i8], [21 x i8]* @.str758203, i32 0, i32 0), %sw.bb89 ], [ getelementptr inbounds ([26 x i8], [26 x i8]* @.str748202, i32 0, i32 0), %sw.bb88 ], [ getelementptr inbounds ([21 x i8], [21 x i8]* @.str738201, i32 0, i32 0), %sw.bb87 ], [ getelementptr inbounds ([1 x i8], [1 x i8]* @.str2104, i32 0, i32 0), %sw.default83 ], [ getelementptr inbounds ([9 x i8], [9 x i8]* @.str718199, i32 0, i32 0), %sw.bb82 ], [ getelementptr inbounds ([1 x i8], [1 x i8]* @.str2104, i32 0, i32 0), %sw.default74 ], [ getelementptr inbounds ([1 x i8], [1 x i8]* @.str2104, i32 0, i32 0), %sw.default61 ], [ getelementptr inbounds ([5 x i8], [5 x i8]* @.str514367, i32 0, i32 0), %sw.bb60 ], [ getelementptr inbounds ([1 x i8], [1 x i8]* @.str2104, i32 0, i32 0), %sw.default54 ], [ getelementptr inbounds ([4 x i8], [4 x i8]* @.str2417235, i32 0, i32 0), %sw.bb53 ], [ getelementptr inbounds ([14 x i8], [14 x i8]* @.str2317234, i32 0, i32 0), %sw.bb52 ], [ getelementptr inbounds ([1 x i8], [1 x i8]* @.str2104, i32 0, i32 0), %lor.lhs.false ], [ getelementptr inbounds ([1 x i8], [1 x i8]* @.str2104, i32 0, i32 0), %entry ], [ %., %sw.bb39 ], [ getelementptr inbounds ([11 x i8], [11 x i8]* @.str2017231, i32 0, i32 0), %sw.bb44 ], [ getelementptr inbounds ([11 x i8], [11 x i8]* @.str2017231, i32 0, i32 0), %sw.bb44 ], [ getelementptr inbounds ([11 x i8], [11 x i8]* @.str2017231, i32 0, i32 0), %sw.bb44 ], [ getelementptr inbounds ([11 x i8], [11 x i8]* @.str2017231, i32 0, i32 0), %sw.bb44 ], [ getelementptr inbounds ([11 x i8], [11 x i8]* @.str2017231, i32 0, i32 0), %sw.bb44 ], [ getelementptr inbounds ([11 x i8], [11 x i8]* @.str2017231, i32 0, i32 0), %sw.bb44 ], [ getelementptr inbounds ([3 x i8], [3 x i8]* @.str20216493, i32 0, i32 0), %sw.bb42 ], [ %.35, %sw.bb56 ], [ getelementptr inbounds ([1 x i8], [1 x i8]* @.str2104, i32 0, i32 0), %sw.bb37 ], [ getelementptr inbounds ([1 x i8], [1 x i8]* @.str2104, i32 0, i32 0), %sw.bb35 ], [ getelementptr inbounds ([4 x i8], [4 x i8]* @.str3217243, i32 0, i32 0), %sw.bb68 ], [ getelementptr inbounds ([4 x i8], [4 x i8]* @.str3217243, i32 0, i32 0), %sw.bb68 ], [ getelementptr inbounds ([4 x i8], [4 x i8]* @.str3217243, i32 0, i32 0), %sw.bb68 ], [ getelementptr inbounds ([4 x i8], [4 x i8]* @.str3217243, i32 0, i32 0), %sw.bb68 ], [ getelementptr inbounds ([6 x i8], [6 x i8]* @.str398167, i32 0, i32 0), %sw.bb79 ], [ getelementptr inbounds ([5 x i8], [5 x i8]* @.str2717238, i32 0, i32 0), %sw.bb84 ], [ %.36, %sw.bb92 ], [ getelementptr inbounds ([22 x i8], [22 x i8]* @.str788206, i32 0, i32 0), %sw.bb96 ], [ getelementptr inbounds ([1 x i8], [1 x i8]* @.str2104, i32 0, i32 0), %sw.bb77 ], [ getelementptr inbounds ([1 x i8], [1 x i8]* @.str2104, i32 0, i32 0), %sw.bb75 ], [ getelementptr inbounds ([1 x i8], [1 x i8]* @.str2104, i32 0, i32 0), %sw.bb66 ], [ getelementptr inbounds ([1 x i8], [1 x i8]* @.str2104, i32 0, i32 0), %sw.bb64 ], [ getelementptr inbounds ([1 x i8], [1 x i8]* @.str2104, i32 0, i32 0), %sw.bb ], [ %.37, %sw.bb120 ], [ getelementptr inbounds ([1 x i8], [1 x i8]* @.str2104, i32 0, i32 0), %sw.bb114 ], [ %.38, %sw.bb135 ], [ getelementptr inbounds ([1 x i8], [1 x i8]* @.str2104, i32 0, i32 0), %sw.bb127 ], [ getelementptr inbounds ([1 x i8], [1 x i8]* @.str2104, i32 0, i32 0), %sw.bb112 ], [ getelementptr inbounds ([1 x i8], [1 x i8]* @.str2104, i32 0, i32 0), %if.end ]
  ret i8* %retval.0
}

; Function Attrs: nounwind readonly
define hidden i8* @_ZN12MediaInfoLib27Mxf_Sequence_DataDefinitionEN6ZenLib7uint128E(%"class.ZenLib::uint128"* nocapture readonly %DataDefinition) #0 {
entry:
  %lo = getelementptr inbounds %"class.ZenLib::uint128", %"class.ZenLib::uint128"* %DataDefinition, i32 0, i32 0
  %0 = load i64, i64* %lo, align 1
  %and = lshr i64 %0, 32
  %conv = trunc i64 %and to i32
  %and2 = lshr i64 %0, 24
  %conv5 = and i32 %conv, 255
  switch i32 %conv5, label %return [
    i32 1, label %sw.bb
    i32 2, label %sw.bb9
  ]

sw.bb:                                            ; preds = %entry
  %conv4 = trunc i64 %and2 to i32
  %conv6 = and i32 %conv4, 255
  switch i32 %conv6, label %sw.default [
    i32 1, label %return
    i32 2, label %return
    i32 3, label %return
    i32 16, label %sw.bb8
  ]

sw.bb8:                                           ; preds = %sw.bb
  br label %return

sw.default:                                       ; preds = %sw.bb
  br label %return

sw.bb9:                                           ; preds = %entry
  %1 = trunc i64 %and2 to i32
  %conv10 = and i32 %1, 255
  switch i32 %conv10, label %sw.default14 [
    i32 1, label %return
    i32 2, label %sw.bb12
    i32 3, label %sw.bb13
  ]

sw.bb12:                                          ; preds = %sw.bb9
  br label %return

sw.bb13:                                          ; preds = %sw.bb9
  br label %return

sw.default14:                                     ; preds = %sw.bb9
  br label %return

return:                                           ; preds = %sw.default14, %sw.bb13, %sw.bb12, %sw.bb9, %sw.default, %sw.bb8, %sw.bb, %sw.bb, %sw.bb, %entry
  %retval.0 = phi i8* [ getelementptr inbounds ([1 x i8], [1 x i8]* @.str2104, i32 0, i32 0), %sw.default14 ], [ getelementptr inbounds ([5 x i8], [5 x i8]* @.str1717689, i32 0, i32 0), %sw.bb13 ], [ getelementptr inbounds ([6 x i8], [6 x i8]* @.str928220, i32 0, i32 0), %sw.bb12 ], [ getelementptr inbounds ([1 x i8], [1 x i8]* @.str2104, i32 0, i32 0), %sw.default ], [ getelementptr inbounds ([21 x i8], [21 x i8]* @.str908218, i32 0, i32 0), %sw.bb8 ], [ getelementptr inbounds ([5 x i8], [5 x i8]* @.str9210864, i32 0, i32 0), %sw.bb ], [ getelementptr inbounds ([5 x i8], [5 x i8]* @.str9210864, i32 0, i32 0), %sw.bb ], [ getelementptr inbounds ([5 x i8], [5 x i8]* @.str9210864, i32 0, i32 0), %sw.bb ], [ getelementptr inbounds ([8 x i8], [8 x i8]* @.str918219, i32 0, i32 0), %sw.bb9 ], [ getelementptr inbounds ([1 x i8], [1 x i8]* @.str2104, i32 0, i32 0), %entry ]
  ret i8* %retval.0
}


define i32 @main() {
entry:
  %retval = alloca i32, align 4
  store i32 0, i32* %retval
  %call = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([15 x i8], [15 x i8]* @.str, i32 0, i32 0))
  ret i32 0
}

declare i32 @printf(i8*, ...)

attributes #0 = { nounwind readonly }

