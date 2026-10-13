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

# Consolidação desktop: retirada do backend antigo

Data: 2026-10-13. Engine **7.367**. Marco 5 concluído no escopo aprovado:
**Linux, Windows MSVC Win32 e macOS arm64**.

O usuário aceitou os exemplos recuperados de mbm-documentation como cobertura dos
jogos antigos e autorizou a consolidação dessas plataformas agora. Android em
aparelho, iOS, Windows x64 real/MinGW, macOS Intel e MAS assinado ficam para uma
etapa posterior. Isso substitui o bloqueio anterior por toda a matriz de plataformas.
A aceitação da flag CMake e do Ratworks já estava registrada.

## Mudanças

- Removidos 127 arquivos do solver LiquidFun/Box2D antigo e do wrapper
  `plugins/box2d-liquid-fun-lua`. O único backend da engine é Box2D 2.4.1.
- Preservados os arquivos de partículas portados em `third-party/box2d-2.4.1` e
  os bindings unificados. `USE_BOX2D_PARTICLES=OFF` exclui essas fontes e métodos;
  ON habilita fluidos. `USE_ALL` mantém ON como padrão e respeita OFF explícito.
- Mantidos `require "box2d"` e a fachada `box2dLiquidFun`. O projeto Visual Studio
  de nome `box2d-liquid-fun` continua necessário: contém a fachada, não o solver antigo.
- `USE_BOX2D_LIQUID_FUN=ON` agora falha com instruções de migração. Remover a flag
  do cache ou passá-la OFF; habilitar Lua, Box2D e partículas para usar fluidos.
- Removidas dependências/includes antigos de CMake, embedded e Visual Studio.
  Gradle recebe o valor explícito de partículas. A distribuição MinGW passa a
  exigir `USE_BOX2D_PARTICLES`, corrigindo a exigência incompatível da flag antiga.
- Harness nativo usa OFF/ON por padrão. Comparação histórica é opt-in com
  `PHYSICS_LEGACY_SOURCE`, exclusivamente no projeto de testes, fora da engine.
- Atualizados guia, API, inventário de símbolos, README, contextos e instruções de
  portabilidade. Links históricos apontam para a revisão anterior à remoção.

## Validação desta alteração

| Verificação Linux | Resultado |
| --- | --- |
| Engine Debug + backend/fachada ON | Build concluído |
| Engine Debug + backend/fachada OFF, output isolado | Build concluído |
| Matriz CMake | 9/9: defaults, USE_ALL, ON/OFF e flags inválidas |
| Harness Release sem fontes antigas | 3/3: rígido, partículas e equivalência rígida OFF/ON |
| Harness com fontes históricas externas | 4/4, incluindo LiquidFun antigo |
| Contratos Lua ON | 6/6: moderno, fachada, emissão, filtros, contorno e profundidade |
| Contrato Lua OFF | 1/1: API rígida e rejeição explícita de fluidos |
| Ratworks reduzido | 5 ciclos, pico 385 partículas, 16 s solicitados / 17,072 s de parede |
| Manifesto MSVS | 59 fontes ON, 50 OFF, duas na fachada |
| Documentação externa | Sphinx inglês e pt_BR sem avisos; traduções novas verificadas no HTML |

Esta rodada não usa sanitizadores e não reexecuta Windows/macOS. As evidências
nativas dessas plataformas permanecem nos relatórios [Windows](box2d-liquidfun-windows.md)
e [macOS](box2d-liquidfun-macos.md). A retirada não modifica o solver unificado nem
os algoritmos de renderização que passaram nessas validações. As 40 cenas recuperadas
foram aceitas pelo usuário; não foram reexecutadas nesta rodada, nem se alegam 140
cenas executadas a partir dos 140 fragmentos inventariados.

Resultados com hashes e caminhos locais: [registro JSON](box2d-liquidfun-retirement-results.json).
A medição de uma repetição nesta etapa serve apenas para verificar o coletor e a
proveniência da fonte histórica; não substitui o estudo de performance anterior.

## Documentação externa e reprodução

Alterações sem commit em `/home/michel/mbm-documentation` (master `4ff9d93`) e
`/home/michel/mbm-documentation-pt-br-retirement` (destacado de pt-br `d8721aa`).
A segunda cópia permite revisar/integrar a alteração sem reservar o branch pt-br.
O checkout destacado antigo `mbm-documentation-pt-br` e suas alterações foram
preservados. Nenhum commit, push ou publicação foi realizado.

O [guia de migração](box2d-liquidfun-migration.md) contém as flags e comandos de
reprodução. Revisão de fallback: `b488870c5c5a42cc2c194d38b0057801c114455b`.
Use um checkout/output separado para a engine histórica; restaurar as fontes
externas no harness atual foi validado. Não é necessário manter o backend antigo
na árvore de produção.

Próxima etapa: aceitação dos alvos adiados, começando por Android em aparelho e
pelo runtime iOS, com builds ON/OFF e os mesmos contratos. Nenhum desses alvos é
marcado como aprovado por esta consolidação desktop.
