#include "assemble.h"

const char asm_header[] =
    ".data\n\
_prompt: .asciiz \"Enter an integer:\"\n\
_ret: .asciiz \"\\n\"\n\n\
.globl main\n\n\
.text\n";

const char asm_io[] =
    "\nread:\n\
  li $v0, 4\n\
  la $a0, _prompt\n\
  syscall\n\
  li $v0, 5\n\
  syscall\n\
  jr $ra\n\n\
write:\n\
  li $v0, 1\n\
  syscall\n\
  li $v0, 4\n\
  la $a0, _ret\n\
  syscall\n\
  move $v0, $0\n\
  jr $ra";

#define fpwrite(fmt, ...)                  \
    do {                                   \
        fputs("  ", file);                 \
        fprintf(file, fmt, ##__VA_ARGS__); \
        fputc('\n', file);                 \
    } while (0)

#define fpcomment(fmt, ...)                \
    do {                                   \
        fputs("    # ", file);             \
        fprintf(file, fmt, ##__VA_ARGS__); \
        fputc('\n', file);                 \
    } while (0)

#define push(source_reg)                                \
    do {                                                \
        fpwrite("addi $sp, $sp, -4");                   \
        fpwrite("sw $%s, 0($sp)", reg_str[source_reg]); \
        fpcomment("push $%s", reg_str[source_reg]);     \
    } while (0)

#define pop(target_reg)                                 \
    do {                                                \
        fpwrite("lw $%s, 0($sp)", reg_str[target_reg]); \
        fpwrite("addi $sp, $sp, 4");                    \
        fpcomment("pop $%s", reg_str[target_reg]);      \
    } while (0)

#define move(dst, src)                                        \
    do {                                                      \
        fpwrite("move $%s, $%s", reg_str[dst], reg_str[src]); \
    } while (0)

#define leave()       \
    do {              \
        move(sp, fp); \
        pop(fp);      \
        pop(ra);      \
    } while (0)

#define ic_comment(code)                   \
    do {                                   \
        char icbuffer[256];                \
        interCodeToString(icbuffer, code); \
        fpcomment("%s", icbuffer);         \
    } while (0)

#define opr_comment(opr, reg)                    \
    do {                                         \
        char oprbuffer[64];                      \
        operandToString(oprbuffer, opr);         \
        fpcomment("%5s -> $%s", oprbuffer, reg); \
    } while (0)

static position* ptable = NULL;
static FILE* file = NULL;

int getOffset(operand opr) {
    assert(OPR_TYPE(opr) == VARIABLE || OPR_TYPE(opr) == TEMP);
    int idx = getOprIndex(opr);
    assert(ptable[idx].allocated == true);
    return ptable[idx].offset;
}

void loadToReg(operand opr, int reg) {
    switch (OPR_TYPE(opr)) {
        case EOPR:
            return;
        case VARIABLE:
        case TEMP:
            fpwrite("lw $%s, %d($fp)", reg_str[reg], getOffset(opr));
            break;
        case CONST:
            fpwrite("li $%s, %d", reg_str[reg], opr.const_value);
            break;
    }
    // opr_comment(opr, reg_str[reg]);
}

void saveFromReg(operand opr, int reg) {
    switch (OPR_TYPE(opr)) {
        case EOPR:
            return;
        case VARIABLE:
        case TEMP:
            fpwrite("sw $%s, %d($fp)", reg_str[reg], getOffset(opr));
            break;
        case CONST:
            assert(0);
    }
}

void genCondCode(interCode* code) {
    assert(code->ic_type == COND);
    loadToReg(code->cond.opr1, t1);
    loadToReg(code->cond.opr2, t2);
    char* op;
    switch (code->cond.op_type) {
        case EQ:
            op = "eq";
            break;
        case NE:
            op = "ne";
            break;
        case GT:
            op = "gt";
            break;
        case GE:
            op = "ge";
            break;
        case LT:
            op = "lt";
            break;
        case LE:
            op = "le";
            break;
        default:
            assert(0);
    }
    fpwrite("b%s $t1, $t2, label%d", op, code->cond.label_id);
}

void genAssignCode(interCode* code) {
    assert(code->ic_type == ASSIGN);
    if (code->assign.op_type != ADDR) {
        loadToReg(code->assign.src1, t1);
        loadToReg(code->assign.src2, t2);
    }
    switch (code->assign.op_type) {
        case AS:
            move(t0, t1);
            break;
        case ADD:
            fpwrite("add $t0, $t1, $t2");
            break;
        case SUB:
            fpwrite("sub $t0, $t1, $t2");
            break;
        case MUL:
            fpwrite("mul $t0, $t1, $t2");
            break;
        case DIVD:
            fpwrite("div $t1, $t2");
            fpwrite("mflo $t0");
            break;
        case ADDR: {
            int offset = getOffset(code->assign.src1);
            if (!IS_EOPR(code->assign.src2)) {
                loadToReg(code->assign.src2, t1);
                if (offset <= 32767 && offset >= -32768) {
                    fpwrite("addi $t1, $t1, %d", offset);
                } else {
                    loadToReg(newOperand(CONST, offset), t3);
                    fpwrite("add $t1, $t1, $t3");
                }
            } else {
                fpwrite("li $t1, %d", offset);
            }
            fpwrite("add $t0, $fp, $t1");
            break;
        }
        case RSTAR:
            // *src1 -> $t0
            fpwrite("lw $t0, 0($t1)");
            break;
        case LSTAR:
            // dst -> $t0
            loadToReg(code->assign.dst, t0);
            fpwrite("sw $t1, 0($t0)");
            return;
        case LRSTAR:
            loadToReg(code->assign.dst, t0);
            // *src1 -> $t2
            fpwrite("lw $t2, 0($t1)");
            fpwrite("sw $t2, 0($t0)");
            return;
        default:
            assert(0);
    }
    saveFromReg(code->assign.dst, t0);
}

void genSingleCode(interCode* code) {
    switch (code->ic_type) {
        case PARAM:
        case DEC:
            /* do nothing */
            return;
        case COND:
            genCondCode(code);
            break;
        case ASSIGN:
            genAssignCode(code);
            break;
        case LABEL:
            fprintf(file, "label%d:\n", code->label_id);
            return;
        case GOTO:
            fpwrite("j label%d", code->label_id);
            return;
        case ARG:
            loadToReg(code->opr, a0);
            push(a0);
            break;
        case CALL:
            if (strcmp(code->call.func_name, "main") == 0)
                fpwrite("jal %s", code->call.func_name);
            else
                fpwrite("jal F_%s", code->call.func_name);
            saveFromReg(code->call.dst, v0);
            break;
        case RETURN_IC:
            loadToReg(code->opr, v0);
            leave();
            fpwrite("jr $ra");
            break;
        case READ:
            fpwrite("jal read");
            saveFromReg(code->opr, v0);
            break;
        case WRITE:
            loadToReg(code->opr, a0);
            fpwrite("jal write");
            break;
        default:
            fpcomment("(NULL)");
            break;
    }
    ic_comment(code);
}

int allocStack(interCode* entry) {
    int byte4Count = 0;
    int paramCount = 0;
    int idx, size;
    interCode* iter = entry;
    do {
        switch (iter->ic_type) {
            case PARAM:
                idx = getOprIndex(iter->opr);
                if (ptable[idx].allocated == false) {
                    paramCount++;
                    ptable[idx].allocated = true;
                    ptable[idx].offset = 4 * (paramCount + 1);
                }
                break;
            case READ:
            case WRITE:
            case RETURN_IC:
            case ARG:
            case DEC:
            case CALL:
                if (iter->ic_type == DEC) {
                    idx = iter->dec.var_id;
                    size = iter->dec.size / 4;
                } else if (iter->ic_type == CALL) {
                    idx = getOprIndex(iter->call.dst);
                    size = 1;
                } else if (IS_CONST(iter->opr)) {
                    idx = -1;
                } else {
                    idx = getOprIndex(iter->opr);
                    size = 1;
                }
                if (idx > 0 && ptable[idx].allocated == false) {
                    byte4Count += size;
                    ptable[idx].allocated = true;
                    ptable[idx].offset = -4 * byte4Count;
                }
                break;
            case COND:
            case ASSIGN: {
                int idxes[3] = {-1, -1, -1};
                if (iter->ic_type == COND) {
                    idxes[0] = getOprIndex(iter->cond.opr1);
                    idxes[1] = getOprIndex(iter->cond.opr2);
                } else {
                    idxes[0] = getOprIndex(iter->assign.dst);
                    idxes[1] = getOprIndex(iter->assign.src1);
                    if (!IS_EOPR(iter->assign.src2))
                        idxes[2] = getOprIndex(iter->assign.src2);
                }
                for (int i = 0; i < 3; i++) {
                    if (idxes[i] != -1 && ptable[idxes[i]].allocated == false) {
                        byte4Count++;
                        ptable[idxes[i]].allocated = true;
                        ptable[idxes[i]].offset = -4 * byte4Count;
                    }
                }
                break;
            }
            default:
                break;
        }
        iter = iter->next;
    } while (iter != entry);
    return byte4Count;
}

void printStack() {
    for (int i = 0; i < VarCount + TempCount + 5; i++) {
        if (ptable[i].allocated == true) {
            operand o = getOprFromIndex(i);
            char t = 't';
            if (IS_VAR(o)) {
                // printf("v%-10d  %d\n", o.var_id, ptable[i].offset);
                fpcomment("v%-10d %d($fp)", o.var_id, ptable[i].offset);
            } else {
                // printf("t%-10d  %d\n", o.tmp_id, ptable[i].offset);
                fpcomment("t%-10d %d($fp)", o.tmp_id, ptable[i].offset);
            }
        }
    }
}

void genFunction(interCode* entry) {
    assert(entry != NULL);

    // create label
    if (strcmp(entry->func_name, "main") == 0)
        fprintf(file, "%s:\n", entry->func_name);
    else
        fprintf(file, "F_%s:\n", entry->func_name);
    // push return address
    push(ra);
    // set frame pointer
    push(fp);
    move(fp, sp);

    int bytes = allocStack(entry);
    if (bytes > 0 && bytes * 4 <= 32768)
        fpwrite("addi $sp, $sp, -%d", bytes * 4);
    else {
        loadToReg(newOperand(CONST, bytes * -4), t0);
        fpwrite("add $sp, $sp, $t0");
    }
    // printStack();

    interCode* iter = entry->next;
    while (iter != entry) {
        genSingleCode(iter);
        iter = iter->next;
    }
}

void assembleGenerate(FILE* f, interCode** codes) {
    if (codes == NULL) return;

    file = f;
    ptable = (position*)malloc(sizeof(position) * (VarCount + TempCount + 5));

    fputs(asm_header, f);

    for (interCode** code = codes; *code != NULL; code++) {
        for (int i = 0; i < VarCount + TempCount + 5; i++) {
            ptable[i] = NullPos;
        }
        fputc('\n', f);
        genFunction(*code);
    }

    fputs(asm_io, f);
}