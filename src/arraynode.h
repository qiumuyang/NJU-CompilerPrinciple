#ifndef __ARRAYNODE_H__
#define __ARRAYNODE_H__

#include <stdbool.h>
#include <stdio.h>

// use List to represent nested array
// e.g.  int v1[5][10] can be represented by
//          Node(1, 5, 40) -> Node(1, 10, 4)

typedef struct _arrayNode {
    int var_id;  // record this node belongs to which array-variable
    int size;    // count of elements of this dimension
    int width;   // width of this dimension
    struct _arrayNode* prev;
    struct _arrayNode* next;
} arrayNode;

arrayNode* newArrayNode(int size);
arrayNode* insertArrayNode(arrayNode* head, arrayNode* node);
arrayNode* getArrayNodeTail(arrayNode* node);
arrayNode* getArrayNodeHead(arrayNode* node);
bool isArrayNodeTail(arrayNode* node);
void setArrayNodeVarId(arrayNode* head, int id);
int getArraySize(arrayNode* head);

// for test purpose
void printArrayNode(arrayNode* node);

#endif