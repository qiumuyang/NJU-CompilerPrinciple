#include "header.h"

const char* reg_str[] = {"zero", "at", "v0", "v1", "a0", "a1", "a2", "a3",
                      "t0",   "t1", "t2", "t3", "t4", "t5", "t6", "t7",
                      "s0",   "s1", "s2", "s3", "s4", "s5", "s6", "s7",
                      "t8",   "t9", "k0", "k1", "gp", "sp", "fp", "ra"};

const char* errmsgT[] = {
    "Code contains variables or parameters of structure type.",
    "Code contains variables or parameters of float type.",
    "Code contains too many function arguments."
};

const char* errmsgN[] = {
    "",
    "Undefined variable '%s'",
    "Undefined function '%s'",
    "Redefined variable '%s'",
    "Redefined function '%s'",
    "Type mismatched for assignment",
    "Lvalue required as left operand of assignment",
    "Type mismatched for operands",
    "Type mismatched for return",
    "Incompatible arguments for function '%s'",
    "'%s' is not an array",
    "'%s' is not a function",
    "'%s' is not an integer",
    "'%s' is not a structure",
    "Non-existent field '%s'",
    "Redefined field or illegal assignment to field '%s'",
    "Redefined structure '%s'",
    "Undefined structure '%s'",
    "Illegal use for structure '%s'"
};

const char* errmsgB[] = {
    "Syntax error",
    "Missing ';'",
    "Missing ','",
    "Missing '('",
    "Missing ')'",
    "Missing '['",
    "Missing ']'",
    "Missing '{'",
    "Missing '}'",
    "Missing specifier",
    "Missing statement in IF clause",
    "Missing statement in WHILE clause",
    "Illegal expression",
    "Illegal array index",
    "Illegal definition",
    "Illegal argument",
    "Illegal statement",
    "Unexpected declaration",
    "Unexpected assignment"
};

const char* errmsgA[] = {
    "Mysterious characters",
    "Illegal identifier",
    "Not implemented value type",
    "Illegal floating point"
};