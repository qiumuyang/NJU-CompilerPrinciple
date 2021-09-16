#include "tree.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHILD_CNT 8
#define INDENT_INC 2
#define LEX_NAME_LEN 4

const char* _LEX_NAMES[] = {"ID", "INT", "FLOAT", "TYPE", "RC"};
enum { _id = 0, _int, _float, _type, _rc };

int tokenIntId(const char* tokenId) {
    for (int i = 0; i < LEX_NAME_LEN; i++) {
        if (strcmp(tokenId, _LEX_NAMES[i]) == 0) return i;
    }
    return -1;
}

treeNode* newNode(int token, const char* tokenId, int tokenType, int lineNum) {
    treeNode* node = (treeNode*)malloc(sizeof(treeNode));
    node->token = token;
    node->childs = NULL;
    node->childCnt = 0;
    node->capacity = 0;
    node->str[0] = 0;
    strcpy(node->tokenId, tokenId);
    node->tokenType = tokenType;
    node->lineNum = lineNum;
    return node;
}

treeNode** alloc_copy(int size, treeNode** array, int cnt) {
    treeNode** ret = (treeNode**)malloc(sizeof(treeNode*) * size);
    for (int i = 0; i < cnt; i++) {
        ret[i] = array[i];
    }
    return ret;
}

treeNode* treeInsert(treeNode* parent, treeNode* child) {
    if (parent->childs == NULL || parent->capacity == parent->childCnt) {
        // not enough space for new-inserted child
        parent->capacity += CHILD_CNT;
        treeNode** newspace =
            alloc_copy(parent->capacity, parent->childs, parent->childCnt);
        if (parent->childs) free(parent->childs);
        parent->childs = newspace;
    }
    child->parent = parent;
    parent->childs[parent->childCnt] = child;
    parent->childCnt++;
    return parent;
}

void printTree(treeNode* token, int indent) {
    if (!token) return;
    // assert(token);
    assert(token->tokenType);
    if (token->tokenType == SyntacticType && token->childCnt == 0) {
        // generate epsilon, pass
        return;
    }
    for (int i = 0; i < indent; i++) printf(" ");
    // printf("%d ", indent);
    if (token->tokenType == LexicalType) {  // lexical unit
        printf("%s", token->tokenId);
        switch (tokenIntId(token->tokenId)) {
            case _type:
            case _id:
                printf(": %s", token->str);
                break;
            case _int:
                printf(": %u", atoi(token->str));
                break;
            case _float:
                printf(": %f", (float)atof(token->str));
                break;
            default:
                break;
        }
        printf("\n");
    } else if (token->tokenType == SyntacticType) {  // syntactic unit
        printf("%s (%d)\n", token->tokenId, token->lineNum);
        for (int i = 0; i < token->childCnt; i++) {
            printTree(token->childs[i], indent + INDENT_INC);
        }
    } else
        assert(0);
}