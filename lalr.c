/* =====================================================================
 * lalr.c - construcao da Tabela de Acao la-LR(1) (cap. 3.8)
 *
 * O livro constroi o automato LR(1) canonico e funde os estados que
 * tem o mesmo nucleo (mesmos itens sem considerar o lookahead). Aqui a
 * fusao e feita durante a propria construcao: ao calcular o goto de um
 * estado, se ja existe um estado com o mesmo nucleo, os lookaheads sao
 * unidos a ele (e o estado volta para a fila se ganhou lookahead novo).
 * O resultado e o mesmo automato la-LR(1), sem materializar o LR(1).
 *
 * Um item e o par (regra, posicao do ponto), numerado por
 * itemId = Base[regra] + ponto. Um conjunto de lookaheads e um mapa de
 * bits sobre os terminais (cabem em 64 bits).
 * ===================================================================== */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "lalr.h"

typedef uint64_t t_set;
_Static_assert(NUM_TERMINALS <= 64, "lookaheads devem caber em 64 bits");
#define BIT(t) ((t_set)1 << (t))

typedef struct {
    int    nKernel;
    int   *items;                     /* ids dos itens do nucleo (ordem crescente) */
    t_set *la;                        /* lookahead de cada item do nucleo */
} t_state;

int  (*Action)[NUM_SYMBOLS] = NULL;
int  nStates = 0;
int  FINAL = -1;
int  nShiftReduce = 0, nReduceReduce = 0;

static t_state *States = NULL;
static int    (*Trans)[NUM_SYMBOLS] = NULL;   /* goto do automato, -1 = nenhum */
static int      capStates = 0;

static int   Base[NUM_RULES + 1];     /* itemId = Base[r] + ponto */
static int   nItems;
static t_set First[NUM_SYMBOLS];
static int   Nullable[NUM_SYMBOLS];

/* fecho do estado corrente */
static t_set *clLA;
static char  *clIn, *clPending;
static int   *clList, clCount, *clStack;

static int ItemRule(int id) { int r = 0; while (Base[r + 1] <= id) r++; return r; }

/* ---------------------------------------------------------------- FIRST */
static void ComputeFirst(void)
{
    int r, k, changed = 1;
    memset(First, 0, sizeof(First));
    memset(Nullable, 0, sizeof(Nullable));
    for (k = 0; k < NUM_TERMINALS; k++) First[k] = BIT(k);
    while (changed) {
        changed = 0;
        for (r = 0; r < NUM_RULES; r++) {
            int A = Left[r], allNullable = 1;
            for (k = 0; k < Len[r] && allNullable; k++) {
                int X = Grammar[r].rhs[k];
                if ((First[A] | First[X]) != First[A]) { First[A] |= First[X]; changed = 1; }
                allNullable = Nullable[X];
            }
            if (allNullable && !Nullable[A]) { Nullable[A] = 1; changed = 1; }
        }
    }
}

/* FIRST da sequencia rhs[from..] da regra r; *nullable indica se ela gera vazio */
static t_set FirstOfSeq(int r, int from, int *nullable)
{
    t_set s = 0;
    int k;
    for (k = from; k < Len[r]; k++) {
        s |= First[Grammar[r].rhs[k]];
        if (!Nullable[Grammar[r].rhs[k]]) { *nullable = 0; return s; }
    }
    *nullable = 1;
    return s;
}

/* -------------------------------------------------------------- closure */
static void AddClosureItem(int id, t_set la, int *top)
{
    if (!clIn[id]) {
        clIn[id] = 1;
        clLA[id] = la;
        clList[clCount++] = id;
    } else if ((clLA[id] | la) != clLA[id]) {
        clLA[id] |= la;
    } else {
        return;
    }
    if (!clPending[id]) { clPending[id] = 1; clStack[(*top)++] = id; }
}

static void Closure(int s)
{
    int i, top = 0;
    for (i = 0; i < clCount; i++) clIn[clList[i]] = 0;
    clCount = 0;
    for (i = 0; i < States[s].nKernel; i++)
        AddClosureItem(States[s].items[i], States[s].la[i], &top);
    while (top > 0) {
        int id = clStack[--top], r = ItemRule(id), d = id - Base[r];
        clPending[id] = 0;
        if (d < Len[r] && !IS_TERMINAL(Grammar[r].rhs[d])) {
            int B = Grammar[r].rhs[d], nullable, p;
            t_set la = FirstOfSeq(r, d + 1, &nullable);
            if (nullable) la |= clLA[id];
            for (p = 0; p < NUM_RULES; p++)
                if (Left[p] == B) AddClosureItem(Base[p], la, &top);
        }
    }
}

/* --------------------------------------------------------------- estados */
static int NewState(int n, const int *items, const t_set *la)
{
    t_state *st;
    int k;
    if (nStates == capStates) {
        capStates = capStates ? 2 * capStates : 256;
        States = realloc(States, capStates * sizeof(*States));
        Trans  = realloc(Trans,  capStates * sizeof(*Trans));
    }
    st = &States[nStates];
    st->nKernel = n;
    st->items = malloc(n * sizeof(int));
    st->la    = malloc(n * sizeof(t_set));
    memcpy(st->items, items, n * sizeof(int));
    memcpy(st->la, la, n * sizeof(t_set));
    for (k = 0; k < NUM_SYMBOLS; k++) Trans[nStates][k] = -1;
    return nStates++;
}

static int FindCore(int n, const int *items)
{
    int s;
    for (s = 0; s < nStates; s++)
        if (States[s].nKernel == n && memcmp(States[s].items, items, n * sizeof(int)) == 0)
            return s;
    return -1;
}

static void BuildAutomaton(void)
{
    int   *kItems = malloc(nItems * sizeof(int));
    t_set *kLA    = malloc(nItems * sizeof(t_set));
    int   *queue  = NULL, qHead = 0, qTail = 0, qCap = 0;
    char  *inQueue = NULL;
    int    start = Base[R_START];
    t_set  none = 0;

    NewState(1, &start, &none);
    queue = malloc(sizeof(int)); qCap = 1; queue[qTail++] = 0;
    inQueue = calloc(capStates, 1); inQueue[0] = 1;

    while (qHead < qTail) {
        int s = queue[qHead++], X, i;
        inQueue[s] = 0;
        Closure(s);
        for (X = 0; X < NUM_SYMBOLS; X++) {
            int n = 0, t;
            /* nucleo do goto(s, X), ordenado por id e sem repeticao */
            for (i = 0; i < clCount; i++) {
                int id = clList[i], r = ItemRule(id), d = id - Base[r], j, pos;
                if (d >= Len[r] || Grammar[r].rhs[d] != X) continue;
                for (pos = 0; pos < n && kItems[pos] < id + 1; pos++)
                    ;
                if (pos < n && kItems[pos] == id + 1) { kLA[pos] |= clLA[id]; continue; }
                for (j = n; j > pos; j--) { kItems[j] = kItems[j - 1]; kLA[j] = kLA[j - 1]; }
                kItems[pos] = id + 1; kLA[pos] = clLA[id]; n++;
            }
            if (n == 0) continue;
            t = FindCore(n, kItems);
            if (t < 0) {                               /* estado novo         */
                int oldCap = capStates;
                t = NewState(n, kItems, kLA);
                if (capStates != oldCap) {
                    inQueue = realloc(inQueue, capStates);
                    memset(inQueue + oldCap, 0, capStates - oldCap);
                }
            } else {                                   /* fusao de nucleos    */
                int changed = 0;
                for (i = 0; i < n; i++)
                    if ((States[t].la[i] | kLA[i]) != States[t].la[i]) {
                        States[t].la[i] |= kLA[i];
                        changed = 1;
                    }
                if (!changed || inQueue[t]) { Trans[s][X] = t; continue; }
            }
            Trans[s][X] = t;
            if (qTail == qCap) { qCap *= 2; queue = realloc(queue, qCap * sizeof(int)); }
            queue[qTail++] = t;
            inQueue[t] = 1;
        }
    }
    free(kItems); free(kLA); free(queue); free(inQueue);
}

/* -------------------------------------------------------- tabela de acao */
void BuildLALRTable(FILE *report)
{
    int r, s, i;

    GrammarInit();
    Base[0] = 0;
    for (r = 0; r < NUM_RULES; r++) Base[r + 1] = Base[r] + Len[r] + 1;
    nItems = Base[NUM_RULES];
    ComputeFirst();

    clLA      = calloc(nItems, sizeof(t_set));
    clIn      = calloc(nItems, 1);
    clPending = calloc(nItems, 1);
    clList    = malloc(nItems * sizeof(int));
    clStack   = malloc(nItems * sizeof(int));
    clCount   = 0;

    BuildAutomaton();

    Action = calloc(nStates, sizeof(*Action));
    nShiftReduce = nReduceReduce = 0;
    for (s = 0; s < nStates; s++) {
        int X;
        for (X = 0; X < NUM_SYMBOLS; X++)
            if (Trans[s][X] >= 0) Action[s][X] = Trans[s][X] + 1;
        Closure(s);
        for (i = 0; i < clCount; i++) {
            int id = clList[i], t;
            r = ItemRule(id);
            if (id - Base[r] != Len[r] || r == R_START) continue;
            for (t = 0; t < NUM_TERMINALS; t++) {
                int cur;
                if (!(clLA[id] & BIT(t))) continue;
                cur = Action[s][t];
                if (cur == 0) {
                    Action[s][t] = -(r + 1);
                } else if (IS_SHIFT(cur)) {           /* empilhar vence      */
                    nShiftReduce++;
                    if (report) {
                        fprintf(report, "Conflito empilha/reduz no estado %d com %s:"
                                " empilhar (em vez de reduzir pela regra)\n", s, SymbolName(t));
                        PrintRule(report, r);
                    }
                } else if (RULE(cur) != r) {           /* menor regra vence   */
                    nReduceReduce++;
                    if (report) {
                        fprintf(report, "Conflito reduz/reduz no estado %d com %s:\n",
                                s, SymbolName(t));
                        PrintRule(report, RULE(cur));
                        PrintRule(report, r);
                    }
                    if (r < RULE(cur)) Action[s][t] = -(r + 1);
                }
            }
        }
    }
    FINAL = Trans[Trans[0][NT_P]][END_OF_FILE];
}

void FreeLALRTable(void)
{
    int s;
    for (s = 0; s < nStates; s++) { free(States[s].items); free(States[s].la); }
    free(States); free(Trans); free(Action);
    free(clLA); free(clIn); free(clPending); free(clList); free(clStack);
    States = NULL; Trans = NULL; Action = NULL;
    nStates = capStates = 0;
}
