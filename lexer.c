/* =====================================================================
 * lexer.c
 *
 * Implementacao do Analisador Lexico da "Simple Script Language"
 * (apostila de Compiladores, cap. 2).
 *
 * Reconhece:
 *   - 19 palavras reservadas, busca binaria em tabela ordenada
 *     (VF: acrescentada "return")
 *   - simbolos (:  ;  ,  =  [  ]  {  }  (  )  &&  ||  <  >  <=  >=
 *               !=  ==  +  ++  -  --  *  /  .  !)
 *   - ID    = letra . (letra + digito)*
 *   - NUM   = digito . digito*
 *   - CHR   = ''' . any . '''
 *   - STR   = '"' . any* . '"'
 *
 * O analisador mantem sempre 1 caractere de lookahead (nextChar), como
 * descrito na apostila, para saber quando um lexema termina.
 * ===================================================================== */

#define _POSIX_C_SOURCE 200809L  /* para strdup() */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lexer.h"

#ifdef _MSC_VER                    /* VF: evita o aviso C4996 do MSVC */
#define strdup _strdup
#endif

/* --------------------------------------------------------------------
 * Tabela de palavras reservadas, EM ORDEM ALFABETICA (obrigatorio para
 * a busca binaria) e alinhada 1:1 com o inicio da enum t_token.
 * -------------------------------------------------------------------- */
static const char *const keywords[] = {
    "array", "boolean", "break", "char", "continue", "do", "else",
    "false", "function", "if", "integer", "of", "return", "string", "struct",
    "true", "type", "var", "while"
};
#define NUM_KEYWORDS (sizeof(keywords) / sizeof(keywords[0]))

/* --------------------------------------------------------------------
 * Estado global do analisador
 * -------------------------------------------------------------------- */
static FILE *src               = NULL;
static char  nextChar          = ' ';   /* garante 1a chamada correta */
static int   atEOF             = 0;

t_token token               = UNKNOWN;
int     tokenSecundario     = -1;
int     lineNumber          = 1;
int     tokenLine           = 1;

/* Pool de constantes (CHARACTER / NUMERAL / STRINGVAL) */
static t_const vConsts[MAX_CONSTS];
static int     nNumConsts = 0;

/* Tabela hash de identificadores (encadeamento separado) */
typedef struct NameEntry {
    char              *name;
    int                id;      /* token secundario (0, 1, 2, ...) */
    struct NameEntry  *next;
} NameEntry;

static NameEntry *nameTable[HASH_SIZE];
static int        nNumNames = 0;

/* ===================================================================
 * Funcoes auxiliares de baixo nivel
 * =================================================================== */

static char readChar(void)
{
    int c;
    if (atEOF) return '\0';
    c = fgetc(src);
    if (c == EOF) { atEOF = 1; return '\0'; }
    if (c == '\n') lineNumber++;
    return (char)c;
}

void lexerInit(FILE *input)
{
    src        = input;
    nextChar   = ' ';
    atEOF      = 0;
    nNumConsts = 0;
    nNumNames  = 0;
    lineNumber = 1;
    memset(nameTable, 0, sizeof(nameTable));
}

void lexerClose(void)
{
    int i;
    NameEntry *e, *n;
    for (i = 0; i < HASH_SIZE; i++) {
        for (e = nameTable[i]; e; e = n) {
            n = e->next;
            free(e->name);
            free(e);
        }
        nameTable[i] = NULL;
    }
    for (i = 0; i < nNumConsts; i++) {
        if (vConsts[i].type == 2) free(vConsts[i]._.sVal);
    }
}

/* ---- busca binaria na tabela de palavras reservadas ---- */
static t_token searchKeyWord(const char *name)
{
    int lo = 0, hi = (int)NUM_KEYWORDS - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int cmp = strcmp(name, keywords[mid]);
        if (cmp == 0) return (t_token)mid;   /* enum comeca em ARRAY = 0 */
        else if (cmp < 0) hi = mid - 1;
        else lo = mid + 1;
    }
    return ID;
}

/* ---- funcao de hash simples (djb2) para identificadores ---- */
static unsigned hashName(const char *s)
{
    unsigned h = 5381;
    while (*s) h = ((h << 5) + h) + (unsigned char)(*s++);
    return h % HASH_SIZE;
}

/* ---- insere/recupera o token secundario de um identificador ---- */
static int searchName(const char *name)
{
    unsigned h = hashName(name);
    NameEntry *e;
    for (e = nameTable[h]; e; e = e->next) {
        if (strcmp(e->name, name) == 0) return e->id;
    }
    e = (NameEntry *)malloc(sizeof(NameEntry));
    e->name = strdup(name);
    e->id   = nNumNames++;
    e->next = nameTable[h];
    nameTable[h] = e;
    return e->id;
}

/* ---- insercao de constantes no pool ---- */
static int addCharConst(char c)
{
    if (nNumConsts >= MAX_CONSTS) {
        fprintf(stderr, "Erro lexico (linha %d): pool de constantes cheio\n", lineNumber);
        exit(1);
    }
    vConsts[nNumConsts].type    = 0;
    vConsts[nNumConsts]._.cVal  = c;
    return nNumConsts++;
}

static int addIntConst(const char *numeral)
{
    if (nNumConsts >= MAX_CONSTS) {
        fprintf(stderr, "Erro lexico (linha %d): pool de constantes cheio\n", lineNumber);
        exit(1);
    }
    vConsts[nNumConsts].type    = 1;
    vConsts[nNumConsts]._.nVal  = atoi(numeral);
    return nNumConsts++;
}

static int addStringConst(const char *s)
{
    if (nNumConsts >= MAX_CONSTS) {
        fprintf(stderr, "Erro lexico (linha %d): pool de constantes cheio\n", lineNumber);
        exit(1);
    }
    vConsts[nNumConsts].type    = 2;
    vConsts[nNumConsts]._.sVal  = strdup(s);
    return nNumConsts++;
}

/* ===================================================================
 * API publica de acesso ao pool de constantes / nomes
 * =================================================================== */

char *getName(int index)
{
    int i;
    for (i = 0; i < HASH_SIZE; i++) {
        NameEntry *e;
        for (e = nameTable[i]; e; e = e->next) {
            if (e->id == index) return e->name;
        }
    }
    return NULL;
}

int getCharConst(int index)   { return vConsts[index]._.cVal; }
int getIntConst(int index)    { return vConsts[index]._.nVal; }
char *getStringConst(int index) { return vConsts[index]._.sVal; }

/* ===================================================================
 * nextToken() - implementa o automato finito do Analisador Lexico
 * =================================================================== */

t_token nextToken(void)
{
    /* --- estado inicial: pula separadores (espaco, tab, \n, \r, \v, \f) --- */
    while (!atEOF && isspace((unsigned char)nextChar)) {
        nextChar = readChar();
    }
    /* VF: lineNumber ja pode ter avancado ao ler o '\n' de lookahead
     * do token anterior; a linha do token e a do seu 1o caractere. */
    tokenLine = lineNumber;

    if (atEOF && !isspace((unsigned char)nextChar) && nextChar == '\0') {
        /* nada mais a ler */
        if (feof(src) || atEOF) {
            token = END_OF_FILE;
            return token;
        }
    }

    /* --- ID / palavra reservada: letra . (letra + digito)* --- */
    if (isalpha((unsigned char)nextChar)) {
        char text[MAX_ID_LEN + 1];
        int i = 0;
        do {
            if (i < MAX_ID_LEN) text[i++] = nextChar;
            nextChar = readChar();
        } while (!atEOF && isalnum((unsigned char)nextChar));
        /* Id = letra . (letra + digito)*  : a primeira posicao exige
         * letra e as seguintes aceitam letra OU digito. Nao aceitamos
         * '_' (o pseudo-codigo do livro aceita, a gramatica formal nao).
         * VF: a VC usava isalpha() aqui, o que partia "x1" em ID + NUM. */
        text[i] = '\0';

        token = searchKeyWord(text);
        if (token == ID) {
            tokenSecundario = searchName(text);
        } else {
            tokenSecundario = -1;
        }
        return token;
    }

    /* --- NUM: digito . digito* --- */
    if (isdigit((unsigned char)nextChar)) {
        char numeral[MAX_NUM_LEN + 1];
        int i = 0;
        do {
            if (i < MAX_NUM_LEN) numeral[i++] = nextChar;
            nextChar = readChar();
        } while (!atEOF && isdigit((unsigned char)nextChar));
        numeral[i] = '\0';

        token = NUMERAL;
        tokenSecundario = addIntConst(numeral);
        return token;
    }

    /* --- STR: '"' . any* . '"' --- */
    if (nextChar == '"') {
        char string[MAX_STR_LEN + 1];
        int i = 0;
        nextChar = readChar();               /* consome a aspa de abertura */
        while (!atEOF && nextChar != '"') {
            if (i < MAX_STR_LEN) string[i++] = nextChar;
            nextChar = readChar();
        }
        if (atEOF) {
            fprintf(stderr, "Erro lexico (linha %d): string nao terminada\n", lineNumber);
            token = UNKNOWN;
            return token;
        }
        string[i] = '\0';
        nextChar = readChar();               /* consome a aspa de fechamento */

        token = STRINGVAL;
        tokenSecundario = addStringConst(string);
        return token;
    }

    /* --- CHR: ''' . any . ''' --- */
    if (nextChar == '\'') {
        char c;
        nextChar = readChar();               /* le o caractere do meio */
        if (atEOF) {
            fprintf(stderr, "Erro lexico (linha %d): caractere nao terminado\n", lineNumber);
            token = UNKNOWN;
            return token;
        }
        c = nextChar;
        nextChar = readChar();               /* deveria ser a aspa de fechamento */
        if (nextChar != '\'') {
            fprintf(stderr, "Erro lexico (linha %d): esperado ''' apos caractere\n", lineNumber);
            token = UNKNOWN;
            return token;
        }
        nextChar = readChar();               /* consome a aspa de fechamento */

        token = CHARACTER;
        tokenSecundario = addCharConst(c);
        return token;
    }

    /* --- simbolos --- */
    {
        char c = nextChar;
        tokenSecundario = -1;

        switch (c) {
            case ':': nextChar = readChar(); token = COLON; break;
            case ';': nextChar = readChar(); token = SEMI_COLON; break;
            case ',': nextChar = readChar(); token = COMMA; break;
            case '[': nextChar = readChar(); token = LEFT_SQUARE; break;
            case ']': nextChar = readChar(); token = RIGHT_SQUARE; break;
            case '{': nextChar = readChar(); token = LEFT_BRACES; break;
            case '}': nextChar = readChar(); token = RIGHT_BRACES; break;
            case '(': nextChar = readChar(); token = LEFT_PARENTHESIS; break;
            case ')': nextChar = readChar(); token = RIGHT_PARENTHESIS; break;
            case '.': nextChar = readChar(); token = DOT; break;
            case '*': nextChar = readChar(); token = TIMES; break;
            case '/': nextChar = readChar(); token = DIVIDE; break;

            case '=':
                nextChar = readChar();
                if (nextChar == '=') { token = EQUAL_EQUAL; nextChar = readChar(); }
                else                 { token = EQUALS; }
                break;

            case '&':
                nextChar = readChar();
                if (nextChar == '&') { token = AND; nextChar = readChar(); }
                else {
                    fprintf(stderr, "Erro lexico (linha %d): '&' isolado (esperado '&&')\n", lineNumber);
                    token = UNKNOWN;
                }
                break;

            case '|':
                nextChar = readChar();
                if (nextChar == '|') { token = OR; nextChar = readChar(); }
                else {
                    fprintf(stderr, "Erro lexico (linha %d): '|' isolado (esperado '||')\n", lineNumber);
                    token = UNKNOWN;
                }
                break;

            case '<':
                nextChar = readChar();
                if      (nextChar == '=') { token = LESS_OR_EQUAL; nextChar = readChar(); }
                else                       { token = LESS_THAN; }
                break;

            case '>':
                nextChar = readChar();
                if      (nextChar == '=') { token = GREATER_OR_EQUAL; nextChar = readChar(); }
                else                       { token = GREATER_THAN; }
                break;

            case '!':
                nextChar = readChar();
                if (nextChar == '=') { token = NOT_EQUAL; nextChar = readChar(); }
                else                  { token = NOT; }
                break;

            case '+':
                nextChar = readChar();
                if (nextChar == '+') { token = PLUS_PLUS; nextChar = readChar(); }
                else                  { token = PLUS; }
                break;

            case '-':
                nextChar = readChar();
                if (nextChar == '-') { token = MINUS_MINUS; nextChar = readChar(); }
                else                  { token = MINUS; }
                break;

            default:
                if (atEOF) {
                    token = END_OF_FILE;
                } else {
                    fprintf(stderr, "Erro lexico (linha %d): caractere invalido '%c' (0x%02x)\n",
                            lineNumber, isprint((unsigned char)c) ? c : '?', (unsigned char)c);
                    token = UNKNOWN;
                    nextChar = readChar();
                }
        }
        return token;
    }
}

/* ===================================================================
 * Utilitario: nome textual do token, para depuracao / testes
 * =================================================================== */
const char *tokenName(t_token tok)
{
    static const char *const names[] = {
        "ARRAY","BOOLEAN","BREAK","CHAR","CONTINUE","DO","ELSE","FALSE",
        "FUNCTION","IF","INTEGER","OF","RETURN","STRING","STRUCT","TRUE","TYPE",
        "VAR","WHILE",
        "COLON","SEMI_COLON","COMMA","EQUALS","LEFT_SQUARE","RIGHT_SQUARE",
        "LEFT_BRACES","RIGHT_BRACES","LEFT_PARENTHESIS","RIGHT_PARENTHESIS",
        "AND","OR","LESS_THAN","GREATER_THAN","LESS_OR_EQUAL",
        "GREATER_OR_EQUAL","NOT_EQUAL","EQUAL_EQUAL","PLUS","PLUS_PLUS",
        "MINUS","MINUS_MINUS","TIMES","DIVIDE","DOT","NOT",
        "CHARACTER","NUMERAL","STRINGVAL","ID",
        "UNKNOWN","END_OF_FILE"
    };
    if (tok < 0 || tok > END_OF_FILE) return "???";
    return names[tok];
}
