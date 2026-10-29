/* =====================================================================
 * grammar.h
 *
 * Gramatica da Simple Script Language usada pelo analisador sintatico
 * (Gramatica.pdf + marcadores semanticos dos cap. 5, 6 e 7 + extensoes
 * documentadas no README). Os terminais sao os proprios t_token do
 * lexer; os nao terminais sao numerados logo apos END_OF_FILE, de modo
 * que um unico inteiro indexa a tabela de acao (cap. 3.8 / fig. 4.6).
 * ===================================================================== */

#ifndef GRAMMAR_H
#define GRAMMAR_H

#include "lexer.h"

#define NUM_TERMINALS (END_OF_FILE + 1)
#define MAX_RHS       11            /* DF tem 10 simbolos + EOR */
#define EOR           (-1)            /* fim do lado direito de uma regra */

typedef enum {
    NT_START = NUM_TERMINALS,         /* simbolo inicial aumentado        */
    NT_P, NT_LDE, NT_DE, NT_T, NT_DT, NT_DC, NT_DF, NT_LP, NT_B, NT_LDV,
    NT_LS, NT_DV, NT_LI, NT_S, NT_E, NT_L, NT_R, NT_Y, NT_F, NT_LE, NT_LV,
    NT_IDD, NT_IDU, NT_ID, NT_TRUE, NT_FALSE, NT_CHR, NT_STR, NT_NUM,
    /* marcadores semanticos (regras vazias).
     * NB, NF, MF, MC: cap. 5 e 6.
     * ML, MT, MW, ME, MD: geracao de codigo dos comandos (cap. 7, 7.6) */
    NT_NB, NT_NF, NT_MF, NT_MC, NT_ML, NT_MT, NT_MW, NT_ME, NT_MD,
    NUM_SYMBOLS
} t_nont;

#define IS_TERMINAL(s) ((s) < NUM_TERMINALS)

typedef enum {
    R_START,                                   /* START -> P EOF          */
    R_P,
    R_LDE_REC, R_LDE,
    R_DE_DF, R_DE_DT,
    R_T_INTEGER, R_T_CHAR, R_T_BOOLEAN, R_T_STRING, R_T_IDU,
    R_DT_ARRAY, R_DT_STRUCT, R_DT_ALIAS,
    R_DC_REC, R_DC,
    R_DF,
    R_LP_REC, R_LP,
    R_B,
    R_LDV_REC, R_LDV,
    R_LS_REC, R_LS,
    R_DV,
    R_LI_REC, R_LI,
    R_S_IF, R_S_IF_ELSE, R_S_WHILE, R_S_DO, R_S_BLOCK, R_S_ASSIGN,
    R_S_BREAK, R_S_CONTINUE, R_S_RETURN,
    R_E_AND, R_E_OR, R_E_L,
    R_L_LT, R_L_GT, R_L_LE, R_L_GE, R_L_EQ, R_L_NE, R_L_R,
    R_R_PLUS, R_R_MINUS, R_R_Y,
    R_Y_TIMES, R_Y_DIVIDE, R_Y_F,
    R_F_LV, R_F_PRE_INC, R_F_PRE_DEC, R_F_POS_INC, R_F_POS_DEC, R_F_PAREN,
    R_F_CALL, R_F_MINUS, R_F_NOT, R_F_TRUE, R_F_FALSE, R_F_CHR, R_F_STR,
    R_F_NUM,
    R_LE_REC, R_LE,
    R_LV_FIELD, R_LV_INDEX, R_LV_IDU,
    R_IDD, R_IDU, R_ID,
    R_TRUE, R_FALSE, R_CHR, R_STR, R_NUM,
    R_NB, R_NF, R_MF, R_MC, R_ML, R_MT, R_MW, R_ME, R_MD,
    NUM_RULES
} t_rule;

typedef struct {
    int left;
    int rhs[MAX_RHS];                 /* terminado por EOR               */
} t_production;

extern const t_production Grammar[NUM_RULES];
extern int Len[NUM_RULES];            /* comprimento do lado direito      */
extern int Left[NUM_RULES];           /* nao terminal do lado esquerdo    */

void        GrammarInit(void);        /* preenche Len[] e Left[]          */
const char *SymbolName(int sym);      /* ex.: "';'", "LV", "identificador" */
void        PrintRule(FILE *f, int r);

#endif /* GRAMMAR_H */
