/* =====================================================================
 * semantics.h - Gramatica de Atributos (cap. 4) para as analises de
 * escopo (cap. 5) e de tipos (cap. 6) e para a geracao de codigo
 * (cap. 7).
 * ===================================================================== */

#ifndef SEMANTICS_H
#define SEMANTICS_H

#include "scope.h"

typedef enum {
    ERR_LEXICAL, ERR_SYNTAX,
    /* escopo (cap. 5) */
    ERR_REDECL, ERR_NOT_DECL,
    /* tipos (cap. 6) */
    ERR_TYPE_EXPECTED, ERR_BOOL_TYPE_EXPECTED, ERR_TYPE_MISMATCH,
    ERR_INVALID_TYPE, ERR_KIND_NOT_STRUCT, ERR_FIELD_NOT_DECL,
    ERR_KIND_NOT_ARRAY, ERR_INVALID_INDEX_TYPE, ERR_KIND_NOT_VAR,
    ERR_KIND_NOT_FUNC, ERR_TOO_MANY_ARGS, ERR_PARAM_TYPE, ERR_TOO_FEW_ARGS,
    /* acrescentados na VF (ver README) */
    ERR_RETURN_TYPE, ERR_NOT_IN_LOOP,
    NUM_ERRORS
} t_error;

/* Atributos de um no da pilha semantica (fig. 4.7): uma uniao com uma
 * estrutura por "familia" de nao terminais. */
typedef struct {
    int nont;
    union {
        struct { pobject obj; int name; } ID;              /* IDD, IDU, ID   */
        /* E, L, R, Y, F, LV (T so usa type). Os demais campos sao da
         * geracao de codigo e so valem para left-values (LV):
         *   simple - LV e uma variavel escalar; Code[idx] e o LOAD_REF val
         *            que ja foi gerado, e pode ser trocado por LOAD_VAR
         *            ou removido (fig. 7.9, "expressoes sem left-values")
         *   str    - LV e um elemento de string: 1 = o string e a
         *            variavel val (CHAR_AT/SET_CHAR); 2 = o string e
         *            qualquer outro left-value (CALL_LIB) */
        struct { pobject type; int simple, idx, val, str; } T;
        struct { pobject first, last; int size; } List;     /* LI, DC, LP     */
        struct { pobject type; int pos, val; } Lit;         /* TRUE..NUM      */
        struct { pobject type, param; int err; } MC;
        struct { pobject param; int n, err; } LE;
        struct { int l1, l2; } Mark;                        /* ML, MT, ME     */
    } _;
} t_attrib;

/* tipos pre-definidos (fig. 6.3) */
extern pobject pInt, pChar, pBool, pString, pUniversal;

/* erros acumulados */
extern int     nErrors;
extern int     bQuietErrors;           /* 1: nao imprime (testes)        */
void RecordError(t_error e);
void Error(t_error e, int name);       /* name: token secundario ou -1   */
int  HasError(t_error e);

void ResetSemantics(void);             /* abre o nivel global (nivel 0)   */
void Semantics(int rule);
int  CheckTypes(pobject t1, pobject t2);

#endif /* SEMANTICS_H */
