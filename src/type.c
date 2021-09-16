#include "type.h"

#include <assert.h>

#include "header.h"

static const int INIT_TYPE_LEN = 256;

type* newType(int typeId) {
    type* ret = (type*)malloc(sizeof(type));
    ret->typeId = typeId;
    switch (typeId) {
        case IntType:
        case FloatType:
        case StructPlaceHolder:
            break;
        case ArrayType:
            ret->array.itemtype = NULL;
            ret->array.size = -1;
            break;
        case StructType:
            ret->structure.name = (char*)malloc(sizeof(char) * 32);
            ret->structure.fields = NULL;
            break;
        case FuncType:
            ret->func.args = NULL;
            ret->func.retval = NULL;
            break;
        default:
            assert(0);
    }
    return ret;
}

field* newField(const char* name, type* type) {
    field* ret = (field*)malloc(sizeof(field));

    ret->name = (char*)malloc(sizeof(char) * 32);
    strcpy(ret->name, name);

    ret->fieldtype = copyType(type);

    ret->next = NULL;
    return ret;
}

bool insertField(type* t, field* f) {
    assert(t->typeId == StructType);
    assert(f->next == NULL);
    if (t->structure.fields == NULL) {
        t->structure.fields = f;
    } else {
        field* p = t->structure.fields;
        if (strcmp(f->name, p->name) == 0) return false;
        while (p->next) {
            if (strcmp(f->name, p->next->name) == 0) return false;
            p = p->next;
        }
        p->next = f;
    }
    return true;
}

bool insertArg(type* t, field* a) {
    assert(t->typeId == FuncType);
    assert(a->next == NULL);
    if (t->func.args == NULL) {
        t->func.args = a;
    } else {
        field* p = t->func.args;
        if (strcmp(a->name, p->name) == 0) return false;
        while (p->next) {
            if (strcmp(a->name, p->name) == 0) return false;
            p = p->next;
        }
        p->next = a;
    }
    return true;
}

void freeType(type* t) {
    if (t) {
        switch (t->typeId) {
            case IntType:
            case FloatType:
            case StructPlaceHolder:
                break;
            case ArrayType:
                freeType(t->array.itemtype);
                break;
            case StructType:
                free(t->structure.name);
                freeField(t->structure.fields);
                break;
            case FuncType:
                freeType(t->func.retval);
                freeField(t->func.args);
                break;
            default:
                assert(0);
        }
        free(t);
    }
}

void freeField(field* f) {
    if (f) {
        free(f->name);
        freeType(f->fieldtype);
        freeField(f->next);
        free(f);
    }
}

type* copyType(type* t) {
    if (!t) return NULL;
    type* ret = newType(t->typeId);
    switch (t->typeId) {
        case IntType:
        case FloatType:
        case StructPlaceHolder:
            return ret;
        case ArrayType:
            ret->array.itemtype = copyType(t->array.itemtype);
            ret->array.size = t->array.size;
            break;
        case StructType:
            strcpy(ret->structure.name, t->structure.name);
            ret->structure.fields = copyField(t->structure.fields);
            break;
        case FuncType:
            ret->func.retval = copyType(t->func.retval);
            ret->func.args = copyField(t->func.args);
            break;
        default:
            assert(0);
    }
    return ret;
}

field* copyField(field* f) {
    field *ret = NULL, *tail = NULL, *tmp;
    while (f) {
        tmp = newField(f->name, NULL);
        tmp->fieldtype = copyType(f->fieldtype);
        if (!ret) {
            ret = tmp;
            tail = tmp;
        } else {
            tail->next = tmp;
            tail = tmp;
        }
        f = f->next;
    }
    return ret;
}

type* findField(type* t, const char* fieldname) {
    if (t == NULL || t->typeId != StructType) return NULL;
    field* f = t->structure.fields;
    while (f) {
        if (strcmp(f->name, fieldname) == 0) {
            return f->fieldtype;
        }
        f = f->next;
    }
    return NULL;
}

void printType(type* t) {
    if (t) {
        switch (t->typeId) {
            case IntType:
                printf("int\n");
                break;
            case FloatType:
                printf("float\n");
                break;
            case ArrayType:
                printf("[%d]", t->array.size);
                printType(t->array.itemtype);
                break;
            case StructType:
                printf("<struct>");
                if (strlen(t->structure.name) == 0)
                    printf(" <unamed>\n");
                else
                    printf(" %s\n", t->structure.name);
                printField(t->structure.fields);
                printf("</struct>\n");
                break;
            case FuncType:
                printf("<function>\n");
                printf("<ret> \n");
                printType(t->func.retval);
                printf("<arg> \n");
                printField(t->func.args);
                printf("</function>\n");
                break;
            case StructPlaceHolder:
                break;
            default:
                assert(0);
        }
    }
}

void printField(field* f) {
    if (f) {
        while (f) {
            printf("%s ", f->name);
            printType(f->fieldtype);
            f = f->next;
        }
    } else {
        printf("void\n");
    }
}

static void makeNewBuffer(char** buffer, int* size, int need) {
    if (strlen(*buffer) + need > *size - 1) {
        *size *= 2;
        char* newbuffer = (char*)malloc(sizeof(char) * *size);
        strcpy(newbuffer, *buffer);
        free(*buffer);
        *buffer = newbuffer;
    }
}

static void _typeToString(type* t, char** buffer, int* size,
                          const char* varname) {
    // size: size of buffer
    if (t == NULL) return;
    switch (t->typeId) {
        case IntType:
            makeNewBuffer(buffer, size, 8);
            strcat(*buffer, "int");
            break;
        case FloatType:
            makeNewBuffer(buffer, size, 8);
            strcat(*buffer, "float");
            break;
        case StructType:
            makeNewBuffer(buffer, size, 40);
            strcat(*buffer, "struct ");
            strcat(*buffer, t->structure.name);
            break;
        case ArrayType:
            _typeToString(t->array.itemtype, buffer, size, NULL);
            makeNewBuffer(buffer, size, 8);
            strcat(*buffer, "[]");
            break;
        case FuncType:
            _typeToString(t->func.retval, buffer, size, NULL);
            makeNewBuffer(buffer, size, 40);
            strcat(*buffer, " ");
            if (varname) strcat(*buffer, varname);
            strcat(*buffer, "(");
            for (field* p = t->func.args; p != NULL; p = p->next) {
                if (p != t->func.args) {
                    makeNewBuffer(buffer, size, 8);
                    strcat(*buffer, ", ");
                }
                _typeToString(p->fieldtype, buffer, size, NULL);
            }
            makeNewBuffer(buffer, size, 8);
            strcat(*buffer, ")");
            break;
        case StructPlaceHolder:
            break;
        default:
            assert(0);
    }
}

char* typeToString(type* t, const char* varname) {
    int maxLen = INIT_TYPE_LEN;
    char* buffer = (char*)malloc(sizeof(char) * maxLen);
    buffer[0] = 0;
    _typeToString(t, &buffer, &maxLen, varname);
    return buffer;
}

bool typeEqual(type* t1, type* t2) {
    if (!t1 || !t2) return false;
    if (t1->typeId != t2->typeId) return false;
    switch (t1->typeId) {
        case StructPlaceHolder:
        case IntType:
        case FloatType:
            return true;
        case ArrayType:
            return typeEqual(t1->array.itemtype, t2->array.itemtype);
        case StructType:
            // for other group, need to judge field equal
            return strcmp(t1->structure.name, t2->structure.name) == 0;
        case FuncType:
            return typeEqual(t1->func.retval, t2->func.retval) &&
                   fieldEqual(t1->func.args, t2->func.args);
        default:
            assert(0);
    }
}

bool fieldEqual(field* f1, field* f2) {
    while (f1 && f2) {
        if (!typeEqual(f1->fieldtype, f2->fieldtype)) return false;
        f1 = f1->next;
        f2 = f2->next;
    }
    if (!f1 && !f2) return true;
    return false;
}