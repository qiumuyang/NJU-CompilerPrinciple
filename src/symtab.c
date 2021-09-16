#include "symtab.h"

static symtab* head = NULL;

// created for map to free key-value pair when not used
void freeValue(void* ptr) { freeType((type*)ptr); }

symtab* newScope() {
    symtab* ret = (symtab*)malloc(sizeof(symtab));
    ret->table = RB_ROOT;
    ret->parent = head;
    head = ret;
    return ret;
}

symtab* delScope() {
    assert(head);
    symtab* del = head;
    head = head->parent;
    DEBUG("in delScope()\n");
    freeMap(&(del->table), freeValue);
    return head;
}

type* tableFind(const char* name) {
    symtab* tab = head;
    map_t* res = NULL;
    while (tab) {
        res = get(&(tab->table), name);
        if (res == NULL) {
            // 'name' not found in this scope
            // search it in parent-scope
            tab = tab->parent;
        } else {
            return (type*)res->val;
        }
    }
    return NULL;
}

type* tableFindInScope(const char* name) {
    // not recursively
    symtab* tab = head;
    map_t* res = get(&(tab->table), name);
    if (res == NULL) {
        // 'name' not found in this scope
        return NULL;
    } else {
        return (type*)res->val;
    }
}

int tableInsert(const char* name, type* t) {
    assert(head);
    return put(&(head->table), name, t);
}

bool isStructDefinition(type* s, const char* structname) {
    assert(structname);
    return s->typeId == StructType &&
           strcmp(structname, s->structure.name) == 0;
}

bool isDupVarName(const char* varname) {
    // find in current scope
    if (tableFindInScope(varname) != NULL) return true;

    // find definition of structure
    type* var_in_global = tableFind(varname);
    if (var_in_global != NULL && isStructDefinition(var_in_global, varname))
        return true;
    return false;
}