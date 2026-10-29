/* =====================================================================
 * vm.h - Maquina Virtual de pilha (cap. 7, secao 7.1)
 *
 * Duas pilhas (fig. 7.1): a Pilha de Valores (sp, bp), polimorfica
 * para os tipos escalares, e a Pilha Operacional (spo), que guarda o
 * registro de ativacao: bp e endereco de retorno de cada chamada.
 * ===================================================================== */

#ifndef VM_H
#define VM_H

#include <stdio.h>

typedef struct {
    int   isStr;                 /* 1: s e uma string; 0: i e inteiro/char/boolean */
    long  i;
    char *s;
} t_cell;

#define MAX_RESULT 1024

extern int bQuietRuntime;               /* 1: erros de execucao nao sao impressos (testes) */

/* Executa a funcao f do Pool de Funcoes com os argumentos dados (um por
 * celula de parametro). O valor de retorno (retSize celulas) vai em res.
 * Devolve 0 se terminou, 1 se houve erro de execucao (mensagem em stderr). */
int  RunFunction(int f, const t_cell *args, int nArgs, t_cell *res);
long VMSteps(void);                         /* instrucoes executadas na ultima execucao */
void VMFree(void);                          /* libera as strings criadas na execucao   */

/* imprime uma celula conforme o tipo (K_INT, K_CHAR, K_BOOL, K_STRING) */
void PrintCell(FILE *f, t_cell c, int kind);

#endif /* VM_H */
