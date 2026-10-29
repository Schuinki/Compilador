/* =====================================================================
 * lexer.h
 *
 * Analisador Lexico para a "Simple Script Language", definida no
 * capitulo 2 da apostila de Compiladores (Prof. Luiz Guedes - IME).
 *
 * A enumeracao abaixo segue a ordem apresentada na secao 2.2 da
 * apostila (VF: RETURN acrescentado em ordem alfabetica): palavras
 * reservadas em ordem alfabetica primeiro
 * (permite busca binaria), depois simbolos, depois os tokens
 * "regulares" (literais e identificador), e por fim UNKNOWN.
 * ===================================================================== */

#ifndef LEXER_H
#define LEXER_H

#include <stdio.h>

#define MAX_ID_LEN     255   /* tamanho maximo de um identificador   */
#define MAX_NUM_LEN    63    /* tamanho maximo de um literal numerico */
#define MAX_STR_LEN    4095  /* tamanho maximo de uma string literal  */
#define MAX_CONSTS     4096  /* tamanho da pool de constantes         */
#define HASH_SIZE      211   /* tamanho da tabela de identificadores  */

typedef enum {
    /* ---- palavras reservadas (ordem alfabetica) ---- */
    ARRAY, BOOLEAN, BREAK, CHAR, CONTINUE, DO, ELSE, FALSE_, FUNCTION, IF,
    INTEGER, OF, RETURN, STRING, STRUCT, TRUE_, TYPE, VAR, WHILE,

    /* ---- simbolos ---- */
    COLON, SEMI_COLON, COMMA, EQUALS, LEFT_SQUARE, RIGHT_SQUARE,
    LEFT_BRACES, RIGHT_BRACES, LEFT_PARENTHESIS, RIGHT_PARENTHESIS, AND,
    OR, LESS_THAN, GREATER_THAN, LESS_OR_EQUAL, GREATER_OR_EQUAL,
    NOT_EQUAL, EQUAL_EQUAL, PLUS, PLUS_PLUS, MINUS, MINUS_MINUS, TIMES,
    DIVIDE, DOT, NOT,

    /* ---- tokens regulares ---- */
    CHARACTER, NUMERAL, STRINGVAL, ID,

    /* ---- token desconhecido ---- */
    UNKNOWN,

    /* ---- fim de arquivo (nao esta na apostila, mas e necessario
     *      para o analisador sintatico saber quando parar)      ---- */
    END_OF_FILE
} t_token;

/* Constante do "pool" de constantes (CHARACTER / NUMERAL / STRINGVAL) */
typedef struct {
    unsigned char type;   /* 0-char, 1-int, 2-string */
    union {
        char  cVal;
        int   nVal;
        char *sVal;        /* alocado dinamicamente (malloc/strdup) */
    } _;
} t_const;

/* API publica do Analisador Lexico */
void          lexerInit(FILE *input);
void          lexerClose(void);
t_token       nextToken(void);
const char   *tokenName(t_token tok);

/* Valores associados ao ultimo token lido */
extern t_token token;            /* token principal            */
extern int     tokenSecundario;  /* token secundario (indice)  */
extern int     lineNumber;       /* linha do caractere de lookahead */
extern int     tokenLine;        /* VF: linha onde o ultimo token comecou */

/* Acesso ao pool de constantes / tabela de nomes */
char *getName(int index);
int   getCharConst(int index);
int   getIntConst(int index);
char *getStringConst(int index);

#endif /* LEXER_H */
