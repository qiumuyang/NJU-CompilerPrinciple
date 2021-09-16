#ifndef __BLOCK_H__
#define __BLOCK_H__

#include <stdbool.h>
#include <stdio.h>

#include "intercode.h"
#include "map.h"

struct _genNode;
struct _outNode;

extern bool DO_GLOBAL_REMOVE;

// block contains a fragment of intercodes
typedef struct _block {
    int id;
    interCode* first;  // start pointer
    interCode* end;    // end pointer
    int gotoId;        // which label this block goto
    struct _block* next;
    struct _block* prev;

    struct _block* flowSeqNext;
    struct _block* flowGotoNext;

    struct _block* flowPrev[2];

    struct _genNode* gen;
    struct _outNode* out;

    root_t genMap;
    root_t outMap;

    char* useDef;
    // 0->null 1->def 2->use
    char* useIn;

    bool isVisited;
} block;

void initBlock();
bool setOutBlocks(block* b, bool visit);
bool setBlockUseIn(block* b);
block* getBlocks(interCode* codes);
void printBlocks(block* b);
block* removeBlock(block* remove);
void freeBlock(block* to_free);
void freeBlocks(block* b);
block* getFlowGraph(block* entry);
void dfs(block* b, bool visit);

void printFlowGraph(block* b, bool flag);
void bfs(block* b);
void adjacent(block* b);

// codeNode
typedef struct _codeNode {
    interCode* code;
    struct _codeNode* next;
} codeNode;

// def
typedef struct _defNode {
    interCode* code;
    struct _defNode* next;
} defNode;

// use
typedef struct _useNode {
    interCode* code;
    struct _useNode* next;
} useNode;

// gen
typedef struct _genNode {
    interCode* code;
    struct _genNode* next;
} genNode;

// out
typedef struct _outNode {
    interCode* code;
    struct _outNode* next;
} outNode;

extern defNode** defTable;
extern useNode** useTable;

void initDefUse();
void getDefsAndUses(interCode* codes);
void printDefs();
void printUses();

#endif