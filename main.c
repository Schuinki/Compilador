/* =====================================================================
 * main.c - driver do compilador da Simple Script Language
 *
 * Uso:  ./ssl arquivo.ssl             compila (lexico, sintatico, escopo,
 *                                     tipos e geracao de codigo) e
 *                                     imprime a Linguagem de Montagem
 *       ./ssl -o saida.asm arquivo.ssl   idem, gravando em saida.asm
 *       ./ssl --montado arquivo.ssl   compila e imprime o resultado da
 *                                     montagem: pools, enderecos de
 *                                     byte e desvios relativos
 *       ./ssl --run arquivo.ssl func [args...]
 *                                     compila e executa a funcao na
 *                                     maquina virtual
 *       ./ssl --tokens arquivo.ssl    tabela de tokens (saida da VC)
 *       ./ssl --tabela                gramatica, estatisticas e conflitos
 *                                     da tabela la-LR(1)
 * Sem arquivo, le da entrada padrao (exceto em --run). Mensagens de erro
 * e o resumo da compilacao vao para stderr; o codigo, para stdout.
 * ===================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "grammar.h"
#include "lalr.h"
#include "parser.h"
#include "codegen.h"
#include "vm.h"

/* Saida da VC (agora com a linha em que o token comeca). */
static void PrintTokens(FILE *in)
{
    t_token t;

    lexerInit(in);
    printf("%-6s %-20s %-12s %s\n", "LINHA", "TOKEN", "SECUNDARIO", "VALOR");
    printf("---------------------------------------------------------\n");
    while ((t = nextToken()) != END_OF_FILE) {
        printf("%-6d %-20s ", tokenLine, tokenName(t));

        if (t == ID) {
            printf("%-12d %s\n", tokenSecundario, getName(tokenSecundario));
        } else if (t == NUMERAL) {
            printf("%-12d %d\n", tokenSecundario, getIntConst(tokenSecundario));
        } else if (t == CHARACTER) {
            printf("%-12d '%c'\n", tokenSecundario, (char)getCharConst(tokenSecundario));
        } else if (t == STRINGVAL) {
            printf("%-12d \"%s\"\n", tokenSecundario, getStringConst(tokenSecundario));
        } else {
            printf("%-12s -\n", "-");
        }
    }
    printf("%-6d %-20s %-12s -\n", tokenLine, "END_OF_FILE", "-");
    lexerClose();
}

static void PrintTable(void)
{
    int r;
    BuildLALRTable(stdout);
    printf("\nGramatica (%d regras):\n", NUM_RULES);
    for (r = 0; r < NUM_RULES; r++) PrintRule(stdout, r);
    printf("\nTerminais: %d  Nao terminais: %d  Estados la-LR(1): %d\n",
           NUM_TERMINALS, NUM_SYMBOLS - NUM_TERMINALS, nStates);
    printf("Conflitos empilha/reduz: %d  reduz/reduz: %d\n",
           nShiftReduce, nReduceReduce);
}

/* A compilacao le o fonte duas vezes (ver Compile), entao a entrada
 * padrao e copiada para um arquivo temporario. */
static FILE *OpenSource(const char *path)
{
    FILE *f;
    int c;
    if (path != NULL) {
        f = fopen(path, "r");
        if (!f) perror(path);
        return f;
    }
    f = tmpfile();
    if (!f) { perror("tmpfile"); return NULL; }
    while ((c = getchar()) != EOF) fputc(c, f);
    rewind(f);
    return f;
}

/* ------------------------------------------------------------ --run */
static void PrintAny(t_cell c)
{
    if (c.isStr) PrintCell(stdout, c, K_STRING);
    else         PrintCell(stdout, c, K_INT);
}

/* Converte o argumento de linha de comando para uma celula do tipo do
 * parametro. Devolve 0 se o argumento nao tem esse tipo. */
static int ParseArg(const char *s, int kind, t_cell *c)
{
    char *end;
    c->isStr = 0; c->i = 0; c->s = NULL;
    switch (kind) {
    case K_INT:
        c->i = strtol(s, &end, 10);
        return *s != '\0' && *end == '\0';
    case K_BOOL:
        if (strcmp(s, "true") == 0)  { c->i = 1; return 1; }
        if (strcmp(s, "false") == 0) { c->i = 0; return 1; }
        return 0;
    case K_CHAR:
        if (strlen(s) == 1)                                { c->i = (unsigned char)s[0]; return 1; }
        if (strlen(s) == 3 && s[0] == '\'' && s[2] == '\'') { c->i = (unsigned char)s[1]; return 1; }
        return 0;
    case K_STRING:
        c->isStr = 1;
        c->s = malloc(strlen(s) + 1);
        strcpy(c->s, s);
        return 1;
    }
    return 0;
}

static int RunProgram(const char *name, int nArg, char *arg[])
{
    t_cell args[MAX_ARGS], res[MAX_RESULT];
    int f, i, ok = 0;
    t_func *fn;

    for (f = 0; f < nFuncPool; f++)
        if (strcmp(FuncPool[f].name, name) == 0) break;
    if (f == nFuncPool) {
        fprintf(stderr, "Funcao '%s' nao existe no programa\n", name);
        return 1;
    }
    fn = &FuncPool[f];
    if (nArg != fn->nArgs || fn->nArgs > MAX_ARGS) {
        fprintf(stderr, "%s espera %d argumento(s), recebeu %d\n", name, fn->nArgs, nArg);
        return 1;
    }
    memset(args, 0, sizeof(args));
    for (i = 0; i < nArg; i++) {
        if (fn->argKind[i] == K_OTHER) {
            fprintf(stderr, "Parametro %d de %s e struct/array: nao suportado em --run\n", i + 1, name);
            goto done;
        }
        if (!ParseArg(arg[i], fn->argKind[i], &args[i])) {
            fprintf(stderr, "Argumento %d ('%s') invalido para o tipo do parametro\n", i + 1, arg[i]);
            goto done;
        }
    }

    printf("%s(", name);
    for (i = 0; i < nArg; i++) {
        if (i) printf(", ");
        PrintCell(stdout, args[i], fn->argKind[i]);
    }
    printf(") = ");
    if (RunFunction(f, args, nArg, res) != 0) { printf("\n"); goto done; }
    if (fn->retKind == K_OTHER) {
        printf("{");
        for (i = 0; i < fn->retSize; i++) { printf(i ? ", " : " "); PrintAny(res[i]); }
        printf(" }");
    } else {
        PrintCell(stdout, res[0], fn->retKind);
    }
    printf("   [%ld instrucoes]\n", VMSteps());
    ok = 1;
done:
    for (i = 0; i < nArg && i < MAX_ARGS; i++)
        if (args[i].isStr) free(args[i].s);
    return ok ? 0 : 1;
}

/* ------------------------------------------------------------ main */
int main(int argc, char *argv[])
{
    enum { M_ASM, M_MONT, M_RUN, M_TOKENS } mode = M_ASM;
    const char *path = NULL, *outPath = NULL;
    int i, nErr, rest = argc;               /* rest: primeiro argumento apos o arquivo */
    FILE *in, *out;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--tabela") == 0) {
            PrintTable();
            FreeLALRTable();
            return 0;
        } else if (strcmp(argv[i], "--tokens") == 0) mode = M_TOKENS;
        else if (strcmp(argv[i], "--montado") == 0)  mode = M_MONT;
        else if (strcmp(argv[i], "--run") == 0)      mode = M_RUN;
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) outPath = argv[++i];
        else { path = argv[i]; rest = i + 1; break; }
    }
    if (mode == M_RUN && (path == NULL || rest >= argc)) {
        fprintf(stderr, "Uso: %s --run arquivo.ssl funcao [argumentos...]\n", argv[0]);
        return 1;
    }

    in = OpenSource(path);
    if (!in) return 1;

    if (mode == M_TOKENS) {
        PrintTokens(in);
        fclose(in);
        return 0;
    }

    nErr = Compile(in);
    fclose(in);

    if (nErr == 0) {
        if (mode == M_RUN) {
            nErr = RunProgram(argv[rest], argc - rest - 1, argv + rest + 1);
        } else if (mode == M_MONT) {
            PrintAssembled(stdout);
        } else {
            out = outPath ? fopen(outPath, "w") : stdout;
            if (!out) { perror(outPath); nErr = 1; }
            else {
                PrintAssembly(out);
                if (outPath) fclose(out);
            }
        }
        if (nErr == 0 && mode != M_RUN) fprintf(stderr, "Compilacao concluida sem erros.\n");
    } else {
        fprintf(stderr, "Compilacao concluida com %d erro(s).\n", nErr);
    }

    VMFree();
    FreeCodegen();
    FreeLALRTable();
    return nErr == 0 ? 0 : 1;
}
