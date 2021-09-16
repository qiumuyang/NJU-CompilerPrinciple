#include "ir.h"

#include "optimize.h"

#define _OPT_

#define MAX_VAR_ID 10000
#define MAX_ARG_CNT 5000

static int funcCount = 0;

root_t FuncTable = RB_ROOT;
// Map <char*, interCode*> (funcname, funccode)
root_t FuncOrder = RB_ROOT;
root_t VarTable = RB_ROOT;
// Map <char*, int> (varname, var_id)
arrayNode* ArrayTable[MAX_VAR_ID];
// Barrel <int, arrayNode*> (var_id, array structure)
bool ParamTable[MAX_VAR_ID];

void translateExtDef(treeNode* extdef_list);
void translateFunction(treeNode* function);
interCode* translateFunDec(treeNode* funDec, char* buffer);
interCode* translateCompSt(treeNode* compst);
interCode* translateDefList(treeNode* deflist);
interCode* translateStmtList(treeNode* stmtlist);
interCode* translateDef(treeNode* def);
interCode* translateStmt(treeNode* stmt);
interCode* translateVarList(treeNode* varlist);
interCode* translateVarDec(treeNode* vardec, treeNode* exp);
interCode* translateExp(treeNode* exp, operand* dst);
interCode* translateArgs(treeNode* args);
interCode* translateIO(treeNode* exp, operand dst);
interCode* translateCond(treeNode* exp, int true_label, int false_label);
interCode* ensureEADDRInt(operand* opr);
interCode* arrayAssign(operand dst, operand src);

operand getOprByVarname(const char* varname) {
    map_t* ret = get(&VarTable, varname);
    if (ret == NULL) {
        operand opr = allocVar();
        int* var_id = (int*)malloc(sizeof(int));
        *var_id = opr.var_id;
        put(&VarTable, varname, var_id);
        return opr;
    }
    return newOperand(VARIABLE, *(int*)ret->val);
}

void addFunction(const char* funcname, interCode* codes) {
    if (codes) {
        put(&FuncTable, funcname, codes);

        char* bufidx = (char*)malloc(sizeof(char) * 32);
        char* bufname = (char*)malloc(sizeof(char) * 64);
        sprintf(bufidx, "%d", funcCount++);
        strcpy(bufname, funcname);
        put(&FuncOrder, bufidx, bufname);
    }
}

void translateError(int msg_id) {
    fprintf(stderr, "Cannot translate: %s\n", errmsgT[msg_id]);
    exit(1);
}

void checkSpecifier(treeNode* specifier) {
    assert(specifier);
    assert(specifier->token == Specifier);
    treeNode* child = specifier->childs[0];
    switch (child->token) {
        case TYPE:
            if (strcmp(child->str, "float") == 0)  //
                translateError(HAS_FLOAT);
            break;
        case StructSpecifier:
            translateError(HAS_STRUCTURE);
            break;
        default:
            assert(0);
    }
}

operand handleArrayDec(treeNode* vardec) {
    assert(vardec->childCnt > 1);

    int size = 0;
    arrayNode* head = NULL;
    while (vardec->childCnt == 4) {
        // [VarDec: VarDec LB INT RB]
        size = atoi(vardec->childs[2]->str);
        head = insertArrayNode(head, newArrayNode(size));
        vardec = vardec->childs[0];
    }
    operand ret = getOprByVarname(vardec->childs[0]->str);
    assert(ret.var_id < MAX_VAR_ID);

    SET_RADDR(ret);                       // set range-addr
    setArrayNodeVarId(head, ret.var_id);  // set node->var_id
    ret.level = head;                     // set current level
    ArrayTable[ret.var_id] = head;        // set ArrayTable

    return ret;
}

void translateExtDef(treeNode* extdef_list) {
    while (extdef_list->childs != NULL) {
        // [ExtDefList: ExtDef ExtDefList]
        treeNode* extdef = extdef_list->childs[0];
        treeNode* next_list = extdef_list->childs[1];

        treeNode* specifier = extdef->childs[0];
        checkSpecifier(specifier);

        switch (extdef->childs[1]->token) {
            case SEMI:
                // [ExtDef: Specifier SEMI]
                break;
            case ExtDecList:
                // [ExtDef: Specifier ExtDecList SEMI]
                break;
            case FunDec:
                // [ExtDef: Specifier FunDec CompSt]
                translateFunction(extdef);
                break;
            default:
                assert(0);
        }

        extdef_list = next_list;
    }
}

void translateFunction(treeNode* function) {
    // [ExtDef: Specifier FunDec CompSt]
    treeNode* funDec = function->childs[1];
    treeNode* compst = function->childs[2];

    char funcname[32];
    interCode* codes = translateFunDec(funDec, funcname);
    interCode* append = translateCompSt(compst);
    codes = mergeCode(codes, append);
    addFunction(funcname, codes);
}

interCode* translateFunDec(treeNode* funDec, char* buffer) {
    // [FunDec: ID LP <VarList> RP]
    treeNode* id = funDec->childs[0];
    strcpy(buffer, id->str);

    interCode* codes = newFunctionCode(id->str);
    interCode* varlist = NULL;
    if (funDec->childCnt == 4)  // has VarList
        varlist = translateVarList(funDec->childs[2]);
    codes = mergeCode(codes, varlist);
    return codes;
}

interCode* translateVarList(treeNode* varlist) {
    // [VarList: ParamDec <COMMA VarList>]
    // [ParamDec: Specifier VarDec]
    interCode* ret = NULL;
    operand var;
    while (true) {
        treeNode* paramdec = varlist->childs[0];
        treeNode* specifier = paramdec->childs[0];
        treeNode* vardec = paramdec->childs[1];

        checkSpecifier(specifier);
        // [VarDec: ID]
        if (vardec->childCnt == 1) {
            var = getOprByVarname(vardec->childs[0]->str);
        } else {
            var = handleArrayDec(vardec);
        }
        ParamTable[var.var_id] = true;

        ret = mergeCode(ret, newSingleOprCode(PARAM, var));

        if (varlist->childCnt == 1)
            break;
        else
            varlist = varlist->childs[2];
    }
    return ret;
}

interCode* translateCompSt(treeNode* compst) {
    // [CompSt: LC DefList StmtList RC]
    interCode* code1 = translateDefList(compst->childs[1]);
    interCode* code2 = translateStmtList(compst->childs[2]);
    return mergeCode(code1, code2);
}

interCode* translateDefList(treeNode* deflist) {
    interCode* ret = NULL;
    while (deflist->childs != NULL) {
        // [DefList: Def DefList]
        treeNode* def = deflist->childs[0];
        treeNode* next_list = deflist->childs[1];

        interCode* append = translateDef(def);
        ret = mergeCode(ret, append);

        deflist = next_list;
    }
    return ret;
}

interCode* translateStmtList(treeNode* stmtlist) {
    interCode* ret = NULL;
    while (stmtlist->childs != NULL) {
        // [StmtList: Stmt StmtList]
        treeNode* stmt = stmtlist->childs[0];
        treeNode* next_list = stmtlist->childs[1];

        interCode* append = translateStmt(stmt);
        ret = mergeCode(ret, append);

        stmtlist = next_list;
    }
    return ret;
}

interCode* translateDef(treeNode* def) {
    // [Def: Specifier DecList SEMI]
    interCode* ret = NULL;
    checkSpecifier(def->childs[0]);
    treeNode* declist = def->childs[1];

    // [DecList: Dec <COMMA DecList>]
    treeNode* dec = declist->childs[0];
    interCode* dec_code = NULL;
    while (true) {
        dec = declist->childs[0];

        // [Dec: VarDec <ASSIGNOP Exp>]
        if (dec->childCnt == 3)
            dec_code = translateVarDec(dec->childs[0], dec->childs[2]);
        else  // childCnt == 1
            dec_code = translateVarDec(dec->childs[0], NULL);
        ret = mergeCode(ret, dec_code);

        if (declist->childCnt == 1)
            break;
        else
            declist = declist->childs[2];
    }
    return ret;
}

interCode* translateStmt(treeNode* stmt) {
    interCode* ret = NULL;
    switch (stmt->childs[0]->token) {
        case Exp:
            // [Stmt: Exp SEMI]
            return translateExp(stmt->childs[0], &nullOpr);
        case CompSt:
            // [Stmt: Compst]
            return translateCompSt(stmt->childs[0]);
        case RETURN: {
            // [Stmt: RETURN Exp SEMI]
            operand ret_temp = allocTemp();
            interCode* exp_code = translateExp(stmt->childs[1], &ret_temp);
            interCode* ret_code = newSingleOprCode(RETURN_IC, ret_temp);
            exp_code = mergeCode(exp_code, ensureEADDRInt(&ret_temp));
            return mergeCode(exp_code, ret_code);
        }
        case IF: {
            // [Stmt: IF LP Exp RP Stmt <ELSE Stmt>]
            int t_lbl = allocLabel();
            int f_lbl = allocLabel();
            interCode* cond_code = translateCond(stmt->childs[2], t_lbl, f_lbl);
            interCode* stmt_code = translateStmt(stmt->childs[4]);
            if (stmt->childCnt == 7) {
                int exit_lbl = allocLabel();
                interCode* else_code = translateStmt(stmt->childs[6]);
                ret = mergeCode(ret, cond_code);
                ret = mergeCode(ret, newLabelCode(t_lbl));
                ret = mergeCode(ret, stmt_code);
                ret = mergeCode(ret, newGotoCode(exit_lbl));
                ret = mergeCode(ret, newLabelCode(f_lbl));
                ret = mergeCode(ret, else_code);
                ret = mergeCode(ret, newLabelCode(exit_lbl));
            } else {
                ret = mergeCode(ret, cond_code);
                ret = mergeCode(ret, newLabelCode(t_lbl));
                ret = mergeCode(ret, stmt_code);
                ret = mergeCode(ret, newLabelCode(f_lbl));
            }
            return ret;
        }
        case WHILE: {
            // [Stmt: WHILE LP Exp RP Stmt]
            int loop_lbl = allocLabel();
            int out_lbl = allocLabel();
            // if exp goto loop
            // goto out
            // loop: loop_code
            // if exp goto loop
            // goto out
            // out:
            interCode* cond_code =
                translateCond(stmt->childs[2], loop_lbl, out_lbl);
            interCode* cond_code2 =
                translateCond(stmt->childs[2], loop_lbl, out_lbl);
            interCode* loop_code = translateStmt(stmt->childs[4]);
            ret = mergeCode(ret, cond_code);
            ret = mergeCode(ret, newLabelCode(loop_lbl));
            ret = mergeCode(ret, loop_code);
            ret = mergeCode(ret, cond_code2);
            ret = mergeCode(ret, newLabelCode(out_lbl));
            return ret;
        }
        default:
            assert(0);
    }
}

interCode* translateVarDec(treeNode* vardec, treeNode* exp) {
    // [VarDec: ID]
    if (vardec->childCnt == 1) {
        operand var = getOprByVarname(vardec->childs[0]->str);
        if (exp != NULL) {
            interCode* codes = translateExp(exp, &var);
            if (ArrayTable[var.var_id] == NULL)
                return mergeCode(codes, ensureEADDRInt(&var));
            else
                return codes;
        } else
            // irsim accepts undeclared vars
            // no need to initialize them
            return NULL;
    }

    // [VarDec: VarDec LB INT RB]
    operand var = handleArrayDec(vardec);
    int size = getArraySize(ArrayTable[var.var_id]);
    interCode* dec_size = newDecCode(var.var_id, size);
    interCode* arr_assign = NULL;
    if (exp != NULL) {
        operand src_array = allocTemp();
        interCode* cal_src = translateExp(exp, &src_array);
        arr_assign = mergeCode(arr_assign, cal_src);
        arr_assign = mergeCode(arr_assign, arrayAssign(var, src_array));
    }
    return mergeCode(dec_size, arr_assign);
}

interCode* translateExp(treeNode* exp, operand* dst) {
    operand src;
    if (exp->childCnt == 1) {
        // [Exp: ID / INT / FLOAT]
        switch (exp->childs[0]->token) {
            case INT:
                src = newOperand(CONST, atoi(exp->childs[0]->str));
                break;
            case FLOAT:
                translateError(HAS_FLOAT);
                break;
            case ID:
                src = getOprByVarname(exp->childs[0]->str);
                if (ArrayTable[src.var_id] != NULL) {
                    dst->level = ArrayTable[src.var_id];
                    SET_RADDR(*dst);
                    // t := &v
                    if (!ParamTable[src.var_id])
                        return newAssignCode(ADDR, *dst, src, nullOpr);
                    else
                        return newAssignCode(AS, *dst, src, nullOpr);
                }
                break;
            default:
                assert(0);
        }
        return newAssignCode(AS, *dst, src, nullOpr);
    } else if (exp->childs[0]->token == LP) {
        // [Exp: LP Exp RP]
        return translateExp(exp->childs[1], dst);
    } else if (exp->childs[0]->token == MINUS) {
        // [Exp: MINUS Exp]
        src = allocTemp();
        interCode* code1 = translateExp(exp->childs[1], &src);
        code1 = mergeCode(code1, ensureEADDRInt(&src));
        interCode* code2 = newAssignCode(SUB, *dst, zeroOpr, src);
        return mergeCode(code1, code2);
    } else if (exp->childs[0]->token == ID) {
        // [Exp: ID LP <Args> RP]
        interCode* io_code = translateIO(exp, *dst);
        if (io_code != NULL) return io_code;

        interCode* call_code;
        if (IS_EOPR(*dst))
            call_code = newCallCode(allocTemp(), exp->childs[0]->str);
        else
            call_code = newCallCode(*dst, exp->childs[0]->str);
        interCode* arg_code = NULL;
        if (exp->childCnt == 4)  // has Args
            arg_code = translateArgs(exp->childs[2]);
        return mergeCode(arg_code, call_code);
    } else {
        switch (exp->childs[1]->token) {
            case ASSIGNOP: {
                // [Exp: Exp ASSIGNOP Exp]
                src = allocTemp();
                interCode* get_src = translateExp(exp->childs[2], &src);
                if (exp->childs[0]->childs[0]->token == ID) {
                    // var := exp
                    // array := exp
                    interCode* ret = NULL;
                    operand lvalue =
                        getOprByVarname(exp->childs[0]->childs[0]->str);
                    if (ArrayTable[lvalue.var_id] != NULL) {
                        SET_RADDR(lvalue);
                        lvalue.level = ArrayTable[lvalue.var_id];
                        ret = mergeCode(ret, get_src);
                        ret = mergeCode(ret, arrayAssign(lvalue, src));
                        if (!IS_EOPR(*dst)) {
                            dst->level = lvalue.level;
                            if (IS_RADDR(lvalue)) {
                                SET_RADDR(*dst);
                                ret = mergeCode(
                                    ret,
                                    newAssignCode(ADDR, *dst, lvalue, nullOpr));
                            }
                            if (IS_EADDR(lvalue)) {
                                SET_EADDR(*dst);
                                ret = mergeCode(
                                    ret,
                                    newAssignCode(AS, *dst, lvalue, nullOpr));
                            }
                        }
                        return ret;
                    } else {
                        // READ t1
                        // v1 := t1  ==>  READ v1
                        if (get_src != NULL && get_src->ic_type == READ &&
                            get_src->next == get_src) {
                            get_src->opr = lvalue;
                            ret = mergeCode(ret, get_src);
                            ret = mergeCode(
                                ret, newAssignCode(AS, *dst, lvalue, nullOpr));
                            return ret;
                        }
                        get_src = mergeCode(get_src, ensureEADDRInt(&src));
                        ret = mergeCode(ret, get_src);
                        ret = mergeCode(
                            ret, newAssignCode(AS, lvalue, src, nullOpr));
                        ret = mergeCode(
                            ret, newAssignCode(AS, *dst, lvalue, nullOpr));
                        return ret;
                    }
                } else {
                    operand laddr = allocTemp();
                    interCode* get_laddr = translateExp(exp->childs[0], &laddr);
                    get_src = mergeCode(get_src, get_laddr);
                    interCode* assign = NULL;
                    if (IS_RADDR(laddr) && IS_RADDR(src)) {
                        assign = arrayAssign(laddr, src);
                    } else if (!IS_ADDR(src)) {
                        assign = newAssignCode(LSTAR, laddr, src, nullOpr);
                    } else if (IS_EADDR(src)) {
                        assign = newAssignCode(LRSTAR, laddr, src, nullOpr);
                    } else {  // Bad Cases
                        assign = newAssignCode(AS, laddr, src, nullOpr);
                    }
                    if (!IS_EOPR(*dst)) {
                        dst->level = laddr.level;
                        if (IS_RADDR(laddr)) SET_RADDR(*dst);
                        if (IS_EADDR(laddr)) SET_EADDR(*dst);
                        assign = mergeCode(
                            assign, newAssignCode(AS, *dst, laddr, nullOpr));
                    }
                    return mergeCode(get_src, assign);
                }
            }
            case PLUS:
            case MINUS:
            case STAR:
            case DIV: {
                interCode* ret = NULL;
                operand src1 = allocTemp();
                operand src2 = allocTemp();
                interCode* code1 = translateExp(exp->childs[0], &src1);
                interCode* code2 = translateExp(exp->childs[2], &src2);
                code1 = mergeCode(code1, ensureEADDRInt(&src1));
                code2 = mergeCode(code2, ensureEADDRInt(&src2));
                int op_type = getArithOpType(exp->childs[1]->str);
                interCode* as_code = newAssignCode(op_type, *dst, src1, src2);
                ret = mergeCode(ret, code1);
                ret = mergeCode(ret, code2);
                return mergeCode(ret, as_code);
            }
            case RELOP:
            case AND:
            case OR:
            case Exp: /* [Exp: NOT Exp] */ {
                int label_true = allocLabel();
                int label_false = allocLabel();
                interCode* ret = NULL;
                interCode* assign_0 = newAssignCode(AS, *dst, zeroOpr, nullOpr);
                interCode* assign_1 =
                    newAssignCode(AS, *dst, newOperand(CONST, 1), nullOpr);
                interCode* cond_code =
                    translateCond(exp, label_true, label_false);
                ret = mergeCode(ret, assign_0);
                ret = mergeCode(ret, cond_code);
                ret = mergeCode(ret, newLabelCode(label_true));
                ret = mergeCode(ret, assign_1);
                ret = mergeCode(ret, newLabelCode(label_false));
                return ret;
            }
            case LB: {
                operand base = allocTemp();
                operand index = allocTemp();
                interCode* base_code = translateExp(exp->childs[0], &base);
                interCode* index_code = translateExp(exp->childs[2], &index);
                index_code = mergeCode(index_code, ensureEADDRInt(&index));
                // index := index * width
                interCode* get_bias = newAssignCode(
                    MUL, index, index, newOperand(CONST, base.level->width));
                // dst := base + index
                interCode* dst_assign = newAssignCode(  //
                    ADD, *dst, base, index);

                if (isArrayNodeTail(base.level)) {
                    // dst becomes an int
                    dst->level = NULL;
                    SET_EADDR(*dst);
                } else {
                    dst->level = base.level->next;
                    SET_RADDR(*dst);
                }
                interCode* ret = NULL;
                ret = mergeCode(ret, base_code);
                ret = mergeCode(ret, index_code);
                ret = mergeCode(ret, get_bias);
                ret = mergeCode(ret, dst_assign);
                return ret;
            }
            case DOT:
                translateError(HAS_STRUCTURE);
                break;
            default:
                assert(0);
        }
    }
    return NULL;
}

inline interCode* ensureEADDRInt(operand* opr) {
    if (IS_EADDR(*opr)) {
        UNSET_ADDR(*opr);
        return newAssignCode(RSTAR, *opr, *opr, nullOpr);
    }
    return NULL;
}

interCode* translateIO(treeNode* exp, operand dst) {
    assert(exp->childs[0]->token == ID);

    if (strcmp(exp->childs[0]->str, "read") == 0) {
        if (exp->childCnt == 4)  // Has Args
            return NULL;
        else
            return newSingleOprCode(READ, dst);
    } else if (strcmp(exp->childs[0]->str, "write") == 0) {
        if (exp->childCnt == 3)  // No Args
            return NULL;

        treeNode* argexp = exp->childs[2]    // ID LP Args RP
                               ->childs[0];  // Exp COMMA Args

        operand opr = allocTemp();
        interCode* opr_code = translateExp(argexp, &opr);
        opr_code = mergeCode(opr_code, ensureEADDRInt(&opr));
        return mergeCode(opr_code, newSingleOprCode(WRITE, opr));
    }
    return NULL;
}

interCode* translateArgs(treeNode* args) {
    // [Args: Exp <COMMA Args>]
    long long arg_exps[MAX_ARG_CNT] = {0};
    int arg_cnt = 0;

    interCode* ret = NULL;
    while (true) {
        if (arg_cnt >= MAX_ARG_CNT) {
            translateError(TOO_MANY_ARGS);
        }
        arg_exps[arg_cnt++] = (long long)args->childs[0];
        if (args->childCnt == 1)
            break;
        else
            args = args->childs[2];
    }
    for (int i = arg_cnt - 1; i >= 0; i--) {
        operand temp = allocTemp();
        ret = mergeCode(ret, translateExp((treeNode*)arg_exps[i], &temp));
        ret = mergeCode(ret, ensureEADDRInt(&temp));
        arg_exps[i] = temp.tmp_id;
    }
    for (int i = arg_cnt - 1; i >= 0; i--) {
        ret = mergeCode(ret,
                        newSingleOprCode(ARG, newOperand(TEMP, arg_exps[i])));
    }
    return ret;
}

interCode* translateCond(treeNode* exp, int true_label, int false_label) {
    interCode* ret = NULL;
    if (exp->childCnt >= 2) {
        if (exp->childs[1]->token == RELOP) {
            // [Exp: Exp RELOP Exp]
            operand t1 = allocTemp();
            operand t2 = allocTemp();
            interCode* t1_code = translateExp(exp->childs[0], &t1);
            interCode* t2_code = translateExp(exp->childs[2], &t2);
            t1_code = mergeCode(t1_code, ensureEADDRInt(&t1));
            t2_code = mergeCode(t2_code, ensureEADDRInt(&t2));
            int op_type = getRelOpType(exp->childs[1]->str);
            interCode* cond_code = newCondCode(op_type, t1, t2, true_label);
            ret = mergeCode(ret, t1_code);
            ret = mergeCode(ret, t2_code);
            ret = mergeCode(ret, cond_code);
            ret = mergeCode(ret, newGotoCode(false_label));
            return ret;
        } else if (exp->childs[0]->token == NOT) {
            // [Exp: NOT Exp]
            return translateCond(exp->childs[1], false_label, true_label);
        } else if (exp->childs[1]->token == AND) {
            // [Exp: Exp AND Exp]
            int next_exp = allocLabel();
            interCode* code1 =
                translateCond(exp->childs[0], next_exp, false_label);
            interCode* code2 =
                translateCond(exp->childs[2], true_label, false_label);
            interCode* label_code = newLabelCode(next_exp);
            ret = mergeCode(ret, code1);
            ret = mergeCode(ret, label_code);
            ret = mergeCode(ret, code2);
            return ret;
        } else if (exp->childs[1]->token == OR) {
            // [Exp: Exp OR Exp]
            int next_exp = allocLabel();
            interCode* code1 =
                translateCond(exp->childs[0], true_label, next_exp);
            interCode* code2 =
                translateCond(exp->childs[2], true_label, false_label);
            interCode* label_code = newLabelCode(next_exp);
            ret = mergeCode(ret, code1);
            ret = mergeCode(ret, label_code);
            ret = mergeCode(ret, code2);
            return ret;
        }
    }
    // if handled, won't reach here
    // unhandled other cases
    operand temp = allocTemp();
    interCode* temp_code = translateExp(exp, &temp);
    temp_code = mergeCode(temp_code, ensureEADDRInt(&temp));
    // temp := eval(exp)
    // if (temp != 0) goto True
    // else goto False
    interCode* cond_code = newCondCode(NE, temp, zeroOpr, true_label);
    interCode* goto_code = newGotoCode(false_label);
    ret = mergeCode(ret, temp_code);
    ret = mergeCode(ret, cond_code);
    ret = mergeCode(ret, goto_code);
    return ret;
}

interCode* arrayAssign(operand dst, operand src) {
    if (IS_EOPR(dst)) return NULL;
    assert(IS_RADDR(dst));
    assert(IS_RADDR(src));
    int width = 4;
    interCode* ret = NULL;
    operand end_dst = allocTemp();
    operand end_src = allocTemp();

    operand ptr_dst = allocTemp();
    operand ptr_src = allocTemp();

    operand base_dst = newOperand(VARIABLE, dst.level->var_id);
    operand base_src = newOperand(VARIABLE, src.level->var_id);
    // int *dst, *src
    int offset_dst = getArraySize(ArrayTable[dst.level->var_id]);
    int offset_src = getArraySize(ArrayTable[src.level->var_id]);

    int op_dst = ADDR;
    int op_src = ADDR;
    if (ParamTable[dst.level->var_id]) op_dst = ADD;
    if (ParamTable[src.level->var_id]) op_src = ADD;

    interCode* cal_end1 =
        newAssignCode(op_dst, end_dst, base_dst, newOperand(CONST, offset_dst));
    interCode* cal_end2 =
        newAssignCode(op_src, end_src, base_src, newOperand(CONST, offset_src));

    if (IS_VAR(dst) && !ParamTable[dst.var_id])
        op_dst = ADDR;
    else
        op_dst = AS;
    if (IS_VAR(src) && !ParamTable[src.var_id])
        op_src = ADDR;
    else
        op_src = AS;

    interCode* cal_ptr1 = newAssignCode(op_dst, ptr_dst, dst, nullOpr);
    interCode* cal_ptr2 = newAssignCode(op_src, ptr_src, src, nullOpr);

    // int *dst_end, *src_end
    int loop = allocLabel();
    int exit = allocLabel();
    // Label Loop :
    interCode* label_loop = newLabelCode(loop);
    // if (dst_end == dst) goto Exit
    interCode* cond1 = newCondCode(EQ, ptr_dst, end_dst, exit);
    // if (src_end == src) goto Exit
    interCode* cond2 = newCondCode(EQ, ptr_src, end_src, exit);
    // *dst = *src;
    interCode* assign = newAssignCode(LRSTAR, ptr_dst, ptr_src, nullOpr);
    // dst ++;
    interCode* inc1 =
        newAssignCode(ADD, ptr_dst, ptr_dst, newOperand(CONST, width));
    // src ++;
    interCode* inc2 =
        newAssignCode(ADD, ptr_src, ptr_src, newOperand(CONST, width));
    // goto Loop
    interCode* goto_loop = newGotoCode(loop);
    // Label Exit :
    interCode* label_exit = newLabelCode(exit);
    ret = mergeCode(ret, cal_end1);
    ret = mergeCode(ret, cal_end2);
    ret = mergeCode(ret, cal_ptr1);
    ret = mergeCode(ret, cal_ptr2);
    ret = mergeCode(ret, label_loop);
    ret = mergeCode(ret, cond1);
    ret = mergeCode(ret, cond2);
    ret = mergeCode(ret, assign);
    ret = mergeCode(ret, inc1);
    ret = mergeCode(ret, inc2);
    ret = mergeCode(ret, goto_loop);
    ret = mergeCode(ret, label_exit);
    return ret;
}

interCode** interCodeGenerate() {
    if (root == NULL) return NULL;

    // initialize
    for (int i = 0; i < MAX_VAR_ID; i++) {
        ArrayTable[i] = NULL;
        ParamTable[i] = false;
    }

    // translate
    treeNode* extdef_list = root->childs[0];
    translateExtDef(extdef_list);

#ifdef _OPT_
    optimize(&FuncTable);
#endif

    interCode** codes =
        (interCode**)malloc(sizeof(interCode*) * (funcCount + 1));
    int ret_idx = 0;
    for (int i = 0; i < funcCount; i++) {
        char buf[32];
        sprintf(buf, "%d", i);

        // FuncOrder <func_id: int, func_name: string>
        map_t* ret = get(&FuncOrder, buf);
        if (ret == NULL) continue;
        char* funcname = ret->val;

#ifdef _OPT_
        map_t* is_inline = get(&InlineTable, funcname);
        if (is_inline != NULL && *(bool*)is_inline->val == true) continue;
#endif

        map_t* func = get(&FuncTable, funcname);
        if (func == NULL) continue;
        interCode* code = (interCode*)func->val;

        codes[ret_idx++] = code;
    }
    codes[ret_idx++] = NULL;

    return codes;
}

void interCodeOutput(FILE* fp, interCode** codes) {
    if (codes == NULL) return;

    for (interCode** code = codes; *code != NULL; code++) {
        interCodeToFile(*code, fp);
    }
}