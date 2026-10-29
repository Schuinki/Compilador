/* =====================================================================
 * tests.c - testes da VF (lexer corrigido, tabela la-LR(1), sintatico,
 * escopo, tipos e geracao de codigo). Uso:  ./testes   (rodar na raiz
 * do repositorio)
 * ===================================================================== */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "grammar.h"
#include "lalr.h"
#include "semantics.h"
#include "parser.h"
#include "codegen.h"
#include "vm.h"

static int nTests = 0;

/* copia de string em memoria propria (strdup nao e C11 puro) */
static char *Dup(const char *s)
{
    char *p = malloc(strlen(s) + 1);
    strcpy(p, s);
    return p;
}

static int CompileString(const char *src)
{
    FILE *f = tmpfile();
    int n;
    assert(f != NULL);
    fputs(src, f);
    rewind(f);
    n = Compile(f);
    fclose(f);
    return n;
}

static int CompileFile(const char *path)
{
    FILE *f = fopen(path, "r");
    int n;
    if (!f) { perror(path); assert(0); }
    n = Compile(f);
    fclose(f);
    return n;
}

/* o programa deve compilar sem nenhum erro */
static void Valid(const char *src)
{
    int n = CompileString(src);
    if (n != 0) fprintf(stderr, "FALHOU (esperava 0 erros, obteve %d):\n%s\n", n, src);
    assert(n == 0);
    nTests++;
}

/* o programa deve ter exatamente um erro, do codigo indicado */
static void OneError(t_error e, const char *src)
{
    int n = CompileString(src);
    if (n != 1 || !HasError(e))
        fprintf(stderr, "FALHOU (esperava 1 erro %d, obteve %d):\n%s\n", e, n, src);
    assert(n == 1 && HasError(e) == 1);
    assert(Mem == NULL);               /* com erros nao ha codigo montado */
    nTests++;
}

/* corpo de funcao padrao para encurtar os casos */
#define FN(decls, cmds) \
    "type Ponto = struct { x, y : integer }\n" \
    "type Vetor = array [5] of integer\n" \
    "function f(n : integer, s : string) : integer\n" \
    "{ var i : integer; var b : boolean; var c : char; var p : Ponto; var v : Vetor;\n" \
    decls "\n" cmds "\n return i; }\n"

static void TestLexer(void)
{
    FILE *f = tmpfile();
    fputs("x1 = 2;\nreturn\nabc9z", f);
    rewind(f);
    lexerInit(f);
    assert(nextToken() == ID && strcmp(getName(tokenSecundario), "x1") == 0);
    assert(tokenLine == 1);
    assert(nextToken() == EQUALS);
    assert(nextToken() == NUMERAL && getIntConst(tokenSecundario) == 2);
    assert(nextToken() == SEMI_COLON && tokenLine == 1);   /* VC dizia 2 */
    assert(nextToken() == RETURN && tokenLine == 2);
    assert(nextToken() == ID && strcmp(getName(tokenSecundario), "abc9z") == 0);
    assert(tokenLine == 3);
    assert(nextToken() == END_OF_FILE);
    lexerClose();
    fclose(f);
    nTests++;
}

static void TestTable(void)
{
    BuildLALRTable(NULL);
    assert(nShiftReduce == 1);
    assert(nReduceReduce == 0);
    nTests++;
}

static void TestValid(void)
{
    assert(CompileFile("exemplos/fatorial.ssl") == 0);
    assert(CompileFile("exemplos/strings.ssl") == 0);
    assert(CompileFile("exemplos/ordena.ssl") == 0);
    assert(CompileFile("exemplos/tipos.ssl") == 0);
    assert(CompileFile("exemplos/estruturas.ssl") == 0);
    nTests += 5;

    /* else pendente: associa ao if mais interno, sem erro */
    Valid(FN("", "if (b) if (b) i = 1; else i = 2;"));
    /* sombreamento em bloco interno; parametro visivel no corpo */
    Valid(FN("", "{ var n : boolean; n = true; } n = n + 1;"));
    /* break/continue em laco aninhado e em do-while */
    Valid(FN("", "while (b) { var k : integer; do { var z : integer; continue; } while (b); break; }"));
    /* alias e sinonimo do tipo base; alias de struct da acesso a campo */
    Valid("type N = integer\n"
          "type P = struct { x : integer }\n"
          "type Q = P\n"
          "function g(n : N, q : Q) : N { var m : integer; var p : P;"
          " m = n; p = q; q.x = m; return q.x + p.x; }\n");
    /* recursao e chamada com varios argumentos */
    Valid("function f(n : integer, s : string) : integer"
          " { var r : integer; r = f(n - 1, s + \"a\"); return r; }\n");
    /* indexacao de string produz char, tambem como left-value */
    Valid(FN("", "c = s[1]; s[0] = c; b = s == \"ab\";"));
}

static void TestSyntax(void)
{
    OneError(ERR_SYNTAX, FN("", "i = n +;"));
    OneError(ERR_SYNTAX, FN("", "i++;"));                 /* expressao nao e comando */
    OneError(ERR_SYNTAX, "var x : integer;\n");           /* nao ha variaveis globais */
    OneError(ERR_SYNTAX, FN("", "if (b) { i = 1; }"));     /* bloco sem declaracao */
    OneError(ERR_SYNTAX, "function f() : integer { var x : integer; return x; }\n");
}

static void TestLexical(void)
{
    int n = CompileString(FN("", "i = 1 # 2;"));
    assert(n == 1 && HasError(ERR_LEXICAL) == 1 && !HasError(ERR_SYNTAX));
    nTests++;
}

static void TestScope(void)
{
    OneError(ERR_REDECL, FN("var i : integer;", ""));
    OneError(ERR_REDECL, "function f(a : integer, a : char) : integer { var x : integer; return x; }\n");
    OneError(ERR_REDECL, "type P = struct { x : integer; x : char }\n");
    OneError(ERR_REDECL, "type P = integer\ntype P = char\n");
    OneError(ERR_NOT_DECL, FN("", "i = y;"));
    /* o nao declarado e definido com kind universal: sem erros em cascata */
    OneError(ERR_NOT_DECL, FN("", "i = y; y = y + 1; i = y(2);"));
    /* variavel de bloco interno nao e visivel depois do bloco */
    OneError(ERR_NOT_DECL, FN("", "{ var w : integer; w = 1; } w = 2;"));
    /* redeclaracao nao altera o tipo da declaracao original (D7) */
    OneError(ERR_REDECL, FN("var i : char;", "i = 1;"));
    /* chamada posterior a declaracao de locais: parametros nao se
     * misturam com as variaveis locais (D7) */
    Valid("function f(a : integer) : integer { var x, y : integer; x = f(1); return x; }\n");
}

static void TestTypes(void)
{
    OneError(ERR_TYPE_EXPECTED,      FN("var k : n;", ""));
    OneError(ERR_TYPE_EXPECTED,      "type P = struct { x : P }\n");
    OneError(ERR_BOOL_TYPE_EXPECTED, FN("", "if (n) i = 1;"));
    OneError(ERR_BOOL_TYPE_EXPECTED, FN("", "while (s) i = 1;"));
    OneError(ERR_BOOL_TYPE_EXPECTED, FN("", "do i = 1; while (i);"));
    OneError(ERR_BOOL_TYPE_EXPECTED, FN("", "b = b && n;"));
    OneError(ERR_TYPE_MISMATCH,      FN("", "b = n + 1;"));
    OneError(ERR_TYPE_MISMATCH,      FN("", "b = n < \"a\";"));
    OneError(ERR_TYPE_MISMATCH,      FN("", "p = v;"));
    OneError(ERR_INVALID_TYPE,       FN("", "b = b + b;"));
    OneError(ERR_INVALID_TYPE,       FN("", "s = s - s;"));
    OneError(ERR_INVALID_TYPE,       FN("", "i = -b;"));
    OneError(ERR_INVALID_TYPE,       FN("", "b = !n;"));
    OneError(ERR_INVALID_TYPE,       FN("", "i = c++;"));
    OneError(ERR_KIND_NOT_STRUCT,    FN("", "i = n.x;"));
    OneError(ERR_FIELD_NOT_DECL,     FN("", "i = p.z;"));
    OneError(ERR_KIND_NOT_ARRAY,     FN("", "i = p[0];"));
    OneError(ERR_INVALID_INDEX_TYPE, FN("", "i = v[b];"));
    OneError(ERR_KIND_NOT_VAR,       FN("", "i = Ponto;"));
    OneError(ERR_KIND_NOT_VAR,       FN("", "i = f;"));
    OneError(ERR_KIND_NOT_FUNC,      FN("", "i = n(1);"));
    OneError(ERR_TOO_MANY_ARGS,      FN("", "i = f(1, \"a\", 2);"));
    OneError(ERR_TOO_FEW_ARGS,       FN("", "i = f(1);"));
    OneError(ERR_PARAM_TYPE,         FN("", "i = f(b, \"a\");"));
    OneError(ERR_RETURN_TYPE,        FN("", "return b;"));
    OneError(ERR_NOT_IN_LOOP,        FN("", "break;"));
    OneError(ERR_NOT_IN_LOOP,        FN("", "if (b) continue;"));
}

/* ------------------------------------------------ geracao de codigo */

/* a Linguagem de Montagem do programa compilado, em uma string (liberar) */
static char *AsmText(void)
{
    FILE *f = tmpfile();
    long n;
    char *s;
    assert(f != NULL);
    PrintAssembly(f);
    n = ftell(f);
    rewind(f);
    s = malloc(n + 1);
    assert(fread(s, 1, n, f) == (size_t)n);
    s[n] = '\0';
    fclose(f);
    return s;
}

static int FuncIndex(const char *name)
{
    int i;
    for (i = 0; i < nFuncPool; i++)
        if (strcmp(FuncPool[i].name, name) == 0) return i;
    fprintf(stderr, "funcao %s nao encontrada\n", name);
    assert(0);
    return -1;
}

/* executa uma funcao do programa compilado; argumentos inteiros (ou char/bool) */
static int RunN(const char *fn, int n, const long *v, t_cell *res)
{
    t_cell args[MAX_ARGS];
    int i;
    for (i = 0; i < n; i++) { args[i].isStr = 0; args[i].i = v[i]; args[i].s = NULL; }
    return RunFunction(FuncIndex(fn), args, n, res);
}

static long RunInt(const char *fn, int n, long a0, long a1)
{
    long v[2];
    t_cell res[MAX_RESULT];
    v[0] = a0; v[1] = a1;
    assert(RunN(fn, n, v, res) == 0);
    nTests++;
    return res[0].i;
}

static void TestCodegen(void)
{
    char *s;
    t_cell res[MAX_RESULT], arg[3];
    int i;

    bQuietRuntime = 1;

    /* --- fatorial.ssl: o codigo de fatorial() e o da secao 7.4.1 da
     * apostila (a apostila escreve LT onde o fonte tem <=; ver README) */
    assert(CompileFile("exemplos/fatorial.ssl") == 0);
    s = AsmText();
    assert(strstr(s,
        "BEGIN_FUNC 0 1 2\t\t; fatorial\n"
        "\tLOAD_CONST 0\n\tSTORE_VAR 1\n\tLOAD_CONST 1\n\tSTORE_VAR 2\n"
        "L0:\n\tLOAD_VAR 1\n\tLOAD_VAR 0\n\tLE\n\tTJMP_FW L1\n"
        "\tLOAD_VAR 2\n\tLOAD_VAR 1\n\tDUP\n\tINC\n\tSTORE_VAR 1\n\tMUL\n\tSTORE_VAR 2\n"
        "\tJMP_BW L0\nL1:\n\tLOAD_VAR 2\n\tRET\nEND_FUNC\n") != NULL);
    /* ... e o de somaFat() (labels L2 e L3, ++i e CALL 0) */
    assert(strstr(s, "BEGIN_FUNC 1 1 2\t\t; somaFat\n") != NULL);
    assert(strstr(s, "L2:\n\tLOAD_VAR 1\n\tINC\n\tDUP\n\tSTORE_VAR 1\n\tLOAD_VAR 0\n\tLE\n\tTJMP_FW L3\n"
                     "\tLOAD_VAR 2\n\tLOAD_VAR 1\n\tCALL 0\n\tADD\n\tSTORE_VAR 2\n\tJMP_BW L2\nL3:\n") != NULL);
    free(s);
    nTests++;

    /* --- montador: enderecos, desvios relativos e pool de funcoes */
    assert(nMem > 0 && FuncPool[0].ip == 0 && FuncPool[1].ip > 0);
    assert(FuncPool[0].nParams == 1 && FuncPool[0].nVars == 2 && FuncPool[0].retSize == 1);
    for (i = 0; i < nMem; i++) {
        t_instr *p = &Mem[i];
        int target;
        if (p->op == OP_JMP_FW || p->op == OP_TJMP_FW)      target = i + p->a;
        else if (p->op == OP_JMP_BW || p->op == OP_TJMP_BW) target = i - p->a;
        else continue;
        assert(target >= 0 && target < nMem && Mem[target].op != OP_NOP);
        assert(p->op == OP_JMP_FW || p->op == OP_TJMP_FW ? p->a > 0 : p->a >= 0);
    }
    nTests++;

    /* --- execucao */
    assert(RunInt("fatorial", 1, 5, 0) == 120);
    assert(RunInt("fatorial", 1, 1, 0) == 1);
    assert(RunInt("fatorial", 1, 10, 0) == 3628800);
    assert(RunInt("somaFat", 1, 4, 0) == 33);                 /* 1 + 2 + 6 + 24 */

    /* --- strings: CHAR_AT e SET_CHAR (apostila 7.4.2 e 7.4.3) */
    assert(CompileFile("exemplos/strings.ssl") == 0);
    s = AsmText();
    assert(strstr(s, "\tCHAR_AT 0\n") != NULL && strstr(s, "\tSET_CHAR 0\n") != NULL);
    free(s);
    arg[0].isStr = 1; arg[0].s = Dup("compiladores"); arg[0].i = 0;
    arg[1].isStr = 0; arg[1].i = 12; arg[1].s = NULL;
    assert(RunFunction(FuncIndex("inverte"), arg, 2, res) == 0);
    assert(strcmp(res[0].s, "serodalipmoc") == 0);
    free(arg[0].s);
    arg[0].isStr = 1; arg[0].s = Dup("abc"); arg[0].i = 0;
    arg[1].isStr = 1; arg[1].s = Dup("abd"); arg[1].i = 0;
    arg[2].isStr = 0; arg[2].i = 2; arg[2].s = NULL;
    assert(RunFunction(FuncIndex("iguais"), arg, 3, res) == 0 && res[0].i == 1);
    arg[2].i = 3;
    assert(RunFunction(FuncIndex("iguais"), arg, 3, res) == 0 && res[0].i == 0);
    free(arg[0].s); free(arg[1].s);
    nTests += 3;

    /* --- array por valor, DE_REF 30 / STORE_REF 30 (apostila 7.4.4) */
    assert(CompileFile("exemplos/ordena.ssl") == 0);
    s = AsmText();
    assert(strstr(s, "BEGIN_FUNC 0 31 6\t\t; bSort\n") != NULL);   /* 30 + 1 celulas de parametros */
    assert(strstr(s, "\tLOAD_REF 0\n\tDE_REF 30\n\tRET\nEND_FUNC\n") != NULL);
    free(s);
    assert(FuncPool[0].retSize == 30);
    assert(RunInt("testeOrdena", 1, 3, 0) == 1347);           /* {3,0,7,4,1} ordenado */

    /* --- structs, arrays de struct, campos, ++/-- em campos, retorno composto,
     *     strings em campos (CALL_LIB), do-while com continue e break */
    assert(CompileFile("exemplos/estruturas.ssl") == 0);
    s = AsmText();
    assert(strstr(s, "\tDUP\n\tDUP\n\tDE_REF 1\n\tINC\n\tSTORE_REF 1\n\tDE_REF 1\n\tDEC\n") != NULL);
    assert(strstr(s, "\tCALL_LIB 0, 0\n") != NULL && strstr(s, "\tCALL_LIB 0, 1\n") != NULL);
    free(s);
    assert(RunInt("perimetro", 1, 3, 0) == 28);
    assert(RunInt("reta", 1, 3, 0) == 546);
    assert(RunInt("laco", 1, 5, 0) == 46);                    /* continue em 3, break em 6 */
    assert(RunInt("logica", 2, 1, 2) == 1);
    assert(RunInt("logica", 2, 0, 2) == 0);
    assert(RunInt("logica", 2, 0, 7) == 1);
    assert(RunInt("divmod", 2, 17, 5) == 302);
    assert(RunInt("ehDigito", 1, '7', 0) == 1);
    assert(RunInt("ehDigito", 1, 'x', 0) == 0);
    assert(RunInt("somaVet", 1, 10, 0) == 60);
    assert(RunInt("usaPonto", 1, 4, 0) == 12);
    assert(RunInt("fib", 1, 15, 0) == 610);
    {
        long v = 2;
        assert(RunN("rotulo", 1, &v, res) == 0 && strcmp(res[0].s, "XbX!") == 0);
        v = 4;
        assert(RunN("ponto", 1, &v, res) == 0 && FuncPool[FuncIndex("ponto")].retSize == 2);
        assert(res[0].i == 4 && res[1].i == 8);
    }
    nTests += 3;

    /* --- erros de execucao */
    assert(CompileString(
        "function f(a : integer, b : integer) : integer { var q : integer; q = a / b; return q; }\n"
        "function g(s : string, i : integer) : char { var c : char; c = s[i]; return c; }\n") == 0);
    {
        long v[2];
        v[0] = 7; v[1] = 0;
        assert(RunN("f", 2, v, res) == 1);                    /* divisao por zero */
        v[1] = 2;
        assert(RunN("f", 2, v, res) == 0 && res[0].i == 3);
    }
    arg[0].isStr = 1; arg[0].s = Dup("ab"); arg[0].i = 0;
    arg[1].isStr = 0; arg[1].i = 5; arg[1].s = NULL;
    assert(RunFunction(FuncIndex("g"), arg, 2, res) == 1);    /* indice fora do string */
    arg[1].i = 1;
    assert(RunFunction(FuncIndex("g"), arg, 2, res) == 0 && res[0].i == 'b');
    free(arg[0].s);
    nTests += 4;

    /* --- laco infinito: o executor desiste */
    assert(CompileString(
        "function f(n : integer) : integer { var i : integer; i = 0; while (true) i = i + 1; return i; }\n") == 0);
    {
        long v = 1;
        assert(RunN("f", 1, &v, res) == 1);
    }
    nTests++;

    /* --- depois de um erro de compilacao nao ha codigo montado (OneError ja confere) */
    bQuietRuntime = 0;
}

int main(void)
{
    bQuietErrors = 1;
    TestLexer();
    TestTable();
    TestValid();
    TestSyntax();
    TestLexical();
    TestScope();
    TestTypes();
    TestCodegen();
    VMFree();
    FreeCodegen();
    FreeLALRTable();
    printf("OK: %d testes\n", nTests);
    return 0;
}
