<!--
/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2015      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|-----------------------------------------------------------------------------------------------------------------------*/
-->

# Baseline para unificação Box2D/LiquidFun

Data: 2026-10-13. Etapa 0 do [plano de unificação](box2d-liquidfun-unification-plan.md).
Base da engine: `02de9bda`; documentação externa: `b1b41b6`.

Esta entrega inicia a execução com ferramentas reproduzíveis, medições dos backends
atuais, uma correção de comportamento indefinido no LiquidFun e um **protótipo nativo
de partículas portadas para 2.4.1**. Não houve troca dos plugins de produção nem
integração da nova base com os bindings Lua de fluidos.

## Entregas e resultados

| Verificação | Resultado | Limite |
| --- | --- | --- |
| Inventário dos exemplos publicados | 135 blocos Lua em `box2d.rst`, 45 em `box2LiquidFun.rst`, com origem, identificador e hash | 140 fragmentos precisam de contexto/harness específico |
| Cenas extraídas da documentação | 31 rígidas e 9 de fluido passaram no smoke test | Dois segundos por cena; não valida efeito visual ou entradas interativas |
| Harness nativo Release | 4/4 testes passaram, incluindo candidato e equivalência rígida OFF/ON | Queda/repouso, impulso, contenção, lifetime, reemissão, queries, filtros, acoplamento bidirecional, flags e distâncias de shapes |
| ASan, UBSan e LeakSanitizer | 4/4 passaram após a correção descrita abaixo | Linux x86_64; execução fora do sandbox para permitir LeakSanitizer |
| Regressões Lua da engine | `fluid_filter`, `fluid_rise`, `fluid_convex`, `fluid_depth`: quatro marcadores de sucesso e saída 0 | O teste de profundidade foi executado, mas não houve aprovação visual de captura |
| Medições nativas | Cinco repetições por backend, executadas serialmente | Coleta exploratória; não é aprovação de performance do candidato |

O runner nunca considera código 0 sozinho como sucesso: exige o marcador da cena.
Timeouts e erros falham a execução. Cada exemplo permanece marcado como
`visual_equivalence=not_checked` e `interactive_inputs=not_replayed`.
Os `.rst`, assets e traduções em mbm-documentation permanecem inalterados.

## Diferenças locais contra upstream 2.4.1

Foi comparada a árvore `include/` + `src/` com o arquivo oficial da tag `v2.4.1`.
Há **18 arquivos C++/headers diferentes**. Preservar essas diferenças no port;
não substituir a base local por upstream sem avaliar cada uma.

| Área | Alterações locais observadas |
| --- | --- |
| API/exportação | `b2_api.h`: macro Windows `BOX_2D_BUILD_DLL` e política de visibilidade |
| Body/joint/settings | Userdata `void*`, setters e `GetMassData()` retornando valor |
| Fixture | Getter const adicional para userdata |
| Gear | Tolerância linear/angular e critério de convergência no solver |
| Distance | Pontos testemunha na periferia mesmo com sobreposição por raios |
| Destruição de fixture | Recalcula massa somente quando a fixture destruída tem densidade positiva |
| Mouse joint | Permite corpos iguais no assert da construção do joint |
| Rope/math | Inicialização de parâmetros e construtores default |
| Outros | Versão declarada, includes, nomes de parâmetros, variáveis removidas e correção de comentário |

O inventário gerado contém hashes por arquivo e `box2d-local.patch` com o diff
completo. Os 18 arquivos são o snapshot anterior ao port; repetir o inventário na
árvore atual inclui também as novas alterações do candidato. A comparação não
altera a cópia upstream nem a engine.
Fonte: [arquivo oficial v2.4.1](https://github.com/erincatto/box2d/releases/tag/v2.4.1).
SHA-256 do tarball usado:
`d6b4650ff897ee1ead27cf77a5933ea197cbeef6705638dd181adc2e816b23c2`.

Também foi comparado o núcleo LiquidFun com a tag oficial `v1.1.0`: seis arquivos
diferentes no snapshot já com a correção de UBSan: `Box2D.h`,
`Common/b2GrowableBuffer.h`, `Common/b2Settings.h`, `Dynamics/b2Body.h`,
`Particle/b2ParticleSystem.cpp` e `Particle/b2ParticleSystem.h`. O diff está em
`build/physics-baseline/evidence/liquidfun-local.patch`. Essa comparação não inclui
o wrapper mini-mbm, que é mantido separadamente do núcleo terceiro.

## Correção encontrada antes do port

`computeRelativeTag` em
[b2ParticleSystem.cpp](../third-party/box2d-liquid-fun-2.3.1/Box2D/Particle/b2ParticleSystem.cpp)
deslocava `x=-1` à esquerda em tipo inteiro com sinal. UBSan interrompeu o teste
na busca de vizinhos. Os deslocamentos agora convertem os offsets para `uint32`
antes de deslocar, mantendo a aritmética módulo 2^32 das tags e eliminando essa
operação indefinida. O comentário identifica a adaptação local no código terceiro.

O cenário nativo com partículas reproduzia o erro antes da alteração e passou
depois, inclusive em ASan/UBSan/LeakSanitizer. O plugin foi recompilado e os quatro
smoke tests Lua passaram. Não houve bump de versão: é correção e infraestrutura
de teste, sem disponibilizar a funcionalidade de unificação.

## Medição inicial

Host: Intel Core i7-9750H, Linux x86_64, GCC 12.2.0, Release. Passo fixo 1/60 s,
8 iterações de velocidade e 3 de posição; LiquidFun usa uma de partículas.
Cada repetição aquece por 600 **passos simulados**, mede 3.600 passos e alterna
a ordem dos backends. A coleta não dura obrigatoriamente 60 segundos de relógio.
Não houve isolamento térmico/afinidade de CPU; outras aplicações estavam ativas.

Valores em microssegundos por `Step`; tabela usa a mediana das cinco medianas e
a mediana dos cinco p95, não percentis de uma distribuição agregada.

| Backend | Cenário | Mediana (us) | p95 (us) |
| --- | --- | ---: | ---: |
| Box2D 2.4.1 | Mundo vazio | 0.260 | 0.262 |
| LiquidFun atual | Mundo vazio | 0.264 | 0.275 |
| Box2D 2.4.1 | 100 corpos, sleeping permitido | 7.987 | 8.680 |
| LiquidFun atual | 100 corpos, sleeping permitido | 7.094 | 7.683 |
| Box2D 2.4.1 | 100 corpos, sleeping desabilitado | 102.024 | 113.568 |
| LiquidFun atual | 100 corpos, sleeping desabilitado | 99.378 | 143.167 |
| LiquidFun atual | 1 sistema vazio | 0.272 | 0.283 |
| LiquidFun atual | 100 sistemas vazios | 0.428 | 0.439 |
| LiquidFun atual | 1.000 sistemas vazios | 4.311 | 4.985 |
| LiquidFun atual | 1 sistema com 1.000 partículas | 125.305 | 147.335 |

Os mundos vazios não têm corpos; cenários com corpos dinâmicos ou partículas têm
três corpos estáticos de contenção. Os números de corpos da tabela são dinâmicos.
Permitir sleeping não significa que todos os corpos estejam dormindo em cada passo.

Há custo mensurável por sistema vazio, mas estes números **não demonstram o gargalo
do Ratworks**: não incluem emissão, buffers, wrapper Lua, renderização ou memória
retida. Tampouco medem overhead de partículas na mesma base 2.4.1; essa comparação
depende do candidato OFF/ON. Não usar pequenas diferenças A/B para escolher solver.

## Protótipo nativo opcional

Os arquivos exclusivos foram portados para `third-party/box2d-2.4.1/src/particle/`
e headers correspondentes em `include/box2d/`. As integrações em mundo, corpo,
timestep, shapes, consultas, callbacks e realocação de stack são condicionadas
a `MBM_BOX2D_PARTICLES`. O candidato usa o solver rígido local 2.4.1; os solvers
de contato, island e joints não foram substituídos pelo código 2.3.

O CMake independente gera `physics_rigid` (OFF), `physics_liquidfun` (legado) e
`physics_candidate` (ON). Cada um liga apenas uma implementação. O plugin normal
`box2d` foi recompilado com as fontes de partículas excluídas; inspeção de símbolos
confirmou ausência de definições de métodos `b2ParticleSystem`. A cópia antiga de
LiquidFun é preservada para baseline e produção durante a transição.

Além das conversões de includes/tipos, o port mantém a aproximação de raiz inversa
do LiquidFun usando `memcpy` em vez de leitura de membro inativo de union. As
divisões vetoriais mantêm divisão por componente. A realocação do último bloco de
stack mantém a contabilidade também ao migrar para heap. Avisos de origem e licença
dos trechos importados foram preservados.

Testes de partículas agora cobrem consulta AABB/raycast do mundo, passagem por
fixture filtrada, fluido empurrando corpo dinâmico e perdendo velocidade no impacto,
nove flags de partículas e distância/normal de circle, polygon e chain/edge.
As flags foram verificadas quanto a finitude/contenção, não equivalência visual.
Um fixture inicial com chão curto permitia partículas tensile saírem pela borda
nos dois backends; o chão do teste foi ampliado sem alterar os parâmetros físicos.

Comparação rígida: pilha de 20 corpos e pêndulo com distance joint, sleeping
desabilitado, 600 passos. O hash de posições/ângulos/velocidades em todos os passos
foi `a928518e6340cb08` tanto OFF quanto ON, sem criar sistemas de partículas. Isso
vale para este cenário/build; não representa prova geral de determinismo.

Nova coleta serial de cinco repetições, com o mesmo protocolo exploratório:

| Cenário | OFF mediana/p95 (us) | ON mediana/p95 (us) | Legado mediana/p95 (us) |
| --- | ---: | ---: | ---: |
| Mundo vazio | 0.260 / 0.262 | 0.260 / 0.263 | 0.264 / 0.274 |
| 100 corpos, sleeping permitido | 8.179 / 8.545 | 7.579 / 8.099 | 7.420 / 7.587 |
| 100 corpos, sleeping desabilitado | 100.640 / 124.140 | 99.873 / 118.049 | 95.676 / 101.116 |
| 1.000 partículas | indisponível | 124.365 / 159.353 | 123.303 / 141.582 |

Não apareceu regressão nos cenários rígidos desta coleta, mas isso não aprova
performance em produção. O p95 de partículas no candidato foi maior; repetir com
ambiente controlado e perfilar antes de aceitar o orçamento de fluido. Os dados
brutos ficam em `candidate-measurements.json` junto à primeira coleta.

Limites do protótipo: caminho escalar, sem NEON validado; desenho de debug de
partículas e deslocamento de origem com partículas ainda não integrados; falta
validar todos os callbacks, flags/grupos, ghost vertices e shapes em movimento.
Não ligar este perfil experimental a jogos. A ABI ON é diferente da OFF; todas
as unidades de um futuro consumidor devem usar a mesma definição. Não definir
`MBM_BOX2D_PARTICLES` globalmente no build de produção nesta etapa.

## Reprodução

As ferramentas ficam em [src/test-lib/physics-compat](../src/test-lib/physics-compat).
O CMake é independente da engine e gera um executável por backend, evitando
colisões entre símbolos das bibliotecas. Não precisa de janela nem áudio.

```sh
cmake -S src/test-lib/physics-compat -B build/physics-baseline \
  -DPLAT=Linux -DCMAKE_BUILD_TYPE=Release
cmake --build build/physics-baseline -j 4
ctest --test-dir build/physics-baseline --output-on-failure
python3 src/test-lib/physics-compat/measure.py \
  --build build/physics-baseline \
  --output build/physics-baseline/evidence/measurements.json

cmake -S src/test-lib/physics-compat -B build/physics-baseline-asan \
  -DPLAT=Linux -DCMAKE_BUILD_TYPE=Debug -DPHYSICS_SANITIZERS=ON
cmake --build build/physics-baseline-asan -j 4
ctest --test-dir build/physics-baseline-asan --output-on-failure
```

Baixar/extrair a tag oficial em uma pasta separada; ajustar os caminhos abaixo.
O inventário não baixa dependências nem executa os blocos extraídos:

```sh
python3 src/test-lib/physics-compat/inventory.py \
  --documentation /home/michel/mbm-documentation \
  --upstream-box2d /tmp/mbm-box2d-upstream/box2d-2.4.1 \
  --output build/physics-baseline/evidence
python3 src/test-lib/physics-compat/run-examples.py \
  --inventory build/physics-baseline/evidence/inventory.json \
  --documentation /home/michel/mbm-documentation \
  --engine bin/debug/linux_x86/mini-mbm \
  --output build/physics-baseline/examples --seconds 2
```

O runner dos exemplos usa um display acessível e o binário existente; não recompila
a engine automaticamente. Seguir [engine-testing](../.agents/skills/engine-testing/SKILL.md)
para preparar o runtime e evitar diálogos. Conferir assets antes da execução.
Em sandbox com ptrace, LeakSanitizer pode falhar por limitação do ambiente; nesta
verificação ele passou fora do sandbox, sem desabilitar a detecção de vazamentos.

Inventário, diff, cenas geradas, logs, hashes de binários e medições brutas ficam
em `build/physics-baseline/`. Um resumo durável da coleta está em
[box2d-liquidfun-baseline-results.json](box2d-liquidfun-baseline-results.json).
Os resultados gerados não devem ser confundidos com exemplos publicados atualizados.

## Próximas condições do plano

- Completar contextos e critérios para os 140 fragmentos; registrar intenção,
  capturas e entradas dos exemplos interativos antes de alegar equivalência.
- Incorporar a comparação LiquidFun ao gerador automático e inventariar os contratos
  e melhorias do wrapper que precisam migrar para o candidato.
- Medir o Ratworks completo, inclusive memória e custo do wrapper/render, e obter
  baseline dos jogos rígidos existentes.
- Completar a prova de conceito nativa (inclusive p95 e limites acima); só depois
  integrar Lua, adaptar exemplos externos e validar plataformas.

As etapas 0 e 1 têm cobertura inicial executável, mas seus critérios completos ainda
não foram satisfeitos. Nenhuma plataforma além de Linux foi validada nesta entrega.
