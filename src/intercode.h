#ifndef __INTERCODE_H__
#define __INTERCODE_H__

#include <stdio.h>

#include "arraynode.h"

extern int LabelCount;
extern int VarCount;
extern int TempCount;

enum block_sign {
    NORMAL_S = 0,
    LABEL_S = 1,
    FUNCTION_S = 2,
    COND_S = 3,
    GOTO_S = 4,
    RETURN_S = 5
};

enum operand_types {
    EOPR = 0,  // empty operand
    CONST = 1,
    VARIABLE = 2,
    TEMP = 3,
    // use AND and OR such bit operation to get/set the following type
    EADDR = 0x1000,  // Element Address e.g. int a[10][10];
                     // &a[0][0] is an element addr
    RADDR = 0x2000   // Range Address   e.g. int a[10][10];
                     // &a is a range addr
};

#define SET_EADDR(opr)            \
    do {                          \
        (opr).opr_type &= ~RADDR; \
        (opr).opr_type |= EADDR;  \
    } while (0)
#define SET_RADDR(opr)            \
    do {                          \
        (opr).opr_type &= ~EADDR; \
        (opr).opr_type |= RADDR;  \
    } while (0)
#define UNSET_ADDR(opr)           \
    do {                          \
        (opr).opr_type &= ~EADDR; \
        (opr).opr_type &= ~RADDR; \
    } while (0)
#define IS_EADDR(opr) ((opr).opr_type & EADDR)
#define IS_RADDR(opr) ((opr).opr_type & RADDR)
#define IS_ADDR(opr) (IS_EADDR(opr) || IS_RADDR(opr))
#define OPR_TYPE(opr) ((opr).opr_type & 0xF)
#define IS_EOPR(opr) (OPR_TYPE(opr) == EOPR)
#define IS_CONST(opr) (OPR_TYPE(opr) == CONST)
#define IS_VAR(opr) (OPR_TYPE(opr) == VARIABLE)
#define IS_TEMP(opr) (OPR_TYPE(opr) == TEMP)
#define IS_ZERO(opr) (OPR_TYPE(opr) == CONST && (opr).const_value == 0)
#define IS_ONE(opr) (OPR_TYPE(opr) == CONST && (opr).const_value == 1)

enum intercode_types {
    ECODE = 0,      // empty intercode
    FUNCTION = 1,   // FUNCTION <funcname>
    LABEL = 2,      // LABEL <label> :
    GOTO = 3,       // GOTO <label>
    COND = 4,       // IF <cond_expr> GOTO <label>
    PARAM = 5,      // PARAM <variable>
    ARG = 6,        // ARG <variable / temp / const>
    DEC = 7,        // DEC <variable> <size>
    CALL = 8,       // <dst> = CALL <funcname>
    RETURN_IC = 9,  // RETURN <variable / temp / const>
    READ = 10,      // READ <variable>
    WRITE = 11,     // WRITE <variable>
    ASSIGN = 12     // <dst> = <src1> <op> <src2>
};

enum op_types {
    AS = 1,      // x = y
    ADD = 2,     // x = y + z
    SUB = 3,     // x = y - z
    MUL = 4,     // x = y * z
    DIVD = 5,    // x = y / z
    ADDR = 6,    // x = &y
    RSTAR = 7,   // x = *y
    LSTAR = 8,   // *x = y
    LRSTAR = 9,  // *x = *y
    EQ = 10,     // x <relop> y
    NE = 11,
    LE = 12,
    LT = 13,
    GE = 14,
    GT = 15
};

typedef struct _operand {
    int opr_type;
    union {
        int const_value;
        int var_id;
        int tmp_id;
    };
    arrayNode* level;
    // record current address level
} operand;

typedef struct _interCode {
    int ic_type;
    union {
        char func_name[32];
        int label_id;  // LABEL & GOTO
        struct {
            int var_id;
            int size;
        } dec;        // DEC
        operand opr;  // RETURN & PARAM & ARG & READ & WRITE
        struct {
            operand opr1, opr2;
            int op_type;
            int label_id;
        } cond;  // IF GOTO
        struct {
            operand dst;
            char func_name[32];
        } call;  // CALL
        struct {
            operand dst;
            operand src1, src2;  // if there's only one oprand,
                                 // src2 should be empty
            int op_type;
        } assign;  // ASSIGN
    };
    struct _interCode* prev;
    struct _interCode* next;
} interCode;

extern operand nullOpr;
extern operand zeroOpr;

operand newOperand(int opr_type, int value);
bool oprEqual(operand opr1, operand opr2);
int getOprIndex(operand opr);
operand getOprFromIndex(int idx);
int allocLabel();
operand allocTemp();
operand allocVar();
bool isDefCode(interCode* code);
operand getCodeDst(interCode* code);
int getCodeUse(interCode* code, operand* uses);
int getArithOpType(const char* arithop);
int reverseRelOp(int op_type);
int getRelOpType(const char* relop);
interCode* copyInterCode(interCode* head);
interCode* newInterCode(int ic_type);
interCode* newLabelCode(int label_id);
interCode* newFunctionCode(const char* funcname);
interCode* newGotoCode(int label_id);
interCode* newDecCode(int var_id, int size);
interCode* newSingleOprCode(int ic_type, operand opr);
interCode* newCondCode(int op_type, operand opr1, operand opr2, int label_id);
interCode* newCallCode(operand dst, const char* funcname);
interCode* newAssignCode(int op_type, operand dst, operand src1, operand src2);
interCode* mergeCode(interCode* head, interCode* codes);
void insertCodeAfter(interCode* where, interCode* codes);
interCode* removeCodeItr(interCode* remove, bool next);
void removeCode(interCode* remove);
void operandToString(char* buffer, operand opr);
void interCodeToString(char* buffer, interCode* code);
void interCodeToFile(interCode* codes, FILE* fp);
void printSingleCode(interCode* code);
int isLeader(interCode* code, int* label);

#endif