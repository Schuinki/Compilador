/* =====================================================================
 * scope.h - Tabela de Simbolos (cap. 5) com o object estendido (cap. 6)
 * e com os campos de tamanho e enderecamento da geracao de codigo
 * (cap. 7, fig. 7.11).
 * ===================================================================== */

#ifndef SCOPE_H
#define SCOPE_H

#define MAX_NEST_LEVEL 64

/* fig. 6.1 */
typedef enum { NO_KIND_DEF_ = -1, VAR_, PARAM_, FUNCTION_, FIELD_,
               ARRAY_TYPE_, STRUCT_TYPE_, ALIAS_TYPE_, SCALAR_TYPE_,
               UNIVERSAL_ } t_kind;

#define IS_TYPE_KIND(k) ((k) == ARRAY_TYPE_ || (k) == STRUCT_TYPE_ || \
                         (k) == ALIAS_TYPE_ || (k) == SCALAR_TYPE_)

/* fig. 6.2, com pNextDecl (ver README, decisao D7): pNext encadeia o
 * nivel da tabela de simbolos; pNextDecl encadeia uma lista de
 * declaracao (variaveis de um 'var', campos de uma struct, parametros).
 *
 * VF (geracao de codigo, fig. 7.11): nSize e nIndex ficam fora da uniao
 * em vez de repetidos em cada estrutura (o livro os coloca no inicio de
 * cada uma, justamente para poderem ser lidos sem conhecer o kind):
 *   nSize  - tipos: tamanho em celulas da pilha de valores;
 *            Var, Param e Field: tamanho da variavel, em celulas
 *   nIndex - Var e Param: endereco (deslocamento a partir de bp);
 *            Field: deslocamento dentro da struct;
 *            Function: indice no Pool de Funcoes */
typedef struct object {
    int            nName;              /* token secundario do identificador */
    struct object *pNext;
    struct object *pNextDecl;
    t_kind         eKind;
    int            nSize;
    int            nIndex;
    union {
        struct { struct object *pType; } Var, Param, Field;
        struct { struct object *pRetType; struct object *pParams;
                 int nParams;          /* celulas ocupadas pelos parametros   */
                 int nVars; } Function; /* celulas de parametros + variaveis  */
        struct { struct object *pElemType; int nNumElems; } Array;
        struct { struct object *pFields; } Struct;
        struct { struct object *pBaseType; } Alias;
    } _;
} object, *pobject;

extern int nCurrentLevel;

void    ResetSymbolTable(void);        /* libera todos os objects criados  */
int     NewBlock(void);                /* fig. 5.4 */
int     EndBlock(void);
pobject Define(int aName);             /* fig. 5.5 */
pobject Search(int aName);             /* fig. 5.6 - so no nivel corrente   */
pobject Find(int aName);               /* fig. 5.7 - em todos os niveis     */
pobject NewObject(int aName);          /* object fora da tabela (D7)        */

#endif /* SCOPE_H */
