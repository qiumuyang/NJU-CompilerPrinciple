%{
    #include "lex.yy.c"
    #include "tree.h"
    treeNode* root = NULL;
    bool yyperr = false;
    int unhandled = 0;
    int preverr = -1;

    #define product(id, location, count, ...) __product(id, #id, location, count, ##__VA_ARGS__)

    treeNode *__product(int id, const char* cid, YYLTYPE loc, int cnt, ...) {
        // alloc parent node & insert child into it
        // param id: nonterminal's name
        // param loc: to get line number
        // param cnt: the number of the following params (i.e. childs count)
        treeNode *ret = newNode(id, cid, SyntacticType, loc.first_line);
        va_list args;
	    va_start(args, cnt);
	    for (int i = 0; i < cnt; i++) {
            treeNode *arg = va_arg(args, treeNode *);
		    treeInsert(ret, arg);
	    }
	    va_end(args);
        return ret;
    }
    // Duty: 1. print detail error message
    //       2. set error flag
    //       3. bison related operation
    #define onError(i) do {  \
        if (unhandled > 0) { \
            fprintf(stderr, "%s.\n", errmsgB[i]); \
            yyperr = true;   \
            yyerrok;         \
            unhandled--;     \
        } \
    } while (0)
%}
%locations
%token SEMI COMMA TYPE
%token LC RC STRUCT RETURN IF ELSE WHILE INT FLOAT ID
%right ASSIGNOP
%left OR
%left AND
%left RELOP
%left PLUS MINUS
%left STAR DIV
%right NOT
%left LP RP LB RB DOT
%%
Program: ExtDefList { $$ = product(Program, @$, 1, $1); root = $$; }
    ;
ExtDefList: /* empty */ { $$ = product(ExtDefList, @$, 0); }
    | ExtDef ExtDefList { $$ = product(ExtDefList, @$, 2, $1, $2); }
    ;
ExtDef: Specifier ExtDecList SEMI { $$ = product(ExtDef, @$, 3, $1, $2, $3); }
    | Specifier SEMI { $$ = product(ExtDef, @$, 2, $1, $2); }
    | error SEMI { onError(SYN_ERR); }
    | Specifier FunDec CompSt { $$ = product(ExtDef, @$, 3, $1, $2, $3); }
    ;
ExtDecList: VarDec { $$ = product(ExtDecList, @$, 1, $1); }
    | VarDec COMMA ExtDecList { $$ = product(ExtDecList, @$, 3, $1, $2, $3); }
    ;
Specifier: TYPE { $$ = product(Specifier, @$, 1, $1); }
    | StructSpecifier { $$ = product(Specifier, @$, 1, $1); }
    ;
StructSpecifier: STRUCT OptTag LC DefList RC { $$ = product(StructSpecifier, @$, 5, $1, $2, $3, $4, $5); }
    | STRUCT Tag { $$ = product(StructSpecifier, @$, 2, $1, $2); }
    ;
OptTag: /* empty */ { $$ = product(OptTag, @$, 0); }
    | ID { $$ = product(OptTag, @$, 1, $1); }
    ;
Tag: ID { $$ = product(Tag, @$, 1, $1); }
    ;
VarDec: ID { $$ = product(VarDec, @$, 1, $1); }
    | VarDec LB INT RB { $$ = product(VarDec, @$, 4, $1, $2, $3, $4); }
    ;
FunDec: ID LP VarList RP { $$ = product(FunDec, @$, 4, $1, $2, $3, $4); }
    | ID LP RP { $$ = product(FunDec, @$, 3, $1, $2, $3); }
    ;
VarList: ParamDec COMMA VarList { $$ = product(VarList, @$, 3, $1, $2, $3); }
    | ParamDec { $$ = product(VarList, @$, 1, $1); }
    ;
ParamDec: Specifier VarDec { $$ = product(ParamDec, @$, 2, $1, $2); }
    ;
CompSt: LC DefList StmtList RC { $$ = product(CompSt, @$, 4, $1, $2, $3, $4); }
    ;
StmtList: /* empty */ { $$ = product(StmtList, @$, 0); }
    | Stmt StmtList { $$ = product(StmtList, @$, 2, $1, $2); }
    ;
Stmt: Exp SEMI { $$ = product(Stmt, @$, 2, $1, $2); }
    | CompSt { $$ = product(Stmt, @$, 1, $1); }
    | RETURN Exp SEMI { $$ = product(Stmt, @$, 3, $1, $2, $3); }
    | IF LP Exp RP Stmt { $$ = product(Stmt, @$, 5, $1, $2, $3, $4, $5); }
    | IF LP Exp RP Stmt ELSE Stmt { $$ = product(Stmt, @$, 7, $1, $2, $3, $4, $5, $6, $7); }
    | WHILE LP Exp RP Stmt { $$ = product(Stmt, @$, 5, $1, $2, $3, $4, $5); }
    | error SEMI { onError(INV_ST); }
    ;
DefList: /* empty */ { $$ = product(DefList, @$, 0); }
    | Def DefList { $$ = product(DefList, @$, 2, $1, $2); }
    ;
Def: Specifier DecList SEMI { $$ = product(Def, @$, 3, $1, $2, $3); }
    | error SEMI { onError(INV_DEF); }
    ;
DecList: Dec { $$ = product(DecList, @$, 1, $1); }
    | Dec COMMA DecList { $$ = product(DecList, @$, 3, $1, $2, $3); }
    ;
Dec: VarDec { $$ = product(Dec, @$, 1, $1); }
    | VarDec ASSIGNOP Exp { $$ = product(Dec, @$, 3, $1, $2, $3); }
    ;
Exp: Exp ASSIGNOP Exp { $$ = product(Exp, @$, 3, $1, $2, $3); }
    | Exp AND Exp { $$ = product(Exp, @$, 3, $1, $2, $3); }
    | Exp OR Exp { $$ = product(Exp, @$, 3, $1, $2, $3); }
    | Exp RELOP Exp { $$ = product(Exp, @$, 3, $1, $2, $3); }
    | Exp PLUS Exp { $$ = product(Exp, @$, 3, $1, $2, $3); }
    | Exp MINUS Exp { $$ = product(Exp, @$, 3, $1, $2, $3); }
    | Exp STAR Exp { $$ = product(Exp, @$, 3, $1, $2, $3); }
    | Exp DIV Exp { $$ = product(Exp, @$, 3, $1, $2, $3); }
    | LP Exp RP { $$ = product(Exp, @$, 3, $1, $2, $3); }
    | MINUS Exp %prec NOT { $$ = product(Exp, @$, 2, $1, $2); }
    | NOT Exp { $$ = product(Exp, @$, 2, $1, $2); }
    | ID LP Args RP { $$ = product(Exp, @$, 4, $1, $2, $3, $4); }
    | ID LP RP { $$ = product(Exp, @$, 3, $1, $2, $3); }
    | Exp LB Exp RB { $$ = product(Exp, @$, 4, $1, $2, $3, $4); }
    | Exp DOT ID { $$ = product(Exp, @$, 3, $1, $2, $3); }
    | ID { $$ = product(Exp, @$, 1, $1); }
    | INT { $$ = product(Exp, @$, 1, $1); }
    | FLOAT { $$ = product(Exp, @$, 1, $1); }
    ;
Args: Exp COMMA Args { $$ = product(Args, @$, 3, $1, $2, $3); }
    | Exp { $$ = product(Args, @$, 1, $1); }
    ;
%%
yyerror() {
    if (unhandled > 0) {
        fprintf(stderr, "%s.\n", errmsgB[SYN_ERR]);
        unhandled--;
    }
    if (yylloc.first_line != preverr) {
        fprintf(stderr, "Error type B at Line %d: ", yylloc.first_line);
        unhandled++;
        preverr = yylloc.first_line;
    }
}