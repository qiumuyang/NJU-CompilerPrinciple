#include "block.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BLOCK_CNT 250
bool DO_GLOBAL_REMOVE;

// increase count when allocating a new block
int blockCount = 0;
static int previous_size = 0;

block** label2Block = NULL;  // record label_id -> block ptr
defNode** defTable = NULL;   // record var_id/temp_id -> codes def var/temp
useNode** useTable = NULL;   // record var_id/temp_id -> codes use var/temp

void initDefUse() {
    if (defTable != NULL) {
        for (int i = 0; i < previous_size; i++) {
            defNode* node = defTable[i];
            while (node != NULL) {
                defNode* to_free = node;
                node = node->next;
                free(to_free);
            }
        }
        free(defTable);
        defTable = NULL;
    }
    if (useTable != NULL) {
        for (int i = 0; i < previous_size; i++) {
            useNode* node = useTable[i];
            while (node != NULL) {
                useNode* to_free = node;
                node = node->next;
                free(to_free);
            }
        }
        free(useTable);
        useTable = NULL;
    }
    defTable = (defNode**)malloc(sizeof(defNode*) * (VarCount + TempCount + 1));
    useTable = (useNode**)malloc(sizeof(useNode*) * (VarCount + TempCount + 1));
    previous_size = VarCount + TempCount + 1;
    for (int i = 0; i < VarCount + TempCount + 1; i++) {
        defTable[i] = NULL;
        useTable[i] = NULL;
    }
}

codeNode* newCodeNode(interCode* code) {
    codeNode* ret = (codeNode*)malloc(sizeof(codeNode));
    ret->code = code;
    ret->next = NULL;
    return ret;
}

codeNode* copyCodeNodeList(codeNode* head) {
    if (head == NULL) return NULL;
    codeNode* ret = newCodeNode(head->code);
    codeNode* tail = ret;
    head = head->next;
    while (head) {
        codeNode* insert = newCodeNode(head->code);
        tail->next = insert;
        tail = insert;
        head = head->next;
    }
    return ret;
}

void freeCodeNodeList(codeNode* head) {
    if (head == NULL) return;
    while (head) {
        codeNode* to_free = head;
        head = head->next;
        free(head);
    }
}

defNode* newDef(interCode* code) {
    defNode* def = (defNode*)malloc(sizeof(defNode));
    def->code = code;
    def->next = NULL;
    return def;
}

useNode* newUse(interCode* code) {
    useNode* use = (useNode*)malloc(sizeof(useNode));
    use->code = code;
    use->next = NULL;
    return use;
}

void insertDef(defNode* def, int idx) {
    def->next = defTable[idx];
    defTable[idx] = def;
}

void insertUse(useNode* use, int idx) {
    use->next = useTable[idx];
    useTable[idx] = use;
}

void getDefsAndUses(interCode* codes) {
    interCode* itr = codes;

    int idx = 0;
    do {
        if (isDefCode(itr)) {
            defNode* def = newDef(itr);
            if (itr->ic_type == ASSIGN)
                idx = getOprIndex(itr->assign.dst);
            else if (itr->ic_type == CALL)
                idx = getOprIndex(itr->call.dst);
            else
                assert(0);
            insertDef(def, idx);
        }

        operand use_opr[3];
        int cnt = getCodeUse(itr, use_opr);
        for (int i = 0; i < cnt; i++) {
            idx = getOprIndex(use_opr[i]);
            insertUse(newUse(itr), idx);
        }

        itr = itr->next;
    } while (itr != codes);
}

void printDefs() {
    printf("DEFTABLE::\n");
    char buffer[100];
    for (int i = 1; i < VarCount + 1; i++) {
        printf("v%d :\n", i);
        defNode* def = defTable[i];
        while (def != NULL) {
            interCode* code = def->code;
            interCodeToString(buffer, code);
            printf("  %s\n", buffer);
            def = def->next;
        }
    }

    for (int i = 1; i < TempCount + 1; i++) {
        printf("t%d :\n", i);
        defNode* def = defTable[i + VarCount];
        while (def != NULL) {
            interCode* code = def->code;
            interCodeToString(buffer, code);
            printf("  %s\n", buffer);
            def = def->next;
        }
    }
}

void printUses() {
    printf("USETABLE::\n");
    char buffer[100];
    for (int i = 1; i < VarCount + 1; i++) {
        printf("v%d :\n", i);
        useNode* use = useTable[i];
        while (use != NULL) {
            interCode* code = use->code;
            interCodeToString(buffer, code);
            printf("  %s\n", buffer);
            use = use->next;
        }
    }

    for (int i = 1; i < TempCount + 1; i++) {
        printf("t%d :\n", i);
        useNode* use = useTable[i + VarCount];
        while (use != NULL) {
            interCode* code = use->code;
            interCodeToString(buffer, code);
            printf("  %s\n", buffer);
            use = use->next;
        }
    }
}

void initBlock() {
    DO_GLOBAL_REMOVE = true;
    blockCount = 0;
    if (label2Block != NULL) {
        free(label2Block);
        label2Block = NULL;
    }
    label2Block = (block**)malloc(sizeof(block*) * (LabelCount + 1));
    for (int i = 0; i < LabelCount + 1; i++) {
        label2Block[i] = NULL;
    }
}

int allocBlock() {
    blockCount++;
    return blockCount;
}

block* newBlock() {
    int id = allocBlock();
    block* b = (block*)malloc(sizeof(block));
    b->id = id;

    b->first = NULL;
    b->end = NULL;

    b->next = NULL;
    b->prev = NULL;

    b->gotoId = 0;
    b->flowSeqNext = NULL;
    b->flowGotoNext = NULL;
    b->flowPrev[0] = NULL;
    b->flowPrev[1] = NULL;

    b->gen = NULL;
    b->out = NULL;
    b->genMap = RB_ROOT;
    b->outMap = RB_ROOT;

    b->useDef = (char*)malloc(sizeof(char) * (VarCount + TempCount + 1));
    b->useIn = (char*)malloc(sizeof(char) * (VarCount + TempCount + 1));
    for (int i = 0; i < VarCount + TempCount + 1; i++) {
        b->useDef[i] = 0;
        b->useIn[i] = 0;
    }
    b->isVisited = false;
    return b;
}

block* removeBlock(block* remove) {
    if (remove->next != NULL) {
        remove->next->prev = remove->prev;
    }
    remove->prev->next = remove->next;

    if (remove->flowGotoNext) {
        if (remove->flowGotoNext->flowPrev[0] == remove->flowGotoNext)
            remove->flowGotoNext->flowPrev[0] =
                remove->flowGotoNext->flowPrev[1];
        remove->flowGotoNext->flowPrev[1] = NULL;
    }
    if (remove->flowSeqNext) {
        if (remove->flowSeqNext->flowPrev[0] == remove->flowSeqNext)
            remove->flowSeqNext->flowPrev[0] = remove->flowSeqNext->flowPrev[1];
        remove->flowSeqNext->flowPrev[1] = NULL;
    }

    block* ret = remove->next;
    interCode* codeItr = remove->first;
    while (codeItr) {
        if (codeItr != remove->end) {
            codeItr = removeCodeItr(codeItr, true);
        } else {
            removeCode(codeItr);
            break;
        }
    }
    freeBlock(remove);
    return ret;
}

bool isOprInGenList(interCode* code, block* b) {
    operand opr = getCodeDst(code);
    char buffer[30];
    sprintf(buffer, "%d", getOprIndex(opr));
    return get(&(b->genMap), buffer) != NULL;
}

bool isDefInOutList(interCode* code, block* b) {
    char buffer[64];
    sprintf(buffer, "%p", code);
    return get(&(b->outMap), buffer) != NULL;
}

bool setOutFromPrev(block* b, codeNode* prev) {
    bool flag = false;
    codeNode* itr = prev;
    while (itr != NULL) {
        if (!isOprInGenList(itr->code, b) && !isDefInOutList(itr->code, b)) {
            outNode* out = (outNode*)newCodeNode(itr->code);
            out->next = b->out;
            b->out = out;
            char buffer[64];
            sprintf(buffer, "%p", itr->code);
            put(&(b->outMap), buffer, NULL);
            flag = true;
        }
        itr = itr->next;
    }
    return flag;
}

bool setOutForOneBlock(block* b) {
    bool modifyFlag = false;
    if (b->flowPrev[0] != NULL) {
        modifyFlag = setOutFromPrev(b, (codeNode*)b->flowPrev[0]->gen);
        bool flag = setOutFromPrev(b, (codeNode*)b->flowPrev[0]->out);
        modifyFlag = flag || modifyFlag;
    }

    if (b->flowPrev[1] != NULL) {
        bool flag =
            setOutFromPrev(b, (codeNode*)b->flowPrev[1]->gen) || modifyFlag;
        modifyFlag = flag || modifyFlag;
        flag = setOutFromPrev(b, (codeNode*)b->flowPrev[1]->out) || modifyFlag;
        modifyFlag = flag || modifyFlag;
    }
    return modifyFlag;
}

bool setOutBlocks(block* b, bool visit) {
    if (visit != b->isVisited) return false;
    b->isVisited = !visit;
    bool modifyFlag = setOutForOneBlock(b);
    if (b->flowSeqNext != NULL) {
        bool flag = setOutBlocks(b->flowSeqNext, visit);
        modifyFlag = flag || modifyFlag;
    }
    if (b->flowGotoNext != NULL) {
        bool flag = setOutBlocks(b->flowGotoNext, visit);
        modifyFlag = flag || modifyFlag;
    }
    return modifyFlag;
}

void dfs(block* b, bool visit) {
    if (visit != b->isVisited) return;
    b->isVisited = !visit;
    if (b->flowSeqNext != NULL) {
        dfs(b->flowSeqNext, visit);
    }
    if (b->flowGotoNext != NULL) {
        dfs(b->flowGotoNext, visit);
    }
}

void setOneUseDef(block* b, interCode* itr) {
    int idx = 0;
    // Deal with Use
    if (itr->ic_type == RETURN_IC || itr->ic_type == ARG ||
        itr->ic_type == WRITE) {
        idx = getOprIndex(itr->opr);
        if (b->useDef[idx] == 0) b->useDef[idx] = 2;
    } else if (itr->ic_type == COND) {
        idx = getOprIndex(itr->cond.opr1);
        if (b->useDef[idx] == 0) b->useDef[idx] = 2;
        idx = getOprIndex(itr->cond.opr2);
        if (b->useDef[idx] == 0) b->useDef[idx] = 2;
    } else if (itr->ic_type == ASSIGN) {
        idx = getOprIndex(itr->assign.src1);
        if (b->useDef[idx] == 0) b->useDef[idx] = 2;
        if (!IS_EOPR(itr->assign.src2)) {
            idx = getOprIndex(itr->assign.src2);
            if (b->useDef[idx] == 0) b->useDef[idx] = 2;
        }
        if (itr->assign.op_type == LSTAR || itr->assign.op_type == LRSTAR) {
            idx = getOprIndex(itr->assign.dst);
            if (b->useDef[idx] == 0) b->useDef[idx] = 2;
        }
    }

    // Deal with Def
    if (isDefCode(itr)) {
        if (itr->ic_type == ASSIGN)
            idx = getOprIndex(itr->assign.dst);
        else if (itr->ic_type == CALL)
            idx = getOprIndex(itr->call.dst);
        else
            assert(0);
        if (b->useDef[idx] == 0) b->useDef[idx] = 1;
    } else if (itr->ic_type == READ) {
        idx = getOprIndex(itr->opr);
        if (b->useDef[idx] == 0) b->useDef[idx] = 1;
    } else if (itr->ic_type == DEC) {
        idx = itr->dec.var_id;
        if (b->useDef[idx] == 0) b->useDef[idx] = 1;
    }
}

block* setBlockUseDef(block* b) {
    // printf("in\n");
    for (interCode* itr = b->first; itr != b->end; itr = itr->next) {
        // printf("testin\n");
        setOneUseDef(b, itr);
        // printf("testout\n");
    }
    setOneUseDef(b, b->end);
    // printf("out\n");
}

bool setBlockUseIn(block* b) {
    bool modifyFlag = false;
    block* itr = b;
    while (itr != NULL) {
        if (itr->flowSeqNext != NULL) {
            for (int i = 1; i < VarCount + TempCount + 1; i++) {
                if (itr->useDef[i] == 0 && itr->useIn[i] == 0) {
                    if (itr->flowSeqNext->useDef[i] == 2 ||
                        itr->flowSeqNext->useIn[i] == 1) {
                        itr->useIn[i] = 1;
                        modifyFlag = true;
                    }
                }
            }
        }
        if (itr->flowGotoNext != NULL) {
            for (int i = 1; i < VarCount + TempCount + 1; i++) {
                if (itr->useDef[i] == 0 && itr->useIn[i] == 0) {
                    if (itr->flowGotoNext->useDef[i] == 2 ||
                        itr->flowGotoNext->useIn[i] == 1) {
                        itr->useIn[i] = 1;
                        modifyFlag = true;
                    }
                }
            }
        }
        itr = itr->next;
    }
    return modifyFlag;
}

block* getBlocks(interCode* codes) {
    block* head = NULL;
    block* tail = head;

    bool newBlockFlag = false;
    interCode* itr = codes;

    int block_incr = 0;

    do {
        int label = 0;
        int flag = isLeader(itr, &label);

        if (newBlockFlag || flag == LABEL_S || flag == FUNCTION_S) {
            newBlockFlag = false;

            // meet a new block's beginning
            // or previous intercode asked for it
            // alloc new block
            block* nb = newBlock();
            block_incr++;

            // insert newblock into list
            if (head == NULL) {
                head = nb;
            } else {
                tail->next = nb;
                nb->prev = tail;
            }

            // handle previous block, if exists
            if (tail != NULL) {
                // set its ending
                tail->end = itr->prev;
                // set its flow graph sequence next
                int lb = 0;
                int tail_flag = isLeader(tail->end, &lb);
                if (tail_flag == GOTO_S || tail_flag == RETURN_S)
                    // force goto || return
                    tail->flowSeqNext = NULL;
                else {
                    tail->flowSeqNext = nb;
                    nb->flowPrev[0] = tail;
                }
            }

            // handle current block
            nb->first = itr;
            if (flag == LABEL_S) {
                // record (label_n -> block ptr)
                label2Block[label] = nb;
            }

            // current block becomes the new tail
            tail = nb;
        }

        if (flag == COND_S || flag == GOTO_S || flag == RETURN_S) {
            // block end
            newBlockFlag = true;
            tail->end = itr;
            if (flag != RETURN_S) {
                tail->gotoId = label;
            }
        }

        itr = itr->next;
    } while (itr != codes);

    if (tail->end == NULL) tail->end = itr->prev;

    if (block_incr >= MAX_BLOCK_CNT) DO_GLOBAL_REMOVE = false;

    for (block* itr = head; itr; itr = itr->next) {
        setBlockUseDef(itr);
    }

    return head;
}

void printBlocks(block* b) {
    char buffer[100];
    while (b != NULL) {
        printf("block %d :\n", b->id);
        for (interCode* itr = b->first; itr; itr = itr->next) {
            interCodeToString(buffer, itr);
            printf("├ %s\n", buffer);
            if (itr == b->end) break;
        }
        /*printf("|\n└Gen: \n");
        for (genNode* itr = b->gen; itr; itr = itr->next) {
            interCodeToString(buffer, itr->code);
            printf("|   %s\n", buffer);
        }
        printf("|\n└Out-Gen: \n");
        for (outNode* itr = b->out; itr; itr = itr->next) {
            interCodeToString(buffer, itr->code);
            printf("    %s\n", buffer);
        }*/

        printf("|\n└Use: \n");
        for (int i = 1; i < VarCount + 1; i++) {
            if (b->useDef[i] == 2) printf("    v%d\n", i);
        }
        for (int i = 1; i < TempCount + 1; i++) {
            if (b->useDef[i + VarCount] == 2) printf("    t%d\n", i);
        }

        printf("|\n└Def: \n");
        for (int i = 1; i < VarCount + 1; i++) {
            if (b->useDef[i] == 1) printf("    v%d\n", i);
        }
        for (int i = 1; i < TempCount + 1; i++) {
            if (b->useDef[i + VarCount] == 1) printf("    t%d\n", i);
        }

        printf("|\n└In-Use: \n");
        for (int i = 1; i < VarCount + 1; i++) {
            if (b->useIn[i] == 1) printf("    v%d\n", i);
        }
        for (int i = 1; i < TempCount + 1; i++) {
            if (b->useIn[i + VarCount] == 1) printf("    t%d\n", i);
        }
        b = b->next;
        printf("\n");
    }
}

void freeBlock(block* to_free) {
    assert(to_free != NULL);
    freeCodeNodeList((codeNode*)to_free->gen);
    freeCodeNodeList((codeNode*)to_free->out);
    freeMap(&(to_free->genMap), NULL);
    freeMap(&(to_free->outMap), NULL);
    free(to_free->useDef);
    free(to_free->useIn);
    free(to_free);
}

void freeBlocks(block* b) {
    while (b != NULL) {
        block* to_free = b;
        b = b->next;
        freeBlock(to_free);
    }
}

block* getFlowGraph(block* entry) {
    block* itr = entry;
    while (itr != NULL) {
        block* gotoItr = label2Block[itr->gotoId];
        itr->flowGotoNext = gotoItr;
        if (gotoItr != NULL) {
            if (gotoItr->flowPrev[0] == NULL)
                gotoItr->flowPrev[0] = itr;
            else
                gotoItr->flowPrev[1] = itr;
        }
        itr = itr->next;
    }
    return entry;
}

void printFlowGraph(block* b, bool flag) {
    if (flag != b->isVisited) return;
    b->isVisited = !flag;
    if (b->flowSeqNext != NULL) {
        printf("block %d --Seq--> block %d\n", b->id, b->flowSeqNext->id);
        printFlowGraph(b->flowSeqNext, flag);
    }
    if (b->flowGotoNext != NULL) {
        printf("block %d --GOTO--> block %d\n", b->id, b->flowGotoNext->id);
        printFlowGraph(b->flowGotoNext, flag);
    }
    if (b->flowGotoNext == NULL && b->flowSeqNext == NULL)
        printf("block %d is an end.\n", b->id);
    if (b->flowPrev[0] != NULL)
        printf("block %d --Prev--> block %d\n", b->id, b->flowPrev[0]->id);
    if (b->flowPrev[1] != NULL)
        printf("block %d --Prev--> block %d\n", b->id, b->flowPrev[1]->id);
}

void bfs(block* b) {
    int qsize = 1000;
    block* queue[qsize];
    for (int i = 0; i < qsize; i++) queue[i] = NULL;
    int qf = 0, qt = 0;
    queue[qt++] = b;
    queue[qt++] = NULL;
    while ((qt >= qf && qt - qf > 1) || (qt < qf && qt - qf + qsize > 1)) {
        block* front = queue[qf];
        qf = (qf + 1) % qsize;
        if (front != NULL) {
            if (front->isVisited) continue;
            front->isVisited = true;
            printf("%d ", front->id);
            if (front->flowSeqNext) {
                queue[qt] = front->flowSeqNext;
                qt = (qt + 1) % qsize;
            }
            if (front->flowGotoNext) {
                queue[qt] = front->flowGotoNext;
                qt = (qt + 1) % qsize;
            }
        } else {
            printf("\n");
            queue[qt] = NULL;
            qt = (qt + 1) % qsize;
        }
    }
    printf("\n");
}

void adjacent(block* b) {
    for (block* iter = b; iter != NULL; iter = iter->next) {
        printf("%d: ", iter->id);
        if (iter->flowSeqNext) {
            printf("%d(S) ", iter->flowSeqNext->id);
        }
        if (iter->flowGotoNext) {
            printf("%d(G) ", iter->flowGotoNext->id);
        }
        printf("\n");
    }
}