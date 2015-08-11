
#include "parser.h"

namespace cashew {

// common strings

thread_local IString TOPLEVEL,
               DEFUN,
               BLOCK,
               STAT,
               ASSIGN,
               NAME,
               VAR,
               CONST,
               CONDITIONAL,
               BINARY,
               RETURN,
               IF,
               ELSE,
               WHILE,
               DO,
               FOR,
               SEQ,
               SUB,
               CALL,
               NUM,
               LABEL,
               BREAK,
               CONTINUE,
               SWITCH,
               STRING,
               INF,
               NaN,
               TEMP_RET0,
               UNARY_PREFIX,
               UNARY_POSTFIX,
               MATH_FROUND,
               SIMD_FLOAT32X4,
               SIMD_INT32X4,
               PLUS,
               MINUS,
               OR,
               AND,
               XOR,
               L_NOT,
               B_NOT,
               LT,
               GE,
               LE,
               GT,
               EQ,
               NE,
               DIV,
               MOD,
               MUL,
               RSHIFT,
               LSHIFT,
               TRSHIFT,
               TEMP_DOUBLE_PTR,
               HEAP8,
               HEAP16,
               HEAP32,
               HEAPF32,
               HEAPU8,
               HEAPU16,
               HEAPU32,
               HEAPF64,
               F0,
               EMPTY,
               FUNCTION,
               OPEN_PAREN,
               OPEN_BRACE,
               OPEN_CURLY,
               CLOSE_CURLY,
               COMMA,
               QUESTION,
               COLON,
               CASE,
               DEFAULT,
               DOT,
               PERIOD,
               NEW,
               ARRAY,
               OBJECT,
               THROW,
               SET;

thread_local IStringSet keywords("var const function if else do while for break continue return switch case default throw try catch finally true false null new");

const char *OPERATOR_INITS = "+-*/%<>&^|~=!,?:.",
           *SEPARATORS = "([;{}";

int MAX_OPERATOR_SIZE = 3;

thread_local std::vector<OperatorClass> operatorClasses;

thread_local static std::vector<std::unordered_map<IString, int>> precedences; // op, type => prec

struct Init {
  Init() {
    // common strings (must initialize them this way to avoid https://gcc.gnu.org/bugzilla/show_bug.cgi?id=55800
    TOPLEVEL.set("toplevel");
    DEFUN.set("defun");
    BLOCK.set("block");
    STAT.set("stat");
    ASSIGN.set("assign");
    NAME.set("name");
    VAR.set("var");
    CONST.set("const");
    CONDITIONAL.set("conditional");
    BINARY.set("binary");
    RETURN.set("return");
    IF.set("if");
    ELSE.set("else");
    WHILE.set("while");
    DO.set("do");
    FOR.set("for");
    SEQ.set("seq");
    SUB.set("sub");
    CALL.set("call");
    NUM.set("num");
    LABEL.set("label");
    BREAK.set("break");
    CONTINUE.set("continue");
    SWITCH.set("switch");
    STRING.set("string");
    INF.set("inf");
    NaN.set("nan");
    TEMP_RET0.set("tempRet0");
    UNARY_PREFIX.set("unary-prefix");
    UNARY_POSTFIX.set("unary-postfix");
    MATH_FROUND.set("Math_fround");
    SIMD_FLOAT32X4.set("SIMD_Float32x4");
    SIMD_INT32X4.set("SIMD_Int32x4");
    PLUS.set("+");
    MINUS.set("-");
    OR.set("|");
    AND.set("&");
    XOR.set("^");
    L_NOT.set("!");
    B_NOT.set("~");
    LT.set("<");
    GE.set(">=");
    LE.set("<=");
    GT.set(">");
    EQ.set("==");
    NE.set("!=");
    DIV.set("/");
    MOD.set("%");
    MUL.set("*");
    RSHIFT.set(">>");
    LSHIFT.set("<<");
    TRSHIFT.set(">>>");
    TEMP_DOUBLE_PTR.set("tempDoublePtr");
    HEAP8.set("HEAP8");
    HEAP16.set("HEAP16");
    HEAP32.set("HEAP32");
    HEAPF32.set("HEAPF32");
    HEAPU8.set("HEAPU8");
    HEAPU16.set("HEAPU16");
    HEAPU32.set("HEAPU32");
    HEAPF64.set("HEAPF64");
    F0.set("f0");
    EMPTY.set("");
    FUNCTION.set("function");
    OPEN_PAREN.set("(");
    OPEN_BRACE.set("[");
    OPEN_CURLY.set("{");
    CLOSE_CURLY.set("}");
    COMMA.set(",");
    QUESTION.set("?");
    COLON.set(":");
    CASE.set("case");
    DEFAULT.set("default");
    DOT.set("dot");
    PERIOD.set(".");
    NEW.set("new");
    ARRAY.set("array");
    OBJECT.set("object");
    THROW.set("throw");
    SET.set("=");

    // operators, rtl, type
    operatorClasses.push_back(OperatorClass(".",         false, OperatorClass::Binary));
    operatorClasses.push_back(OperatorClass("! ~ + -",   true,  OperatorClass::Prefix));
    operatorClasses.push_back(OperatorClass("* / %",     false, OperatorClass::Binary));
    operatorClasses.push_back(OperatorClass("+ -",       false, OperatorClass::Binary));
    operatorClasses.push_back(OperatorClass("<< >> >>>", false, OperatorClass::Binary));
    operatorClasses.push_back(OperatorClass("< <= > >=", false, OperatorClass::Binary));
    operatorClasses.push_back(OperatorClass("== !=",     false, OperatorClass::Binary));
    operatorClasses.push_back(OperatorClass("&",         false, OperatorClass::Binary));
    operatorClasses.push_back(OperatorClass("^",         false, OperatorClass::Binary));
    operatorClasses.push_back(OperatorClass("|",         false, OperatorClass::Binary));
    operatorClasses.push_back(OperatorClass("? :",       true,  OperatorClass::Tertiary));
    operatorClasses.push_back(OperatorClass("=",         true,  OperatorClass::Binary));
    operatorClasses.push_back(OperatorClass(",",         true,  OperatorClass::Binary));

    precedences.resize(OperatorClass::Tertiary + 1);

    for (size_t prec = 0; prec < operatorClasses.size(); prec++) {
      for (auto curr : operatorClasses[prec].ops) {
        precedences[operatorClasses[prec].type][curr] = prec;
      }
    }
  }
};

thread_local Init init;

int OperatorClass::getPrecedence(Type type, IString op) {
  return precedences[type][op];
}

bool OperatorClass::getRtl(int prec) {
  return operatorClasses[prec].rtl;
}

bool isIdentInit(char x) { return (x >= 'a' && x <= 'z') || (x >= 'A' && x <= 'Z') || x == '_' || x == '$'; }
bool isIdentPart(char x) { return isIdentInit(x) || (x >= '0' && x <= '9'); }

} // namespace cashew

