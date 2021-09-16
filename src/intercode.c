#include "intercode.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// increase count when allocating a new item
int LabelCount = 0;
int VarCount = 0;
int TempCount = 0;

// EQ, NE, LE, LT, GE, GT
static const char* RelOps[] = {"==", "!=", "<=", "<", ">=", ">"};
// ADD, SUB, MUL, DIVD
static const char* ArithOps[] = {"+", "-", "*", "/"};

operand nullOpr = {EOPR, 0, NULL};
operand zeroOpr = {CONST, 0, NULL};

operand newOperand(int opr_type, int value) {
    operand ret;
    ret.opr_type = opr_type;
    switch (OPR_TYPE(ret)) {
        case EOPR:
            break;
        case CONST:
            ret.const_value = value;
            break;
        case TEMP:
            ret.tmp_id = value;
            break;
        case VARIABLE:
            ret.var_id = value;
            break;
        default:
            assert(0);
    }
    ret.level = NULL;
    return ret;
}

inline bool oprEqual(operand opr1, operand opr2) {
    return OPR_TYPE(opr1) == OPR_TYPE(opr2) && opr1.var_id == opr2.var_id;
}

int allocLabel() {
    LabelCount++;
    return LabelCount;
}

operand allocTemp() {
    TempCount++;
    return newOperand(TEMP, TempCount);
}

operand allocVar() {
    VarCount++;
    return newOperand(VARIABLE, VarCount);
}

bool isDefCode(interCode* code) {
    if (code->ic_type == ASSIGN) {
        if (code->assign.op_type != LSTAR && code->assign.op_type != LRSTAR)
            return true;
    } else if (code->ic_type == CALL)
        return true;
    return false;
}

operand getCodeDst(interCode* code) {
    if (code->ic_type == ASSIGN)
        return code->assign.dst;
    else if (code->ic_type == CALL)
        return code->assign.dst;
    else if (code->ic_type == READ)
        return code->opr;
    else
        assert(0);
    return nullOpr;
}

int getCodeUse(interCode* code, operand* uses) {
    // Note: uses should at least be operand[3]
    int cnt = 0;
    if (code->ic_type == COND) {
        if (!IS_CONST(code->cond.opr1)) uses[cnt++] = code->cond.opr1;
        if (!IS_CONST(code->cond.opr2)) uses[cnt++] = code->cond.opr2;
    } else if (code->ic_type == PARAM || code->ic_type == READ) {
        uses[cnt++] = code->opr;
    } else if (code->ic_type == RETURN_IC || code->ic_type == ARG ||
               code->ic_type == WRITE) {
        if (!IS_CONST(code->opr)) uses[cnt++] = code->opr;
    } else if (code->ic_type == ASSIGN) {
        // dst
        if (!isDefCode(code) && !IS_CONST(code->assign.dst)) {
            uses[cnt++] = code->assign.dst;
        }
        // src1
        if (!IS_CONST(code->assign.src1)) {
            uses[cnt++] = code->assign.src1;
        }
        // src2
        if (!IS_EOPR(code->assign.src2) && !IS_CONST(code->assign.src2) &&
            !oprEqual(code->assign.src1, code->assign.src2)) {
            uses[cnt++] = code->assign.src2;
        }
    }
    for (int i = cnt; i < 3; i++) uses[i] = nullOpr;
    return cnt;
}

int getOprIndex(operand opr) {
    if (IS_TEMP(opr))
        return opr.tmp_id + VarCount;
    else if (IS_VAR(opr))
        return opr.var_id;
    else {
        // printf("%d %d\n", OPR_TYPE(opr), opr.var_id);
        return 0;
    }
    return -1;
}

operand getOprFromIndex(int idx) {
    if (idx > VarCount) {
        return newOperand(TEMP, idx - VarCount);
    }
    return newOperand(VARIABLE, idx);
}

int getRelOpType(const char* relop) {
    for (int i = EQ; i <= GT; i++) {
        if (strcmp(RelOps[i - EQ], relop) == 0)  //
            return i;
    }
    assert(0);
}

int reverseRelOp(int op_type) {
    switch (op_type) {
        case EQ:
            return NE;
        case NE:
            return EQ;
        case GE:
            return LT;
        case GT:
            return LE;
        case LE:
            return GT;
        case LT:
            return GE;
        default:
            assert(0);
    }
    return -1;
}

int getArithOpType(const char* arithop) {
    for (int i = ADD; i <= DIVD; i++) {
        if (strcmp(ArithOps[i - ADD], arithop) == 0)  //
            return i;
    }
    assert(0);
}

interCode* copyInterCode(interCode* head) {
    assert(head);
    assert(head->ic_type == FUNCTION);
    interCode* ret = newFunctionCode(head->func_name);
    interCode* iter = head->next;
    interCode* cp;
    while (iter != head) {
        cp = newInterCode(iter->ic_type);
        switch (iter->ic_type) {
            case PARAM:
            case RETURN_IC:
            case ARG:
            case READ:
            case WRITE:
                cp->opr = iter->opr;
                break;
            case LABEL:
            case GOTO:
                cp->label_id = iter->label_id;
                break;
            case DEC:
                cp->dec = iter->dec;
                break;
            case COND:
                cp->cond = iter->cond;
                break;
            case CALL:
                cp->call = iter->call;
                break;
            case ASSIGN:
                cp->assign = iter->assign;
                break;
            default:
                assert(0);
        }
        ret = mergeCode(ret, cp);
        iter = iter->next;
    }
    return ret;
}

interCode* newInterCode(int ic_type) {
    assert(ECODE <= ic_type && ic_type <= ASSIGN);
    interCode* ret = (interCode*)malloc(sizeof(interCode));
    ret->ic_type = ic_type;
    ret->next = ret;
    ret->prev = ret;
    return ret;
}

interCode* newLabelCode(int label_id) {
    interCode* ret = newInterCode(LABEL);
    ret->label_id = label_id;
    return ret;
}

interCode* newFunctionCode(const char* funcname) {
    interCode* ret = newInterCode(FUNCTION);
    strncpy(ret->func_name, funcname, 32);
    return ret;
}

interCode* newGotoCode(int label_id) {
    interCode* ret = newInterCode(GOTO);
    ret->label_id = label_id;
    return ret;
}

interCode* newDecCode(int var_id, int size) {
    interCode* ret = newInterCode(DEC);
    ret->dec.var_id = var_id;
    ret->dec.size = size;
    return ret;
}

interCode* newSingleOprCode(int ic_type, operand opr) {
    // RETURN & PARAM & ARG & READ & WRITE
    interCode* ret = newInterCode(ic_type);
    ret->opr = opr;
    return ret;
}

interCode* newCondCode(int op_type, operand opr1, operand opr2, int label_id) {
    assert(EQ <= op_type && op_type <= GT);
    interCode* ret = newInterCode(COND);
    ret->cond.opr1 = opr1;
    ret->cond.opr2 = opr2;
    ret->cond.op_type = op_type;
    ret->cond.label_id = label_id;
    return ret;
}

interCode* newCallCode(operand dst, const char* funcname) {
    interCode* ret = newInterCode(CALL);
    ret->call.dst = dst;
    strncpy(ret->call.func_name, funcname, 32);
    return ret;
}

interCode* newAssignCode(int op_type, operand dst, operand src1, operand src2) {
    assert(AS <= op_type && op_type <= LRSTAR);
    if (IS_EOPR(dst)) return NULL;
    interCode* ret = newInterCode(ASSIGN);
    ret->assign.op_type = op_type;
    ret->assign.dst = dst;
    ret->assign.src1 = src1;
    ret->assign.src2 = src2;
    return ret;
}

interCode* mergeCode(interCode* head, interCode* codes) {
    if (head == NULL) return codes;
    if (codes == NULL) return head;
    interCode* codes_tail = codes->prev;
    codes_tail->next = head;
    codes->prev = head->prev;
    head->prev->next = codes;
    head->prev = codes_tail;
    return head;
}

void insertCodeAfter(interCode* where, interCode* codes) {
    assert(where);
    if (codes == NULL) return;
    interCode* next = where->next;
    where->next = codes;
    next->prev = codes->prev;  // codes->tail
    codes->prev->next = next;
    codes->prev = where;
}

interCode* removeCodeItr(interCode* remove, bool next) {
    assert(remove != NULL);
    // assert(remove->ic_type != FUNCTION);
    if (remove->next == remove) {
        free(remove);
        return NULL;
    }
    remove->prev->next = remove->next;
    remove->next->prev = remove->prev;
    interCode* ret;
    if (next)
        ret = remove->next;
    else
        ret = remove->prev;
    free(remove);
    return ret;
}

void removeCode(interCode* remove) {
    assert(remove != NULL);
    assert(remove->ic_type != FUNCTION);
    if (remove->next == remove) {
        free(remove);
    }
    remove->prev->next = remove->next;
    remove->next->prev = remove->prev;
    free(remove);
}

void operandToString(char* buffer, operand opr) {
    // Note: buffer length should be 16 at least
    assert(buffer);
    buffer[0] = 0;
    switch (OPR_TYPE(opr)) {
        case ECODE:
            break;
        case CONST:
            sprintf(buffer, "#%d", opr.const_value);
            break;
        case VARIABLE:
            sprintf(buffer, "v%d", opr.var_id);
            break;
        case TEMP:
            sprintf(buffer, "t%d", opr.tmp_id);
            break;
        default:
            assert(0);
    }
}

void printSingleCode(interCode* code) {
    char buf[256];
    interCodeToString(buf, code);
    printf("%s\n", buf);
}

void interCodeToString(char* buffer, interCode* code) {
    // Note: buffer length should be 64 at least
    assert(buffer);
    buffer[0] = 0;

    char opr1_buffer[32];  // get operand1 string
    char opr2_buffer[32];  // get operand2 string
    switch (code->ic_type) {
        case ECODE:
            break;
        case FUNCTION:
            sprintf(buffer, "FUNCTION %s :", code->func_name);
            break;
        case LABEL:
            sprintf(buffer, "LABEL label%d :", code->label_id);
            break;
        case GOTO:
            sprintf(buffer, "GOTO label%d", code->label_id);
            break;
        case COND:
            operandToString(opr1_buffer, code->cond.opr1);
            operandToString(opr2_buffer, code->cond.opr2);
            switch (code->cond.op_type) {
                case EQ:
                    sprintf(buffer, "IF %s == %s GOTO label%d", opr1_buffer,
                            opr2_buffer, code->cond.label_id);
                    break;
                case NE:
                    sprintf(buffer, "IF %s != %s GOTO label%d", opr1_buffer,
                            opr2_buffer, code->cond.label_id);
                    break;
                case LE:
                    sprintf(buffer, "IF %s <= %s GOTO label%d", opr1_buffer,
                            opr2_buffer, code->cond.label_id);
                    break;
                case LT:
                    sprintf(buffer, "IF %s < %s GOTO label%d", opr1_buffer,
                            opr2_buffer, code->cond.label_id);
                    break;
                case GE:
                    sprintf(buffer, "IF %s >= %s GOTO label%d", opr1_buffer,
                            opr2_buffer, code->cond.label_id);
                    break;
                case GT:
                    sprintf(buffer, "IF %s > %s GOTO label%d", opr1_buffer,
                            opr2_buffer, code->cond.label_id);
                    break;
                default:
                    assert(0);
            }
            break;
        case PARAM:
            operandToString(opr1_buffer, code->opr);
            sprintf(buffer, "PARAM %s", opr1_buffer);
            break;
        case ARG:
            operandToString(opr1_buffer, code->opr);
            sprintf(buffer, "ARG %s", opr1_buffer);
            break;
        case DEC:
            sprintf(buffer, "DEC v%d %d", code->dec.var_id, code->dec.size);
            break;
        case CALL:
            operandToString(opr1_buffer, code->call.dst);
            sprintf(buffer, "%s := CALL %s", opr1_buffer, code->call.func_name);
            break;
        case RETURN_IC:
            operandToString(opr1_buffer, code->opr);
            sprintf(buffer, "RETURN %s", opr1_buffer);
            break;
        case READ:
            operandToString(opr1_buffer, code->opr);
            sprintf(buffer, "READ %s", opr1_buffer);
            break;
        case WRITE:
            operandToString(opr1_buffer, code->opr);
            sprintf(buffer, "WRITE %s", opr1_buffer);
            break;
        case ASSIGN:
            operandToString(opr1_buffer, code->assign.src1);
            char dst_buffer[16];
            operandToString(dst_buffer, code->assign.dst);
            switch (code->assign.op_type) {
                case AS:
                    sprintf(buffer, "%s := %s", dst_buffer, opr1_buffer);
                    break;
                case ADD:
                    operandToString(opr2_buffer, code->assign.src2);
                    sprintf(buffer, "%s := %s + %s", dst_buffer, opr1_buffer,
                            opr2_buffer);
                    break;
                case SUB:
                    operandToString(opr2_buffer, code->assign.src2);
                    sprintf(buffer, "%s := %s - %s", dst_buffer, opr1_buffer,
                            opr2_buffer);
                    break;
                case MUL:
                    operandToString(opr2_buffer, code->assign.src2);
                    sprintf(buffer, "%s := %s * %s", dst_buffer, opr1_buffer,
                            opr2_buffer);
                    break;
                case DIVD:
                    operandToString(opr2_buffer, code->assign.src2);
                    sprintf(buffer, "%s := %s / %s", dst_buffer, opr1_buffer,
                            opr2_buffer);
                    break;
                case ADDR:
                    // Special Case: x := &y + z is allowed
                    if (IS_EOPR(code->assign.src2))
                        sprintf(buffer, "%s := &%s", dst_buffer, opr1_buffer);
                    else {
                        operandToString(opr2_buffer, code->assign.src2);
                        sprintf(buffer, "%s := &%s + %s", dst_buffer,
                                opr1_buffer, opr2_buffer);
                    }
                    break;
                case RSTAR:
                    sprintf(buffer, "%s := *%s", dst_buffer, opr1_buffer);
                    break;
                case LSTAR:
                    sprintf(buffer, "*%s := %s", dst_buffer, opr1_buffer);
                    break;
                case LRSTAR:
                    sprintf(buffer, "*%s := *%s", dst_buffer, opr1_buffer);
                    break;
                default:
                    assert(0);
            }
            break;
        default:
            assert(0);
    }
}

void interCodeToFile(interCode* codes, FILE* fp) {
    interCode* itr = codes;
    char buffer[256];
    do {
        interCodeToString(buffer, itr);
        fputs(buffer, fp);
        fputc('\n', fp);
        itr = itr->next;
    } while (itr != codes);
}

int isLeader(interCode* code, int* label) {
    switch (code->ic_type) {
        case LABEL:
            *label = code->label_id;
            return LABEL_S;
        case FUNCTION:
            return FUNCTION_S;
        case GOTO:
            *label = code->label_id;
            return GOTO_S;
        case COND:
            *label = code->cond.label_id;
            return COND_S;
        case RETURN_IC:
            return RETURN_S;
        default:
            return NORMAL_S;
    }
    return 0;
}