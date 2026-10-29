/* =====================================================================
 * vm.c - interpretador da Maquina Virtual (cap. 7, secoes 7.1 e 7.2)
 *
 * Cada "case" abaixo e uma linha da tabela da fig. 7.2. Registradores:
 *   ip  - instrucao corrente (endereco de byte no Array de Codigo)
 *   sp  - topo da Pilha de Valores       bp - base do registro de ativacao
 *   spo - topo da Pilha Operacional (guarda bp e o endereco de retorno)
 * ===================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen.h"
#include "vm.h"

#define ST_SIZE    100000
#define OST_SIZE   20000
#define MAX_STEPS  200000000L

static t_cell st[ST_SIZE];              /* Pilha de Valores       */
static int    ost[OST_SIZE];            /* Pilha Operacional      */
static long   steps = 0;
int           bQuietRuntime = 0;

/* strings criadas durante a execucao (literais copiados e concatenacoes) */
static char **arena = NULL;
static int    nArena = 0, capArena = 0;

static char *NewStr(size_t n)
{
    char *s = malloc(n + 1);
    if (nArena == capArena) {
        capArena = capArena ? 2 * capArena : 64;
        arena = realloc(arena, capArena * sizeof(char *));
    }
    arena[nArena++] = s;
    s[0] = '\0';
    return s;
}

void VMFree(void)
{
    int i;
    for (i = 0; i < nArena; i++) free(arena[i]);
    free(arena);
    arena = NULL;
    nArena = capArena = 0;
}

long VMSteps(void) { return steps; }

static t_cell Int(long v)  { t_cell c; c.isStr = 0; c.i = v; c.s = NULL; return c; }
static t_cell Str(char *s) { t_cell c; c.isStr = 1; c.i = 0; c.s = s; return c; }
/* variavel string nao inicializada vale "" */
static const char *StrOf(t_cell c) { return (c.isStr && c.s) ? c.s : ""; }

static int Compare(t_cell a, t_cell b)
{
    if (a.isStr || b.isStr) return strcmp(StrOf(a), StrOf(b));
    return (a.i > b.i) - (a.i < b.i);
}

void PrintCell(FILE *f, t_cell c, int kind)
{
    switch (kind) {
    case K_BOOL:   fprintf(f, "%s", c.i ? "true" : "false"); break;
    case K_CHAR:   fprintf(f, "'%c'", (char)c.i);            break;
    case K_STRING: fprintf(f, "\"%s\"", StrOf(c));           break;
    default:       fprintf(f, "%ld", c.i);
    }
}

#define FAIL(...) do { \
        if (!bQuietRuntime) { \
            fprintf(stderr, "Erro de execucao (ip = %d): ", ip); \
            fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } \
        return 1; } while (0)

int RunFunction(int f, const t_cell *args, int nArgs, t_cell *res)
{
    int sp = -1, bp = 0, spo = -1, ip, i;
    t_func *fn;

    VMFree();
    steps = 0;
    ip = -1;
    if (Mem == NULL || f < 0 || f >= nFuncPool) FAIL("funcao inexistente");
    fn = &FuncPool[f];
    if (nArgs != fn->nParams)
        FAIL("%s espera %d celula(s) de parametros, recebeu %d", fn->name, fn->nParams, nArgs);
    if (fn->retSize > MAX_RESULT) FAIL("valor de retorno grande demais");

    /* chamada inicial: argumentos na pilha, registro de ativacao com
     * endereco de retorno -1, que encerra a execucao */
    for (i = 0; i < nArgs; i++) st[++sp] = args[i];
    ost[++spo] = bp;
    ost[++spo] = -1;
    bp = sp - fn->nParams + 1;
    for (i = 0; i < fn->nVars; i++) st[++sp] = Int(0);
    ip = fn->ip;

    for (;;) {
        t_instr *p;
        t_cell   a, b;
        int      n;

        if (ip < 0 || ip >= nMem) FAIL("ip fora do codigo");
        if (++steps > MAX_STEPS) FAIL("limite de %ld instrucoes excedido (laco infinito?)", MAX_STEPS);
        if (sp > ST_SIZE - MAX_RESULT - 8) FAIL("estouro da Pilha de Valores");
        p = &Mem[ip];

        switch (p->op) {

        /* ---- aritmeticas ---- */
        case OP_ADD:
            b = st[sp]; a = st[sp - 1];
            if (a.isStr || b.isStr) {                  /* concatenacao de strings */
                const char *x = StrOf(a), *y = StrOf(b);
                char *s = NewStr(strlen(x) + strlen(y));
                strcpy(s, x); strcat(s, y);
                st[sp - 1] = Str(s);
            } else {
                st[sp - 1] = Int(a.i + b.i);
            }
            sp--; ip += 1; break;
        case OP_SUB: st[sp - 1] = Int(st[sp - 1].i - st[sp].i); sp--; ip += 1; break;
        case OP_MUL: st[sp - 1] = Int(st[sp - 1].i * st[sp].i); sp--; ip += 1; break;
        case OP_DIV:
            if (st[sp].i == 0) FAIL("divisao por zero");
            st[sp - 1] = Int(st[sp - 1].i / st[sp].i); sp--; ip += 1; break;
        case OP_INC: st[sp].i = st[sp].i + 1; ip += 1; break;
        case OP_DEC: st[sp].i = st[sp].i - 1; ip += 1; break;
        case OP_NEG: st[sp].i = -st[sp].i;    ip += 1; break;

        /* ---- logicas ---- */
        case OP_AND: st[sp - 1] = Int(st[sp - 1].i && st[sp].i); sp--; ip += 1; break;
        case OP_OR:  st[sp - 1] = Int(st[sp - 1].i || st[sp].i); sp--; ip += 1; break;
        case OP_NOT: st[sp] = Int(!st[sp].i); ip += 1; break;

        /* ---- relacionais ---- */
        case OP_EQ:  st[sp - 1] = Int(Compare(st[sp - 1], st[sp]) == 0); sp--; ip += 1; break;
        case OP_NEQ: st[sp - 1] = Int(Compare(st[sp - 1], st[sp]) != 0); sp--; ip += 1; break;
        case OP_LT:  st[sp - 1] = Int(Compare(st[sp - 1], st[sp]) <  0); sp--; ip += 1; break;
        case OP_LE:  st[sp - 1] = Int(Compare(st[sp - 1], st[sp]) <= 0); sp--; ip += 1; break;
        case OP_GT:  st[sp - 1] = Int(Compare(st[sp - 1], st[sp]) >  0); sp--; ip += 1; break;
        case OP_GE:  st[sp - 1] = Int(Compare(st[sp - 1], st[sp]) >= 0); sp--; ip += 1; break;

        /* ---- desvios ---- */
        case OP_JMP_FW: ip += p->a; break;
        case OP_JMP_BW: ip -= p->a; break;
        case OP_TJMP_FW:                               /* desvia se o topo for FALSO */
            if (!st[sp].i) ip += p->a; else ip += 3;
            sp--; break;
        case OP_TJMP_BW:
            if (!st[sp].i) ip -= p->a; else ip += 3;
            sp--; break;

        /* ---- chamada e retorno ---- */
        case OP_CALL: {
            t_func *c = &FuncPool[p->a];
            if (spo + 2 >= OST_SIZE) FAIL("estouro da Pilha Operacional (recursao demais)");
            ost[++spo] = bp;
            ost[++spo] = ip + 2;
            bp = sp - c->nParams + 1;
            for (i = 0; i < c->nVars; i++) st[++sp] = Int(0);
            ip = c->ip;
            break;
        }
        case OP_RET:                                   /* p->a = celulas devolvidas */
            n = p->a;
            memmove(&st[bp], &st[sp - n + 1], n * sizeof(t_cell));
            sp = bp + n - 1;
            ip = ost[spo--];
            bp = ost[spo--];
            if (ip == -1) {
                for (i = 0; i < n; i++) res[i] = st[sp - n + 1 + i];
                return 0;
            }
            break;
        case OP_CALL_LIB: {
            /* biblioteca 0 (strings): referencia ao string e indice na pilha */
            const char *s; size_t len; long idx, ref; t_cell *v;
            if (p->a != 0 || p->b < 0 || p->b > 1) FAIL("CALL_LIB %d, %d inexistente", p->a, p->b);
            if (p->b == 0) { idx = st[sp].i;     ref = st[sp - 1].i; }
            else           { idx = st[sp - 1].i; ref = st[sp - 2].i; }
            if (bp + ref < 0 || bp + ref > sp) FAIL("referencia invalida");
            v = &st[bp + ref];
            s = StrOf(*v); len = strlen(s);
            if (p->b == 0) {                           /* char_at(ref, idx) */
                if (idx < 0 || idx > (long)len) FAIL("indice %ld fora do string", idx);
                st[sp - 1] = Int((unsigned char)s[idx]);
                sp -= 1;
            } else {                                   /* set_char(ref, idx, ch) */
                if (!v->isStr || v->s == NULL) FAIL("string nao inicializado");
                if (idx < 0 || idx >= (long)len) FAIL("indice %ld fora do string", idx);
                v->s[idx] = (char)st[sp].i;
                sp -= 3;
            }
            ip += 3;
            break;
        }

        /* ---- variaveis e constantes ---- */
        case OP_LOAD_VAR:  st[sp + 1] = st[bp + p->a]; sp++; ip += 2; break;
        case OP_STORE_VAR: st[bp + p->a] = st[sp]; sp--; ip += 2; break;
        case OP_LOAD_CONST: {
            t_pconst *k = &ConstPool[p->a];
            if (k->kind < 0) FAIL("constante %d inexistente", p->a);
            if (k->kind == K_STRING) {                 /* copia: strings sao mutaveis */
                char *s = NewStr(strlen(k->s));
                strcpy(s, k->s);
                st[sp + 1] = Str(s);
            } else {
                st[sp + 1] = Int(k->i);
            }
            sp++; ip += 2;
            break;
        }
        case OP_LOAD_TRUE:  st[sp + 1] = Int(1); sp++; ip += 1; break;
        case OP_LOAD_FALSE: st[sp + 1] = Int(0); sp++; ip += 1; break;

        /* ---- caracteres de strings ---- */
        case OP_CHAR_AT: {
            const char *s = StrOf(st[bp + p->a]);
            long idx = st[sp].i;
            if (idx < 0 || idx > (long)strlen(s)) FAIL("indice %ld fora do string", idx);
            st[sp] = Int((unsigned char)s[idx]);
            ip += 2;
            break;
        }
        case OP_SET_CHAR: {
            t_cell *v = &st[bp + p->a];
            long idx = st[sp - 1].i;
            if (!v->isStr || v->s == NULL) FAIL("string nao inicializado");
            if (idx < 0 || idx >= (long)strlen(v->s)) FAIL("indice %ld fora do string", idx);
            v->s[idx] = (char)st[sp].i;
            sp -= 2; ip += 2;
            break;
        }

        /* ---- pilha ---- */
        case OP_POP: sp--; ip += 1; break;
        case OP_DUP: st[sp + 1] = st[sp]; sp++; ip += 1; break;

        /* ---- campos e arrays ---- */
        case OP_ADD_N: st[sp].i = st[sp].i + p->a; ip += 3; break;
        case OP_MUL_N: st[sp].i = st[sp].i * p->a; ip += 3; break;

        /* ---- left-values ---- */
        case OP_LOAD_REF: st[sp + 1] = Int(p->a); sp++; ip += 3; break;
        case OP_DE_REF: {
            long addr = bp + st[sp].i;
            n = p->a;
            if (addr < 0 || addr + n - 1 >= sp) FAIL("DE_REF fora da pilha (endereco %ld)", addr);
            memmove(&st[sp], &st[addr], n * sizeof(t_cell));
            sp += n - 1; ip += 3;
            break;
        }
        case OP_STORE_REF: {
            long addr;
            n = p->a;
            addr = bp + st[sp - n].i;
            if (addr < 0 || addr + n - 1 >= sp - n) FAIL("STORE_REF fora da pilha (endereco %ld)", addr);
            memmove(&st[addr], &st[sp - n + 1], n * sizeof(t_cell));
            sp -= n + 1; ip += 3;
            break;
        }

        default:
            FAIL("instrucao invalida (%s)", OpName(p->op));
        }
    }
}
