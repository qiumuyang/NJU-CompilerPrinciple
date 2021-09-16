#ifndef __GTREE_H__
#define __GTREE_H__

enum { LexicalType = 1, SyntacticType = 2 };

typedef struct node {
    int token;         // enum value
    char tokenId[32];  // name
    int tokenType;     // 0: not initialized 1: lexical unit 2: syntactic unit
    char str[50];
    // for childs
    struct node* parent;
    struct node** childs;
    int childCnt;
    int capacity;
    // ----------
    int lineNum;  // token position
} treeNode;

treeNode* newNode(int token, const char* tokenId, int tokenType, int lineNum);
treeNode* treeInsert(treeNode* parent, treeNode* child);
void printTree(treeNode* current, int indent);

#endif