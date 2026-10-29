/* =====================================================================
 * parser.h - Analisador Sintatico Ascendente la-LR(1) (fig. 4.6)
 * ===================================================================== */

#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>

/* Token secundario e linha do ultimo terminal EMPILHADO. Na reducao
 * IDD -> Id, o lexer ja leu o lookahead, entao a variavel global
 * tokenSecundario do lexer ja pertence ao token seguinte (decisao D8). */
extern int lastSecundario;
extern int lastLine;

int Parse(void);                /* 1 = aceito, 0 = erro sintatico      */
int Compile(FILE *in);          /* lexico -> sintatico -> semantico
                                   -> geracao de codigo -> montagem;
                                   devolve o numero de erros           */

#endif /* PARSER_H */
