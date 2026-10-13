<!--
┌──────────────────────────────────────────────────────────────────────────────┐
│ MIT License                                                                 │
│ Copyright (c) 2026 Michel Braz de Morais                                      │
│                                                                            │
│ Permission is hereby granted, free of charge, to any person obtaining a copy │
│ of this software and associated documentation files (the "Software"), to    │
│ deal in the Software without restriction, including without limitation the │
│ rights to use, copy, modify, merge, publish, distribute, sublicense, and/or │
│ sell copies of the Software, and to permit persons to whom the Software is │
│ furnished to do so, subject to the following conditions:                   │
│ The above copyright notice and this permission notice shall be included in│
│ all copies or substantial portions of the Software.                        │
│ THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR │
│ IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,  │
│ FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL   │
│ THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER│
│ LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING   │
│ FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER       │
│ DEALINGS IN THE SOFTWARE.                                                  │
└──────────────────────────────────────────────────────────────────────────────┘
-->

# Revisão e limpeza Linux antes da validação Windows

Data: 2026-10-13. Revisão da configuração, dependências, artefatos e documentação
de Box2D/LiquidFun. Sem alteração de solver, API Lua ou defaults das flags.

## Correções

| Local | Correção |
| --- | --- |
| CMake raiz | Resumo das flags mostra Box2D e partículas ON/OFF; ajuda distingue a extensão unificada do fallback legado |
| `show_built` | Aguarda o target da fachada nos perfis que a produzem separadamente, antes de listar os artefatos |
| CMake do plugin | Removidos variável sem uso e blocos comentados obsoletos |
| Listas de fontes Linux | Removidas duas referências a `BOX2D_LIQUID_FUN_WRAP_SOURCES`, variável não definida no projeto; plugins continuam compilados em seus targets |
| README EN/PT e contextos de agentes | Descrições passam a identificar o port opcional na base 2.4.1 e a árvore antiga como fallback de comparação |
| `check-cmake.py` | Perfis unificados explícitos e combinação com flag antiga; verifica isolamento das fontes, presença da fachada e dependência de `show_built`; outputs isolados por caso |

## Verificações

Nove configurações verificadas: padrão, USE_ALL, USE_ALL com partículas OFF,
unificado ON, unificado OFF, unificado com flag antiga, legado isolado e rejeições
esperadas de partículas sem Lua/sem Box2D. Os testes usam Unix Makefiles
explicitamente para inspecionar as dependências geradas.

Os dois targets de fachada ON/OFF foram reconstruídos. Contratos Lua: **6/6 ON e
1/1 OFF**, com ASan/UBSan/LSan e ODR padrão, seleção Mesa e sem supressões, no mesmo
ambiente das rodadas anteriores. A seção dinâmica ELF da fachada confirma a
dependência de `box2d.so`. Resultados: [registro](box2d-liquidfun-linux-cleanup-results.json).

A seleção das fontes não inclui wrapper nem solver legado em perfis unificados;
o fallback inclui a árvore antiga e exclui a 2.4.1. Os entry points de
compatibilidade e sua disponibilidade em OFF permanecem intencionais: em OFF,
a tentativa de carregar fluidos emite o erro de configuração esperado.

## Distribuição e itens preservados

Não há regra `install()` de plugins da engine no CMake Linux atual. O fluxo usa
os artefatos do diretório de saída. A regra de instalação existente no projeto
standalone upstream de Box2D não é a distribuição do plugin Lua da engine.
Nenhum novo instalador ou mudança de RPATH foi introduzido nesta limpeza.

`box2dLiquidFun.so` depende de `box2d.so` e das bibliotecas da engine/Lua; copiar
somente a fachada não é suficiente. O RUNPATH do build inspecionado aponta ao seu
diretório de saída. Para mover um conjunto de bibliotecas, configurar a busca
adequadamente, como fazem os runners com `LD_LIBRARY_PATH` e `LUA_CPATH` isolados.
Não misturar outputs do perfil legado com o unificado no mesmo processo.

As pastas antigas e referências explícitas de comparação foram preservadas. Os
relatórios históricos não foram reescritos como se descrevessem a versão atual.
Referências antigas nas listas de fontes de outras plataformas ficam fora desta
limpeza Linux. Windows, Apple e execução Android em aparelho continuam pendentes;
a revisão não autoriza remover o fallback nem certifica um pacote relocável.
