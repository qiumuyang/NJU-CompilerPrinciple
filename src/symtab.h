#ifndef __SYMTAB_H__
#define __SYMTAB_H__

#include "header.h"
#include "map.h"
#include "type.h"

typedef struct _stb {
    root_t table;
    struct _stb* parent;
} symtab;

symtab* newScope();
symtab* delScope();
type* tableFind(const char* name);
type* tableFindInScope(const char* name);
int tableInsert(const char* name, type* t);
bool isStructDefinition(type* s, const char* structname);
bool isDupVarName(const char* varname);

#endif