/* =====================================================================
 * grammar.c - producoes da Simple Script Language (ver grammar.h)
 * ===================================================================== */

#include "grammar.h"

#define R(...) { __VA_ARGS__, EOR }

const t_production Grammar[NUM_RULES] = {
    [R_START]      = { NT_START, R(NT_P, END_OF_FILE) },
    [R_P]          = { NT_P,   R(NT_LDE) },
    [R_LDE_REC]    = { NT_LDE, R(NT_LDE, NT_DE) },
    [R_LDE]        = { NT_LDE, R(NT_DE) },
    [R_DE_DF]      = { NT_DE,  R(NT_DF) },
    [R_DE_DT]      = { NT_DE,  R(NT_DT) },

    [R_T_INTEGER]  = { NT_T, R(INTEGER) },
    [R_T_CHAR]     = { NT_T, R(CHAR) },
    [R_T_BOOLEAN]  = { NT_T, R(BOOLEAN) },
    [R_T_STRING]   = { NT_T, R(STRING) },
    [R_T_IDU]      = { NT_T, R(NT_IDU) },

    [R_DT_ARRAY]   = { NT_DT, R(TYPE, NT_IDD, EQUALS, ARRAY, LEFT_SQUARE,
                                NT_NUM, RIGHT_SQUARE, OF, NT_T) },
    [R_DT_STRUCT]  = { NT_DT, R(TYPE, NT_IDD, EQUALS, STRUCT, NT_NB,
                                LEFT_BRACES, NT_DC, RIGHT_BRACES) },
    [R_DT_ALIAS]   = { NT_DT, R(TYPE, NT_IDD, EQUALS, NT_T) },

    [R_DC_REC]     = { NT_DC, R(NT_DC, SEMI_COLON, NT_LI, COLON, NT_T) },
    [R_DC]         = { NT_DC, R(NT_LI, COLON, NT_T) },

    [R_DF]         = { NT_DF, R(FUNCTION, NT_IDD, NT_NF, LEFT_PARENTHESIS,
                                NT_LP, RIGHT_PARENTHESIS, COLON, NT_T,
                                NT_MF, NT_B) },
    [R_LP_REC]     = { NT_LP, R(NT_LP, COMMA, NT_IDD, COLON, NT_T) },
    [R_LP]         = { NT_LP, R(NT_IDD, COLON, NT_T) },

    [R_B]          = { NT_B, R(LEFT_BRACES, NT_LDV, NT_LS, RIGHT_BRACES) },
    [R_LDV_REC]    = { NT_LDV, R(NT_LDV, NT_DV) },
    [R_LDV]        = { NT_LDV, R(NT_DV) },
    [R_LS_REC]     = { NT_LS, R(NT_LS, NT_S) },
    [R_LS]         = { NT_LS, R(NT_S) },
    [R_DV]         = { NT_DV, R(VAR, NT_LI, COLON, NT_T, SEMI_COLON) },
    [R_LI_REC]     = { NT_LI, R(NT_LI, COMMA, NT_IDD) },
    [R_LI]         = { NT_LI, R(NT_IDD) },

    /* VF (geracao de codigo): MT, ME, ML e MW/MD sao os marcadores MT,
     * ME e MW da secao 7.6; ver README (decisao D16). */
    [R_S_IF]       = { NT_S, R(IF, LEFT_PARENTHESIS, NT_E, RIGHT_PARENTHESIS,
                               NT_MT, NT_S) },
    [R_S_IF_ELSE]  = { NT_S, R(IF, LEFT_PARENTHESIS, NT_E, RIGHT_PARENTHESIS,
                               NT_MT, NT_S, ELSE, NT_ME, NT_S) },
    [R_S_WHILE]    = { NT_S, R(WHILE, NT_ML, LEFT_PARENTHESIS, NT_E,
                               RIGHT_PARENTHESIS, NT_MW, NT_S) },
    [R_S_DO]       = { NT_S, R(DO, NT_ML, NT_S, WHILE, NT_MD, LEFT_PARENTHESIS,
                               NT_E, RIGHT_PARENTHESIS, SEMI_COLON) },
    [R_S_BLOCK]    = { NT_S, R(NT_NB, NT_B) },
    [R_S_ASSIGN]   = { NT_S, R(NT_LV, EQUALS, NT_E, SEMI_COLON) },
    [R_S_BREAK]    = { NT_S, R(BREAK, SEMI_COLON) },
    [R_S_CONTINUE] = { NT_S, R(CONTINUE, SEMI_COLON) },
    [R_S_RETURN]   = { NT_S, R(RETURN, NT_E, SEMI_COLON) },

    [R_E_AND]      = { NT_E, R(NT_E, AND, NT_L) },
    [R_E_OR]       = { NT_E, R(NT_E, OR, NT_L) },
    [R_E_L]        = { NT_E, R(NT_L) },

    [R_L_LT]       = { NT_L, R(NT_L, LESS_THAN, NT_R) },
    [R_L_GT]       = { NT_L, R(NT_L, GREATER_THAN, NT_R) },
    [R_L_LE]       = { NT_L, R(NT_L, LESS_OR_EQUAL, NT_R) },
    [R_L_GE]       = { NT_L, R(NT_L, GREATER_OR_EQUAL, NT_R) },
    [R_L_EQ]       = { NT_L, R(NT_L, EQUAL_EQUAL, NT_R) },
    [R_L_NE]       = { NT_L, R(NT_L, NOT_EQUAL, NT_R) },
    [R_L_R]        = { NT_L, R(NT_R) },

    [R_R_PLUS]     = { NT_R, R(NT_R, PLUS, NT_Y) },
    [R_R_MINUS]    = { NT_R, R(NT_R, MINUS, NT_Y) },
    [R_R_Y]        = { NT_R, R(NT_Y) },

    [R_Y_TIMES]    = { NT_Y, R(NT_Y, TIMES, NT_F) },
    [R_Y_DIVIDE]   = { NT_Y, R(NT_Y, DIVIDE, NT_F) },
    [R_Y_F]        = { NT_Y, R(NT_F) },

    [R_F_LV]       = { NT_F, R(NT_LV) },
    [R_F_PRE_INC]  = { NT_F, R(PLUS_PLUS, NT_LV) },
    [R_F_PRE_DEC]  = { NT_F, R(MINUS_MINUS, NT_LV) },
    [R_F_POS_INC]  = { NT_F, R(NT_LV, PLUS_PLUS) },
    [R_F_POS_DEC]  = { NT_F, R(NT_LV, MINUS_MINUS) },
    [R_F_PAREN]    = { NT_F, R(LEFT_PARENTHESIS, NT_E, RIGHT_PARENTHESIS) },
    [R_F_CALL]     = { NT_F, R(NT_IDU, NT_MC, LEFT_PARENTHESIS, NT_LE,
                               RIGHT_PARENTHESIS) },
    [R_F_MINUS]    = { NT_F, R(MINUS, NT_F) },
    [R_F_NOT]      = { NT_F, R(NOT, NT_F) },
    [R_F_TRUE]     = { NT_F, R(NT_TRUE) },
    [R_F_FALSE]    = { NT_F, R(NT_FALSE) },
    [R_F_CHR]      = { NT_F, R(NT_CHR) },
    [R_F_STR]      = { NT_F, R(NT_STR) },
    [R_F_NUM]      = { NT_F, R(NT_NUM) },

    [R_LE_REC]     = { NT_LE, R(NT_LE, COMMA, NT_E) },
    [R_LE]         = { NT_LE, R(NT_E) },

    [R_LV_FIELD]   = { NT_LV, R(NT_LV, DOT, NT_ID) },
    [R_LV_INDEX]   = { NT_LV, R(NT_LV, LEFT_SQUARE, NT_E, RIGHT_SQUARE) },
    [R_LV_IDU]     = { NT_LV, R(NT_IDU) },

    [R_IDD]        = { NT_IDD, R(ID) },
    [R_IDU]        = { NT_IDU, R(ID) },
    [R_ID]         = { NT_ID,  R(ID) },
    [R_TRUE]       = { NT_TRUE,  R(TRUE_) },
    [R_FALSE]      = { NT_FALSE, R(FALSE_) },
    [R_CHR]        = { NT_CHR, R(CHARACTER) },
    [R_STR]        = { NT_STR, R(STRINGVAL) },
    [R_NUM]        = { NT_NUM, R(NUMERAL) },

    /* marcadores semanticos: regras vazias */
    [R_NB]         = { NT_NB, { EOR } },
    [R_NF]         = { NT_NF, { EOR } },
    [R_MF]         = { NT_MF, { EOR } },
    [R_MC]         = { NT_MC, { EOR } },
    [R_ML]         = { NT_ML, { EOR } },
    [R_MT]         = { NT_MT, { EOR } },
    [R_MW]         = { NT_MW, { EOR } },
    [R_ME]         = { NT_ME, { EOR } },
    [R_MD]         = { NT_MD, { EOR } },
};

int Len[NUM_RULES];
int Left[NUM_RULES];

void GrammarInit(void)
{
    int r, n;
    for (r = 0; r < NUM_RULES; r++) {
        for (n = 0; Grammar[r].rhs[n] != EOR; n++)
            ;
        Len[r]  = n;
        Left[r] = Grammar[r].left;
    }
}

const char *SymbolName(int sym)
{
    static const char *const terminals[NUM_TERMINALS] = {
        "'array'", "'boolean'", "'break'", "'char'", "'continue'", "'do'",
        "'else'", "'false'", "'function'", "'if'", "'integer'", "'of'",
        "'return'", "'string'", "'struct'", "'true'", "'type'", "'var'",
        "'while'",
        "':'", "';'", "','", "'='", "'['", "']'", "'{'", "'}'", "'('", "')'",
        "'&&'", "'||'", "'<'", "'>'", "'<='", "'>='", "'!='", "'=='", "'+'",
        "'++'", "'-'", "'--'", "'*'", "'/'", "'.'", "'!'",
        "caractere", "numero", "string", "identificador",
        "desconhecido", "fim de arquivo"
    };
    static const char *const nonterminals[NUM_SYMBOLS - NUM_TERMINALS] = {
        "START", "P", "LDE", "DE", "T", "DT", "DC", "DF", "LP", "B", "LDV",
        "LS", "DV", "LI", "S", "E", "L", "R", "Y", "F", "LE", "LV",
        "IDD", "IDU", "ID", "TRUE", "FALSE", "CHR", "STR", "NUM",
        "NB", "NF", "MF", "MC", "ML", "MT", "MW", "ME", "MD"
    };
    if (sym < 0 || sym >= NUM_SYMBOLS) return "???";
    return IS_TERMINAL(sym) ? terminals[sym] : nonterminals[sym - NUM_TERMINALS];
}

void PrintRule(FILE *f, int r)
{
    int k;
    fprintf(f, "%3d  %-5s ->", r, SymbolName(Left[r]));
    if (Len[r] == 0) fprintf(f, " (vazio)");
    for (k = 0; k < Len[r]; k++) fprintf(f, " %s", SymbolName(Grammar[r].rhs[k]));
    fprintf(f, "\n");
}
