/* =====================================================================
 * parser.c - Analisador Sintatico Ascendente la-LR(1) com avaliacao de
 * atributos (cap. 3.8 e fig. 4.6)
 * ===================================================================== */

#include <stdlib.h>
#include "lexer.h"
#include "grammar.h"
#include "lalr.h"
#include "semantics.h"
#include "codegen.h"
#include "parser.h"

#define MAX_PARSE_STACK 1000

int lastSecundario = -1;
int lastLine = 1;

static int Stack[MAX_PARSE_STACK];
static int nStack = 0;

static void Push(int q)
{
    if (nStack == MAX_PARSE_STACK) {
        fprintf(stderr, "Erro fatal: pilha sintatica cheia\n");
        exit(1);
    }
    Stack[nStack++] = q;
}
static void Pop(int n) { nStack -= n; }
static int  Top(void)  { return Stack[nStack - 1]; }

/* linha, token encontrado e tokens esperados no estado q */
static void SyntaxError(int q, int a)
{
    int t, first = 1;
    RecordError(ERR_SYNTAX);
    if (bQuietErrors) return;
    fprintf(stderr, "Erro sintatico (linha %d): encontrado %s", tokenLine, SymbolName(a));
    if (a == ID)        fprintf(stderr, " '%s'", getName(tokenSecundario));
    if (a == NUMERAL)   fprintf(stderr, " %d", getIntConst(tokenSecundario));
    fprintf(stderr, "; esperado: ");
    for (t = 0; t < NUM_TERMINALS; t++)
        if (Action[q][t] != 0) {
            fprintf(stderr, "%s%s", first ? "" : ", ", SymbolName(t));
            first = 0;
        }
    fprintf(stderr, "\n");
}

/* Figura 4.6 */
int Parse(void)
{
    int q, a, p, r;

    nStack = 0;
    q = 0; Push(0); a = nextToken();
    do {
        p = Action[q][a];
        if (IS_SHIFT(p)) {
            Push(STATE(p));
            lastSecundario = tokenSecundario;
            lastLine = tokenLine;
            a = nextToken();
        } else if (IS_REDUCTION(p)) {
            r = RULE(p);
            Pop(Len[r]);
            Push(STATE(Action[Top()][Left[r]]));
            Semantics(r);
        } else {
            SyntaxError(q, a);
            return 0;
        }
        q = Top();
    } while (q != FINAL);
    return 1;
}

/* Erros lexicos impedem a analise sintatica: uma primeira passada so do
 * lexer (que ja imprime as mensagens) conta os tokens UNKNOWN. Sem
 * nenhum erro, o codigo gerado durante a analise e entregue ao Montador
 * (cap. 7); com erros ele e descartado (ERROR_FLAG, fig. 7.28). */
int Compile(FILE *in)
{
    int nLexErrors = 0;

    if (Action == NULL) BuildLALRTable(NULL);
    ResetSemantics();

    lexerInit(in);
    while (nextToken() != END_OF_FILE)
        if (token == UNKNOWN) { RecordError(ERR_LEXICAL); nLexErrors++; }
    lexerClose();
    if (nLexErrors > 0) return nErrors;

    rewind(in);
    lexerInit(in);
    Parse();
    lexerClose();
    if (nErrors == 0 && Assemble() != 0) nErrors++;
    return nErrors;
}
