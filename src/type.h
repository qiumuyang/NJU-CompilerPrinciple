#ifndef __TYPE_H__
#define __TYPE_H__

#include <stdbool.h>

enum {
    IntType = 1,
    FloatType = 2,
    StructType = 3,
    ArrayType = 4,
    FuncType = 5,
    StructPlaceHolder = 6
    // TODO: add new type to replace NULL-ptr
};

struct _field;
struct _type;

struct _field {
    char* name;               // name of this field
    struct _type* fieldtype;  // type of this field
    struct _field* next;      // next field
};

struct _type {
    int typeId;  // kind of type
    union {
        struct {
            struct _type* itemtype;  // type of array item
            int size;                // size of array
        } array;
        struct {
            char* name;             // name of struct
                                    // (Definition's name, can be anonymous)
            struct _field* fields;  // fields of struct
        } structure;
        struct {
            struct _field* args;   // params of function
            struct _type* retval;  // type of function's return value
        } func;
    };
};

typedef struct _field field;
typedef struct _type type;

type* newType(int typeId);
field* newField(const char* name, type* type);
bool insertField(type* t, field* f);
bool insertArg(type* t, field* a);
void freeType(type* t);
void freeField(field* f);
void printType(type* t);    // only for test
void printField(field* f);  // only for test
type* copyType(type* t);
field* copyField(field* f);
bool typeEqual(type* t1, type* t2);
bool fieldEqual(field* f1, field* f2);
type* findField(type* t, const char* fieldname);
char* typeToString(type* t, const char* varname);

#endif