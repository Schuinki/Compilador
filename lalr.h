/* =====================================================================
 * lalr.h - construcao da Tabela de Acao la-LR(1) (cap. 3.8)
 *
 * Codificacao de Action[q][s] (q = estado, s = terminal ou nao terminal):
 *     0          erro
 *     p > 0      empilhar / desviar para o estado p-1
 *     p < 0      reduzir pela regra -p-1
 * ===================================================================== */

#ifndef LALR_H
#define LALR_H

#include <stdio.h>
#include "grammar.h"

#define IS_SHIFT(p)      ((p) > 0)
#define IS_REDUCTION(p)  ((p) < 0)
#define STATE(p)         ((p) - 1)
#define RULE(p)          (-(p) - 1)

extern int  (*Action)[NUM_SYMBOLS];
extern int  nStates;
extern int  FINAL;                    /* estado de aceitacao              */
extern int  nShiftReduce, nReduceReduce;

void BuildLALRTable(FILE *report);    /* report != NULL: lista conflitos */
void FreeLALRTable(void);

#endif /* LALR_H */
