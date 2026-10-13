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

# Consolidação: custo nativo de partículas opcionais

Data: 2026-10-13. Continuação dos marcos 4–5, engine 7.366.2.
Esta rodada amplia a evidência de performance; não encerra o orçamento de CPU
no aparelho alvo do Ratworks nem a validação de plataformas.

## Protocolo

Três executáveis separados, Release, mesmo compilador e flags `-O3 -DNDEBUG`:
`rigid` é a árvore local 2.4.1 com partículas OFF; `candidate` é a mesma árvore
com ON; `liquidfun` é o backend antigo. O nome `rigid` não significa uma cópia
intocada do upstream: esta rodada compara C/D e B/D do plano, não A/C.

Cinco repetições sequenciais, ordem de backends alternada, afinidade na CPU 2.
Não há Lua, renderização ou VSync. O host manteve seu governador `powersave`;
frequência, processos concorrentes e temperatura não foram controlados.
Foram executados 600 passos de aquecimento e 3.600 passos medidos, com timestep
1/60. São 10 e 60 segundos **simulados**, não períodos de coleta em tempo real.
Não confundir esse protocolo com o ensaio completo sugerido no plano.

O harness verifica a contagem final e a finitude das posições das partículas.
Os quatro testes nativos também passaram, incluindo igualdade do trace rígido
OFF/ON. `measure.py --cpu` permite repetir a afinidade e o relatório verifica
que os executáveis não mudaram durante a coleta.

Os critérios relativos propostos no plano (3% mediana e 5% p95) são usados como
sinalizadores, não como aprovação automática. Especialmente no mundo vazio,
percentuais de frações de microssegundo podem representar ruído do relógio/host.
Para fluidos, esta amostra não estabelece significância estatística nem um
orçamento universal de partículas por frame.

## Resultados

Mediana das cinco medianas / mediana dos cinco p95, em **microssegundos**.
Não são percentis de uma distribuição agregada.

| Cenário | OFF | ON | Legado |
| --- | ---: | ---: | ---: |
| Mundo vazio | 0.260 / 0.262 | 0.255 / 0.263 | 0.265 / 0.276 |
| 100 corpos, sleeping permitido | 8.361 / 9.165 | 8.640 / 9.377 | 7.317 / 7.755 |
| 100 corpos, acordados | 101.044 / 130.065 | 101.082 / 127.924 | 95.834 / 128.600 |
| 1 sistema vazio | — | 0.261 / 0.264 | 0.265 / 0.276 |
| 100 sistemas vazios | — | 0.428 / 0.439 | 0.417 / 0.428 |
| 1.000 sistemas vazios | — | 4.837 / 5.240 | 4.467 / 4.699 |
| 1.000 partículas, 1 sistema | — | 124.046 / 157.329 | 124.987 / 160.036 |
| 1.000 partículas, 10 sistemas | — | 66.136 / 72.954 | 66.316 / 75.790 |
| 5.000 partículas | — | 870.077 / 973.837 | 879.691 / 984.448 |
| 10.000 partículas | — | 1956.052 / 2186.134 | 1980.908 / 2213.368 |

A primeira coleta sinalizou +3,34% na mediana de `rigid_sleep_allowed`, com
faixas de medianas sobrepostas. Repetimos cinco vezes OFF/ON, sem recompilar,
na mesma CPU. A confirmação resultou em:

| Cenário | OFF mediana / p95 (us) | ON mediana / p95 (us) | Variação mediana / p95 |
| --- | ---: | ---: | ---: |
| Mundo vazio | 0.260 / 0.268 | 0.260 / 0.263 | 0,00% / -1,87% |
| 100 corpos, sleeping permitido | 7.783 / 8.619 | 7.598 / 8.481 | -2,38% / -1,60% |
| 100 corpos, acordados | 100.088 / 125.904 | 100.639 / 126.022 | +0,55% / +0,09% |

Não foi observada regressão repetível OFF/ON nesses cenários. Isso é evidência
favorável à flag opcional, não prova de custo zero ou aprovação geral de performance.
O candidato ficou próximo do legado nas cargas ativas de partículas; não se
conclui superioridade estatística. Com 1.000 sistemas vazios, ON teve mediana
0,370 us e p95 0,541 us acima do legado na primeira coleta: esse custo também
fica registrado, embora pequeno em valor absoluto.

Dados completos, hashes, flags e repetições:
[registro das duas coletas](box2d-liquidfun-performance-results.json).
Nenhum solver ou parâmetro de produção foi alterado nesta etapa.

## Interpretação e limites

- OFF exclui as fontes de partículas e o loop correspondente de `b2World::Step`.
- ON sem sistemas percorre uma lista vazia; não executa o solver de partículas.
- Sistemas vazios ainda são visitados. `b2ParticleSystem::Solve` retorna quando
  a contagem é zero; manter muitos sistemas tem custo próprio.
- O cenário de 1.000 partículas em dez sistemas contém cem partículas por sistema,
  com posições iniciais sobrepostas. Sistemas distintos não interagem entre si;
  eventual custo menor não representa uma otimização equivalente da mesma simulação.
- Os casos de 5.000/10.000 partículas usam o mesmo recipiente, aumentando a densidade
  da carga. Não medem emissão gradual, contatos com corpos móveis ou desenho.

A extensão permanece opcional por build. Não se recomenda autodestruir sistemas
vazios: isso quebraria reemissão e handles existentes. Otimizações no wrapper,
buffers e desenho precisam de medição própria na engine e de aceitação do gameplay.
Windows, Apple, Android em aparelho, jogos rígidos publicados e equivalência
interativa/visual dos exemplos externos continuam pendentes. O legado permanece.

## Reprodução

```sh
cmake -S src/test-lib/physics-compat -B build/physics-performance \
  -DPLAT=Linux -DCMAKE_BUILD_TYPE=Release
cmake --build build/physics-performance -j4
ctest --test-dir build/physics-performance --output-on-failure
python3 src/test-lib/physics-compat/measure.py \
  --build build/physics-performance --runs 5 --cpu 2 \
  --output /tmp/physics-performance-results.json
```

Escolher uma CPU permitida pelo ambiente; `--cpu` é opcional e depende da API de
afinidade Linux. Não compilar nem executar outras cargas voluntariamente durante
as medições. O relatório contém hashes, cache CMake, afinidade e todas as repetições.
