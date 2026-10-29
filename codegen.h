/* =====================================================================
 * codegen.h - Geracao de Codigo (cap. 7)
 *
 * O compilador gera codigo para a maquina virtual de pilha da secao
 * 7.1 (fig. 7.1). O codigo e gerado durante a analise sintatica, pelas
 * regras de atributos (semantics.c), que chamam Emit(). Ele fica numa
 * lista de instrucoes em memoria (em vez de um arquivo de texto, para
 * poder corrigir o BEGIN_FUNC sem fseek) e e impresso como Linguagem de
 * Montagem (PrintAssembly). Em seguida o Montador (Assemble) resolve os
 * labels em deslocamentos relativos, preenche o Pool de Funcoes e monta
 * o Array de Codigo que a maquina virtual (vm.c) executa.
 * ===================================================================== */

#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>

/* fig. 7.2 (instrucoes) e 7.3 (pseudo-instrucoes). OP_ADD_N e OP_MUL_N
 * sao o "ADD n" e o "MUL n" dos acessos a campos e arrays. */
typedef enum {
    OP_NOP,                                   /* instrucao removida (peephole) */
    OP_BEGIN_FUNC, OP_END_FUNC, OP_LABEL,     /* pseudo-instrucoes             */
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_INC, OP_DEC, OP_NEG,
    OP_AND, OP_OR, OP_NOT,
    OP_EQ, OP_NEQ, OP_LT, OP_LE, OP_GT, OP_GE,
    OP_JMP_FW, OP_JMP_BW, OP_TJMP_FW, OP_TJMP_BW,
    OP_CALL, OP_RET, OP_CALL_LIB,
    OP_LOAD_VAR, OP_STORE_VAR, OP_LOAD_CONST, OP_LOAD_TRUE, OP_LOAD_FALSE,
    OP_CHAR_AT, OP_SET_CHAR,
    OP_POP, OP_DUP,
    OP_ADD_N, OP_MUL_N,
    OP_LOAD_REF, OP_DE_REF, OP_STORE_REF,
    NUM_OPS
} t_op;

/* Uma instrucao. Argumentos:
 *   BEGIN_FUNC a=indice b=tamanho dos parametros c=tamanho das variaveis
 *   LABEL      a=numero do label
 *   JMP_*,TJMP_*  a=label (apos a montagem: a=deslocamento, c=label)
 *   CALL_LIB   a=biblioteca b=funcao
 *   demais com argumento: a */
typedef struct { t_op op; int a, b, c; } t_instr;

/* --------------------------------------------------- Pool de Funcoes */
#define MAX_FUNCS 256
#define MAX_ARGS  16

/* tipos escalares reconhecidos pelo executor (--run) */
enum { K_INT, K_CHAR, K_BOOL, K_STRING, K_OTHER };

typedef struct {
    char *name;
    int   nParams;               /* tamanho dos parametros (celulas)      */
    int   nVars;                 /* tamanho das variaveis locais (celulas) */
    int   ip;                    /* endereco da 1a instrucao (montador)   */
    int   retSize, retKind;      /* tamanho e tipo do valor de retorno    */
    int   nArgs;                 /* quantidade de parametros declarados   */
    int   argKind[MAX_ARGS];     /* tipo de cada parametro                */
} t_func;

extern t_func FuncPool[MAX_FUNCS];
extern int    nFuncPool;

/* --------------------------------------------------- Pool de Constantes
 * Indexado pelo token secundario do literal (como no livro: a numeracao
 * e unica em todo o arquivo e cada literal tem o seu indice). */
typedef struct { int kind; long i; char *s; } t_pconst;   /* kind: K_INT, K_CHAR ou K_STRING; -1 = vazio */
extern t_pconst ConstPool[];
extern int      nConstPool;
void AddConst(int pos, int kind);

/* --------------------------------------------------- Codigo gerado */
extern t_instr *Code;
extern int      nCode;

void ResetCodegen(void);
void FreeCodegen(void);
int  NewLabel(void);                          /* fig. 7.20 */
int  Emit(t_op op, int a, int b, int c);      /* devolve o indice da instrucao */

/* Linguagem de Montagem (a saida do compilador, "P.I" da fig. 1.3) */
void PrintAssembly(FILE *f);

/* --------------------------------------------------- Montador
 * Mem e o Array de Codigo: um t_instr por BYTE de codigo, na posicao do
 * primeiro byte de cada instrucao (tamanhos da fig. 7.2), de modo que o
 * registrador ip da maquina virtual e os deslocamentos dos desvios sao
 * enderecos de byte, como no livro. Posicoes sem instrucao tem OP_NOP. */
extern t_instr *Mem;
extern int      nMem;
int  InstrSize(t_op op);
const char *OpName(t_op op);
int  Assemble(void);                          /* 0 = ok */
void PrintAssembled(FILE *f);                 /* enderecos, pools e desvios relativos */

#endif /* CODEGEN_H */
