/* =====================================================================
 * scope.c - Tabela de Simbolos como pilha de niveis (cap. 5, fig. 5.1)
 *
 * Cada nivel e uma lista encadeada (pNext) com ponteiro para o ultimo
 * elemento, exatamente como nas figuras 5.3 a 5.7. Como no livro,
 * EndBlock() nao libera os objects do nivel encerrado: os parametros e
 * os campos continuam referenciados pelas funcoes e structs. Todos os
 * objects sao liberados juntos em ResetSymbolTable().
 * ===================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include "scope.h"

static pobject SymbolTable[MAX_NEST_LEVEL];
static pobject SymbolTableLast[MAX_NEST_LEVEL];
int nCurrentLevel = -1;

static pobject *allObjects = NULL;   /* para liberar ao final */
static int      nAllObjects = 0, capAllObjects = 0;

pobject NewObject(int aName)
{
    pobject obj = (pobject)calloc(1, sizeof(object));
    obj->nName = aName;
    obj->eKind = NO_KIND_DEF_;
    if (nAllObjects == capAllObjects) {
        capAllObjects = capAllObjects ? 2 * capAllObjects : 64;
        allObjects = realloc(allObjects, capAllObjects * sizeof(pobject));
    }
    allObjects[nAllObjects++] = obj;
    return obj;
}

void ResetSymbolTable(void)
{
    int i;
    for (i = 0; i < nAllObjects; i++) free(allObjects[i]);
    free(allObjects);
    allObjects = NULL;
    nAllObjects = capAllObjects = 0;
    nCurrentLevel = -1;
}

int NewBlock(void)
{
    if (nCurrentLevel + 1 >= MAX_NEST_LEVEL) {
        fprintf(stderr, "Erro fatal: mais de %d niveis de aninhamento\n", MAX_NEST_LEVEL);
        exit(1);
    }
    SymbolTable[++nCurrentLevel] = NULL;
    SymbolTableLast[nCurrentLevel] = NULL;
    return nCurrentLevel;
}

int EndBlock(void)
{
    return --nCurrentLevel;
}

pobject Define(int aName)
{
    pobject obj = NewObject(aName);
    if (SymbolTable[nCurrentLevel] == NULL) {
        SymbolTable[nCurrentLevel] = obj;
        SymbolTableLast[nCurrentLevel] = obj;
    } else {
        SymbolTableLast[nCurrentLevel]->pNext = obj;
        SymbolTableLast[nCurrentLevel] = obj;
    }
    return obj;
}

pobject Search(int aName)
{
    pobject obj = SymbolTable[nCurrentLevel];
    while (obj != NULL) {
        if (obj->nName == aName) break;
        obj = obj->pNext;
    }
    return obj;
}

pobject Find(int aName)
{
    int i;
    pobject obj = NULL;
    for (i = nCurrentLevel; i >= 0; --i) {
        obj = SymbolTable[i];
        while (obj != NULL) {
            if (obj->nName == aName) break;
            obj = obj->pNext;
        }
        if (obj != NULL) break;
    }
    return obj;
}
