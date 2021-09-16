
#include "optimize.h"

#define INLINE_MAX_LINE 150
#define LABEL_MAX_CNT 8

#define DIV_LIKE_PYTHON 0

bool HAS_PROGRESS;
root_t InlineTable = RB_ROOT;  // <string, bool>

interCode* getInlineFunction(const char* funcname, root_t* functable) {
    map_t* find = get(&InlineTable, funcname);
    map_t* function = get(functable, funcname);
    if (function == NULL) return NULL;

    interCode* head = function->val;
    if (find == NULL) {  // check whether this function suits for inline
        interCode* iter = head;
        int lineCount = 1;
        int labelCount = 1;
        bool* boolean = (bool*)malloc(sizeof(bool));
        *boolean = true;
        do {
            switch (iter->ic_type) {
                case CALL:
                    // contains func call
                    *boolean = false;
                    break;
                case LABEL:
                    labelCount++;
                    break;
                default:
                    break;
            }
            if (iter->ic_type != PARAM) lineCount++;
            if (lineCount * labelCount > INLINE_MAX_LINE * LABEL_MAX_CNT)
                *boolean = false;
            iter = iter->next;
        } while (iter != head);
        put(&InlineTable, funcname, boolean);
        if (*boolean) return head;
        return NULL;
    }
    if (*(bool*)(find->val) == true) return head;
    return NULL;
}

interCode* findPreviousDef(interCode* from, operand opr) {
    interCode* iter = from->prev;
    while (iter) {
        switch (iter->ic_type) {
            case FUNCTION:
            case PARAM:
            case LABEL:
            case GOTO:
            case COND:
            case RETURN_IC:
                return NULL;
            case ARG:
                if (IS_ADDR(opr) && oprEqual(iter->opr, opr)) return NULL;
                break;
            case DEC:
            case WRITE:
                break;
            case CALL:
                if (oprEqual(opr, iter->call.dst)) return iter;
                break;
            case ASSIGN:
                if (oprEqual(opr, iter->assign.dst)) return iter;
                break;
            case READ:
                if (oprEqual(opr, iter->opr)) return iter;
                break;
            default:
                assert(0);
        }
        iter = iter->prev;
    }
    return NULL;
}

interCode* findNextUse(interCode* from, operand opr, operand excp1,
                       operand excp2) {
    // opr := excp1 op excp2
    // xxx := opr
    // from: from where starts to search
    // excp1, excp2: if changed, return NULL
    interCode* iter = from->next;
    while (iter) {
        switch (iter->ic_type) {
            case FUNCTION:
            case PARAM:
            case LABEL:
            case GOTO:
                return NULL;
            case DEC:
                break;
            case RETURN_IC:
            case ARG:
            case WRITE:
                if (oprEqual(opr, iter->opr)) return iter;
                break;
            case COND:
                if (oprEqual(opr, iter->cond.opr1)) return iter;
                if (oprEqual(opr, iter->cond.opr2)) return iter;
                break;
            case CALL:
                if (oprEqual(opr, iter->call.dst)) return NULL;
                if (oprEqual(excp1, iter->call.dst)) return NULL;
                if (oprEqual(excp2, iter->call.dst)) return NULL;
                break;
            case ASSIGN:
                if (oprEqual(opr, iter->assign.dst)) return NULL;
                if (oprEqual(excp1, iter->assign.dst)) return NULL;
                if (oprEqual(excp2, iter->assign.dst)) return NULL;
                if (oprEqual(opr, iter->assign.src1)) return iter;
                if (oprEqual(opr, iter->assign.src2)) return iter;
                break;
            case READ:
                if (oprEqual(opr, iter->opr)) return NULL;
                if (oprEqual(excp1, iter->opr)) return NULL;
                if (oprEqual(excp2, iter->opr)) return NULL;
                break;
            default:
                assert(0);
        }
        iter = iter->next;
    }
    return NULL;
}

bool replacePrevOpr(interCode* current, operand* target) {
    interCode* src = findPreviousDef(current, *target);
    if (!src) return false;
    if (src != current->prev) return false;
    // only check the adjacent previous code
    // to ensure correctness

    if (src->ic_type == ASSIGN && src->assign.op_type == AS) {
        *target = src->assign.src1;
        return true;
    }
    return false;
}

interCode* adjacentReplace(interCode* head) {
    assert(head->ic_type == FUNCTION);
    interCode* iter = head;
    interCode* tmp = NULL;
    do {
        switch (iter->ic_type) {
            case RETURN_IC:
            case WRITE:
            case ARG:
                replacePrevOpr(iter, &(iter->opr));
                break;
            case COND: {
                operand prev1 = iter->cond.opr1;
                operand prev2 = iter->cond.opr2;
                replacePrevOpr(iter, &(iter->cond.opr2));
                replacePrevOpr(iter, &(iter->cond.opr1));
                // not replace var with a temp
                if (IS_TEMP(iter->cond.opr1) && IS_VAR(prev1))
                    iter->cond.opr1 = prev1;
                if (IS_TEMP(iter->cond.opr2) && IS_VAR(prev2))
                    iter->cond.opr2 = prev2;
                if (IS_CONST(iter->cond.opr1) && IS_CONST(iter->cond.opr2)) {
                    // Note: Here MUST use long long in case of overflow
                    long long val1 = iter->cond.opr1.const_value;
                    long long val2 = iter->cond.opr2.const_value;
                    long long diff = val1 - val2;
                    bool match;
                    switch (iter->cond.op_type) {
                        case EQ:
                            match = diff == 0;
                            break;
                        case NE:
                            match = diff != 0;
                            break;
                        case GE:
                            match = diff >= 0;
                            break;
                        case GT:
                            match = diff > 0;
                            break;
                        case LE:
                            match = diff <= 0;
                            break;
                        case LT:
                            match = diff < 0;
                            break;
                        default:
                            assert(0);
                    }
                    if (match) {
                        iter->ic_type = GOTO;
                        iter->label_id = iter->cond.label_id;
                    } else {
                        iter = removeCodeItr(iter, true);
                    }
                }
                break;
            }
            case ASSIGN:
                switch (iter->assign.op_type) {
                    case AS:
                        if (iter->prev->ic_type == ASSIGN &&
                            iter->prev->assign.op_type == ADDR &&
                            IS_EOPR(iter->prev->assign.src2) &&
                            oprEqual(iter->prev->assign.dst,
                                     iter->assign.src1)) {
                            iter->assign.op_type = iter->prev->assign.op_type;
                            iter->assign.src1 = iter->prev->assign.src1;
                            break;
                        }
                        replacePrevOpr(iter, &(iter->assign.src1));
                        break;
                    case ADD:
                    case SUB:
                    case MUL:
                    case DIVD:
                        if (iter->prev->ic_type == ASSIGN &&
                            iter->prev->assign.op_type == ADDR &&
                            IS_EOPR(iter->prev->assign.src2) &&
                            iter->assign.op_type == ADD) {
                            // t1 := &v
                            // t2 := t1 + t3  ==> t2 := &v + t3
                            if (oprEqual(iter->prev->assign.dst,
                                         iter->assign.src1)) {
                                iter->assign.op_type = ADDR;
                                iter->assign.src1 = iter->prev->assign.src1;
                                break;
                            } else if (oprEqual(iter->prev->assign.dst,
                                                iter->assign.src2)) {
                                iter->assign.op_type = ADDR;
                                iter->assign.src2 = iter->assign.src1;
                                iter->assign.src1 = iter->prev->assign.src1;
                                break;
                            }
                        }
                        replacePrevOpr(iter, &(iter->assign.src1));
                        replacePrevOpr(iter, &(iter->assign.src2));
                        if (iter->assign.op_type == SUB &&
                            oprEqual(iter->assign.src1, iter->assign.src2)) {
                            iter->assign.op_type = AS;
                            iter->assign.src1 = zeroOpr;
                            iter->assign.src2 = nullOpr;
                            break;
                        }
                        if (iter->assign.op_type == DIVD &&
                            oprEqual(iter->assign.src1, iter->assign.src2)) {
                            iter->assign.op_type = AS;
                            iter->assign.src1 = newOperand(CONST, 1);
                            iter->assign.src2 = nullOpr;
                            break;
                        }

                        if (IS_CONST(iter->assign.src1) &&
                            IS_CONST(iter->assign.src2)) {
                            int val1 = iter->assign.src1.const_value;
                            int val2 = iter->assign.src2.const_value;
                            int res = 0;
                            if (iter->assign.op_type == ADD)
                                res = val1 + val2;
                            else if (iter->assign.op_type == SUB)
                                res = val1 - val2;
                            else if (iter->assign.op_type == MUL)
                                res = val1 * val2;
                            else {
                                if (val2 == 0) break;
                                res = val1 / val2;
                                if (DIV_LIKE_PYTHON) {
                                    float tmp_res = (float)val1 / (float)val2;
                                    if (tmp_res < 0 && val1 % val2 != 0)
                                        res = (int)tmp_res - 1;
                                }
                            }
                            iter->assign.op_type = AS;
                            iter->assign.src1 = newOperand(CONST, res);
                            iter->assign.src2 = nullOpr;
                        }
                        break;
                }
                break;
        }
        if (iter->ic_type == ASSIGN) {
            switch (iter->assign.op_type) {
                case ADD:
                    if (IS_ZERO(iter->assign.src1)) {
                        iter->assign.op_type = AS;
                        iter->assign.src1 = iter->assign.src2;
                        iter->assign.src2 = nullOpr;
                    } else if (IS_ZERO(iter->assign.src2)) {
                        iter->assign.op_type = AS;
                        iter->assign.src2 = nullOpr;
                    }
                    break;
                case SUB:
                    if (IS_ZERO(iter->assign.src2)) {
                        iter->assign.op_type = AS;
                        iter->assign.src2 = nullOpr;
                    }
                    break;
                case MUL:
                    if (IS_ONE(iter->assign.src1)) {
                        iter->assign.op_type = AS;
                        iter->assign.src1 = iter->assign.src2;
                        iter->assign.src2 = nullOpr;
                    } else if (IS_ONE(iter->assign.src2)) {
                        iter->assign.op_type = AS;
                        iter->assign.src2 = nullOpr;
                    }
                    break;
                case DIVD:
                    if (IS_ONE(iter->assign.src2)) {
                        iter->assign.op_type = AS;
                        iter->assign.src2 = nullOpr;
                    }
                    break;
            }
        }
        iter = iter->next;
    } while (iter != head);
    return head;
}

interCode* useReplace(interCode* head) {
    assert(head->ic_type == FUNCTION);
    interCode* iter = head;
    do {
        // search for code which is an assign code
        if (iter->ic_type != ASSIGN) {
            iter = iter->next;
            if (iter == head)
                break;
            else
                continue;
        }

        if (iter->assign.op_type == AS) {
            interCode* repl =
                findNextUse(iter, iter->assign.dst, iter->assign.src1, nullOpr);
            while (repl) {
                switch (repl->ic_type) {
                    case RETURN_IC:
                    case ARG:
                    case WRITE:
                        repl->opr = iter->assign.src1;
                        break;
                    case ASSIGN:
                        if (oprEqual(repl->assign.src1, iter->assign.dst))
                            repl->assign.src1 = iter->assign.src1;
                        else
                            repl->assign.src2 = iter->assign.src1;
                        break;
                    case COND:
                        if (oprEqual(repl->cond.opr1, iter->assign.dst))
                            repl->cond.opr1 = iter->assign.src1;
                        else
                            repl->cond.opr2 = iter->assign.src1;
                        break;
                    default:
                        assert(0);
                }
                if (repl->ic_type != COND && repl->ic_type != RETURN_IC)
                    // current can be replaced, but not the following
                    repl = findNextUse(repl, iter->assign.dst,
                                       iter->assign.src1, nullOpr);
                else
                    break;
            }
        } else if (iter->assign.op_type == ADDR && IS_EOPR(iter->assign.src2)) {
            // x := &v
            // ...
            // y := x + w ==> y := &v + w
            interCode* repl =
                findNextUse(iter, iter->assign.dst, iter->assign.src1, nullOpr);
            while (repl) {
                if (repl->ic_type == ASSIGN &&
                    oprEqual(repl->assign.src1, iter->assign.dst)) {
                    repl->assign.src1 = iter->assign.src1;
                    repl->assign.op_type = ADDR;
                }
                repl = findNextUse(repl, iter->assign.dst, iter->assign.src1,
                                   nullOpr);
            }
        } else if (iter->assign.op_type == ADD || iter->assign.op_type == SUB ||
                   iter->assign.op_type == MUL ||
                   iter->assign.op_type == DIVD) {
            interCode* repl = findNextUse(iter, iter->assign.dst,
                                          iter->assign.src1, iter->assign.src2);
            while (repl) {
                if (repl->ic_type == ASSIGN) {
                    if (repl->assign.op_type == AS) {
                        // t1 := a op b
                        // t2 := t1
                        repl->assign.op_type = iter->assign.op_type;
                        repl->assign.src1 = iter->assign.src1;
                        repl->assign.src2 = iter->assign.src2;
                    } else if (((IS_CONST(iter->assign.src1) &&
                                 !IS_CONST(iter->assign.src2)) ||
                                (IS_CONST(iter->assign.src2) &&
                                 !IS_CONST(iter->assign.src1))) &&
                               (IS_CONST(repl->assign.src1) ||
                                IS_CONST(repl->assign.src2))) {
                        // t1 := a op const
                        // t2 := t1 op const
                        int op1 = iter->assign.op_type;
                        int op2 = repl->assign.op_type;
                        operand var;
                        int val1, val2;
                        if ((op1 == ADD || op1 == SUB) &&
                            (op2 == ADD || op2 == SUB)) {
                            bool neg = false;
                            if (IS_CONST(iter->assign.src1)) {
                                // t1 := imm + (-var)
                                val1 = iter->assign.src1.const_value;
                                var = iter->assign.src2;
                                if (op1 == SUB) neg = true;
                            } else {
                                // t1 := var + (-imm)
                                val1 = iter->assign.src2.const_value;
                                var = iter->assign.src1;
                                if (op1 == SUB) val1 = -val1;
                            }
                            if (IS_CONST(repl->assign.src1)) {
                                // t2 := imm +- t1
                                val2 = repl->assign.src1.const_value;
                                if (op2 == SUB) {
                                    neg = !neg;
                                    val1 = -val1;
                                }
                            } else {
                                val2 = repl->assign.src2.const_value;
                                if (op2 == SUB) val2 = -val2;
                            }
                            if (neg) {
                                repl->assign.op_type = SUB;
                            } else {
                                repl->assign.op_type = ADD;
                            }
                            repl->assign.src2 = var;
                            repl->assign.src1 = newOperand(CONST, val1 + val2);
                        }
                        if (op1 == MUL && op2 == MUL) {
                            if (IS_CONST(iter->assign.src1)) {
                                val1 = iter->assign.src1.const_value;
                                var = iter->assign.src2;
                            } else {
                                val1 = iter->assign.src2.const_value;
                                var = iter->assign.src1;
                            }
                            if (IS_CONST(repl->assign.src1)) {
                                val2 = repl->assign.src1.const_value;
                            } else {
                                val2 = repl->assign.src2.const_value;
                            }
                            if ((unsigned long long)val1 * val2 ==
                                val1 * val2) {
                                repl->assign.src1 = var;
                                repl->assign.src2 =
                                    newOperand(CONST, val1 * val2);
                            }
                        }
                    }
                } else
                    break;
                repl = findNextUse(repl, iter->assign.dst, iter->assign.src1,
                                   iter->assign.src2);
            }
        }

        iter = iter->next;
    } while (iter != head);
    return head;
}

void clearList(bool* list, int size) {
    for (int i = 0; i < size; i++) {
        list[i] = true;
    }
}

void setOutActive(bool* active, block* b) {
    active[0] = false;
    for (int i = 1; i < VarCount + TempCount + 1; i++) active[i] = false;
    if (b->flowSeqNext != NULL) {
        for (int i = 1; i < VarCount + TempCount + 1; i++) {
            if (b->flowSeqNext->useDef[i] == 2 || b->flowSeqNext->useIn[i] == 1)
                active[i] = true;
        }
    }
    if (b->flowGotoNext != NULL) {
        for (int i = 1; i < VarCount + TempCount + 1; i++) {
            if (b->flowGotoNext->useDef[i] == 2 ||
                b->flowGotoNext->useIn[i] == 1)
                active[i] = true;
        }
    }
}

void inactiveRemoveInBlock(block* b) {
    interCode* iter = b->end;
    int listSize = VarCount + TempCount + 1;
    bool* active = (bool*)malloc(sizeof(bool) * listSize);
    setOutActive(active, b);
    do {
        // remove inactive
        switch (iter->ic_type) {
            case FUNCTION:
            case PARAM:
            case LABEL:
                free(active);
                return;
            case GOTO:
                if (iter == b->first) {
                    free(active);
                    return;
                } else
                    iter = iter->prev;
                continue;
            case ASSIGN: {
                operand dst = getCodeDst(iter);
                if (!IS_EOPR(dst) && !IS_CONST(dst) &&
                    iter->assign.op_type != LSTAR &&
                    iter->assign.op_type != LRSTAR) {
                    int idx = getOprIndex(dst);
                    if (active[idx] == false) {
                        // dst is an inactive operand
                        HAS_PROGRESS = true;
                        if (iter == b->first) {
                            b->first = iter->next;
                            removeCode(iter);
                            free(active);
                            return;
                        } else if (iter == b->end) {
                            b->end = iter->prev;
                        }
                        iter = removeCodeItr(iter, false);
                        continue;
                    }
                }
                break;
            }
            default:
                break;
        }
        // set inactive
        if (isDefCode(iter)) {
            operand dst = getCodeDst(iter);
            int idx = getOprIndex(dst);
            active[idx] = false;
        }
        // set active
        operand use[3];
        int cnt = getCodeUse(iter, use);
        for (int i = 0; i < cnt; i++) {
            int idx = getOprIndex(use[i]);
            active[idx] = true;
        }
        if (iter == b->first)
            break;
        else
            iter = iter->prev;
    } while (true);

    free(active);
}

void globalInactiveRemove(block* entry) {
    block* itr = entry;
    while (itr != NULL) {
        inactiveRemoveInBlock(itr);
        itr = itr->next;
    }
}

interCode* inactiveRemove(interCode* head) {
    assert(head->ic_type == FUNCTION);
    interCode* iter = head->prev;
    int listSize = VarCount + TempCount + 1;
    bool* active = (bool*)malloc(sizeof(bool) * listSize);
    clearList(active, listSize);
    do {
        switch (iter->ic_type) {
            case FUNCTION:
            case PARAM:
                free(active);
                return head;
            case LABEL:
            case GOTO:
            case COND:
            case RETURN_IC:
                clearList(active, listSize);
                iter = iter->prev;  // Warning Here
                continue;
            case ASSIGN: {
                operand dst = getCodeDst(iter);
                if (!IS_EOPR(dst) && !IS_CONST(dst)) {
                    int idx = getOprIndex(dst);
                    if (active[idx] == false) {
                        // dst is an inactive operand
                        HAS_PROGRESS = true;
                        iter = removeCodeItr(iter, false);
                        continue;
                    }
                }
                break;
            }
            default:
                break;
        }
        if (isDefCode(iter)) {
            operand dst = getCodeDst(iter);
            int idx = getOprIndex(dst);
            active[idx] = false;
        }
        operand use[3];
        int cnt = getCodeUse(iter, use);
        for (int i = 0; i < cnt; i++) {
            int idx = getOprIndex(use[i]);
            active[idx] = true;
        }

        iter = iter->prev;
    } while (iter != head);

    free(active);
    return head;
}

void replaceBlockOpr(block* entry) {
    interCode*** defs = NULL;
    interCode* BAD = (void*)-1;
    int size = VarCount + TempCount + 1;
    for (block* b = entry; b; b = b->next) {
        // merge block's 2 in
        if (defs == NULL) {
            defs = (interCode***)malloc(sizeof(interCode**) * (size + 5));
            assert(defs != NULL);
            for (int i = 0; i < size; i++) {
                defs[i] = NULL;
            }
        }
        for (int i = 0; i < size; i++) {
            if (defs[i] == NULL) {
                defs[i] = (interCode**)malloc(sizeof(interCode*) * 2);
                assert(defs[i] != NULL);
            }
            defs[i][0] = NULL;
            defs[i][1] = NULL;
        }

        // defs[idx][from]
        // idx: OprIndex  from: which prev def comes from (0 / 1)
        for (int i = 0; i < 2; i++) {
            if (b->flowPrev[i] != NULL) {
                for (genNode* iter = b->flowPrev[i]->gen; iter;
                     iter = iter->next) {
                    int idx = getOprIndex(getCodeDst(iter->code));
                    assert(idx >= 0 && idx < size);
                    if (defs[idx][i] == NULL)
                        defs[idx][i] = iter->code;
                    else
                        defs[idx][i] = BAD;
                }
                for (outNode* iter = b->flowPrev[i]->out; iter;
                     iter = iter->next) {
                    int idx = getOprIndex(getCodeDst(iter->code));
                    assert(idx >= 0 && idx < size);
                    if (defs[idx][i] == NULL)
                        defs[idx][i] = iter->code;
                    else if (defs[idx][i] != BAD) {
                        char buf1[256];
                        char buf2[256];
                        interCodeToString(buf1, iter->code);
                        interCodeToString(buf2, defs[idx][i]);
                        if (strcmp(buf1, buf2) != 0) defs[idx][i] = BAD;
                    } else
                        defs[idx][i] = BAD;
                }
            }
        }
        // check opr
        operand* repl[2];
        for (interCode* code = b->first; code; code = code->next) {
            int idxTmp = 0;
            repl[0] = NULL;
            repl[1] = NULL;
            switch (code->ic_type) {
                case FUNCTION:
                case PARAM:
                case LABEL:
                case GOTO:
                case DEC:
                case CALL:
                    break;
                case READ:
                    idxTmp = getOprIndex(code->opr);
                    assert(idxTmp >= 0 && idxTmp < size);
                    if (defs[idxTmp][0] != BAD)  //
                        defs[idxTmp][0] = BAD;
                    break;
                case RETURN_IC:
                case ARG:
                case WRITE:
                    repl[0] = &(code->opr);
                    break;
                case COND:
                    repl[0] = &(code->cond.opr1);
                    repl[1] = &(code->cond.opr2);
                    break;
                case ASSIGN:
                    repl[0] = &(code->assign.src1);
                    if (!IS_EOPR(code->assign.src2))
                        repl[1] = &(code->assign.src2);
                    idxTmp = getOprIndex(code->assign.dst);
                    assert(idxTmp >= 0 && idxTmp < size);
                    if (defs[idxTmp][0] != BAD)  //
                        defs[idxTmp][0] = BAD;
                    break;
                default:
                    assert(0);
            }
            for (int i = 0; i < 2; i++) {
                if (repl[i] != NULL && !IS_CONST(*repl[i])) {
                    int idx = getOprIndex(*repl[i]);
                    assert(idx >= 0 && idx < size);
                    if (defs[idx][0] == BAD || defs[idx][1] == BAD) continue;

                    if (defs[idx][1] == NULL) {
                        if (defs[idx][0] != NULL && b->flowPrev[1] == NULL) {
                            interCode* prev = defs[idx][0];
                            if (prev->ic_type == ASSIGN &&
                                prev->assign.op_type == AS) {
                                *repl[i] = prev->assign.src1;
                                HAS_PROGRESS = true;
                            }
                        }
                    } else {
                        if (defs[idx][0] == NULL) {
                            continue;
                        } else {  // both not NULL
                            interCode* prev1 = defs[idx][0];
                            interCode* prev2 = defs[idx][1];
                            if (prev1->ic_type == ASSIGN &&
                                prev1->assign.op_type == AS &&
                                prev2->ic_type == ASSIGN &&
                                prev2->assign.op_type == AS &&
                                oprEqual(prev1->assign.src1,
                                         prev2->assign.src1)) {
                                *repl[i] = prev1->assign.src1;
                                HAS_PROGRESS = true;
                            }
                        }
                    }
                }
            }
            if (code == b->end) break;
        }
    }
    if (defs != NULL) {
        for (int i = 0; i < size; i++) {
            if (defs[i] != NULL) free(defs[i]);
        }
        free(defs);
    }
}

void removeUselessGoto(interCode* head) {
    interCode* iter = head;
    do {
        if (iter->ic_type == GOTO && iter->next->ic_type == LABEL &&
            iter->next->label_id == iter->label_id) {
            // GOTO A
            // LABEL A:
            iter = removeCodeItr(iter, true);
        } else if (iter->ic_type == COND && iter->next->ic_type == LABEL &&
                   iter->next->label_id == iter->cond.label_id) {
            // IF rel GOTO A
            // LABEL A:
            iter = removeCodeItr(iter, true);
        } else {
            iter = iter->next;
        }
    } while (iter != head);
}

void mergeCondGoto(interCode* head) {
    interCode* iter = head;
    // IF a relop b GOTO A              IF a not relop b GOTO B
    // GOTO B                   ==>     LABEL A :
    // LABEL A :
    do {
        if (iter->ic_type == COND && iter->next->ic_type == GOTO &&
            iter->next->next->ic_type == LABEL) {
            if (iter->cond.label_id == iter->next->next->label_id) {
                iter->cond.op_type = reverseRelOp(iter->cond.op_type);
                iter->cond.label_id = iter->next->label_id;
                removeCode(iter->next);
                HAS_PROGRESS = true;
            } else {
                iter = iter->next->next->next;
            }
        } else {
            iter = iter->next;
        }
    } while (iter != head);
}

interCode* removeUselessLabel(interCode* head) {
    interCode* iter = head;
    bool* used = (bool*)malloc(sizeof(bool) * (LabelCount + 1));
    for (int i = 0; i < LabelCount + 1; i++) used[i] = false;
    do {
        if (iter->ic_type == LABEL) {
            if (iter->next->ic_type == LABEL) {
                // Label A :
                // Label B :
                interCode* modifyIter = head;
                int modifyId = iter->next->label_id;
                int thisId = iter->label_id;
                removeCode(iter->next);
                HAS_PROGRESS = true;
                do {
                    if (modifyIter->ic_type == GOTO) {
                        if (modifyIter->label_id == modifyId)
                            modifyIter->label_id = thisId;
                    }
                    if (modifyIter->ic_type == COND) {
                        if (modifyIter->cond.label_id == modifyId)
                            modifyIter->cond.label_id = thisId;
                    }
                    modifyIter = modifyIter->next;
                } while (modifyIter != head);
            } else if (iter->next->ic_type == GOTO) {
                // Label A :
                // Goto B
                interCode* modifyIter = head;
                int modifyId = iter->next->label_id;
                int thisId = iter->label_id;
                do {
                    if (modifyIter->ic_type == GOTO) {
                        if (modifyIter->label_id == thisId)
                            modifyIter->label_id = modifyId;
                    }
                    if (modifyIter->ic_type == COND) {
                        if (modifyIter->cond.label_id == thisId)
                            modifyIter->cond.label_id = modifyId;
                    }
                    modifyIter = modifyIter->next;
                } while (modifyIter != head);
            }
        }
        iter = iter->next;
    } while (iter != head);

    do {
        if (iter->ic_type == COND) {
            used[iter->cond.label_id] = true;
        }
        if (iter->ic_type == GOTO) {
            used[iter->label_id] = true;
        }
        iter = iter->next;
    } while (iter != head);

    do {
        if (iter->ic_type == LABEL && used[iter->label_id] == false) {
            iter = removeCodeItr(iter, true);
            HAS_PROGRESS = true;
        } else {
            iter = iter->next;
        }
    } while (iter != head);

    free(used);

    return head;
}

interCode* removeUselessOpr(interCode* head) {
    for (int i = 1; i < VarCount + TempCount + 1; i++) {
        int idx = i;
        if (useTable[idx] == NULL) {
            // var / temp not used
            // remove related def codes
            defNode* iter = defTable[idx];
            while (iter != NULL) {
                if (iter->code->ic_type != CALL) {
                    removeCode(iter->code);
                    HAS_PROGRESS = true;
                }
                defNode* to_free = iter;
                iter = iter->next;
                free(to_free);
            }
            defTable[idx] = NULL;
        }
    }
    return head;
}

void removeUnreachableBlock(block* entry) {
    dfs(entry, entry->isVisited);
    block* iter = entry;
    while (iter) {
        if (iter->isVisited != entry->isVisited) {  // Not Reached
            HAS_PROGRESS = true;
            iter = removeBlock(iter);
        } else {
            iter = iter->next;
        }
    }
}

void replaceInlineFunction(interCode* head, root_t* functable) {
    interCode* iter = head->next;
    while (iter != head) {
        if (iter->ic_type != CALL) {
            iter = iter->next;
            continue;
        }
        interCode* repl = getInlineFunction(iter->call.func_name, functable);
        if (repl == NULL) {
            iter = iter->next;
            continue;
        }

        operand dst = iter->call.dst;
        repl = copyInterCode(repl);

        interCode* next = iter->next;
        // record where iter should go

        int previous_id = VarCount;

        // replace PARAM vi; ARG ai ==> vi = ai
        interCode* arg_iter = iter->prev;
        interCode* param_iter = repl->next;
        while (param_iter->ic_type == PARAM) {
            operand param = param_iter->opr;
            operand new_param = allocVar();
            param_iter->ic_type = ASSIGN;
            param_iter->assign.op_type = AS;
            param_iter->assign.dst = new_param;
            param_iter->assign.src1 = arg_iter->opr;
            param_iter->assign.src2 = nullOpr;
            arg_iter = removeCodeItr(arg_iter, false);
            param_iter = param_iter->next;
            interCode* var_iter = param_iter;
            do {
                if (var_iter->ic_type == ASSIGN) {
                    if (oprEqual(param, var_iter->assign.dst)) {
                        var_iter->assign.dst = new_param;
                    }
                    if (oprEqual(param, var_iter->assign.src1)) {
                        var_iter->assign.src1 = new_param;
                    }
                    if (oprEqual(param, var_iter->assign.src2)) {
                        var_iter->assign.src2 = new_param;
                    }
                } else if (var_iter->ic_type == READ ||
                           var_iter->ic_type == WRITE ||
                           var_iter->ic_type == RETURN_IC) {
                    if (oprEqual(param, var_iter->opr)) {
                        var_iter->opr = new_param;
                    }
                } else if (var_iter->ic_type == COND) {
                    if (oprEqual(param, var_iter->cond.opr1)) {
                        var_iter->cond.opr1 = new_param;
                    }
                    if (oprEqual(param, var_iter->cond.opr2)) {
                        var_iter->cond.opr2 = new_param;
                    }
                }
                var_iter = var_iter->next;
            } while (var_iter != repl);
        }
        // add label for return
        int ret_label = allocLabel();
        // replace RETURN ret; x := CALL ==> x := ret; GOTO return
        interCode* repl_iter = param_iter;
        while (repl_iter != repl) {
            if (repl_iter->ic_type == RETURN_IC) {
                operand ret = repl_iter->opr;
                repl_iter->ic_type = ASSIGN;
                repl_iter->assign.op_type = AS;
                repl_iter->assign.dst = dst;
                repl_iter->assign.src1 = ret;
                repl_iter->assign.src2 = nullOpr;
                insertCodeAfter(repl_iter, newGotoCode(ret_label));
            } else if (repl_iter->ic_type == LABEL) {
                int old_lb = repl_iter->label_id;
                int new_lb = allocLabel();
                interCode* lb_iter = repl;
                do {
                    if (lb_iter->ic_type == LABEL || lb_iter->ic_type == GOTO) {
                        if (lb_iter->label_id == old_lb)
                            lb_iter->label_id = new_lb;
                    } else if (lb_iter->ic_type == COND) {
                        if (lb_iter->cond.label_id == old_lb)
                            lb_iter->cond.label_id = new_lb;
                    }
                    lb_iter = lb_iter->next;
                } while (lb_iter != repl);
            } else if (repl_iter->ic_type == DEC) {
                int old_id = repl_iter->dec.var_id;
                operand old_opr = newOperand(VARIABLE, old_id);
                operand new_opr = allocVar();
                int new_id = new_opr.var_id;
                repl_iter->dec.var_id = new_id;

                interCode* dec_iter = repl_iter;
                do {
                    if (dec_iter->ic_type == ASSIGN) {
                        if (oprEqual(old_opr, dec_iter->assign.dst)) {
                            dec_iter->assign.dst = new_opr;
                        }
                        if (oprEqual(old_opr, dec_iter->assign.src1)) {
                            dec_iter->assign.src1 = new_opr;
                        }
                        if (oprEqual(old_opr, dec_iter->assign.src2)) {
                            dec_iter->assign.src2 = new_opr;
                        }
                    }
                    dec_iter = dec_iter->next;
                } while (dec_iter != repl);
            } else if (repl_iter->ic_type == READ ||
                       repl_iter->ic_type == ASSIGN) {
                operand old_opr = getCodeDst(repl_iter);
                if (old_opr.var_id <= previous_id) {
                    operand new_opr = allocVar();
                    interCode* var_iter = repl_iter;
                    do {
                        if (var_iter->ic_type == ASSIGN) {
                            if (oprEqual(old_opr, var_iter->assign.dst)) {
                                var_iter->assign.dst = new_opr;
                            }
                            if (oprEqual(old_opr, var_iter->assign.src1)) {
                                var_iter->assign.src1 = new_opr;
                            }
                            if (oprEqual(old_opr, var_iter->assign.src2)) {
                                var_iter->assign.src2 = new_opr;
                            }
                        } else if (var_iter->ic_type == READ ||
                                   var_iter->ic_type == WRITE ||
                                   var_iter->ic_type == RETURN_IC) {
                            if (oprEqual(old_opr, var_iter->opr)) {
                                var_iter->opr = new_opr;
                            }
                        } else if (var_iter->ic_type == COND) {
                            if (oprEqual(old_opr, var_iter->cond.opr1)) {
                                var_iter->cond.opr1 = new_opr;
                            }
                            if (oprEqual(old_opr, var_iter->cond.opr2)) {
                                var_iter->cond.opr2 = new_opr;
                            }
                        }
                        var_iter = var_iter->next;
                    } while (var_iter != repl);
                }
            }
            repl_iter = repl_iter->next;
        }

        // Label return :
        insertCodeAfter(repl->prev, newLabelCode(ret_label));
        // remove FUNCTION :
        repl = removeCodeItr(repl, true);
        // remove CALL
        iter = removeCodeItr(iter, false);

        interCode* end = repl->prev;
        insertCodeAfter(iter, repl);

        HAS_PROGRESS = true;

        iter = next;
    }
}

void simpleOptimize(root_t* funtable) {
    initDefUse();

    for (map_t* node = map_first(funtable); node;
         node = map_next(&(node->node))) {
        interCode* code = (interCode*)node->val;

        // removeUselessLabel(code);
        // delay this
        removeUselessGoto(code);
        adjacentReplace(code);
        useReplace(code);
        inactiveRemove(code);

        getDefsAndUses(code);
        removeUselessOpr(code);
    }
}

void globalOptimize(root_t* funtable) {
    initBlock();

    int recordSize = LabelCount + 5;  // in case LabelCount == 0
    int recordCnt = 0;
    block** records = (block**)malloc(sizeof(block*) * recordSize);

    for (map_t* node = map_first(funtable); node;
         node = map_next(&(node->node))) {
        interCode* code = (interCode*)node->val;

        block* b = getBlocks(code);
        block* entry = getFlowGraph(b);

        removeUnreachableBlock(entry);

        if (DO_GLOBAL_REMOVE) {
            while (setBlockUseIn(entry))
                ;
            globalInactiveRemove(entry);
        }

        // while (setOutBlocks(entry, entry->isVisited)) {}
        // replaceBlockOpr(entry);

        if (recordCnt >= recordSize) {
            recordSize *= 2;
            block** new_records = (block**)malloc(sizeof(block*) * recordSize);
            for (int i = 0; i < recordCnt; i++) new_records[i] = records[i];
            free(records);
            records = new_records;
        }
        records[recordCnt++] = b;
    }

    for (int i = 0; i < recordCnt; i++) {
        freeBlocks(records[i]);
    }
}

void optimize(root_t* funtable) {
    int time = 0;
    do {
        HAS_PROGRESS = false;
        simpleOptimize(funtable);
        if (!HAS_PROGRESS && time <= 0) {
            // only do once global optimize
            ++time;

            for (map_t* node = map_first(funtable); node;
                 node = map_next(&(node->node))) {
                replaceInlineFunction((interCode*)node->val, funtable);
            }
            globalOptimize(funtable);
        }
    } while (HAS_PROGRESS);

    // Remove Label after all things done
    // To simplify Block Relation

    for (map_t* node = map_first(funtable); node;
         node = map_next(&(node->node))) {
        interCode* code = (interCode*)node->val;
        do {
            HAS_PROGRESS = false;
            mergeCondGoto(code);
            removeUselessLabel(code);
        } while (HAS_PROGRESS);
    }
    do {
        HAS_PROGRESS = false;
        simpleOptimize(funtable);
    } while (HAS_PROGRESS);
}