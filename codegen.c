/* =====================================================================
 * codegen.c - lista de instrucoes, Linguagem de Montagem e Montador
 * (cap. 7, secoes 7.2 e 7.3)
 * ===================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "codegen.h"

t_func   FuncPool[MAX_FUNCS];
int      nFuncPool = 0;
t_pconst ConstPool[MAX_CONSTS];
int      nConstPool = 0;
t_instr *Code = NULL;
int      nCode = 0;
t_instr *Mem = NULL;
int      nMem = 0;

static int capCode = 0;
static int labelNo = 0;

/* ------------------------------------------------------------ tabelas */
static const char *const OpNames[NUM_OPS] = {
    [OP_NOP] = "NOP",
    [OP_BEGIN_FUNC] = "BEGIN_FUNC", [OP_END_FUNC] = "END_FUNC", [OP_LABEL] = "LABEL",
    [OP_ADD] = "ADD", [OP_SUB] = "SUB", [OP_MUL] = "MUL", [OP_DIV] = "DIV",
    [OP_INC] = "INC", [OP_DEC] = "DEC", [OP_NEG] = "NEG",
    [OP_AND] = "AND", [OP_OR] = "OR", [OP_NOT] = "NOT",
    [OP_EQ] = "EQ", [OP_NEQ] = "NEQ", [OP_LT] = "LT", [OP_LE] = "LE",
    [OP_GT] = "GT", [OP_GE] = "GE",
    [OP_JMP_FW] = "JMP_FW", [OP_JMP_BW] = "JMP_BW",
    [OP_TJMP_FW] = "TJMP_FW", [OP_TJMP_BW] = "TJMP_BW",
    [OP_CALL] = "CALL", [OP_RET] = "RET", [OP_CALL_LIB] = "CALL_LIB",
    [OP_LOAD_VAR] = "LOAD_VAR", [OP_STORE_VAR] = "STORE_VAR",
    [OP_LOAD_CONST] = "LOAD_CONST", [OP_LOAD_TRUE] = "LOAD_TRUE",
    [OP_LOAD_FALSE] = "LOAD_FALSE", [OP_CHAR_AT] = "CHAR_AT",
    [OP_SET_CHAR] = "SET_CHAR", [OP_POP] = "POP", [OP_DUP] = "DUP",
    [OP_ADD_N] = "ADD", [OP_MUL_N] = "MUL",
    [OP_LOAD_REF] = "LOAD_REF", [OP_DE_REF] = "DE_REF", [OP_STORE_REF] = "STORE_REF",
};

const char *OpName(t_op op) { return OpNames[op]; }

/* numero de operandos impressos */
static int NumOperands(t_op op)
{
    switch (op) {
    case OP_BEGIN_FUNC:
        return 3;
    case OP_CALL_LIB:
        return 2;
    case OP_LABEL: case OP_JMP_FW: case OP_JMP_BW: case OP_TJMP_FW:
    case OP_TJMP_BW: case OP_CALL: case OP_LOAD_VAR: case OP_STORE_VAR:
    case OP_LOAD_CONST: case OP_CHAR_AT: case OP_SET_CHAR: case OP_ADD_N:
    case OP_MUL_N: case OP_LOAD_REF: case OP_DE_REF: case OP_STORE_REF:
        return 1;
    default:
        return 0;
    }
}

/* tamanho em bytes (secao 7.2): 1 byte a mais para o indice das
 * instrucoes de variavel e de constante, 2 a mais para os desvios, e o
 * mesmo para as instrucoes com o argumento n de 2 bytes */
int InstrSize(t_op op)
{
    switch (op) {
    case OP_NOP: case OP_BEGIN_FUNC: case OP_END_FUNC: case OP_LABEL:
        return 0;
    case OP_CALL: case OP_LOAD_VAR: case OP_STORE_VAR: case OP_LOAD_CONST:
    case OP_CHAR_AT: case OP_SET_CHAR:
        return 2;
    case OP_JMP_FW: case OP_JMP_BW: case OP_TJMP_FW: case OP_TJMP_BW:
    case OP_CALL_LIB: case OP_ADD_N: case OP_MUL_N: case OP_LOAD_REF:
    case OP_DE_REF: case OP_STORE_REF:
        return 3;
    default:
        return 1;
    }
}

static int IsJump(t_op op)
{
    return op == OP_JMP_FW || op == OP_JMP_BW || op == OP_TJMP_FW || op == OP_TJMP_BW;
}

/* ------------------------------------------------------------ geracao */
void ResetCodegen(void)
{
    int i;
    FreeCodegen();
    nCode = capCode = 0;
    labelNo = 0;
    nFuncPool = 0;
    nConstPool = 0;
    for (i = 0; i < MAX_CONSTS; i++) ConstPool[i].kind = -1;
}

void FreeCodegen(void)
{
    int i;
    for (i = 0; i < nFuncPool; i++) free(FuncPool[i].name);
    nFuncPool = 0;
    for (i = 0; i < nConstPool; i++)
        if (ConstPool[i].kind == K_STRING) free(ConstPool[i].s);
    nConstPool = 0;
    free(Code); Code = NULL; nCode = capCode = 0;
    free(Mem);  Mem = NULL;  nMem = 0;
}

int NewLabel(void)                      /* fig. 7.20 */
{
    return labelNo++;
}

int Emit(t_op op, int a, int b, int c)
{
    if (nCode == capCode) {
        capCode = capCode ? 2 * capCode : 256;
        Code = realloc(Code, capCode * sizeof(t_instr));
    }
    Code[nCode].op = op;
    Code[nCode].a = a;
    Code[nCode].b = b;
    Code[nCode].c = c;
    return nCode++;
}

/* copia o literal do pool do lexer (que e liberado ao fim da analise)
 * para o Pool de Constantes, na mesma posicao */
void AddConst(int pos, int kind)
{
    t_pconst *k = &ConstPool[pos];
    const char *s;
    size_t n;
    k->kind = kind;
    k->s = NULL;
    if (kind == K_INT) {
        k->i = getIntConst(pos);
    } else if (kind == K_CHAR) {
        k->i = (unsigned char)getCharConst(pos);
    } else {
        s = getStringConst(pos);
        n = strlen(s) + 1;
        k->s = malloc(n);
        memcpy(k->s, s, n);
    }
    if (pos >= nConstPool) nConstPool = pos + 1;
}

/* ------------------------------------------------- Linguagem de Montagem */
static void PrintConst(FILE *f, int i)
{
    t_pconst *k = &ConstPool[i];
    if (k->kind == K_INT)         fprintf(f, "integer %ld", k->i);
    else if (k->kind == K_CHAR)   fprintf(f, "char '%c'", (char)k->i);
    else                          fprintf(f, "string \"%s\"", k->s);
}

static void PrintConstPool(FILE *f, const char *prefix)
{
    int i, any = 0;
    for (i = 0; i < nConstPool; i++) {
        if (ConstPool[i].kind < 0) continue;
        if (!any) fprintf(f, "%sPool de Constantes:\n", prefix);
        any = 1;
        fprintf(f, "%s  %3d = ", prefix, i);
        PrintConst(f, i);
        fprintf(f, "\n");
    }
}

void PrintAssembly(FILE *f)
{
    int i;
    for (i = 0; i < nCode; i++) {
        t_instr *p = &Code[i];
        switch (p->op) {
        case OP_NOP:
            break;
        case OP_BEGIN_FUNC:
            fprintf(f, "BEGIN_FUNC %d %d %d\t\t; %s\n", p->a, p->b, p->c,
                    FuncPool[p->a].name);
            break;
        case OP_END_FUNC:
            fprintf(f, "END_FUNC\n\n");
            break;
        case OP_LABEL:
            fprintf(f, "L%d:\n", p->a);
            break;
        default:
            if (IsJump(p->op))
                fprintf(f, "\t%s L%d\n", OpName(p->op), p->a);
            else if (NumOperands(p->op) == 2)
                fprintf(f, "\t%s %d, %d\n", OpName(p->op), p->a, p->b);
            else if (NumOperands(p->op) == 1)
                fprintf(f, "\t%s %d\n", OpName(p->op), p->a);
            else
                fprintf(f, "\t%s\n", OpName(p->op));
        }
    }
    PrintConstPool(f, "; ");
}

/* ------------------------------------------------------------ Montador */
int Assemble(void)
{
    int  i, addr = 0, curFunc = -1;
    int *labelAddr, *at;

    free(Mem); Mem = NULL; nMem = 0;
    labelAddr = malloc((labelNo + 1) * sizeof(int));
    at = malloc((nCode + 1) * sizeof(int));
    for (i = 0; i < labelNo; i++) labelAddr[i] = -1;

    /* 1a passada: enderecos de cada instrucao, de cada label e de cada funcao */
    for (i = 0; i < nCode; i++) {
        at[i] = addr;
        if (Code[i].op == OP_LABEL)           labelAddr[Code[i].a] = addr;
        else if (Code[i].op == OP_BEGIN_FUNC) FuncPool[Code[i].a].ip = addr;
        addr += InstrSize(Code[i].op);
    }
    nMem = addr;
    Mem = calloc(nMem ? nMem : 1, sizeof(t_instr));        /* OP_NOP == 0 */

    /* 2a passada: monta, resolvendo os desvios em deslocamentos relativos */
    for (i = 0; i < nCode; i++) {
        t_instr in = Code[i];
        if (in.op == OP_BEGIN_FUNC) curFunc = in.a;
        if (InstrSize(in.op) == 0) continue;
        if (IsJump(in.op)) {
            int target = labelAddr[in.a];
            int n = (in.op == OP_JMP_FW || in.op == OP_TJMP_FW) ? target - at[i]
                                                                : at[i] - target;
            if (target < 0 || n < 0) {
                fprintf(stderr, "Erro interno: label L%d invalido\n", in.a);
                free(labelAddr); free(at);
                return 1;
            }
            in.c = in.a;
            in.a = n;
        } else if (in.op == OP_RET && curFunc >= 0) {
            in.a = FuncPool[curFunc].retSize;     /* celulas devolvidas */
        }
        Mem[at[i]] = in;
    }
    free(labelAddr); free(at);
    return 0;
}

void PrintAssembled(FILE *f)
{
    int i;
    fprintf(f, "Pool de Funcoes:\n");
    fprintf(f, "  %-3s %-12s %-8s %-8s %-6s %s\n", "n", "nome", "params", "vars", "ip", "retorno");
    for (i = 0; i < nFuncPool; i++)
        fprintf(f, "  %-3d %-12s %-8d %-8d %-6d %d celula(s)\n", i, FuncPool[i].name,
                FuncPool[i].nParams, FuncPool[i].nVars, FuncPool[i].ip, FuncPool[i].retSize);
    fprintf(f, "\n");
    PrintConstPool(f, "");
    fprintf(f, "\nArray de Codigo (%d bytes):\n", nMem);
    for (i = 0; i < nMem; i++) {
        t_instr *p = &Mem[i];
        int fn;
        if (p->op == OP_NOP) continue;
        for (fn = 0; fn < nFuncPool; fn++)
            if (FuncPool[fn].ip == i) fprintf(f, "              ; ---- funcao %s ----\n", FuncPool[fn].name);
        fprintf(f, "  %04d  ", i);
        if (IsJump(p->op))
            fprintf(f, "%-10s %-4d ; -> %04d (L%d)\n", OpName(p->op), p->a,
                    (p->op == OP_JMP_FW || p->op == OP_TJMP_FW) ? i + p->a : i - p->a, p->c);
        else if (p->op == OP_CALL)
            fprintf(f, "%-10s %-4d ; %s\n", OpName(p->op), p->a, FuncPool[p->a].name);
        else if (p->op == OP_LOAD_CONST) {
            fprintf(f, "%-10s %-4d ; ", OpName(p->op), p->a);
            PrintConst(f, p->a);
            fprintf(f, "\n");
        } else if (NumOperands(p->op) == 2)
            fprintf(f, "%-10s %d, %d\n", OpName(p->op), p->a, p->b);
        else if (NumOperands(p->op) == 1)
            fprintf(f, "%-10s %d\n", OpName(p->op), p->a);
        else
            fprintf(f, "%s\n", OpName(p->op));
    }
}
