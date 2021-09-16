#include "semantic.h"

const static int INIT_EXP_LEN = 1024;
static unsigned long long unamed_struct = 0;
// count unamed_structs in order that each of them can get a unique name
// similarly, we have var 'dup_param' in parseVarList

// TODO: write semantic analysis here
type* parseSpecifier(treeNode* spec);
type* parseStructSpecifier(treeNode* spec);
type* parseExp(treeNode* exp);
type* parseVarDec(treeNode* vardec, type* this, char* varname);
void parseExtDef(treeNode* extdeflist);
void parseExtDec(treeNode* list, type* this);
void parseExtFunc(treeNode* extdef);
void parseExtSpec(treeNode* spec);
void parseExtStructSpec(treeNode* spec);
void parseDef(treeNode* list, type* parent);
void parseDec(treeNode* list, type* this, type* parent);
void parseFuncDec(treeNode* fundec, type* retval);
void parseVarList(treeNode* varlist, type* parent);
void parseCompSt(treeNode* compst, type* retval);
void parseStmtList(treeNode* list, type* retval);
void parseStmt(treeNode* list, type* retval);
bool checkArgs(treeNode* arglist, type* func);
bool isLeftValue(treeNode* exp);
char* showExp(treeNode* exp);
static void _showExpR(treeNode* exp, char** buffer, int* len, int* maxLen);

void semError(int errid, int lineno, const char* msg) {
    fprintf(stderr, "Error type %d at Line %d: ", errid, lineno);
    if (msg) {
        fprintf(stderr, errmsgN[errid], msg);
    } else {
        fprintf(stderr, errmsgN[errid]);
    }
    fprintf(stderr, ".\n");
}

void expError(int errid, int lineno, treeNode* exp) {
    if (exp == NULL) {
        semError(errid, lineno, NULL);
    } else {
        char* msg = showExp(exp);
        semError(errid, lineno, msg);
        free(msg);
    }
}

void parseExtDef(treeNode* extdeflist) {
    if (extdeflist->childs == NULL) return;
    treeNode* extdef = extdeflist->childs[0];
    treeNode* nextlist = extdeflist->childs[1];

    treeNode* spec = extdef->childs[0];
    type* vartype;
    switch (extdef->childs[1]->token) {
        case SEMI:
            // [ExtDef: Specifier SEMI]
            parseExtSpec(spec);
            break;
        case FunDec:
            // [ExtDef: Specifier FunDec CompSt]
            parseExtFunc(extdef);
            break;
        case ExtDecList:
            vartype = parseSpecifier(spec);
            // [ExtDef: Specifier ExtDecList SEMI]
            if (vartype != NULL) parseExtDec(extdef->childs[1], vartype);
            freeType(vartype);
            break;
        default:
            assert(0);
    }
    parseExtDef(nextlist);
}

type* parseSpecifier(treeNode* spec) {
    // Note: free retval after use
    treeNode* child = spec->childs[0];
    // [Specifier: TYPE | StructSpecifier]
    switch (child->token) {
        case TYPE:
            if (strcmp(child->str, "int") == 0)
                return newType(IntType);
            else
                return newType(FloatType);
        case StructSpecifier:
            return parseStructSpecifier(child);
        default:
            assert(0);
    }
}

void parseExtSpec(treeNode* spec) {
    // Special for [ExtDef: Specifier SEMI]
    treeNode* child = spec->childs[0];
    // [Specifier: TYPE | StructSpecifier]
    switch (child->token) {
        case TYPE:
            return;
        case StructSpecifier:
            parseExtStructSpec(child);
            return;
        default:
            assert(0);
    }
}

type* parseStructSpecifier(treeNode* spec) {
    treeNode* tag = spec->childs[1];
    if (tag->token == Tag) {
        // [StructSpecifier: STRUCT Tag]
        const char* structname = tag->childs[0]->str;
        type* t = tableFind(structname);
        if (t == NULL || t->typeId == StructPlaceHolder) {
            semError(UN_STRUCT, tag->lineNum, structname);
            return NULL;
        }
        if (t->typeId != StructType || !isStructDefinition(t, structname)) {
            // find an item but it's not a struct definition
            semError(NOT_STRUCT, tag->lineNum, structname);
            return NULL;
        }
        return copyType(t);
    }
    // [StructSpecifier: STRUCT OptTag LC DefList RC]
    // [OptTag]
    char structname[32];
    type* placeholder = newType(StructPlaceHolder);
    if (tag->childs == NULL) {
        // OptTag -> epsilon
        sprintf(structname, "<uname-%llx>", unamed_struct);
        unamed_struct++;
    } else {
        strcpy(structname, tag->childs[0]->str);
        if (isDupVarName(structname)) {
            // duplicated
            semError(RE_STRUCT, tag->childs[0]->lineNum, structname);
            return NULL;
        }
        // insert placeholder first
        // replace with real type later
        tableInsert(structname, placeholder);
    }
    // alloc return value
    type* ret = newType(StructType);
    strcpy(ret->structure.name, structname);
    // [DefList]
    treeNode* deflist = spec->childs[3];
    parseDef(deflist, ret);
    // add to symbol table if ret has a name
    if (tag->childs != NULL) {
        // OptTag is not empty
        tableInsert(structname, copyType(ret));
    }
    freeType(placeholder);
    return ret;
}

void parseExtStructSpec(treeNode* spec) {
    treeNode* tag = spec->childs[1];
    if (tag->token == Tag) {
        // [StructSpecifier: STRUCT Tag]
        // Not consider this case
        return;
    }
    // [StructSpecifier: STRUCT OptTag LC DefList RC]
    // [OptTag]
    char structname[32];
    type* placeholder = newType(StructPlaceHolder);
    if (tag->childs == NULL) {
        // OptTag -> epsilon
        sprintf(structname, "<uname-%llx>", unamed_struct);
        unamed_struct++;
    } else {
        strcpy(structname, tag->childs[0]->str);
        if (isDupVarName(structname)) {
            // duplicated
            semError(RE_STRUCT, tag->childs[0]->lineNum, structname);
            return;
        }
        // insert placeholder first
        // replace with real type later
        tableInsert(structname, placeholder);
    }
    // alloc return value
    type* ret = newType(StructType);
    strcpy(ret->structure.name, structname);
    // [DefList]
    treeNode* deflist = spec->childs[3];
    parseDef(deflist, ret);
    // add to symbol table if ret has a name
    if (tag->childs != NULL) {
        // OptTag is not empty
        tableInsert(structname, copyType(ret));
    }
    freeType(placeholder);
    freeType(ret);
    return;
}

void parseDef(treeNode* list, type* parent) {
    // add to symbol table if parent is null
    // otherwise, insert as field and disallow assignment
    if (list->childs == NULL) return;
    treeNode* def = list->childs[0];
    treeNode* nextlist = list->childs[1];

    // parse Def here
    // [Def: Specifier DecList SEMI]
    treeNode* spec = def->childs[0];
    treeNode* declist = def->childs[1];
    // [Specifier]
    type* t = parseSpecifier(spec);
    // [DecList]
    if (t != NULL) {
        parseDec(declist, t, parent);
    }
    freeType(t);

    // [DefList: Def DefList]
    parseDef(nextlist, parent);
}

void parseExtDec(treeNode* list, type* this) {
    // [ExtDecList: VarDec | VarDec COMMA ExtDecList]
    char varname[32];
    type* vartype = parseVarDec(list->childs[0], this, varname);
    if (vartype) {
        if (isDupVarName(varname)) {
            semError(RE_VAR, list->childs[0]->lineNum, list->childs[0]->str);
        } else {
            tableInsert(varname, copyType(vartype));
        }
    }
    freeType(vartype);
    if (list->childCnt == 3) {
        parseExtDec(list->childs[2], this);
    }
}

void parseDec(treeNode* list, type* this, type* parent) {
    // list: DecList Node
    // this: type of vars declared in list
    // parent: insert to symtab / struct field
    treeNode* dec = list->childs[0];
    treeNode* vardec = dec->childs[0];
    // parse Dec here
    // [DecList: Dec]
    char varname[32];
    type* vartype = parseVarDec(vardec, this, varname);
    if (dec->childCnt == 1) {
        // [Dec: VarDec]
        if (parent != NULL) {
            // struct : insert into struct field
            if (!insertField(parent, newField(varname, vartype))) {
                // duplicate names in structure field
                semError(RE_FIELD, dec->childs[0]->lineNum, varname);
            }
        } else {
            // insert into symtab
            // need to check whether is the same name with structure
            if (isDupVarName(varname)) {
                semError(RE_VAR, dec->childs[0]->lineNum, varname);
            } else {
                tableInsert(varname, copyType(vartype));
            }
        }
    } else {
        // dec->childCnt == 3
        // [Dec: VarDec ASSIGNOP Exp]
        if (parent != NULL) {
            // assign in structure
            semError(RE_FIELD, dec->childs[1]->lineNum, varname);
            // still try to insert this field despite error
            insertField(parent, newField(varname, vartype));
        } else {
            // insert into symtab
            if (isDupVarName(varname)) {
                semError(RE_VAR, dec->childs[0]->lineNum, varname);
            } else {
                tableInsert(varname, copyType(vartype));
            }

            type* exptype = parseExp(dec->childs[2]);
            if (exptype && !typeEqual(exptype, vartype))
                semError(AS_TYPE, dec->childs[1]->lineNum, NULL);

            freeType(exptype);
        }
    }
    freeType(vartype);

    // [DecList: Dec COMMA DecList]
    if (list->childCnt == 3) parseDec(list->childs[2], this, parent);
}

type* parseVarDec(treeNode* vardec, type* this, char* varname) {
    if (vardec->childCnt == 1) {
        // [VarDec: ID]
        strcpy(varname, vardec->childs[0]->str);
        return copyType(this);
    }
    // [VarDec: VarDec LB INT RB]
    type* arr = newType(ArrayType);
    arr->array.itemtype = parseVarDec(vardec->childs[0], this, varname);
    arr->array.size = atoi(vardec->childs[2]->str);
    return arr;
}

void parseExtFunc(treeNode* extdef) {
    treeNode* spec = extdef->childs[0];
    treeNode* fundec = extdef->childs[1];
    treeNode* compst = extdef->childs[2];

    type* retval = parseSpecifier(spec);
    // FuncDec will create a new scope
    parseFuncDec(fundec, retval);
    // NOT call parseCompst here
    // Need to add param into new scope
    parseDef(compst->childs[1], NULL);
    parseStmtList(compst->childs[2], retval);

    freeType(retval);

    delScope();  // del the scope created by FuncDec, kind of weired
}

void parseFuncDec(treeNode* fundec, type* retval) {
    // Add this function to global symbol table
    // Add param to inner symbol table
    // No matter whether there're errs in next Comptst
    // [FunDec: ID LP VarList RP]
    bool isValid = false;
    char funcname[32];
    strcpy(funcname, fundec->childs[0]->str);
    type* fun = newType(FuncType);
    fun->func.retval = copyType(retval);
    if (isDupVarName(funcname))
        semError(RE_FUNC, fundec->childs[0]->lineNum, funcname);
    else {
        isValid = true;
        tableInsert(funcname, fun);
        // Note: pointer 'fun' will be modified afterwards
        //       shouldn't make a copy here
    }
    // call newScope after the function was inserted
    // then insert params, weired
    newScope();
    if (fundec->childs[2]->token == VarList) {
        parseVarList(fundec->childs[2], fun);
    }
    if (!isValid) {
        // func can be freed only when not inserted
        freeType(fun);
    }
}

void parseVarList(treeNode* varlist, type* parent) {
    // [VarList: ParamDec COMMA VarList]
    // [ParamDec: Specifier VarDec]
    treeNode* paramnode = NULL;
    char paramname[32];
    unsigned int dup_param = 0;
    while (true) {
        paramname[0] = 0;
        paramnode = varlist->childs[0];
        type* spec = parseSpecifier(paramnode->childs[0]);
        if (spec != NULL) {
            type* vartype = parseVarDec(paramnode->childs[1], spec, paramname);
            if (vartype != NULL) {
                if (isDupVarName(paramname)) {
                    // duplicate name, give it a new name
                    semError(RE_VAR, paramnode->childs[1]->lineNum, paramname);
                    sprintf(paramname, "<dup-%x>", dup_param);
                    dup_param++;
                }
                tableInsert(paramname, copyType(vartype));
                if (!insertArg(parent, newField(paramname, vartype)))
                    DEBUG("failed to insert param\n");
            }
            freeType(spec);
            freeType(vartype);
        }
        if (varlist->childCnt == 3)
            varlist = varlist->childs[2];
        else
            break;
    }
}

void parseCompSt(treeNode* compst, type* retval) {
    // [CompSt: LC DefList StmtList RC]
    newScope();
    parseDef(compst->childs[1], NULL);
    parseStmtList(compst->childs[2], retval);
    delScope();
}

void parseStmtList(treeNode* list, type* retval) {
    // [StmtList: Stmt StmtList]
    if (list->childCnt == 0) return;
    treeNode* stmt = list->childs[0];
    parseStmt(stmt, retval);
    parseStmtList(list->childs[1], retval);
}

void parseStmt(treeNode* stmt, type* retval) {
    type* tmp = NULL;
    switch (stmt->childs[0]->token) {
        case Exp:
            parseExp(stmt->childs[0]);
            break;
        case CompSt:
            parseCompSt(stmt->childs[0], retval);
            break;
        case RETURN:
            tmp = parseExp(stmt->childs[1]);
            // if exp failed, don't report return type
            if (tmp && !typeEqual(tmp, retval))
                semError(RET_TYPE, stmt->childs[1]->lineNum, NULL);
            break;
        case IF:
        case WHILE:
            tmp = parseExp(stmt->childs[2]);
            if (tmp && tmp->typeId != IntType)
                expError(NOT_INT, stmt->childs[2]->lineNum, stmt->childs[2]);
            parseStmt(stmt->childs[4], retval);
            if (stmt->childCnt == 7) parseStmt(stmt->childs[6], retval);
            break;
        default:
            DEBUG("CurrentId: %d TokenId: %d at Line %d\nChildren:\n",
                  stmt->token, stmt->childs[0]->token,
                  stmt->childs[0]->lineNum);
            for (int i = 0; i < stmt->childCnt; i++)
                DEBUG("  [%d] %d\n", i + 1, stmt->childs[i]->token);
            assert(0);
    }
    freeType(tmp);
}

type* parseExp(treeNode* exp) {
    type *opr1 = NULL, *opr2 = NULL;
    char* varname = NULL;

    switch (exp->childs[0]->token) {
        case ID:
            // [Exp: ID | ID LP Args RP | ID LP RP]
            varname = exp->childs[0]->str;
            opr1 = tableFind(varname);

            if (exp->childCnt == 1) {
                if (opr1 == NULL) {
                    // undefined variable
                    semError(UN_VAR, exp->childs[0]->lineNum, varname);
                    return NULL;
                }
                // struct A a; a = A;
                // ensure ID is not a name of struct definition
                if (opr1 && opr1->typeId == StructType &&
                    isStructDefinition(opr1, varname)) {
                    semError(INV_STRUCT, exp->childs[0]->lineNum, varname);
                    return NULL;
                }
                // from symbol table, copy
                return copyType(opr1);
            } else {
                if (opr1 == NULL) {
                    // undefined function
                    semError(UN_FUNC, exp->childs[0]->lineNum, varname);
                    return NULL;
                }
                // check function call here
                if (opr1->typeId != FuncType) {
                    semError(NOT_FUNC, exp->childs[0]->lineNum, varname);
                    return NULL;
                }
                treeNode* arglist = NULL;  // NULL stands for no arguments
                // [Exp: ID LP Args RP]
                if (exp->childCnt == 4) arglist = exp->childs[2];
                if (!checkArgs(arglist, opr1)) {
                    // incompatible arguments
                    char* func = typeToString(opr1, varname);
                    semError(INC_ARG, exp->childs[0]->lineNum, func);
                    free(func);
                }
                // return-value's type
                return copyType(opr1->func.retval);
            }
        case INT:
            // [Exp: INT]
            return newType(IntType);
        case FLOAT:
            // [Exp: FLOAT]
            return newType(FloatType);
        case LP:
            // [Exp: LP Exp RP]
            return parseExp(exp->childs[1]);
        case MINUS:
            // [Exp: MINUS Exp]
            opr1 = parseExp(exp->childs[1]);
            if (opr1 == NULL)
                return NULL;  // has failed before
            else if (opr1->typeId == IntType || opr1->typeId == FloatType) {
                return opr1;  // needn't copy here
            } else {
                // free type when it can no more be used
                freeType(opr1);
                semError(OP_TYPE, exp->childs[1]->lineNum, NULL);
                return NULL;
            }
        case NOT:
            // [Exp: NOT Exp]
            opr1 = parseExp(exp->childs[1]);
            if (opr1 == NULL)
                return NULL;  // has failed before
            else if (opr1->typeId == IntType) {
                return opr1;  // needn't copy here
            } else {
                // free type when it can no more be used
                freeType(opr1);
                semError(OP_TYPE, exp->childs[1]->lineNum, NULL);
                return NULL;
            }
        default:  // i.e. production startswith [Exp]
            assert(exp->childs[0]->token == Exp);
            assert(exp->childCnt >= 3);
            break;
    }
    opr1 = parseExp(exp->childs[0]);
    // if opr1 fails, don't check the rest of exp
    if (opr1 == NULL) return NULL;
    assert(opr1);
    if (exp->childs[2]->token == ID) {
        // Special Case
        // [Exp: Exp DOT ID]
        if (opr1 == NULL) return NULL;
        if (opr1->typeId != StructType) {
            freeType(opr1);
            expError(NOT_STRUCT, exp->childs[0]->lineNum, exp->childs[0]);
            return NULL;
        } else {
            char* idname = exp->childs[2]->str;
            type* ret = findField(opr1, idname);
            if (ret == NULL) {
                freeType(opr1);
                semError(NON_FIELD, exp->childs[2]->lineNum, idname);
                return NULL;
            }
            return copyType(ret);
        }
    }
    // [Exp: Exp <Operator> Exp]
    opr2 = parseExp(exp->childs[2]);
    // if opr2 fails, don't check the rest of exp
    if (opr2 == NULL) return NULL;
    assert(opr2);
    switch (exp->childs[1]->token) {
        // Assignment
        case ASSIGNOP:
            if (!typeEqual(opr1, opr2)) {
                freeType(opr1);
                freeType(opr2);
                semError(AS_TYPE, exp->childs[2]->lineNum, NULL);
                return NULL;
            } else {
                if (!isLeftValue(exp->childs[0])) {
                    freeType(opr1);
                    freeType(opr2);
                    semError(AS_RVAL, exp->childs[0]->lineNum, NULL);
                    return NULL;
                }
                freeType(opr2);
                return opr1;
            }
        // Logic operation
        case AND:
        case OR:
            // only int can do logic operation
            if (opr1->typeId != opr2->typeId || opr1->typeId != IntType) {
                freeType(opr1);
                freeType(opr2);
                semError(OP_TYPE, exp->childs[1]->lineNum, NULL);
                return NULL;
            }
            freeType(opr2);
            return opr1;
        // Arithmetic operation
        case PLUS:
        case MINUS:
        case STAR:
        case DIV:
            // only int & float can do arithmetic operation
            if (opr1->typeId != opr2->typeId ||
                (opr1->typeId != IntType && opr1->typeId != FloatType)) {
                freeType(opr1);
                freeType(opr2);
                semError(OP_TYPE, exp->childs[1]->lineNum, NULL);
                return NULL;
            }
            freeType(opr2);
            return opr1;
        case RELOP:
            // only int & float can do arithmetic operation
            if (opr1->typeId != opr2->typeId ||
                (opr1->typeId != IntType && opr1->typeId != FloatType)) {
                freeType(opr1);
                freeType(opr2);
                semError(OP_TYPE, exp->childs[1]->lineNum, NULL);
                return NULL;
            }
            freeType(opr1);
            freeType(opr2);
            return newType(IntType);  // Here Must Return Int Type, Failed D-3
        // Array index
        case LB:
            // array index, return array[index]'s type
            if (opr1->typeId != ArrayType) {
                freeType(opr1);
                freeType(opr2);
                expError(NOT_ARR, exp->childs[0]->lineNum, exp->childs[0]);
                return NULL;
            } else if (opr2->typeId != IntType) {
                freeType(opr1);
                freeType(opr2);
                expError(NOT_INT, exp->childs[2]->lineNum, exp->childs[2]);
                return NULL;
            }
            if (!opr1) return NULL;
            type* ret = copyType(opr1->array.itemtype);
            freeType(opr1);
            freeType(opr2);
            return ret;
        default:
            assert(0);
    }
}

bool checkArgs(treeNode* arglist, type* func) {
    assert(func != NULL);
    assert(func->typeId == FuncType);
    if (arglist == NULL && func->func.args == NULL) return true;
    // [Args: Exp COMMA Args | Exp]
    treeNode* node = arglist;
    field* fp = func->func.args;
    type* arg = NULL;
    while (node && fp) {
        arg = parseExp(node->childs[0]);
        if (!typeEqual(arg, fp->fieldtype)) {
            freeType(arg);
            return false;
        }
        freeType(arg);
        if (node->childCnt == 1)
            node = NULL;
        else
            node = node->childs[2];
        fp = fp->next;
    }
    return (fp == NULL) && (node == NULL);
}

bool isLeftValue(treeNode* exp) {
    // only [ID] [Exp LB Exp RB] [Exp DOT ID] can be leftValue
    if (exp == NULL || exp->token != Exp) {
        // not Exp or is null
        return false;
    }
    if (exp->childCnt == 1 && exp->childs[0]->token == ID) {
        // [ID]
        return true;
    } else if (exp->childCnt == 4 && exp->childs[1]->token == LB) {
        // [Exp LB Exp RB]
        return isLeftValue(exp->childs[0]);
    } else if (exp->childCnt == 3 && exp->childs[1]->token == DOT) {
        //[Exp DOT ID]
        return isLeftValue(exp->childs[0]);
    }
    return false;
}

char* showExp(treeNode* exp) {
    int len = 0;
    int maxLen = INIT_EXP_LEN;
    char* buffer = (char*)malloc(sizeof(char) * (maxLen));
    buffer[0] = 0;
    _showExpR(exp, &buffer, &len, &maxLen);
    return buffer;
}

static void _showExpR(treeNode* exp, char** buffer, int* len, int* maxLen) {
    if (exp == NULL) return;
    for (int i = 0; i < exp->childCnt; i++) {
        treeNode* child = exp->childs[i];
        _showExpR(child, buffer, len, maxLen);
    }
    int curLen = *len;
    if (exp->tokenType == LexicalType) {
        if (curLen + 50 > (*maxLen)) {
            char* newBuffer = (char*)malloc(sizeof(char) * (*maxLen) * 2);
            strcpy(newBuffer, *buffer);
            free(*buffer);
            *buffer = newBuffer;
            *maxLen = (*maxLen) * 2;
        }
        strcpy(*buffer + curLen, exp->str);
        int idx = 0;
        while (exp->str[idx] != 0) idx++;
        curLen += idx;
    }
    *len = curLen;
}

void semantic() {
    treeNode* extdeflist = root->childs[0];
    newScope();  // global scope
    parseExtDef(extdeflist);
}