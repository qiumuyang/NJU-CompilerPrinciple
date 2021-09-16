#include "arraynode.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static int width = 4;

arrayNode* newArrayNode(int size) {
    arrayNode* node = (arrayNode*)malloc(sizeof(arrayNode));
    node->size = size;
    node->width = width;
    node->prev = node;
    node->next = node;
}

arrayNode* insertArrayNode(arrayNode* head, arrayNode* node) {
    // Insert node BEFORE head
    // return node as the new head
    // use head = insertArrayNode(head, newnode);
    if (head == NULL) return node;
    node->next = head;
    node->prev = head->prev;
    node->width = head->size * head->width;
    head->prev->next = node;
    head->prev = node;
    return node;
}

void setArrayNodeVarId(arrayNode* head, int id) {
    // Caution: MUST use this after DEC an array
    arrayNode* node = head;
    for (; !isArrayNodeTail(node); node = node->next) {
        node->var_id = id;
    }
    //FIX BY JS
    node->var_id = id;
}

inline bool isArrayNodeTail(arrayNode* node) { return node->width == width; }

arrayNode* getArrayNodeTail(arrayNode* node) {
    while (node->width != width)  //
        node = node->next;
    return node;
}

arrayNode* getArrayNodeHead(arrayNode* node) {
    while (node->width < node->next->width || node->width < node->prev->width)
        node = node->next;
    return node;
}

inline int getArraySize(arrayNode* head) { return head->size * head->width; }

void printArrayNode(arrayNode* node) {
    node = getArrayNodeHead(node);
    while (node->width > node->next->width) {
        printf("array(size = %d, width = %d, var_id = %d)", node->size, node->width, node->var_id);
        if (node->width != 4)  //
            printf(" -> ");
        node = node->next;
    }
    printf("size = %d, var_id = %d\n", node->size, node->var_id);
}