/* =====================================================================
 * semantics.c - funcao Semantics(rule) chamada a cada reducao (fig. 4.6)
 *
 * Convencao (sugerida no final do cap. 4.1): TODO nao terminal empilha
 * exatamente um no na pilha semantica, mesmo sem atributos. Assim cada
 * caso le os nos do lado direito com TopSem(k) (k = 0 e o topo, que
 * corresponde ao nao terminal mais a direita), calcula o no resultante
 * em "res" e, ao final, os nos do lado direito sao desempilhados e
 * "res" e empilhado. Marcadores (regras vazias) leem nos abaixo do topo,
 * que pertencem a regra em andamento (ex.: MF, MC, LE, ME, MW).
 *
 * O livro escreve TopSem(-1), TopSem(-2) no cap. 6 e TopSem(offFromTop)
 * no cap. 4; aqui o deslocamento e sempre positivo (fig. 4.8).
 *
 * VF: as mesmas regras que verificam escopo e tipos tambem GERAM O
 * CODIGO (cap. 7): a chamada Emit() de cada regra corresponde a um
 * fprintf das figs. 7.18 a 7.29. O codigo vai para uma lista em memoria
 * (codegen.c) que so e usada se nao houver nenhum erro (ERROR_FLAG).
 * ===================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "grammar.h"
#include "parser.h"
#include "semantics.h"
#include "codegen.h"

/* ---------------------------------------------- tipos pre-definidos */
/* fig. 6.3, com o universal de kind UNIVERSAL_ (decisao D5). Cada tipo
 * escalar ocupa 1 celula da pilha de valores (fig. 7.12). */
static object int_       = { -1, NULL, NULL, SCALAR_TYPE_, 1, 0, { { NULL } } };
static object char_      = { -1, NULL, NULL, SCALAR_TYPE_, 1, 0, { { NULL } } };
static object bool_      = { -1, NULL, NULL, SCALAR_TYPE_, 1, 0, { { NULL } } };
static object string_    = { -1, NULL, NULL, SCALAR_TYPE_, 1, 0, { { NULL } } };
static object universal_ = { -1, NULL, NULL, UNIVERSAL_,   1, 0, { { NULL } } };
pobject pInt = &int_, pChar = &char_, pBool = &bool_, pString = &string_,
        pUniversal = &universal_;

/* ---------------------------------------------------------- erros */
#define MAX_ERRORS 1000
int nErrors = 0;
int bQuietErrors = 0;
static t_error ErrorList[MAX_ERRORS];

static const char *const ErrorMsg[NUM_ERRORS] = {
    [ERR_LEXICAL]            = "erro lexico",
    [ERR_SYNTAX]             = "erro sintatico",
    [ERR_REDECL]             = "identificador '%s' ja declarado neste escopo",
    [ERR_NOT_DECL]           = "identificador '%s' nao declarado",
    [ERR_TYPE_EXPECTED]      = "'%s' nao e um tipo",
    [ERR_BOOL_TYPE_EXPECTED] = "expressao booleana esperada",
    [ERR_TYPE_MISMATCH]      = "tipos incompativeis",
    [ERR_INVALID_TYPE]       = "tipo invalido para o operador",
    [ERR_KIND_NOT_STRUCT]    = "acesso a campo em algo que nao e struct",
    [ERR_FIELD_NOT_DECL]     = "campo '%s' nao declarado na struct",
    [ERR_KIND_NOT_ARRAY]     = "indexacao em algo que nao e array nem string",
    [ERR_INVALID_INDEX_TYPE] = "indice deve ser do tipo integer",
    [ERR_KIND_NOT_VAR]       = "'%s' nao e variavel nem parametro",
    [ERR_KIND_NOT_FUNC]      = "'%s' nao e funcao",
    [ERR_TOO_MANY_ARGS]      = "argumentos demais na chamada",
    [ERR_PARAM_TYPE]         = "tipo do argumento %d incompativel com o parametro",
    [ERR_TOO_FEW_ARGS]       = "argumentos de menos na chamada",
    [ERR_RETURN_TYPE]        = "tipo do return incompativel com o retorno da funcao",
    [ERR_NOT_IN_LOOP]        = "break/continue fora de comando de repeticao",
};

void RecordError(t_error e)
{
    if (nErrors < MAX_ERRORS) ErrorList[nErrors] = e;
    nErrors++;
}

int HasError(t_error e)
{
    int i, n = 0;
    for (i = 0; i < nErrors && i < MAX_ERRORS; i++) n += (ErrorList[i] == e);
    return n;
}

/* name: token secundario do identificador envolvido (ou o numero do
 * argumento em ERR_PARAM_TYPE); -1 quando nao se aplica */
void Error(t_error e, int name)
{
    RecordError(e);
    if (bQuietErrors) return;
    fprintf(stderr, "Erro %s (linha %d): ",
            (e == ERR_REDECL || e == ERR_NOT_DECL) ? "de escopo" :
            (e == ERR_NOT_IN_LOOP) ? "semantico" : "de tipo", lastLine);
    if (e == ERR_PARAM_TYPE) fprintf(stderr, ErrorMsg[e], name);
    else if (name >= 0)      fprintf(stderr, ErrorMsg[e], getName(name));
    else                     fprintf(stderr, "%s", ErrorMsg[e]);
    fprintf(stderr, "\n");
}

/* ------------------------------------------------- pilha semantica */
#define MAX_STACK_SIZE 1000
static t_attrib StackSem[MAX_STACK_SIZE];
static int      nSem = 0;

static void PushSem(t_attrib a)
{
    if (nSem == MAX_STACK_SIZE) {
        fprintf(stderr, "Erro fatal: pilha semantica cheia\n");
        exit(1);
    }
    StackSem[nSem++] = a;
}
static void     PopSem(int n)   { nSem -= n; }
static t_attrib TopSem(int off) { return StackSem[nSem - 1 - off]; }

/* ------------------------------------------------- estado auxiliar */
static pobject curFunction = NULL;     /* funcao cujo corpo esta em analise */
static int     nBeginFunc  = 0;        /* indice do BEGIN_FUNC no codigo    */
static int     nNonTerms[NUM_RULES];   /* nao terminais do lado direito     */

/* comandos de repeticao abertos: labels de inicio, de fim e (so no
 * do-while, criado quando o primeiro continue o exige) da reavaliacao da
 * condicao */
#define MAX_LOOPS 64
static struct { int start, end, cont, isWhile; } Loop[MAX_LOOPS];
static int nLoopDepth = 0;

void ResetSemantics(void)
{
    int r, k;
    nErrors = 0;
    nSem = 0;
    curFunction = NULL;
    nLoopDepth = 0;
    ResetSymbolTable();
    ResetCodegen();
    NewBlock();                         /* nivel 0: tipos e funcoes */
    for (r = 0; r < NUM_RULES; r++) {
        nNonTerms[r] = 0;
        for (k = 0; k < Len[r]; k++)
            if (!IS_TERMINAL(Grammar[r].rhs[k])) nNonTerms[r]++;
    }
}

/* ------------------------------------------------------ tipos */
/* Um alias e sinonimo do tipo base (decisao D6). */
static pobject BaseType(pobject t)
{
    while (t->eKind == ALIAS_TYPE_) t = t->_.Alias.pBaseType;
    return t;
}

/* Compatibilidade por nome (fig. 6.16) apos resolver os aliases. */
int CheckTypes(pobject t1, pobject t2)
{
    t1 = BaseType(t1);
    t2 = BaseType(t2);
    if (t1 == t2) return 1;
    if (t1->eKind == UNIVERSAL_ || t2->eKind == UNIVERSAL_) return 1;
    return 0;
}

/* tipo escalar de um tipo, para o executor (--run) */
static int KindOf(pobject t)
{
    t = BaseType(t);
    if (t == pInt)    return K_INT;
    if (t == pChar)   return K_CHAR;
    if (t == pBool)   return K_BOOL;
    if (t == pString) return K_STRING;
    return K_OTHER;
}

/* Preenche kind e tipo de uma lista de declaracao (figs. 6.6 e 6.10). */
static void SetKind(pobject p, t_kind kind, pobject t)
{
    for (; p != NULL; p = p->pNextDecl) {
        if (p->eKind != NO_KIND_DEF_) continue;
        p->eKind = kind;
        p->_.Var.pType = t;            /* Var, Param e Field: mesma forma */
    }
}

/* Calcula tamanho e enderecamento de uma lista de declaracao (figs.
 * 7.15 a 7.19): cada elemento ocupa o tamanho de T, a partir de *n. */
static void Layout(pobject p, pobject t, int *n)
{
    for (; p != NULL; p = p->pNextDecl) {
        p->nIndex = *n;
        p->nSize  = t->nSize;
        *n += t->nSize;
    }
}

static void CheckBool(pobject t)
{
    if (!CheckTypes(t, pBool)) Error(ERR_BOOL_TYPE_EXPECTED, -1);
}

static void CheckInt(pobject t, t_error e)
{
    if (!CheckTypes(t, pInt)) Error(e, -1);
}

/* ---------------------------------------- geracao de codigo auxiliar */
/* ++ e -- sobre um left-value (figs. 7.8 e 7.9). inc: 1 = ++, 0 = --;
 * pre: 1 = prefixo (o valor da expressao e o novo), 0 = posfixo. */
static void GenIncDec(t_attrib lv, int inc, int pre)
{
    t_op op = inc ? OP_INC : OP_DEC;
    t_op inv = inc ? OP_DEC : OP_INC;

    if (lv._.T.simple) {               /* variavel escalar: fig. 7.9 */
        Code[lv._.T.idx].op = OP_LOAD_VAR;
        if (pre) { Emit(op, 0, 0, 0); Emit(OP_DUP, 0, 0, 0); }
        else     { Emit(OP_DUP, 0, 0, 0); Emit(op, 0, 0, 0); }
        Emit(OP_STORE_VAR, lv._.T.val, 0, 0);
    } else {                           /* qualquer left-value: fig. 7.8 */
        Emit(OP_DUP, 0, 0, 0);
        Emit(OP_DUP, 0, 0, 0);
        Emit(OP_DE_REF, 1, 0, 0);
        Emit(op, 0, 0, 0);
        Emit(OP_STORE_REF, 1, 0, 0);
        Emit(OP_DE_REF, 1, 0, 0);
        if (!pre) Emit(inv, 0, 0, 0);
    }
}

static int PushLoop(int start, int end)
{
    if (nLoopDepth == MAX_LOOPS) {
        fprintf(stderr, "Erro fatal: mais de %d lacos aninhados\n", MAX_LOOPS);
        exit(1);
    }
    Loop[nLoopDepth].start = start;
    Loop[nLoopDepth].end = end;
    Loop[nLoopDepth].cont = -1;
    Loop[nLoopDepth].isWhile = 0;
    return nLoopDepth++;
}

/* ------------------------------------------------------ Semantics */
void Semantics(int rule)
{
    t_attrib res, a, b, c, d;
    pobject  p, t, f;
    int      name, n, l;

    memset(&res, 0, sizeof(res));
    res.nont = Left[rule];

    switch (rule) {

    /* ---------------- identificadores (figs. 5.8 e 6.4) ---------------- */
    case R_IDD:
        name = lastSecundario;
        if ((p = Search(name)) != NULL) {
            Error(ERR_REDECL, name);
            p = NewObject(name);       /* fora da tabela (decisao D7) */
        } else {
            p = Define(name);
        }
        p->eKind = NO_KIND_DEF_;
        res._.ID.obj = p;
        res._.ID.name = name;
        break;

    case R_IDU:
        name = lastSecundario;
        if ((p = Find(name)) == NULL) {
            Error(ERR_NOT_DECL, name);
            p = Define(name);
            p->eKind = UNIVERSAL_;     /* evita erros em cascata (D5) */
            p->nSize = 1;
        }
        res._.ID.obj = p;
        res._.ID.name = name;
        break;

    case R_ID:                         /* campo: resolvido pelo tipo (D2) */
        res._.ID.obj = NULL;
        res._.ID.name = lastSecundario;
        break;

    /* ---------------- literais (fig. 6.7) ---------------- */
    case R_TRUE:  res._.Lit.type = pBool; res._.Lit.val = 1; break;
    case R_FALSE: res._.Lit.type = pBool; res._.Lit.val = 0; break;
    case R_CHR:
        res._.Lit.type = pChar;
        res._.Lit.pos = lastSecundario;
        res._.Lit.val = getCharConst(res._.Lit.pos);
        break;
    case R_STR:
        res._.Lit.type = pString;
        res._.Lit.pos = lastSecundario;
        break;
    case R_NUM:
        res._.Lit.type = pInt;
        res._.Lit.pos = lastSecundario;
        res._.Lit.val = getIntConst(res._.Lit.pos);
        break;

    /* ---------------- uso de tipos (figs. 6.5 e 7.12) ---------------- */
    case R_T_INTEGER: res._.T.type = pInt;    break;
    case R_T_CHAR:    res._.T.type = pChar;   break;
    case R_T_BOOLEAN: res._.T.type = pBool;   break;
    case R_T_STRING:  res._.T.type = pString; break;
    case R_T_IDU:
        a = TopSem(0);
        p = a._.ID.obj;
        if (IS_TYPE_KIND(p->eKind) || p->eKind == UNIVERSAL_) {
            res._.T.type = p;
        } else {
            res._.T.type = pUniversal;
            Error(ERR_TYPE_EXPECTED, a._.ID.name);
        }
        break;

    /* ---------------- declaracao de tipos (figs. 6.8, 6.9, 6.11, 7.13 a 7.16) */
    case R_DT_ARRAY:                   /* IDD NUM T */
        a = TopSem(2); b = TopSem(1); c = TopSem(0);
        p = a._.ID.obj;
        p->eKind = ARRAY_TYPE_;
        p->_.Array.nNumElems = b._.Lit.val;
        p->_.Array.pElemType = c._.T.type;
        p->nSize = b._.Lit.val * c._.T.type->nSize;
        break;

    case R_DT_STRUCT:                  /* IDD NB DC */
        a = TopSem(2); c = TopSem(0);
        p = a._.ID.obj;
        p->eKind = STRUCT_TYPE_;
        p->_.Struct.pFields = c._.List.first;
        p->nSize = c._.List.size;
        EndBlock();
        break;

    case R_DT_ALIAS:                   /* IDD T */
        a = TopSem(1); b = TopSem(0);
        p = a._.ID.obj;
        p->eKind = ALIAS_TYPE_;
        p->_.Alias.pBaseType = b._.T.type;
        p->nSize = b._.T.type->nSize;
        break;

    case R_NB:                         /* figs. 5.9 e 6.14 */
        NewBlock();
        break;

    /* ---------------- campos de struct (figs. 6.10 e 7.15) ---------------- */
    case R_DC:                         /* LI T */
        a = TopSem(1); b = TopSem(0);
        n = 0;
        SetKind(a._.List.first, FIELD_, b._.T.type);
        Layout(a._.List.first, b._.T.type, &n);
        res._.List = a._.List;
        res._.List.size = n;
        break;

    case R_DC_REC:                     /* DC LI T */
        a = TopSem(2); b = TopSem(1); c = TopSem(0);
        n = a._.List.size;
        SetKind(b._.List.first, FIELD_, c._.T.type);
        Layout(b._.List.first, c._.T.type, &n);
        a._.List.last->pNextDecl = b._.List.first;
        res._.List.first = a._.List.first;
        res._.List.last = b._.List.last;
        res._.List.size = n;
        break;

    /* ---------------- variaveis (figs. 6.6 e 7.19) ---------------- */
    case R_LI:                         /* IDD */
        a = TopSem(0);
        res._.List.first = res._.List.last = a._.ID.obj;
        break;

    case R_LI_REC:                     /* LI IDD */
        a = TopSem(1); b = TopSem(0);
        a._.List.last->pNextDecl = b._.ID.obj;
        res._.List.first = a._.List.first;
        res._.List.last = b._.ID.obj;
        break;

    case R_DV:                         /* LI T */
        a = TopSem(1); b = TopSem(0);
        SetKind(a._.List.first, VAR_, b._.T.type);
        /* todas as variaveis da funcao, inclusive as de blocos internos,
         * compartilham a numeracao dos parametros (fig. 7.18) */
        n = curFunction != NULL ? curFunction->_.Function.nVars : 0;
        Layout(a._.List.first, b._.T.type, &n);
        if (curFunction != NULL) curFunction->_.Function.nVars = n;
        break;

    /* ---------------- funcoes (figs. 6.12, 6.13, 7.17 e 7.18; NF de 7.18) */
    case R_NF:                         /* ... IDD | NF */
        a = TopSem(0);
        f = a._.ID.obj;
        f->eKind = FUNCTION_;
        f->_.Function.pRetType = pUniversal;
        f->_.Function.pParams = NULL;
        if (nFuncPool == MAX_FUNCS) {
            fprintf(stderr, "Erro fatal: mais de %d funcoes\n", MAX_FUNCS);
            exit(1);
        }
        f->nIndex = nFuncPool++;       /* indice no Pool de Funcoes */
        {
            const char *s = getName(a._.ID.name);
            t_func *e = &FuncPool[f->nIndex];
            memset(e, 0, sizeof(*e));
            e->name = malloc(strlen(s) + 1);
            strcpy(e->name, s);
        }
        NewBlock();                    /* parametros e locais (D4) */
        break;

    case R_LP:                         /* IDD T */
        a = TopSem(1); b = TopSem(0);
        p = a._.ID.obj;
        p->eKind = PARAM_;
        p->_.Param.pType = b._.T.type;
        p->nIndex = 0;                 /* fig. 7.17: enderecamento comeca em 0 */
        p->nSize = b._.T.type->nSize;
        res._.List.first = res._.List.last = p;
        res._.List.size = p->nSize;
        break;

    case R_LP_REC:                     /* LP IDD T */
        a = TopSem(2); b = TopSem(1); c = TopSem(0);
        p = b._.ID.obj;
        p->eKind = PARAM_;
        p->_.Param.pType = c._.T.type;
        p->nIndex = a._.List.size;
        p->nSize = c._.T.type->nSize;
        a._.List.last->pNextDecl = p;
        res._.List.first = a._.List.first;
        res._.List.last = p;
        res._.List.size = a._.List.size + p->nSize;
        break;

    case R_MF:                         /* ... IDD NF LP T | MF */
        d = TopSem(3); b = TopSem(1); a = TopSem(0);
        f = d._.ID.obj;
        f->_.Function.pRetType = a._.T.type;
        f->_.Function.pParams = b._.List.first;
        f->_.Function.nParams = b._.List.size;
        f->_.Function.nVars = b._.List.size;      /* locais comecam apos os parametros */
        curFunction = f;
        {                              /* Pool de Funcoes */
            t_func *e = &FuncPool[f->nIndex];
            e->nParams = b._.List.size;
            e->retSize = a._.T.type->nSize;
            e->retKind = KindOf(a._.T.type);
            for (p = b._.List.first; p != NULL; p = p->pNextDecl) {
                if (e->nArgs < MAX_ARGS)
                    e->argKind[e->nArgs] = (p->nSize == 1) ? KindOf(p->_.Param.pType) : K_OTHER;
                e->nArgs++;
            }
        }
        /* o numero de variaveis so e conhecido no fim da funcao: o valor
         * 0 e corrigido em R_DF (o livro usa fseek para isso) */
        nBeginFunc = Emit(OP_BEGIN_FUNC, f->nIndex, f->_.Function.nParams, 0);
        break;

    case R_DF:
        Code[nBeginFunc].c = curFunction->_.Function.nVars - curFunction->_.Function.nParams;
        FuncPool[curFunction->nIndex].nVars = Code[nBeginFunc].c;
        Emit(OP_END_FUNC, 0, 0, 0);
        EndBlock();
        curFunction = NULL;
        break;

    /* ---------------- comandos (figs. 6.14, 6.15, 7.21 a 7.23) ---------------- */
    case R_S_BLOCK:                    /* NB B */
        EndBlock();
        break;

    case R_MT:                         /* depois de 'if' '(' E ')' */
        l = NewLabel();
        Emit(OP_TJMP_FW, l, 0, 0);
        res._.Mark.l1 = l;
        break;

    case R_S_IF:                       /* E MT S */
        CheckBool(TopSem(2)._.T.type);
        Emit(OP_LABEL, TopSem(1)._.Mark.l1, 0, 0);
        break;

    case R_ME:                         /* E MT S 'else' */
        a = TopSem(1);                 /* MT */
        l = NewLabel();
        Emit(OP_JMP_FW, l, 0, 0);
        Emit(OP_LABEL, a._.Mark.l1, 0, 0);
        res._.Mark.l1 = l;
        break;

    case R_S_IF_ELSE:                  /* E MT S ME S */
        CheckBool(TopSem(4)._.T.type);
        Emit(OP_LABEL, TopSem(1)._.Mark.l1, 0, 0);
        break;

    case R_ML:                         /* inicio de laco (D11): label de retorno */
        res._.Mark.l1 = NewLabel();
        res._.Mark.l2 = NewLabel();
        Emit(OP_LABEL, res._.Mark.l1, 0, 0);
        PushLoop(res._.Mark.l1, res._.Mark.l2);
        break;

    case R_MW:                         /* ML E: desvio para fora do while */
        a = TopSem(1);
        Emit(OP_TJMP_FW, a._.Mark.l2, 0, 0);
        Loop[nLoopDepth - 1].isWhile = 1;
        break;

    case R_S_WHILE:                    /* ML E MW S */
        a = TopSem(3);
        CheckBool(TopSem(2)._.T.type);
        Emit(OP_JMP_BW, a._.Mark.l1, 0, 0);
        Emit(OP_LABEL, a._.Mark.l2, 0, 0);
        nLoopDepth--;
        break;

    case R_MD:                         /* ML S 'while': destino do continue */
        if (Loop[nLoopDepth - 1].cont >= 0)
            Emit(OP_LABEL, Loop[nLoopDepth - 1].cont, 0, 0);
        break;

    case R_S_DO:                       /* ML S MD E */
        a = TopSem(3);
        CheckBool(TopSem(0)._.T.type);
        Emit(OP_NOT, 0, 0, 0);
        Emit(OP_TJMP_BW, a._.Mark.l1, 0, 0);
        Emit(OP_LABEL, a._.Mark.l2, 0, 0);
        nLoopDepth--;
        break;

    case R_S_ASSIGN:                   /* LV E */
        a = TopSem(1); b = TopSem(0);
        if (!CheckTypes(a._.T.type, b._.T.type))
            Error(ERR_TYPE_MISMATCH, -1);
        if (a._.T.str == 1) {          /* s[i] = c, s variavel */
            Emit(OP_SET_CHAR, a._.T.val, 0, 0);
        } else if (a._.T.str == 2) {   /* s[i] = c, s qualquer left-value */
            Emit(OP_CALL_LIB, 0, 1, 0);
        } else if (a._.T.simple) {     /* fig. 7.9: STORE_VAR n */
            Code[a._.T.idx].op = OP_NOP;           /* o LOAD_REF nao e mais preciso */
            Emit(OP_STORE_VAR, a._.T.val, 0, 0);
        } else {                       /* fig. 7.8: STORE_REF n */
            Emit(OP_STORE_REF, a._.T.type->nSize, 0, 0);
        }
        break;

    case R_S_BREAK:
        if (nLoopDepth == 0) Error(ERR_NOT_IN_LOOP, -1);
        else Emit(OP_JMP_FW, Loop[nLoopDepth - 1].end, 0, 0);
        break;

    case R_S_CONTINUE:
        if (nLoopDepth == 0) {
            Error(ERR_NOT_IN_LOOP, -1);
        } else if (Loop[nLoopDepth - 1].isWhile) {
            Emit(OP_JMP_BW, Loop[nLoopDepth - 1].start, 0, 0);
        } else {                       /* do-while: a condicao vem depois */
            if (Loop[nLoopDepth - 1].cont < 0) Loop[nLoopDepth - 1].cont = NewLabel();
            Emit(OP_JMP_FW, Loop[nLoopDepth - 1].cont, 0, 0);
        }
        break;

    case R_S_RETURN:                   /* E (extensao D3) */
        if (curFunction != NULL &&
            !CheckTypes(TopSem(0)._.T.type, curFunction->_.Function.pRetType))
            Error(ERR_RETURN_TYPE, -1);
        Emit(OP_RET, 0, 0, 0);         /* o montador completa o tamanho */
        break;

    /* ---------------- expressoes logicas (figs. 6.18 e 7.23) ---------------- */
    case R_E_AND:
    case R_E_OR:                       /* E L */
        CheckBool(TopSem(1)._.T.type);
        CheckBool(TopSem(0)._.T.type);
        res._.T.type = pBool;
        Emit(rule == R_E_AND ? OP_AND : OP_OR, 0, 0, 0);
        break;

    /* ---------------- relacionais (figs. 6.19 e 7.24) ---------------- */
    case R_L_LT: case R_L_GT: case R_L_LE:
    case R_L_GE: case R_L_EQ: case R_L_NE:          /* L R */
        if (!CheckTypes(TopSem(1)._.T.type, TopSem(0)._.T.type))
            Error(ERR_TYPE_MISMATCH, -1);
        res._.T.type = pBool;
        switch (rule) {
        case R_L_LT: Emit(OP_LT,  0, 0, 0); break;
        case R_L_GT: Emit(OP_GT,  0, 0, 0); break;
        case R_L_LE: Emit(OP_LE,  0, 0, 0); break;
        case R_L_GE: Emit(OP_GE,  0, 0, 0); break;
        case R_L_EQ: Emit(OP_EQ,  0, 0, 0); break;
        default:     Emit(OP_NEQ, 0, 0, 0); break;
        }
        break;

    /* ---------------- aritmeticas (figs. 6.20 e 7.25) ---------------- */
    case R_R_PLUS:                     /* R Y: integer ou concatenacao */
        a = TopSem(1); b = TopSem(0);
        if (!CheckTypes(a._.T.type, b._.T.type)) Error(ERR_TYPE_MISMATCH, -1);
        if (!CheckTypes(a._.T.type, pInt) && !CheckTypes(a._.T.type, pString))
            Error(ERR_INVALID_TYPE, -1);
        res._.T.type = a._.T.type;
        Emit(OP_ADD, 0, 0, 0);
        break;

    case R_R_MINUS: case R_Y_TIMES: case R_Y_DIVIDE:
        a = TopSem(1); b = TopSem(0);
        if (!CheckTypes(a._.T.type, b._.T.type)) Error(ERR_TYPE_MISMATCH, -1);
        CheckInt(a._.T.type, ERR_INVALID_TYPE);
        res._.T.type = a._.T.type;
        Emit(rule == R_R_MINUS ? OP_SUB : rule == R_Y_TIMES ? OP_MUL : OP_DIV, 0, 0, 0);
        break;

    /* regras de renomeacao: copiam o tipo e nao geram codigo */
    case R_E_L: case R_L_R: case R_R_Y: case R_Y_F: case R_F_PAREN:
        res._.T.type = TopSem(0)._.T.type;
        break;

    /* ---------------- fatores (figs. 6.21, 7.7, 7.8 e 7.26) ---------------- */
    case R_F_LV:                       /* left-value como right-value */
        a = TopSem(0);
        res._.T.type = a._.T.type;
        if (a._.T.str == 1) {
            Emit(OP_CHAR_AT, a._.T.val, 0, 0);
        } else if (a._.T.str == 2) {
            Emit(OP_CALL_LIB, 0, 0, 0);
        } else if (a._.T.simple) {     /* LOAD_REF n + DE_REF 1 -> LOAD_VAR n */
            Code[a._.T.idx].op = OP_LOAD_VAR;
        } else {
            Emit(OP_DE_REF, a._.T.type->nSize, 0, 0);
        }
        break;

    case R_F_PRE_INC: case R_F_PRE_DEC: case R_F_POS_INC: case R_F_POS_DEC:
        a = TopSem(0);
        CheckInt(a._.T.type, ERR_INVALID_TYPE);
        res._.T.type = pInt;
        GenIncDec(a, rule == R_F_PRE_INC || rule == R_F_POS_INC,
                     rule == R_F_PRE_INC || rule == R_F_PRE_DEC);
        break;

    case R_F_MINUS:
        CheckInt(TopSem(0)._.T.type, ERR_INVALID_TYPE);
        res._.T.type = pInt;
        Emit(OP_NEG, 0, 0, 0);
        break;

    case R_F_NOT:
        if (!CheckTypes(TopSem(0)._.T.type, pBool)) Error(ERR_INVALID_TYPE, -1);
        res._.T.type = pBool;
        Emit(OP_NOT, 0, 0, 0);
        break;

    case R_F_TRUE:  res._.T.type = pBool; Emit(OP_LOAD_TRUE,  0, 0, 0); break;
    case R_F_FALSE: res._.T.type = pBool; Emit(OP_LOAD_FALSE, 0, 0, 0); break;
    case R_F_CHR:
        a = TopSem(0);
        res._.T.type = a._.Lit.type;
        AddConst(a._.Lit.pos, K_CHAR);
        Emit(OP_LOAD_CONST, a._.Lit.pos, 0, 0);
        break;
    case R_F_STR:
        a = TopSem(0);
        res._.T.type = a._.Lit.type;
        AddConst(a._.Lit.pos, K_STRING);
        Emit(OP_LOAD_CONST, a._.Lit.pos, 0, 0);
        break;
    case R_F_NUM:
        a = TopSem(0);
        res._.T.type = a._.Lit.type;
        AddConst(a._.Lit.pos, K_INT);
        Emit(OP_LOAD_CONST, a._.Lit.pos, 0, 0);
        break;

    /* ---------------- left-values (figs. 6.22, 7.8 e 7.27) ---------------- */
    case R_LV_FIELD:                   /* LV ID */
        a = TopSem(1); b = TopSem(0);
        t = BaseType(a._.T.type);
        if (t->eKind != STRUCT_TYPE_) {
            if (t->eKind != UNIVERSAL_) Error(ERR_KIND_NOT_STRUCT, -1);
            res._.T.type = pUniversal;
        } else {
            for (p = t->_.Struct.pFields; p != NULL; p = p->pNextDecl)
                if (p->nName == b._.ID.name) break;
            if (p == NULL) {
                Error(ERR_FIELD_NOT_DECL, b._.ID.name);
                res._.T.type = pUniversal;
            } else {
                res._.T.type = p->_.Field.pType;
                Emit(OP_ADD_N, p->nIndex, 0, 0);   /* deslocamento do campo */
            }
        }
        break;

    case R_LV_INDEX:                   /* LV E */
        a = TopSem(1); b = TopSem(0);
        t = BaseType(a._.T.type);
        if (t == pString) {            /* elemento de string: um char */
            res._.T.type = pChar;
            if (a._.T.simple) {        /* CHAR_AT / SET_CHAR sobre a variavel */
                Code[a._.T.idx].op = OP_NOP;
                res._.T.str = 1;
                res._.T.val = a._.T.val;
            } else {                   /* referencia + indice na pilha, via CALL_LIB */
                res._.T.str = 2;
            }
        } else if (t->eKind != ARRAY_TYPE_) {
            if (t->eKind != UNIVERSAL_) Error(ERR_KIND_NOT_ARRAY, -1);
            res._.T.type = pUniversal;
        } else {
            res._.T.type = t->_.Array.pElemType;
            Emit(OP_MUL_N, t->_.Array.pElemType->nSize, 0, 0);
            Emit(OP_ADD, 0, 0, 0);
        }
        CheckInt(b._.T.type, ERR_INVALID_INDEX_TYPE);
        break;

    case R_LV_IDU:                     /* IDU */
        a = TopSem(0);
        p = a._.ID.obj;
        if (p->eKind != VAR_ && p->eKind != PARAM_) {
            if (p->eKind != UNIVERSAL_) Error(ERR_KIND_NOT_VAR, a._.ID.name);
            res._.T.type = pUniversal;
        } else {
            res._.T.type = p->_.Var.pType;
            res._.T.idx = Emit(OP_LOAD_REF, p->nIndex, 0, 0);
            res._.T.val = p->nIndex;
            res._.T.simple = (p->nSize == 1);
        }
        break;

    /* ---------------- chamada de funcao (figs. 6.23, 7.10 e 7.28) ---------------- */
    case R_MC:                         /* ... IDU | MC */
        a = TopSem(0);
        f = a._.ID.obj;
        if (f->eKind != FUNCTION_) {
            if (f->eKind != UNIVERSAL_) Error(ERR_KIND_NOT_FUNC, a._.ID.name);
            res._.MC.type = pUniversal;
            res._.MC.param = NULL;
            res._.MC.err = 1;
        } else {
            res._.MC.type = f->_.Function.pRetType;
            res._.MC.param = f->_.Function.pParams;
            res._.MC.err = 0;
        }
        break;

    case R_LE:                         /* ... MC '(' | E */
    case R_LE_REC:                     /* LE E */
        b = TopSem(0);                 /* E                       */
        a = TopSem(1);                 /* MC (R_LE) ou LE (R_LE_REC) */
        if (rule == R_LE) {
            p = a._.MC.param; res._.LE.err = a._.MC.err; n = 1;
        } else {
            p = a._.LE.param; res._.LE.err = a._.LE.err; n = a._.LE.n;
        }
        res._.LE.param = NULL;
        res._.LE.n = n + 1;
        if (!res._.LE.err) {
            if (p == NULL) {
                Error(ERR_TOO_MANY_ARGS, -1);
                res._.LE.err = 1;
            } else {
                if (!CheckTypes(p->_.Param.pType, b._.T.type))
                    Error(ERR_PARAM_TYPE, n);
                res._.LE.param = p->pNextDecl;
            }
        }
        break;

    case R_F_CALL:                     /* IDU MC LE */
        a = TopSem(2); b = TopSem(1); c = TopSem(0);
        res._.T.type = b._.MC.type;
        if (!b._.MC.err && !c._.LE.err && c._.LE.param != NULL)
            Error(ERR_TOO_FEW_ARGS, -1);
        /* os argumentos ja foram empilhados pelo codigo das expressoes */
        if (!b._.MC.err) Emit(OP_CALL, a._.ID.obj->nIndex, 0, 0);
        break;

    /* regras sem atributos: P, LDE, DE, B, LDV, LS */
    default:
        break;
    }

    PopSem(nNonTerms[rule]);
    PushSem(res);
}
