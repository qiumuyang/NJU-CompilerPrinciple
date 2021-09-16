#ifndef __HEADER_H__
#define __HEADER_H__

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "syntax.tab.h"
#include "tree.h"

extern int yydebug;
extern bool yyperr;
extern int unhandled;
extern int preverr;
extern treeNode* root;

extern const char* errmsgT[];
extern const char* errmsgN[];
extern const char* errmsgB[];
extern const char* errmsgA[];
extern const char* reg_str[];

static const int DebugOn = 0;

#define DEBUG(fmt, ...)                                                        \
    do {                                                                       \
        if (DebugOn) {                                                         \
            printf("[DEBUG] %s(%d) <%s>: ", __FILE__, __LINE__, __FUNCTION__); \
            printf(fmt, ##__VA_ARGS__);                                        \
        }                                                                      \
    } while (0)

enum errT_types { HAS_STRUCTURE = 0, HAS_FLOAT, TOO_MANY_ARGS };

enum errN_types {
    EMPTY = 0,
    UN_VAR,
    UN_FUNC,
    RE_VAR,
    RE_FUNC,
    AS_TYPE,
    AS_RVAL,
    OP_TYPE,
    RET_TYPE,
    INC_ARG,
    NOT_ARR,
    NOT_FUNC,
    NOT_INT,
    NOT_STRUCT,
    NON_FIELD,
    RE_FIELD,
    RE_STRUCT,
    UN_STRUCT,
    INV_STRUCT
};

enum non_terminal_types {
    Program = 1024,
    ExtDefList = 1025,
    ExtDef = 1026,
    ExtDecList = 1027,
    Specifier = 1028,
    StructSpecifier = 1029,
    OptTag = 1030,
    Tag = 1031,
    VarDec = 1032,
    FunDec = 1033,
    VarList = 1034,
    ParamDec = 1035,
    CompSt = 1036,
    StmtList = 1037,
    Stmt = 1038,
    DefList = 1039,
    Def = 1040,
    DecList = 1041,
    Dec = 1042,
    Exp = 1043,
    Args = 1044
};

enum errB_types {
    SYN_ERR = 0,
    MS_SEMI,
    MS_CM,
    MS_LP,
    MS_RP,
    MS_LB,
    MS_RB,
    MS_LC,
    MS_RC,
    MS_SP,
    MS_ST_IF,
    MS_ST_WH,
    INV_EXP,
    INV_IDX,
    INV_DEF,
    INV_ARG,
    INV_ST,
    UE_DEC,
    UE_AS
};

enum errA_types { INV_CHR = 0, INV_ID, NOT_IMP, INV_FL };

#endif