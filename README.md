# Compilador da Simple Script Language — Léxico, Sintático, Escopo, Tipos e Geração de Código

Trabalho da disciplina de Compiladores (Prof. Luiz Guedes – IME). A VC
entregou o analisador léxico; a VF o reaproveitou e acrescentou as
análises sintática (la-LR(1)), de escopo e de tipos (caps. 3 a 6 da
apostila). **Esta versão acrescenta a Geração de Código (cap. 7)**: o
compilador gera código para a máquina virtual de pilha da apostila
(seção 7.1), imprime a Linguagem de Montagem, monta o programa
(pool de funções, pool de constantes, endereços de byte, desvios
relativos) e o executa numa implementação da máquina virtual.

## 1. Entendimento

O compilador é um *front-end* de uma passada só, como na fig. 1.3 da
apostila. A geração de código é feita pelas mesmas regras de atributos
que verificam escopo e tipos:

```
fonte ──► léxico (cap. 2) ──► sintático la-LR(1) (cap. 3.8 + fig. 4.6)
                                   │ a cada redução
                                   ▼
                          Semantics(regra) (cap. 4)
                          ├─ escopo: tabela de símbolos (cap. 5)
                          ├─ tipos: kinds, CheckTypes (cap. 6)
                          └─ código: tamanhos, endereços, Emit() (cap. 7)
                                   │ sem erros
                                   ▼
                          Linguagem de Montagem  (P.I da fig. 1.3)
                                   │
                          Montador ──► Array de Código + Pool de Funções
                                   │                    + Pool de Constantes
                                   ▼
                          Máquina Virtual (7.1 / 7.2)   ./ssl --run
```

| Fase | Capítulo | Onde |
|---|---|---|
| Léxico: tokens, nomes, constantes | 2 | `lexer.c` (da VC) |
| Sintático ascendente la-LR(1) dirigido por tabela | 3.8 | `lalr.c` constrói a tabela; `parser.c` executa a fig. 4.6 |
| Gramática de atributos: pilha semântica paralela à sintática, `Semantics(r)` com `switch` por regra | 4 | `semantics.c` |
| Escopo: pilha de níveis, `NewBlock/EndBlock/Define/Search/Find`, `IDD`/`IDU`, marcador `NB` | 5 | `scope.c` e casos `R_IDD`, `R_IDU`, `R_NB` |
| Tipos: `kind`, tipos pré-definidos, tipo universal, `CheckTypes`, marcadores `MF`/`MC` | 6 | `semantics.c` |
| **Geração de código: tamanhos, endereçamento, instruções, labels** | **7.3 a 7.6** | **`semantics.c` (regras) e `codegen.c` (lista de instruções, Linguagem de Montagem, Montador)** |
| **Máquina virtual: duas pilhas, pool de funções, pool de constantes** | **7.1 e 7.2** | **`vm.c`** |

A fase sintática só começa se não houver erro léxico. Os erros de escopo
e de tipo são acumulados até o fim do arquivo; um erro sintático
interrompe a análise. O código só é impresso, montado e executado se não
houve nenhum erro de nenhuma fase (o `ERROR_FLAG` da fig. 7.28).

## 2. Gramática efetivamente usada

É a do `Gramática.pdf`, acrescida dos marcadores semânticos dos caps. 5,
6 e 7 e de duas extensões (`return` e os marcadores de laço). A listagem
abaixo é gerada pelo próprio programa (`./ssl --tabela`):

```
  0  START -> P fim de arquivo
  1  P     -> LDE
  2  LDE   -> LDE DE
  3  LDE   -> DE
  4  DE    -> DF
  5  DE    -> DT
  6  T     -> 'integer'
  7  T     -> 'char'
  8  T     -> 'boolean'
  9  T     -> 'string'
 10  T     -> IDU
 11  DT    -> 'type' IDD '=' 'array' '[' NUM ']' 'of' T
 12  DT    -> 'type' IDD '=' 'struct' NB '{' DC '}'
 13  DT    -> 'type' IDD '=' T
 14  DC    -> DC ';' LI ':' T
 15  DC    -> LI ':' T
 16  DF    -> 'function' IDD NF '(' LP ')' ':' T MF B
 17  LP    -> LP ',' IDD ':' T
 18  LP    -> IDD ':' T
 19  B     -> '{' LDV LS '}'
 20  LDV   -> LDV DV
 21  LDV   -> DV
 22  LS    -> LS S
 23  LS    -> S
 24  DV    -> 'var' LI ':' T ';'
 25  LI    -> LI ',' IDD
 26  LI    -> IDD
 27  S     -> 'if' '(' E ')' MT S
 28  S     -> 'if' '(' E ')' MT S 'else' ME S
 29  S     -> 'while' ML '(' E ')' MW S
 30  S     -> 'do' ML S 'while' MD '(' E ')' ';'
 31  S     -> NB B
 32  S     -> LV '=' E ';'
 33  S     -> 'break' ';'
 34  S     -> 'continue' ';'
 35  S     -> 'return' E ';'
 36  E     -> E '&&' L
 37  E     -> E '||' L
 38  E     -> L
 39  L     -> L '<' R
 40  L     -> L '>' R
 41  L     -> L '<=' R
 42  L     -> L '>=' R
 43  L     -> L '==' R
 44  L     -> L '!=' R
 45  L     -> R
 46  R     -> R '+' Y
 47  R     -> R '-' Y
 48  R     -> Y
 49  Y     -> Y '*' F
 50  Y     -> Y '/' F
 51  Y     -> F
 52  F     -> LV
 53  F     -> '++' LV
 54  F     -> '--' LV
 55  F     -> LV '++'
 56  F     -> LV '--'
 57  F     -> '(' E ')'
 58  F     -> IDU MC '(' LE ')'
 59  F     -> '-' F
 60  F     -> '!' F
 61  F     -> TRUE
 62  F     -> FALSE
 63  F     -> CHR
 64  F     -> STR
 65  F     -> NUM
 66  LE    -> LE ',' E
 67  LE    -> E
 68  LV    -> LV '.' ID
 69  LV    -> LV '[' E ']'
 70  LV    -> IDU
 71  IDD   -> identificador
 72  IDU   -> identificador
 73  ID    -> identificador
 74  TRUE  -> 'true'
 75  FALSE -> 'false'
 76  CHR   -> caractere
 77  STR   -> string
 78  NUM   -> numero
 79  NB    -> (vazio)
 80  NF    -> (vazio)
 81  MF    -> (vazio)
 82  MC    -> (vazio)
 83  ML    -> (vazio)
 84  MT    -> (vazio)
 85  MW    -> (vazio)
 86  ME    -> (vazio)
 87  MD    -> (vazio)
```

Diferenças em relação ao `Gramática.pdf`, todas justificadas na seção 7:

| Regra | Diferença | Motivo |
|---|---|---|
| 0 | regra aumentada `START → P $` | detecção do fim da análise (cap. 3); o estado alcançado após empilhar `$` é o `FINAL` da fig. 4.6 |
| 12, 31 | marcador `NB` | fig. 5.9 e fig. 6.14 |
| 14, 15 | `T` em vez de `'T'` | D1 |
| 16 | marcadores `NF` e `MF` | fig. 6.13 e D4 |
| 27, 28 | marcadores `MT` e `ME` | figs. 7.21 e D16 |
| 29, 30 | marcadores `ML`, `MW` e `MD` | figs. 7.22, D11 e D16 |
| 35 | `S → 'return' E ';'` (nova) | D3 |
| 58 | marcador `MC` | fig. 6.23 |
| 68 | `ID` em vez de `IDU` | D2 |
| 73 | `ID → Id` (nova) | D2 |

O `README` da VC transcrevia `DE → DF | DT | DV` e usava `ID` em todo
lugar. Prevalece o `Gramática.pdf`, em que `DE → DF | DT` (não há
variáveis globais) e os identificadores são `IDD`/`IDU` (D10).

## 3. Análise sintática

**Construção la-LR(1)** (`lalr.c`). A tabela é construída pelo próprio
programa a partir da gramática, sem gerador externo:

1. `FIRST` e anuláveis por ponto fixo sobre as regras;
2. `Closure` de um conjunto de itens LR(1): para cada item `A → α·Bβ, a`,
   acrescenta `B → ·γ, FIRST(βa)`;
3. `goto` por símbolo;
4. **fusão de núcleos**: quando o `goto` produz um núcleo (itens sem
   lookahead) já existente, os lookaheads são unidos ao estado existente,
   que volta à fila se ganhou lookahead novo. Isso dá o mesmo autômato que
   construir o LR(1) canônico e fundir depois (cap. 3.8), sem precisar
   materializá-lo.

Um item é o par (regra, ponto), numerado por `Base[regra] + ponto`. Como
há 51 terminais, um conjunto de lookaheads cabe em um `uint64_t`.

A tabela segue a codificação do livro: `Action[q][a]` serve tanto para
terminais (empilha/reduz) quanto para não terminais (desvio), com
`Len[r]` e `Left[r]` por regra. Valor `p > 0` empilha ou desvia para
`p-1`; `p < 0` reduz pela regra `-p-1`; `0` é erro.

**Estatísticas** (`./ssl --tabela`):

| Item | Valor |
|---|---|
| Regras | 88 (0 a 87) |
| Terminais | 51 |
| Não terminais | 39 (9 deles marcadores) |
| Estados la-LR(1) | 175 |
| Conflitos empilha/reduz | **1** |
| Conflitos reduz/reduz | **0** |

A gramática foi conferida, fora do projeto, com o bison: o bison também
relata um único conflito empilha/reduz, no mesmo estado (163), e nenhum
reduz/reduz. Ele gera 176 estados, um a mais que o programa porque conta
o estado de aceitação depois de `$end`, que a fig. 4.6 dispensa. Os
marcadores novos da geração de código não introduziram nenhum conflito:
um marcador sempre é reduzido no mesmo ponto de uma regra, então não há
com o que competir.

**Else pendente.** O único conflito é o esperado:

```
Conflito empilha/reduz no estado 163 com 'else': empilhar (em vez de reduzir pela regra)
 27  S     -> 'if' '(' E ')' MT S
```

Ele é resolvido por empilhar, de modo que o `else` se associa ao `if`
mais interno. Conflitos reduz/reduz, se existissem, seriam resolvidos
pela regra de menor número, mas a contagem é 0.

**Algoritmo.** `Parse()` em `parser.c` é a figura 4.6 transcrita: empilha
em `IS_SHIFT`, e em `IS_REDUCTION` desempilha `Len[r]`, empilha
`Action[Top()][Left[r]]` e chama `Semantics(r)`, até `q == FINAL`.

**Mensagens de erro sintático** trazem linha, token encontrado e
terminais esperados, que são os que têm ação não nula no estado:

```
Erro sintatico (linha 4): encontrado ';'; esperado: 'false', 'true', '(', '++', '-', '--', '!', caractere, numero, string, identificador
```

Como a tabela é la-LR(1), estados com o mesmo núcleo são fundidos. Por
isso a lista de esperados pode incluir tokens válidos apenas em outro
contexto que compartilha o estado. O token encontrado e a linha estão
sempre corretos. Não há recuperação de erro sintático: a análise para no
primeiro erro.

## 4. Análise de escopo

**Estrutura** (`scope.c`, figs. 5.3 a 5.7). A tabela de símbolos é uma
pilha de níveis, `SymbolTable[MAX_NEST_LEVEL]`, em que cada nível é uma
lista encadeada por `pNext` com ponteiro para o último elemento. O nome do
identificador é o seu token secundário no lexer.

| Função | Papel |
|---|---|
| `NewBlock()` / `EndBlock()` | abre e fecha um nível; `EndBlock` não libera nada, pois campos e parâmetros continuam referenciados |
| `Define(nome)` | insere no nível corrente |
| `Search(nome)` | busca só no nível corrente (declaração) |
| `Find(nome)` | busca do nível corrente até o 0 (uso) |

**Regras:**

- **Nível 0** contém os tipos e as funções. Ele é aberto antes da análise,
  em `ResetSemantics()`.
- **`IDD → Id`** (declaração): se `Search` encontra o nome, reporta
  `ERR_REDECL`; senão chama `Define`.
- **`IDU → Id`** (uso): se `Find` não encontra o nome, reporta
  `ERR_NOT_DECL` e o define com kind universal, para não repetir o erro
  nem gerar erros de tipo em cascata.
- **`ID → Id`** (campo): não consulta a tabela. O campo é resolvido pelo
  tipo da struct, na análise de tipos (D2).
- **`NB`**: abre um nível no início do corpo de uma `struct` (os campos
  ficam num nível próprio) e de um bloco-comando (`S → NB B`, com
  `EndBlock` na redução).
- **`NF`**: na função, `NF` substitui o `NB`. O nome da função é declarado
  (`IDD`) antes de `NF`, portanto no nível externo; `NF` abre o nível em
  que ficam **parâmetros e variáveis locais juntos** (fig. 5.2), e a
  redução de `DF` o fecha. O corpo `B` da função não abre outro nível.
- **Recursão**: o nome da função está no nível externo e já tem kind e
  assinatura quando o corpo começa (`MF`), então a chamada recursiva é
  verificada normalmente.

A análise é de passada única: uma função só pode chamar a si mesma e às
funções declaradas antes dela.

## 5. Análise de tipos

**Kinds** (fig. 6.1): `NO_KIND_DEF_`, `VAR_`, `PARAM_`, `FUNCTION_`,
`FIELD_`, `ARRAY_TYPE_`, `STRUCT_TYPE_`, `ALIAS_TYPE_`, `SCALAR_TYPE_` e
`UNIVERSAL_`. O `object` da fig. 6.2 guarda o kind e uma união com os
dados de cada um.

**Tipos pré-definidos** (fig. 6.3): objetos estáticos `pInt`, `pChar`,
`pBool` e `pString`, de kind `SCALAR_TYPE_`. O universal (`pUniversal`)
tem kind `UNIVERSAL_` (D5).

**Declarações:**

- `T → IDU` exige um kind de tipo, ou universal.
- Variáveis (fig. 6.6), campos (fig. 6.10) e parâmetros (fig. 6.12)
  recebem kind e tipo ao reduzir a regra que fecha a lista.
- Arrays (fig. 6.8), structs (fig. 6.11) e aliases (fig. 6.9) preenchem
  o object do tipo.
- Funções: `NF` marca o kind `FUNCTION_` e `MF` (fig. 6.13) preenche o
  tipo de retorno e a lista de parâmetros antes do corpo.

**Compatibilidade** (`CheckTypes`). É a compatibilidade por nome da
fig. 6.16, aplicada depois de resolver aliases: um alias é sinônimo do
seu tipo base (D6). Duas declarações distintas de array ou struct não são
compatíveis entre si. O tipo universal é compatível com tudo.

**Expressões e comandos** (figs. 6.15 a 6.23):

| Construção | Exigência | Tipo resultante |
|---|---|---|
| `if`, `while`, `do-while` | condição boolean | — |
| `LV = E` | `LV` e `E` compatíveis | — |
| `&&`, `\|\|`, `!` | operandos boolean | boolean |
| `< > <= >= == !=` | operandos compatíveis entre si | boolean |
| `+` | operandos compatíveis, integer ou string (concatenação) | o do operando esquerdo |
| `- * /` e `-` unário | integer | integer |
| `++`/`--` (pré e pós) | `LV` integer | integer |
| `LV . campo` | `LV` struct (ou alias de struct), campo existente | tipo do campo |
| `LV [ E ]` | `LV` array ou string, `E` integer | elemento, ou `char` se for string |
| `IDU` como `LV` | kind variável ou parâmetro | tipo declarado |
| `f(args)` | `f` função; quantidade e tipo dos argumentos conferidos um a um no momento em que cada um é reconhecido (`MC`, `LE`) | tipo de retorno |
| `return E` | `E` compatível com o retorno da função corrente | — |
| `break`, `continue` | dentro de `while` ou `do-while` | — |

**Lista de erros** (`semantics.h`):

| Código | Fase | Mensagem |
|---|---|---|
| `ERR_LEXICAL` | léxico | impressa pelo lexer (caractere inválido, char ou string não terminados) |
| `ERR_SYNTAX` | sintático | encontrado X; esperado: ... |
| `ERR_REDECL` | escopo | identificador já declarado neste escopo |
| `ERR_NOT_DECL` | escopo | identificador não declarado |
| `ERR_TYPE_EXPECTED` | tipo | X não é um tipo |
| `ERR_BOOL_TYPE_EXPECTED` | tipo | expressão booleana esperada |
| `ERR_TYPE_MISMATCH` | tipo | tipos incompatíveis |
| `ERR_INVALID_TYPE` | tipo | tipo inválido para o operador |
| `ERR_KIND_NOT_STRUCT` | tipo | acesso a campo em algo que não é struct |
| `ERR_FIELD_NOT_DECL` | tipo | campo não declarado na struct |
| `ERR_KIND_NOT_ARRAY` | tipo | indexação em algo que não é array nem string |
| `ERR_INVALID_INDEX_TYPE` | tipo | índice deve ser do tipo integer |
| `ERR_KIND_NOT_VAR` | tipo | X não é variável nem parâmetro |
| `ERR_KIND_NOT_FUNC` | tipo | X não é função |
| `ERR_TOO_MANY_ARGS` | tipo | argumentos demais na chamada |
| `ERR_PARAM_TYPE` | tipo | tipo do argumento n incompatível com o parâmetro |
| `ERR_TOO_FEW_ARGS` | tipo | argumentos de menos na chamada |
| `ERR_RETURN_TYPE` | tipo (VF) | tipo do return incompatível com o retorno da função |
| `ERR_NOT_IN_LOOP` | semântico (VF) | break/continue fora de comando de repetição |

A linha reportada nos erros semânticos é a do último terminal empilhado,
que é o fim da construção em que o erro foi detectado.

## 6. Geração de código

Segue o cap. 7 da apostila. O código é gerado durante a análise
sintática: cada regra de atributos que, no livro, faz `fprintf(f, ...)`,
aqui chama `Emit(...)` (`semantics.c`). O código fica numa lista de
instruções em memória (`codegen.c`), impressa como Linguagem de Montagem
depois da análise e descartada se houve erro.

### 6.1 A máquina virtual (7.1)

Máquina de pilha, de "zero endereço". Duas pilhas, como na fig. 7.1:

| Estrutura | Papel | Registradores |
|---|---|---|
| Pilha de Valores | operandos, resultados, parâmetros e variáveis locais; cada posição guarda um valor escalar (integer, char, boolean ou string) | `sp` (topo), `bp` (base do registro de ativação) |
| Pilha Operacional | registro de ativação: `bp` e endereço de retorno de cada chamada | `spo` |
| Array de Código | instruções, endereçadas por byte | `ip` |
| Pool de Funções | por função: tamanho dos parâmetros, tamanho das variáveis, índice da 1ª instrução | — |
| Pool de Constantes | literais do programa, indexados pelo token secundário | — |

Parâmetros e variáveis locais são endereçados por deslocamento a partir
de `bp`, reiniciado em cada função, o que permite a recursão. Parâmetros
são passados por valor: um array ou struct é copiado célula a célula.

### 6.2 Conjunto de instruções (7.2)

Implementado em `vm.c`, uma linha de `case` por linha da tabela da fig. 7.2:

| Grupo | Instruções |
|---|---|
| Aritméticas | `ADD SUB MUL DIV INC DEC NEG` (`ADD` também concatena strings) |
| Lógicas | `AND OR NOT` |
| Relacionais | `EQ NEQ LT LE GT GE` (também comparam strings) |
| Desvios | `JMP_FW n`, `JMP_BW n`, `TJMP_FW n`, `TJMP_BW n` (desviam se o topo for **falso**) |
| Funções | `CALL n`, `RET`, `CALL_LIB l, f` |
| Variáveis e constantes | `LOAD_VAR n`, `STORE_VAR n`, `LOAD_CONST n`, `LOAD_TRUE`, `LOAD_FALSE` |
| Strings | `CHAR_AT n`, `SET_CHAR n` |
| Pilha | `POP`, `DUP` |
| Campos e arrays | `ADD n`, `MUL n` |
| Left-values | `LOAD_REF n`, `DE_REF n`, `STORE_REF n` |
| Pseudo-instruções | `BEGIN_FUNC f p v`, `END_FUNC`, labels `Ln:` |

Tamanhos em bytes (7.2): 1 byte para as sem argumento; 2 para
`LOAD_VAR`, `STORE_VAR`, `LOAD_CONST`, `CHAR_AT`, `SET_CHAR` e `CALL`;
3 para os desvios, `ADD n`, `MUL n`, `LOAD_REF`, `DE_REF`, `STORE_REF` e
`CALL_LIB`.

### 6.3 Regras de atributos implementadas

| Construção | Código gerado | Onde |
|---|---|---|
| `DF` | `BEGIN_FUNC n p v` ... `END_FUNC` | `R_MF` e `R_DF` (fig. 7.18) |
| `if (E) S` | `‖E‖ TJMP_FW L1 ‖S‖ L1:` | `R_MT`, `R_S_IF` (fig. 7.21) |
| `if (E) S1 else S2` | `‖E‖ TJMP_FW L1 ‖S1‖ JMP_FW L2 L1: ‖S2‖ L2:` | `R_MT`, `R_ME`, `R_S_IF_ELSE` |
| `while (E) S` | `L1: ‖E‖ TJMP_FW L2 ‖S‖ JMP_BW L1 L2:` | `R_ML`, `R_MW`, `R_S_WHILE` (fig. 7.22) |
| `do S while (E);` | `L1: ‖S‖ ‖E‖ NOT TJMP_BW L1 L2:` | `R_ML`, `R_MD`, `R_S_DO` |
| `break` | `JMP_FW` para o fim do laço | `R_S_BREAK` |
| `continue` | `JMP_BW` para o início (`while`) ou `JMP_FW` para a reavaliação da condição (`do`) | `R_S_CONTINUE` |
| `return E` | `‖E‖ RET` | `R_S_RETURN` |
| `LV = E` | `‖LV‖ ‖E‖ STORE_REF n` (ou `STORE_VAR n`, ver D19) | `R_S_ASSIGN` (fig. 7.23) |
| `E && L`, `E \|\| L` | `‖E‖ ‖L‖ AND` / `OR` | fig. 7.23 |
| `L < R` etc. | `‖L‖ ‖R‖ LT` ... | fig. 7.24 |
| `R + Y`, `R - Y`, `Y * F`, `Y / F` | `‖R‖ ‖Y‖ ADD` ... | fig. 7.25 |
| `-F`, `!F` | `‖F‖ NEG` / `NOT` | fig. 7.26 |
| `true`, `false`, `CHR`, `STR`, `NUM` | `LOAD_TRUE`, `LOAD_FALSE`, `LOAD_CONST n` | fig. 7.26 (D18) |
| `LV` (right-value) | `‖LV‖ DE_REF n` | `R_F_LV` (fig. 7.26) |
| `++LV`, `--LV`, `LV++`, `LV--` | `DUP DUP DE_REF 1 INC STORE_REF 1 DE_REF 1 [DEC]` | `GenIncDec` (fig. 7.8) |
| `LV . ID` | `‖LV‖ ADD n` (n = deslocamento do campo) | `R_LV_FIELD` (fig. 7.27) |
| `LV [E]` | `‖LV‖ ‖E‖ MUL n ADD` (n = tamanho do elemento) | `R_LV_INDEX` |
| `IDU` (left-value) | `LOAD_REF n` | `R_LV_IDU` |
| `s[E]` (string) | `CHAR_AT n` / `SET_CHAR n`, ou `CALL_LIB 0, 0` / `0, 1` | D20 |
| `IDU MC (LE)` | `‖LE‖ CALL n` | `R_F_CALL` (fig. 7.28) |

### 6.4 Tamanho e endereçamento (7.5)

Todo tipo tem o tamanho em células da pilha (`nSize` do `object`,
fig. 7.11): 1 para `integer`, `char`, `boolean` e `string`; para o array,
número de elementos × tamanho do elemento; para a struct, a soma dos
campos; para o alias, o do tipo base. Um parâmetro recebe como endereço a
soma dos tamanhos dos anteriores (o primeiro é 0). As variáveis locais
continuam a numeração dos parâmetros e todas as de uma função, inclusive
as de blocos internos, compartilham o mesmo contador (fig. 7.18). Um campo
recebe como `nIndex` o seu deslocamento dentro da struct. A função recebe
o seu índice no Pool de Funções em `NF` (`nFuncs++`).

### 6.5 Montador (7.2 e 7.3)

`Assemble()` (`codegen.c`) faz duas passadas sobre a lista de
instruções: a primeira calcula o endereço de byte de cada instrução, de
cada label e da primeira instrução de cada função (preenche o `ip` do
Pool de Funções); a segunda monta o Array de Código e troca cada label
pelo deslocamento relativo (`ip = ip + n` ou `ip = ip - n`). `./ssl
--montado` imprime o resultado.

### 6.6 Execução

`RunFunction()` (`vm.c`) empilha os argumentos, abre o registro de
ativação e interpreta o Array de Código até o `RET` da função inicial. Há
verificação de divisão por zero, de índice de string, de acesso fora da
pilha e um limite de 200 milhões de instruções (laço infinito).

A linguagem não tem entrada e saída, então o resultado de um programa é o
valor que uma função devolve:

```
./ssl --run exemplos/fatorial.ssl fatorial 5
fatorial(5) = 120   [58 instrucoes]
```

Os argumentos são convertidos para o tipo dos parâmetros da função
(inteiros, `true`/`false`, um caractere, string). Funções com parâmetro
array ou struct são chamadas por outras funções do programa (por exemplo,
`testeOrdena` chama `bSort`).

## 7. Decisões e ambiguidades

**D1 — `'T'` em `DC`.** O `Gramática.pdf` escreve `DC → DC ';' LI ':' 'T'`.
Não existe o terminal `'T'`: é o não terminal `T`, como nas outras
declarações. O README da VC já o transcrevia assim.

**D2 — Acesso a campo usa `ID`, não `IDU`.** O `Gramática.pdf` e a lista
da seção 5.1 trazem `LV → LV '.' IDU`, mas o texto do cap. 5 e a fig. 6.22
usam `ID`. Com `IDU`, `p.x` daria "x não declarado", pois `x` só existe no
nível da struct, já fechado. Foi criado `ID → Id` (fig. 5.8, terceira
regra), e o campo é procurado na lista de campos do tipo de `LV`.

**D3 — `return` (extensão da linguagem).** A gramática não tem `return`,
mas todos os exemplos dos cap. 6 e 7 o usam. Sem ele, uma função não
devolve valor: o tipo de retorno de `DF` ficaria sem verificação e, como
parâmetros são por valor e não há variáveis globais, nenhum programa teria
efeito observável. A extensão é a menor possível: a palavra reservada
`return` no lexer e `S → 'return' E ';'`, com `E` compatível com o tipo de
retorno da função corrente (`ERR_RETURN_TYPE`). A presença de `return` em
todos os caminhos não é verificada, pois exigiria análise de fluxo. Em
tempo de execução, se o fluxo chegar ao fim da função sem `return`, a
execução falha com "instrução inválida".

**D4 — `NF` substitui `NB` na função.** A fig. 6.13 usa `NB` antes de `LP`
e a fig. 7.18 chama `NewBlock()` dentro de `MF`, que é reduzido **depois**
de `LP`, o que poria os parâmetros no nível da função. O texto da 7.18
diz que `NF` substitui `NB`. Foi adotado `NF`: ele chama `NewBlock()`
antes dos parâmetros e marca o kind `FUNCTION_`, e `MF` só preenche
retorno e parâmetros. O `EndBlock()` fica na redução de `DF`.

**D5 — Tipo universal com kind `UNIVERSAL_`.** Na fig. 6.3, o universal
tem kind `SCALAR_TYPE_`, então `LV . campo` sobre ele reportaria
`ERR_KIND_NOT_STRUCT`, gerando erro em cascata. Ele passa a ter kind
`UNIVERSAL_`. Pelo mesmo motivo:

- o identificador não declarado, definido por `IDU`, recebe kind `UNIVERSAL_`;
- `MC` não reporta `ERR_KIND_NOT_FUNC` para um universal.

Cada linha de `exemplos/erros_tipo.ssl` gera exatamente um erro.

**D6 — `CheckTypes`.** A versão estrutural (fig. 6.17) não avança `f1` e
`f2` no laço dos campos, o que causa laço infinito em structs. A versão
por nome (fig. 6.16) torna `type N = integer` incompatível com `integer`,
contrariando a ideia de "tipo sinônimo" (fig. 6.9). Foi adotada a
compatibilidade por nome após resolver aliases (`BaseType`). O mesmo
`BaseType` é usado em `LV . campo` e `LV [ E ]`, de modo que um alias de
struct, array ou string se comporta como o tipo base. Tipos recursivos
não são possíveis: o nome do tipo ainda tem kind `NO_KIND_DEF_` dentro da
própria declaração (`ERR_TYPE_EXPECTED`).

**D7 — Listas de declaração separadas (`pNextDecl`) e redeclaração.** No
livro, a lista de variáveis, campos ou parâmetros é a própria lista
`pNext` do nível (figs. 6.6 a 6.13). Isso causa três problemas:

1. Parâmetros e variáveis locais estão no mesmo nível, então a lista de
   parâmetros continua nas variáveis locais. Uma chamada feita depois da
   declaração dos locais, como a recursiva, acusaria argumentos de menos.
2. Na redeclaração, a fig. 6.4 devolve o object **original** e faz
   `p->eKind = NO_KIND_DEF_`, de modo que a declaração repetida sobrescreve
   o tipo da original.
3. Se o primeiro nome de um `var` for redeclarado, a lista começaria num
   object antigo e os demais nomes ficariam sem kind.

Solução: o object ganhou o campo `pNextDecl`, que encadeia só a lista de
declaração. `LI`, `DC` e `LP` carregam o primeiro e o último elemento da
lista. Em caso de redeclaração, `IDD` reporta o erro e cria um object
**fora** da tabela (`NewObject`), que segue normalmente na lista mas não
altera a declaração original. Os três casos têm teste. Na geração de
código, o mesmo encadeamento é o que permite calcular o tamanho dos
parâmetros (`LP.size`) sem contar as variáveis locais.

**D8 — Token secundário do terminal empilhado.** As figs. 5.8 e 6.7 leem
`tokenSecundario` na redução `IDD → Id`. Nesse momento, porém, o
analisador já leu o lookahead, e a variável do lexer pertence ao próximo
token. O parser guarda `lastSecundario` e `lastLine` ao empilhar cada
terminal, e `Semantics` usa esses valores. O mesmo vale para o índice
das constantes em `LOAD_CONST` (D22).

**D9 — `B → '{' LDV LS '}'` mantido.** A regra exige ao menos uma
declaração e um comando em todo bloco, inclusive no corpo da função. Os
exemplos do livro violam isso, mas a linguagem continua utilizável:

- um laço com um só comando dispensa o bloco;
- um bloco com vários comandos declara a variável de que precisa (como o
  `var c : char;` da troca em `strings.ssl`).

Por isso a gramática oficial não foi alterada, e os exemplos foram
reescritos.

**D10 — Sem variáveis globais.** `DE → DF | DT` no `Gramática.pdf`. O
README da VC trazia `| DV`, o que foi corrigido. `var` no nível externo é
erro sintático (há teste).

**D11 — `break`/`continue` fora de laço.** Não há regras de atributos para
eles. A verificação é simples e evita um erro que, de outra forma, só
apareceria na geração de código (desvio sem destino). O marcador `ML`
(agora antes da condição do `while` e antes do corpo do `do`) empilha o
laço numa pilha de laços; a redução do comando de repetição o desempilha.
`break`/`continue` com a pilha vazia dá `ERR_NOT_IN_LOOP`. Como o corpo é
reduzido antes do comando que o contém, a contagem é correta também com
laços aninhados.

**D12 — Exemplos do livro reescritos** (`exemplos/`). A linguagem não tem
comentários, e o lexer não tem sequências de escape.

- `fatorial.ssl` (7.4.1): os blocos de laço com um só comando viraram
  comandos simples, e `somaFat` chama `fatorial`, pois o livro chama
  `fat`, que não existe. O código gerado é idêntico ao da apostila.
- `strings.ssl` (7.4.2 e 7.4.3): `'\0'` não é um literal válido (são dois
  caracteres entre aspas), então o tamanho virou parâmetro.
  `s1[i] - s2[i]` subtrai caracteres, o que a fig. 6.20 proíbe, então a
  comparação devolve `boolean`.
- `ordena.ssl` (7.4.4): sem `;` após `type intArray = ...`, pois `DT` não
  termina em `;`. Foi acrescentada `testeOrdena`, que preenche um array,
  chama `bSort` e devolve um número com os cinco primeiros elementos, para
  que o resultado da ordenação possa ser observado.
- `estruturas.ssl` (novo): structs, structs aninhadas, array de struct,
  `++`/`--` em campos, retorno de struct, string em campo, `do-while` com
  `continue` e `break`, recursão e operadores lógicos. Exercita todos os
  caminhos do gerador.
- O `exemplo.ssl` da VC continua sendo exemplo de tokens. Para a VF ele é
  inválido (`main()` sem parâmetro, bloco `{ break; }` sem declaração,
  `v[0]++;` como comando, `resultado` não declarado em `main`).

**D13 — Correções no lexer da VC** (as únicas alterações feitas nele):

| Alteração | Detalhe |
|---|---|
| palavra reservada `return` | inserida em ordem alfabética na tabela `keywords`, no `enum t_token` e em `tokenName`, que precisam continuar alinhados para a busca binária |
| identificadores com dígitos | a VC usava `isalpha` no laço do identificador e partia `x1` em `ID` + `NUMERAL`; a ER `letra . (letra + digito)*` aceita dígitos após a primeira letra, então passou a `isalnum`. Continua sem `_`, como decidido na VC |
| `tokenLine` | `lineNumber` avança ao ler o `\n` de lookahead, de modo que um `;` no fim da linha 2 aparecia como linha 3. `tokenLine` guarda a linha do primeiro caractere do token e é usada em todas as mensagens e no modo `--tokens` |
| `strdup` → `_strdup` sob MSVC | evita o aviso C4996 do `cl.exe` |

**D14 — Pilha semântica.** O cap. 4 (fig. 4.8) usa
`TopSem(offFromTop)`, e o cap. 6 escreve `TopSem(-1)`, `TopSem(-2)`.
Aqui o deslocamento é sempre positivo, com 0 no topo.

**D15 — Adaptações de implementação** (permitidas pelo enunciado):

- Todo não terminal empilha um nó, como recomenda o fim do cap. 4.1.
  `Semantics` lê o lado direito com `TopSem(k)`, calcula `res`, e no fim
  desempilha os nós do lado direito e empilha `res`.
- Os objects são alocados um a um, em memória estável, e liberados juntos
  ao fim da compilação.
- `Compile` lê o fonte duas vezes: primeiro só o léxico, para que erros
  léxicos impeçam o sintático; depois a análise completa.
- `NUM` em `array [NUM]` não é verificado contra 0, pois não há regra no
  livro para isso.

Decisões da geração de código (cap. 7):

**D16 — Marcadores `MT`, `ME`, `ML`, `MW` e `MD`.** O livro usa `MT`
depois da condição do `if` e do `while`, `ME` antes do `else` e `MW` antes
da condição do `while` (figs. 7.21 e 7.22). Na gramática, `MT` não pode
servir aos dois comandos: ao ser reduzido, ele só enxerga `E` na pilha
semântica e não sabe se está num `if` (label novo) ou num `while` (label de
fim do laço, criado antes da condição). Por isso os papéis foram separados
em marcadores distintos:

```
S → 'if' '(' E ')' MT S                      MT: TJMP_FW L1             fim: L1:
S → 'if' '(' E ')' MT S 'else' ME S          ME: JMP_FW L2, L1:         fim: L2:
S → 'while' ML '(' E ')' MW S                ML: L1: (início)           MW: TJMP_FW L2   fim: JMP_BW L1, L2:
S → 'do' ML S 'while' MD '(' E ')' ';'       ML: L1:                    MD: Lc:          fim: NOT, TJMP_BW L1, L2:
```

`ML` (que na etapa anterior só contava a profundidade de laços, D11)
agora cria os labels de início e de fim do laço e os guarda numa pilha de
laços, de onde `break` e `continue` tiram o destino. `MD` existe porque,
no `do-while`, o `continue` deve ir para a reavaliação da condição, que
vem **depois** do corpo: o livro só escreve "`JMP_BW L / JMP_FW L`". O
label da reavaliação é criado quando o primeiro `continue` o exige e só
então é impresso. Os marcadores são regras vazias reduzidas sempre no mesmo
ponto, então não criam conflitos (o bison confirma a mesma contagem).

**D17 — `BEGIN_FUNC n p v`: `v` conta só as variáveis locais.** O exemplo
7.4.1 mostra `BEGIN_FUNC 0 1 2` para `fatorial(n)` com `var i, f`, ou
seja, `p` = 1 parâmetro e `v` = 2 locais. Já as figs. 7.18 e 7.19 fazem
`nVars` incluir os parâmetros. Foi seguido o exemplo, que é o que o Pool
de Funções descreve ("tamanho das variáveis"): internamente `nVars`
inclui os parâmetros (o endereçamento continua a numeração deles), e o
`BEGIN_FUNC` imprime `nVars - nParams`. Como o valor só é conhecido no
fim da função, o livro usa `fseek`; aqui o código está em memória e o
`BEGIN_FUNC` é corrigido na redução de `DF`. `p` e `v` são tamanhos em
células, não quantidades de nomes (um parâmetro array de 30 elementos
conta 30).

**D18 — `true` e `false` geram `LOAD_TRUE` e `LOAD_FALSE`** (fig. 7.7),
não `LOAD_CONST` (fig. 7.26, que usa o token secundário de uma
palavra reservada, sem valor). Assim não há constante a registrar.
As duas instruções ocupam 1 byte e fazem parte do conjunto da VM.

**D19 — Left-values e variáveis escalares.** Todo left-value usa o
esquema geral das figs. 7.8 e 7.27 (`LOAD_REF`, `DE_REF`, `STORE_REF`),
necessário para structs e arrays. Para uma variável escalar (`IDU` de
tamanho 1), a fig. 7.9 dá a forma curta (`LOAD_VAR`, `STORE_VAR`, e
`++`/`--` sem `DUP DUP DE_REF`), que é a que aparece no exemplo 7.4.1. O
atributo de `LV` guarda o índice do `LOAD_REF` já gerado, e quem o usa o
reescreve no lugar: `LOAD_REF n` seguido de `DE_REF 1` vira `LOAD_VAR n`,
e numa atribuição o `LOAD_REF n` é removido e `STORE_REF 1` vira
`STORE_VAR n`. O resultado é o código da apostila, sem gerar duas vezes.
Na fig. 7.27, `LOAD_REF` usa `t->_.Var.pType._.Type.nSize`, mas `t` é o
tipo da expressão anterior; aqui se usa o tamanho da variável.

**D20 — Strings.** Um `string` ocupa uma célula (um ponteiro) e é
mutável (`s[i] = c`), como no exemplo 7.4.3: variáveis e parâmetros string
compartilham o conteúdo ao serem copiados. Literais são copiados a cada
`LOAD_CONST`, para que alterar um não altere o literal. `+` concatena
(`ADD` é polimórfico) e `==`, `<` etc. comparam o texto.

- `s[E]` com `s` variável: `CHAR_AT n` e `SET_CHAR n` (fig. 7.29,
  exemplos 7.4.2 e 7.4.3).
- `s[E]` com `s` qualquer outro left-value (campo ou elemento de array do
  tipo string): o livro manda usar funções de biblioteca, então foi usada
  a biblioteca 0 de `CALL_LIB`: função 0 `char_at(ref, i)` e função 1
  `set_char(ref, i, c)`.
- A fig. 7.29 gera `SET_CHAR n` **e** `CHAR_AT n` para `IDU[E1] = E2`
  (destacado no PDF). Aqui só `SET_CHAR`: a atribuição é um comando e não
  deixa valor na pilha.

**D21 — `RET` e valores compostos.** A instrução `RET` do livro não tem
argumento, mas o exemplo 7.4.4 devolve um array de 30 células com um
`RET` simples. O Pool de Funções guarda o tamanho do retorno
(`retSize`) e o Montador o coloca no `RET` do Array de Código. A
Linguagem de Montagem continua mostrando só `RET`.

**D22 — Pool de Constantes.** Indexado pelo token secundário do literal
(fig. 7.26: `n = tokenSecundario`), como no livro: cada ocorrência tem o
seu índice (no exemplo 7.4.1 os quatro literais são `LOAD_CONST 0` a
`3`), sem reaproveitar valores iguais. Como o índice é o do lexer, o
`NUM` de `array [NUM]` também ocupa uma posição, e a lista pode ter
lacunas (ver `ordena.ssl`). O literal é copiado para o pool do gerador na
hora, porque o do lexer é liberado no fim da análise. A listagem em
Assembly traz o pool no final, como comentário.

**D23 — Tamanho de `CALL`.** A fig. 7.2 não dá o tamanho de `CALL n`; foi
adotado 2 bytes (opcode e índice da função), como `LOAD_VAR`.

**D24 — Sem entrada e saída.** A linguagem não tem comandos de E/S. A
chamada de biblioteca da fig. 7.10 (`IDU '.' ID '(' LE ')'`) não está na
gramática e não pode ser acrescentada como está: depois de um `IDU`, o
analisador teria que escolher entre reduzir `LV → IDU` e empilhar o
`'.'`. Por isso o resultado de um programa é observado pelo valor devolvido
pela função (`./ssl --run`). A infraestrutura de `CALL_LIB` existe e é
usada nas strings (D20).

**D25 — Verificações em tempo de execução.** Existem as que o livro não
precisa de instrução para fazer: divisão por zero, índice de string,
acesso fora da pilha, estouro das pilhas e limite de instruções. O índice
de um array **não** é verificado contra o tamanho declarado, pois o livro
não tem instrução para isso: como em C, um índice fora dos limites lê ou
escreve em outra variável da pilha.

**D26 — Saída do programa.** O código (Assembly ou montado) vai para
`stdout`; erros e o resumo vão para `stderr`, de modo que
`./ssl exemplo.ssl > saida.asm` grava só o código.

**Inconsistências do cap. 7 notadas** (e como foram tratadas): na 7.4.1
o laço de `fatorial` mostra `LT` onde o fonte tem `<=` (o PDF destaca e
o de `somaFat` mostra `LE`; foi gerado `LE`); na 7.4.2 aparece `LOAC_CONST`
(erro de digitação) e a comparação usa `'\0'` (D12); na fig. 7.18 o
`MF` lê `TopSem(-2)` como `NB`, onde o texto diz que é `NF` (D4); na fig.
7.3 os desvios do assembler são descritos como `ip = L`, mas o texto da
7.2 e a tabela 7.2 os definem como deslocamentos relativos, que foram os
usados.

## 8. Como compilar, executar e testar

**gcc (Linux / WSL / MinGW):**

```
make            # gera ./ssl e ./testes (gcc -std=c11 -Wall -Wextra -Wpedantic)
make test       # roda os testes
```

ou, sem `make`:

```
gcc -std=c11 -Wall -Wextra -Wpedantic -o ssl main.c lexer.c grammar.c lalr.c parser.c scope.c semantics.c codegen.c vm.c
gcc -std=c11 -Wall -Wextra -Wpedantic -o testes tests.c lexer.c grammar.c lalr.c parser.c scope.c semantics.c codegen.c vm.c
```

**cl.exe (MSVC):** precisa de `/std:c11`, por causa de `_Static_assert`
e dos inicializadores designados (Visual Studio 2019 16.8 ou mais novo):

```
cl /std:c11 /W4 /Fe:ssl.exe main.c lexer.c grammar.c lalr.c parser.c scope.c semantics.c codegen.c vm.c
cl /std:c11 /W4 /Fe:testes.exe tests.c lexer.c grammar.c lalr.c parser.c scope.c semantics.c codegen.c vm.c
```

O build com `gcc -Wall -Wextra -Wpedantic` não gera nenhum aviso. O build
com `cl.exe` não foi testado nesta máquina.

**Uso:**

```
./ssl exemplos/fatorial.ssl                  # análises + Linguagem de Montagem (stdout)
./ssl -o fatorial.asm exemplos/fatorial.ssl  # idem, gravando em arquivo
./ssl --montado exemplos/fatorial.ssl        # pools, endereços e desvios relativos
./ssl --run exemplos/fatorial.ssl fatorial 5 # compila e executa fatorial(5)
./ssl --tokens exemplo.ssl                   # tabela de tokens (saída da VC)
./ssl --tabela                               # gramática, estados e conflitos
./ssl < arquivo.ssl                          # lê da entrada padrão
```

O código de saída é 0 sem erros e 1 com erros.

**Testes** (`./testes`, rodar na raiz). Todos usam `assert`:

| Grupo | O que verifica |
|---|---|
| Lexer | `x1` é um identificador só, `return` é palavra reservada, `tokenLine` está correta |
| Tabela | exatamente 1 conflito empilha/reduz e 0 reduz/reduz |
| Válidos | os cinco exemplos de `exemplos/` mais casos de else pendente, sombreamento, laços aninhados, alias, recursão e indexação de string compilam sem erro |
| Erros | cada código de erro tem ao menos um caso que o dispara, cada caso produz **exatamente um** erro (sem cascata) e **nenhum código é montado** |
| Código gerado | a Linguagem de Montagem de `fatorial` e `somaFat` é exatamente a da apostila (7.4.1); `bSort` tem `BEGIN_FUNC 0 31 6` e `DE_REF 30` (7.4.4); `++` em campo usa a sequência da fig. 7.8; strings usam `CHAR_AT`, `SET_CHAR` e `CALL_LIB` |
| Montador | `ip` das funções, e todo desvio (`JMP`/`TJMP`, `FW`/`BW`) cai no início de uma instrução |
| Execução | 20 programas rodados na máquina virtual com resultado conferido à mão: fatorial, soma de fatoriais, inversão e comparação de strings, ordenação de array, perímetro (array de structs), `++` em campos, string em campo, `do-while` com `continue`/`break`, operadores lógicos, divisão, `char`, array por valor, retorno de struct, recursão (`fib(15) = 610`) |
| Erros de execução | divisão por zero, índice de string fora dos limites e laço infinito terminam com erro |

Os testes e os exemplos também rodaram sem falhas com AddressSanitizer e
UBSan (inclusive vazamentos), inclusive sobre cerca de 130 entradas
truncadas de `estruturas.ssl`, `tipos.ssl` e `ordena.ssl`.

## 9. Exemplos de entrada e saída

### 9.1 Geração de código: `fatorial.ssl` (comparar com a seção 7.4.1 da apostila)

Fonte:

```
function fatorial(n : integer) : integer
{
    var i, f : integer;
    i = 2;
    f = 1;
    while (i <= n)
        f = f * i++;
    return f;
}

function somaFat(n : integer) : integer
{
    var i, s : integer;
    i = 0;
    s = 0;
    while (++i <= n)
        s = s + fatorial(i);
    return s;
}
```

`./ssl exemplos/fatorial.ssl`:

```
BEGIN_FUNC 0 1 2		; fatorial
	LOAD_CONST 0
	STORE_VAR 1
	LOAD_CONST 1
	STORE_VAR 2
L0:
	LOAD_VAR 1
	LOAD_VAR 0
	LE
	TJMP_FW L1
	LOAD_VAR 2
	LOAD_VAR 1
	DUP
	INC
	STORE_VAR 1
	MUL
	STORE_VAR 2
	JMP_BW L0
L1:
	LOAD_VAR 2
	RET
END_FUNC

BEGIN_FUNC 1 1 2		; somaFat
	LOAD_CONST 2
	STORE_VAR 1
	LOAD_CONST 3
	STORE_VAR 2
L2:
	LOAD_VAR 1
	INC
	DUP
	STORE_VAR 1
	LOAD_VAR 0
	LE
	TJMP_FW L3
	LOAD_VAR 2
	LOAD_VAR 1
	CALL 0
	ADD
	STORE_VAR 2
	JMP_BW L2
L3:
	LOAD_VAR 2
	RET
END_FUNC

; Pool de Constantes:
;     0 = integer 2
;     1 = integer 1
;     2 = integer 0
;     3 = integer 0
```

Montado (`./ssl --montado exemplos/fatorial.ssl`):

```
Pool de Funcoes:
  n   nome         params   vars     ip     retorno
  0   fatorial     1        2        0      1 celula(s)
  1   somaFat      1        2        33     1 celula(s)

Pool de Constantes:
    0 = integer 2
    1 = integer 1
    2 = integer 0
    3 = integer 0

Array de Codigo (68 bytes):
              ; ---- funcao fatorial ----
  0000  LOAD_CONST 0    ; integer 2
  0002  STORE_VAR  1
  0004  LOAD_CONST 1    ; integer 1
  0006  STORE_VAR  2
  0008  LOAD_VAR   1
  0010  LOAD_VAR   0
  0012  LE
  0013  TJMP_FW    17   ; -> 0030 (L1)
  0016  LOAD_VAR   2
  0018  LOAD_VAR   1
  0020  DUP
  0021  INC
  0022  STORE_VAR  1
  0024  MUL
  0025  STORE_VAR  2
  0027  JMP_BW     19   ; -> 0008 (L0)
  0030  LOAD_VAR   2
  0032  RET
              ; ---- funcao somaFat ----
  0033  LOAD_CONST 2    ; integer 0
  0035  STORE_VAR  1
  0037  LOAD_CONST 3    ; integer 0
  0039  STORE_VAR  2
  0041  LOAD_VAR   1
  0043  INC
  0044  DUP
  0045  STORE_VAR  1
  0047  LOAD_VAR   0
  0049  LE
  0050  TJMP_FW    15   ; -> 0065 (L3)
  0053  LOAD_VAR   2
  0055  LOAD_VAR   1
  0057  CALL       0    ; fatorial
  0059  ADD
  0060  STORE_VAR  2
  0062  JMP_BW     21   ; -> 0041 (L2)
  0065  LOAD_VAR   2
  0067  RET
```

Execução:

```
fatorial(5) = 120   [58 instrucoes]
fatorial(10) = 3628800   [118 instrucoes]
somaFat(4) = 33   [177 instrucoes]
```

### 9.2 Programas com erros

Erros de escopo (`exemplos/erros_escopo.ssl`):

```
type Ponto = struct { x, y : integer; x : char }

function f(a : integer, a : char) : integer
{
    var b : integer;
    var b : char;
    b = c;
    {
        var b : boolean;
        b = true;
    }
    return g(b);
}
```
```
Erro de escopo (linha 1): identificador 'x' ja declarado neste escopo
Erro de escopo (linha 3): identificador 'a' ja declarado neste escopo
Erro de escopo (linha 6): identificador 'b' ja declarado neste escopo
Erro de escopo (linha 7): identificador 'c' nao declarado
Erro de escopo (linha 12): identificador 'g' nao declarado
Compilacao concluida com 5 erro(s).
```

O `b` do bloco interno (linha 9) não é redeclaração, pois está em outro
nível. Depois que `c` e `g` são reportados, não aparece nenhum erro de
tipo derivado deles.

Erros de tipo (`exemplos/erros_tipo.ssl`, um erro por linha):

```
Erro de tipo (linha 9): 'n' nao e um tipo
Erro de tipo (linha 10): expressao booleana esperada
Erro de tipo (linha 11): tipos incompativeis
Erro de tipo (linha 12): tipo invalido para o operador
Erro de tipo (linha 13): acesso a campo em algo que nao e struct
Erro de tipo (linha 14): indexacao em algo que nao e array nem string
Erro de tipo (linha 15): indice deve ser do tipo integer
Erro de tipo (linha 16): campo 'z' nao declarado na struct
Erro de tipo (linha 17): 'Ponto' nao e variavel nem parametro
Erro de tipo (linha 18): 'n' nao e funcao
Erro de tipo (linha 19): argumentos demais na chamada
Erro de tipo (linha 20): argumentos de menos na chamada
Erro de tipo (linha 21): tipo do argumento 1 incompativel com o parametro
Erro de tipo (linha 22): tipos incompativeis
Erro de tipo (linha 23): tipo invalido para o operador
Erro de tipo (linha 24): tipo invalido para o operador
Erro de tipo (linha 25): expressao booleana esperada
Erro de tipo (linha 26): tipos incompativeis
Erro de tipo (linha 27): tipo do return incompativel com o retorno da funcao
Erro semantico (linha 33): break/continue fora de comando de repeticao
Compilacao concluida com 20 erro(s).
```

Erro sintático (`exemplos/erro_sintatico.ssl`, linha 4 é `x = n +;`):

```
Erro sintatico (linha 4): encontrado ';'; esperado: 'false', 'true', '(', '++', '-', '--', '!', caractere, numero, string, identificador
Compilacao concluida com 1 erro(s).
```

Erros léxicos impedem o sintático (`exemplos/erro_lexico.ssl`):

```
Erro lexico (linha 4): caractere invalido '#' (0x23)
Erro lexico (linha 5): '&' isolado (esperado '&&')
Compilacao concluida com 2 erro(s).
```

Programa com erro não gera código: nada é impresso em `stdout`, só os
erros em `stderr`.

## Estrutura do projeto

| Arquivo | Conteúdo |
|---|---|
| `lexer.h`, `lexer.c` | analisador léxico da VC, com as correções de D13 |
| `grammar.h`, `grammar.c` | enumerações de não terminais e de regras, produções, `Len[]`, `Left[]` |
| `lalr.h`, `lalr.c` | construção da tabela la-LR(1) e contagem dos conflitos |
| `parser.h`, `parser.c` | algoritmo da fig. 4.6, mensagens de erro sintático, `Compile()` |
| `scope.h`, `scope.c` | `object` (figs. 6.2 e 7.11) e tabela de símbolos (figs. 5.3 a 5.7) |
| `semantics.h`, `semantics.c` | atributos, pilha semântica, `Semantics()` com as regras de escopo, tipos e **geração de código**, `CheckTypes()`, erros |
| `codegen.h`, `codegen.c` | **lista de instruções, `Emit`, `NewLabel`, pool de constantes, Linguagem de Montagem e Montador** |
| `vm.h`, `vm.c` | **máquina virtual (fig. 7.1) e conjunto de instruções (fig. 7.2)** |
| `main.c` | driver: compilação, `--montado`, `--run`, `--tokens`, `--tabela` |
| `tests.c` | executável único de testes |
| `exemplos/` | programas válidos e com erros |
| `exemplo.ssl` | exemplo de tokens da VC |
| `Makefile` | build com gcc |

## Prompts utilizados

Como pedido pelo professor, seguem os prompts usados para gerar as
etapas com IA.

**Etapa anterior (VF: sintático, escopo e tipos):**

> Você está trabalhando na continuação (VF) do trabalho da disciplina de
> Compiladores (Prof. Luiz Guedes – IME), de Rafael Schuinki e Mateus
> Mota. A primeira parte (VC) já foi entregue: um analisador léxico em C
> (C11) para a Simple Script Language (SSL), no repositório
> https://github.com/Schuinki/Compiladores.
>
> O enunciado agora é: "Sintático / Escopo / Tipo — é a continuação do
> trabalho da VC." A geração de código (cap. 7) não faz parte deste
> trabalho: não implemente nada de geração de código. O cap. 7 só deve ser
> consultado onde afeta sintaxe, escopo ou tipos.
>
> **Fontes:** `Gramática.pdf` (fonte primária da sintaxe); a apostila do
> professor, capítulos 3 (Análise Sintática, la-LR(1)), 4 (Gramática de
> Atributos, pilha semântica, `Semantics(rule)`, fig. 4.6), 5 (Análise de
> Escopo, `NewBlock/EndBlock/Define/Search/Find`, `IDD`/`IDU`, `NB`) e 6
> (Análise de Tipos, kinds, tipos pré-definidos, universal, `CheckTypes`,
> `MF`, `MC`); o código da VC (clone o repositório e leia `lexer.h`,
> `lexer.c`, `main.c`, `exemplo.ssl` e `README.md`), que deve ser
> reaproveitado, não reescrito.
>
> **Antes de escrever código:** leia a gramática e os capítulos 3 a 6 (o
> 7 só nos pontos citados). Leia todo o código da VC; as decisões dela
> continuam valendo (identificador sem `_`, erros léxicos em `stderr` sem
> interromper o léxico), exceto estes bugs, que devem ser corrigidos e
> documentados:
> * o laço do identificador usa `isalpha`, e `x1` vira `ID` + `NUMERAL`;
> * `lineNumber` avança no `\n` de lookahead e as linhas saem erradas;
> * o README da VC transcreve `DE → DF | DT | DV` e `ID`, mas o
>   `Gramática.pdf` (que prevalece) tem `DE → DF | DT` e `IDD`/`IDU`;
> * `while` e `=` aparecem sem aspas no PDF, mas são terminais;
> * `exemplo.ssl` não é válido para a VF; crie exemplos novos.
>
> Liste e decida, com justificativa, as inconsistências entre a
> gramática, o texto e os exemplos do livro. Já identificadas:
> * `'T'` em `DC` é o não terminal `T`;
> * o acesso a campo usa `ID`, não `IDU`;
> * `return` não existe na gramática nem no lexer;
> * `B` exige declaração e comando;
> * `NF` × `NB`/`MF` na fig. 7.18;
> * identificadores com `_` nos exemplos;
> * tipo universal com kind `SCALAR_TYPE_`;
> * listas `pNext` que misturam parâmetros e locais;
> * os dois `CheckTypes`;
> * indexação de string;
> * `break`/`continue` sem regras de atributos.
>
> Altere a linguagem apenas quando for indispensável, e documente cada
> alteração.
>
> **Implementação** em C11, compilável com `gcc` e `cl.exe`, sem
> bison/yacc/GOLD nem gerador externo:
> 1. analisador la-LR(1) dirigido por tabela, com o algoritmo da fig. 4.6
>    e a tabela construída pelo programa (FIRST, closure, goto, fusão de
>    núcleos); conflitos contados e mostrados, sendo esperado só o do
>    else pendente, resolvido por empilhar; erro sintático com linha,
>    token encontrado e tokens esperados;
> 2. escopo como pilha de níveis, com `IDD`, `IDU`, `NB` em struct e
>    bloco, `NF` na função e recursão permitida;
> 3. tipos: kinds, pré-definidos, universal, declarações, expressões,
>    condições, atribuição, left-values, chamadas (`MC`) e retorno;
> 4. manter a organização do repositório, com o lexer compartilhado e um
>    modo `--tokens` com a saída da VC; erros léxicos impedem o
>    sintático, e erros de escopo e tipo são acumulados.
>
> **Verificação:** um executável de testes com `assert`, cobrindo a
> tabela (1 conflito empilha/reduz, 0 reduz/reduz), programas válidos e
> um caso para cada erro sintático, de escopo e de tipo; exemplos `.ssl`
> válidos (com os do livro reescritos) e com erros; saída de `--tokens`
> igual à da VC, exceto pelas correções documentadas; compilação com
> `gcc -std=c11 -Wall -Wextra -Wpedantic` sem avisos.
>
> **Regras:** não reimplementar o léxico (alterações permitidas: `return`
> na posição alfabética da tabela, do enum e de `tokenName`; os bugs
> acima; o estritamente necessário para o parser). Seguir a estrutura de
> código do livro (pilha semântica, `Semantics` com `switch`,
> marcadores). Código mínimo, legível e acadêmico. Ao final, revisar se
> toda regra tem ação semântica e se toda regra de atributos dos cap. 5 e
> 6 foi implementada ou teve a omissão justificada. Atualize o README com
> o entendimento, a gramática usada, as análises, as decisões, a forma de
> compilar e testar, exemplos de saída e este prompt.

**Etapa de Geração de Código (cap. 7):**

> O contexto é um projeto da matéria de Compiladores. Acho que falta a
> etapa de "Geração de Código". Tome como base a apostila (PDF): deve ter
> alguma informação sobre o exercício lá. Tenho quase certeza que o
> trabalho implementa algo que é dito na apostila.
>
> Preciso que você me retorne um ZIP com o projeto inteiro, completo,
> funcional, e com um PDF da saída dele, pra eu mandar para o professor
> mostrando qual é a saída do projeto.
>
> Ou seja: adicione a etapa de Geração de Código, seguindo a apostila, e
> me retorne o ZIP completo e um PDF mostrando o output final do nosso
> trabalho.

## Autores

Rafael Schuinki e Mateus Mota
