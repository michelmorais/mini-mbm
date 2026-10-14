<!--
┌──────────────────────────────────────────────────────────────────────────────┐
│ MIT License                                                                  │
│ Copyright (c) 2026 Michel Braz de Morais                                     │
│                                                                              │
│ Permission is hereby granted, free of charge, to any person obtaining a copy │
│ of this software and associated documentation files (the "Software"), to     │
│ deal in the Software without restriction, including without limitation the   │
│ rights to use, copy, modify, merge, publish, distribute, sublicense, and/or  │
│ sell copies of the Software, and to permit persons to whom the Software is   │
│ furnished to do so, subject to the following conditions:                     │
│ The above copyright notice and this permission notice shall be included in   │
│ all copies or substantial portions of the Software.                          │
│ THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR   │
│ IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,     │
│ FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL      │
│ THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER   │
│ LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING      │
│ FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER          │
│ DEALINGS IN THE SOFTWARE.                                                    │
└──────────────────────────────────────────────────────────────────────────────┘
-->

# Plano de avaliação de um plugin Box2D 3 para física rígida

Atualizado em **14 de outubro de 2026**. Escopo desta entrega: documentação e análise; nenhum plugin, benchmark ou alteração de dependência de produção foi implementado.

## Direção adotada

**Manter o plugin atual com Box2D 2.4.1 e avaliar um novo plugin opcional `box2d3`, exclusivamente para física rígida e projetos novos.** O nome `box2d3` é provisório. A candidata técnica desta avaliação é a release estável **3.1.1**, publicada em 4 de junho de 2025, confirmada na investigação inicial por `releases/latest` (`prerelease=false`).

O primeiro investimento será uma **comparação nativa pequena de desempenho e qualidade**, antes da implementação do plugin completo. Ela deve responder se há benefício relevante nas cargas e plataformas da mini-mbm. A evidência upstream justifica medir; não determina o retorno local desse investimento.

Esta direção substitui a proposta de migrar o backend atual com compatibilidade integral:

- `box2d` continua em 2.4.1, com partículas opcionais por `USE_BOX2D_PARTICLES`.
- `box2dLiquidFun` continua fachada do mesmo backend 2.4.1. Seus defaults, `legacyApi`, API Lua e comportamento permanecem contratos existentes.
- `box2d3` terá API própria baseada na 3.x, sem LiquidFun, sem port de partículas e sem camada obrigatória de compatibilidade 2.x.
- Ratworks permanece no plugin atual. Não haverá migração de seus scripts, assets ou fluidos como requisito deste projeto.
- Gear, pulley, friction joint e callbacks antigos não precisam ser emulados no novo plugin. A API nova declara somente capacidades efetivamente suportadas.
- A avaliação não inclui `vocaro/liquidfun`: trata-se de um port comunitário experimental de partículas, sem utilidade necessária para o escopo rígido escolhido.

O requisito anterior de um único backend para toda a engine é substituído pela possibilidade de **dois plugins independentes**, cada um com seus próprios mundos. O plugin atual continua unificado: não se reintroduz um solver LiquidFun separado. Mundos 2.4 e 3.x não compartilham corpos, joints ou contatos; não haverá ponte de forças entre eles. A distribuição conjunta e o carregamento simultâneo dependem de validação de símbolos e ownership, descrita abaixo.

## Evidência, inferência e hipóteses

- **Fato confirmado (F):** observado em fontes identificadas. Um relato upstream é evidência publicada, não uma medição nossa.
- **Inferência (I):** consequência técnica plausível da arquitetura; precisa de validação na integração.
- **Hipótese (H):** benefício, orçamento ou proposta que ainda requer experimento.

A investigação realizada até aqui foi estática: código local, tags upstream, documentação e resultados publicados. **Não executamos benchmarks comparativos locais nem validamos runtime do plugin novo.** Artefatos temporários de investigação ficam em `/tmp/` ou `build/`, nunca em `docs/`.

## O que sustenta a expectativa de ganho

| Evidência oficial | O que demonstra | Limite da conclusão |
| --- | --- | --- |
| Erin Catto relata que seus testes da 3.0 foram mais de duas vezes mais rápidos que 2.4, inclusive em uma thread | Há evidência direta publicada de ganho entre gerações, associada a organização de dados e SIMD | Não é resultado da nossa 2.4.1 modificada, nem garantia para todas as cenas ou para a 3.1.1 integrada |
| Benchmark de pirâmide com 5.050 corpos e quatro workers: AMD 7950X, scalar 1,91 ms e SSE2 1,02 ms; Apple M2, scalar 1,47 ms e NEON 0,95 ms | SIMD pode trazer ganho mensurável no solver de contatos, aproximadamente 1,87x e 1,55x nesses casos | Comparação dentro da geração 3; não é 3.1 versus 2.4. Os tempos são da simulação, não do frame completo de um jogo |
| Anúncio 3.1 relata desempenho aproximadamente dobrado em colisão contínua, GJK 20% mais rápido e otimizações NEON | Melhorias adicionais em componentes relevantes | Relato de evolução da 3.0 para 3.1; percentuais não se somam nem equivalem ao ganho total |
| Tag 3.1.1 contém `benchmark/main.c` e CSVs por CPU/SIMD/threads | Há código e resultados inspecionáveis para estudar cargas e escalabilidade | CSVs não constituem por si só uma comparação controlada com a nossa 2.4.1; preservar proveniência/configuração ao reproduzir |

Fontes: [anúncio oficial da 3.0](https://box2d.org/posts/2024/08/releasing-box2d-3.0/), [experimento SIMD](https://box2d.org/posts/2024/08/simd-matters/), [anúncio oficial da 3.1](https://box2d.org/posts/2025/04/box2d-3.1/), [benchmarks da tag 3.1.1](https://github.com/erincatto/box2d/tree/v3.1.1/benchmark).

**I:** cenas com muitos contatos ativos são candidatas fortes a ganho; cenas pequenas ou majoritariamente adormecidas podem economizar pouco tempo absoluto. Um jogo limitado por renderização ou Lua pode ter pouca melhoria de frame mesmo com solver mais rápido. Exemplo hipotético: reduzir física de 4 para 2 ms economiza 2 ms; reduzir de 0,2 para 0,1 ms economiza 0,1 ms, embora a aceleração proporcional seja igual.

Outros benefícios possíveis para projetos novos: cápsulas/shape casts, sensores revisados, eventos de movimento, controle por substeps e handles com geração. Só serão benefícios da API mini-mbm quando implementados e testados. O ganho não vem simplesmente de o código upstream ser C: organização de dados, algoritmos, SIMD e uso de threads são os mecanismos relevantes.

## Fontes e recorte reproduzível

Referências upstream fixadas na tag, para evitar confundir a documentação corrente com desenvolvimento:

| Referência | Uso |
| --- | --- |
| [Box2D v3.1.1](https://github.com/erincatto/box2d/tree/v3.1.1), árvore `8c66146` | Candidata estável inspecionada |
| [Box2D v2.4.1](https://github.com/erincatto/box2d/tree/v2.4.1), árvore `9ebbbcd` | Base para comparação do vendor local |
| [Guia de migração da tag](https://github.com/erincatto/box2d/blob/v3.1.1/docs/migration.md) | Mudanças 2.4 → 3.0; não cobre sozinho 3.1 |
| [Notas 3.1](https://github.com/erincatto/box2d/blob/v3.1.1/docs/release_notes_v310.md) | Materiais, filtros, sensores, eventos, SIMD e mudanças posteriores ao guia |
| [API pública](https://github.com/erincatto/box2d/blob/v3.1.1/include/box2d/box2d.h), [tipos](https://github.com/erincatto/box2d/blob/v3.1.1/include/box2d/types.h), [IDs](https://github.com/erincatto/box2d/blob/v3.1.1/include/box2d/id.h) | Autoridade para assinaturas e disponibilidade |
| [README](https://github.com/erincatto/box2d/blob/v3.1.1/README.md), [CMake](https://github.com/erincatto/box2d/blob/v3.1.1/src/CMakeLists.txt), [CPU/SIMD](https://github.com/erincatto/box2d/blob/v3.1.1/src/core.h), [CI](https://github.com/erincatto/box2d/blob/v3.1.1/.github/workflows/build.yml) | Requisitos e limites da evidência de plataforma |
| [FAQ da tag](https://github.com/erincatto/box2d/blob/v3.1.1/docs/FAQ.md) | Determinismo, limites e escopo rígido |
| [LiquidFun oficial](https://github.com/google/liquidfun), [guia de partículas](https://google.github.io/liquidfun/Programmers-Guide/html/md__chapter11__particles.html) | Origem do solver de fluidos; não constitui suporte oficial a 3.1.1 |

`main` do Box2D é desenvolvimento: não selecionar seu HEAD nem inferir capacidades da candidata por documentação não versionada. Reconsultar releases antes de iniciar a prova; uma nova release exige atualizar esta comparação.

Snapshots locais usados: engine `87b2452edbf18cdf8d36675f4d1745957747a26d`; Ratworks `3b87510bcab2047c0a32345bcbca2e3bd3a79ef4`; documentação externa `master` em `ab6bdd846fadc606fe53d588b1286dbed6c7e82e` e `pt-br` em `ea7e5edcd61af158af715f6b1ac54a7d7bcb4eca`. Leitura de `master` por `git show`, sem trocar o checkout `pt-br`.

## Arquitetura proposta do novo plugin

Tudo nesta seção é **proposta**, não API disponível. Priorizar uso da API pública 3.1.1 e evitar fork do solver. Um recurso que exija patch profundo deve ter benefício próprio demonstrado antes de ampliar o escopo.

| Área | Direção para `box2d3` | Relação com o plugin atual |
| --- | --- | --- |
| Módulo/build | `require "box2d3"`, alias `libbox2d3`; flag proposta `USE_BOX2D3=1`, opcional e inicialmente fora de `USE_ALL` | Não redirecionar `box2d` nem `box2dLiquidFun`; `USE_BOX2D_PARTICLES` continua exclusivo da integração 2.4 |
| API Lua | Vocabulário e defaults próprios, próximos de world/body/shape/joint da 3.x; sem `legacyApi` | Não prometer substituição direta de scripts; publicar guia de escolha |
| Ownership | Objetos Lua com owner world e handles privados validados por geração; destruição explícita e GC seguro | Não aceitar userdata/handles de outro plugin ou mundo |
| Corpos/renderizables | Separar corpo físico de vínculo visual; definir unidades, escala, ângulos e offsets explicitamente | Não copiar automaticamente defaults históricos de escala/gravidade; impedir que dois mundos atualizem o mesmo renderizable |
| Shapes/materiais | Expor shapes e chains nativos, densidade, material e filtros conforme suporte implementado | Não emular fixture antiga nem transportar toda a API de polígonos/fluidos por obrigação |
| Joints | Começar com subconjunto útil dos joints nativos, documentado e testado; Hz/ratio/limites conforme 3.x | Gear/pulley/friction ausentes não bloqueiam o projeto; não fornecer substitutos de mesmo nome com semântica diferente |
| Eventos | Entregar eventos de contato/sensor/movimento após Step, no thread principal, com validade documentada | Não reconstruir sequência Pre/PostSolve 2.x nem apresentar hit events como PostSolve |
| PreSolve | Inicialmente sem callback Lua síncrono dentro do solver; se necessário, política nativa restrita e validada | Evita acoplar Lua ao worker e ao instante de resolução de contatos |
| Queries | AABB, overlap/raycast e shape casts por prioridade; definir retornos, filtros e early termination | Sem consulta de partículas, sem herdar ambiguidades das fixtures antigas |
| Stepping | `dt` e substeps explícitos; proposta de passo fixo com acumulador e limite de recuperação, sincronização visual definida | Sem converter velocityIterations/positionIterations antigos; não alterar loop do plugin 2.4 |
| Threading | Primeiro um worker; scheduler opcional numa etapa posterior | SIMD pode ser aproveitado sem começar por multithreading |

Antes de congelar a API, produzir exemplos mínimos de criação, evento, query e remoção de uma cena nova. Definir manual versus automatic stepping sem permitir atualização dupla do mesmo mundo, política de pause, tempo acumulado, descarte/limite de backlog e instante de publicação visual. Esses detalhes ainda são questões de design; não herdar implicitamente a ordem pré-Step do wrapper antigo.

### Integração e isolamento

Seguir a [skill new-plugin](../.agents/skills/new-plugin/SKILL.md): entry points `luaopen_box2d3` e `luaopen_libbox2d3`, export macro e metatables próprios, helpers de validação/registro e lifecycle da engine quando houver atualização automática. Destruir recursos com segurança em `onDestroy` e GC, inclusive quando ambos forem chamados. Estado de mundos, scheduler e registros deve permanecer privado ao plugin; não expor storage/backend em headers públicos do core.

A flag nova requer Lua e não depende de `USE_BOX2D` ou de fluidos. Validar builds com nenhum plugin, só o atual OFF, só o atual ON, só o novo e ambos. O target upstream chamado `box2d` conflita com o target existente: atribuir nomes internos distintos sem alterar o nome público do plugin atual. Compilar `.c` como C17 e wrapper como C++17; não aplicar cegamente flags C++ ao solver C.

Um nome de DLL diferente não garante isolamento. As versões compartilham nomes `b2*` em tipos e algumas funções C++ inline, além de possíveis símbolos globais. Manter headers em translation units separadas, exportar apenas a superfície necessária e auditar tabelas de símbolos/ODR. Desktop exige teste de carregamento em ambas as ordens. Embedded/static exige link de ambas as versões, sem definições duplicadas/interposição e com Lua/core/helper fornecidos conforme o padrão de iOS/MAS. Se a coexistência exigir renomeação de símbolos, estimar o custo; não prometer suporte antes da prova.

Cada mundo tem exatamente um solver. A troca de plugin exige recriação explícita da cena e adaptação de código, nunca troca transparente durante simulação. Não há partículas em `box2d3`, nem interação delas com seus corpos, mesmo que o módulo legado também esteja carregado.

## Plano de medição antes da implementação completa

### Comparação principal

Comparar **mini-mbm Box2D 2.4.1 local com partículas OFF** e **Box2D 3.1.1 oficial**, em executáveis nativos separados, sem Lua/render. Isso mede o solver e inclui os patches reais da base atual; 2.4.1 upstream puro pode ser um controle adicional, identificado separadamente.

Reutilizar a infraestrutura de `src/test-lib/physics-compat/`, com adaptadores de chamadas nativas e inputs comuns. Não migrar os testes legados para fazer a candidata passar: preservar seus asserts e acrescentar caminhos específicos para a API nova. O nome atual `candidate` no harness significa 2.4.1 com partículas ON; usar identificação inequívoca para 3.1.1 nos resultados.

`physics-baseline.cpp` já contém workloads rígidos acordados/adormecidos, aquecimento e coleta; `measure.py` mede simulação nativa. A suíte também contém fluidos, que continuam testes do backend atual e não entram no comparativo rígido 3.x.

| Carga | Objetivo e critério de qualidade |
| --- | --- |
| Mundo vazio e cena pequena | Medir overhead absoluto por passo; incluir cenário com dezenas de corpos, não só stress tests |
| Corpos ativos com muitos contatos | Pilhas/pirâmides com centenas e milhares de corpos; penetração, estabilidade, altura e tempo para repouso |
| Maioria dos corpos adormecida | Custo em idle, wake e movimentação posterior; não omitir contatos/eventos necessários para simular ganho |
| Joints comuns às duas versões | Cadeias/estruturas com distance, revolute e outros selecionados; erro de anchor/limites e estabilidade sob razões de massa |
| Corpos rápidos e paredes finas | CCD, tunneling e erro de trajetória dentro de tolerância; mesmos requisitos de colisão em ambas as versões |
| Queries/casts comparáveis | AABB e raycast nas mesmas geometrias, resultados equivalentes; shape casts novos avaliados como recurso, não contra operação inexistente |
| Criação/destruição e eventos | Custo e memória de churn; carga de contatos/sensores, com semânticas comparáveis explicitadas |

Mesmos inputs, seed, ordem de criação, unidades, duração simulada e passo externo. Iterações 2.x não equivalem a substeps 3.x: variar configurações e apresentar curvas **custo versus qualidade**. Fixar tolerâncias físicas por cena antes da medição. Não exigir igualdade bit a bit de trajetórias entre versões; verificar que não houve perda de corpos, contatos ou qualidade para ganhar velocidade.

Começar com um worker e SIMD padrão, registrando scalar como diagnóstico. Depois comparar 1/2/4 workers em cenas adequadas e contabilizar scheduler e total de CPU, não só wall time. Usar compilador/otimização, máquina, energia e afinidade comparáveis; registrar CPU, flags, matemática de ponto flutuante, hashes e configurações. Separar medições Debug/sanitizers das medições Release.

Manter aquecimento e duração suficientes; o harness atual usa 600 passos de aquecimento e 3600 medidos. Fazer ao menos cinco processos independentes, alternando ordem das versões; registrar mediana, p95/p99, dispersão, alocações, pico e tendência de memória. Nenhum resultado deve ser resumido apenas como um multiplicador global.

### Segundo gate: benefício na engine

Só após resultado nativo favorável, implementar um protótipo mínimo Lua/engine para uma cena rígida nova. Medir separadamente solver, registro/dispatch de eventos, binding Lua, sincronização visual e render. Comparar com cena equivalente no plugin atual; manter câmera, resolução, assets e eventos necessários. Medir VSync separadamente e observar frame completo.

Essa etapa detecta custos que o benchmark upstream não representa: alocação de tabelas Lua por contato, consultas por corpo, cópia de estado e sincronização de renderizables. Eventos de movimento podem reduzir scans de corpos adormecidos, mas isso precisa aparecer na medição. Multithreading não pode chamar Lua ou renderer nos workers; terminar tasks antes de publicar eventos/estado ou destruir o mundo.

### Critérios de decisão propostos (H)

- Correção e qualidade mínimas são gates obrigatórios, inclusive memória e lifecycle.
- Antes de medir, selecionar uma carga rígida representativa de um jogo novo que justificaria `box2d3` e um orçamento em ms para ela. Stress test sintético sozinho não justifica o produto.
- Como ponto de partida, procurar pelo menos **20% de redução repetível no p95 do solver** nessa carga, fora da dispersão, com benefício absoluto relevante ao orçamento escolhido. É um limiar proposto, não resultado ou exigência já validada.
- Cenas pequenas não devem sofrer regressão relevante de tempo absoluto. Definir o orçamento antes do experimento; percentuais em tempos quase nulos são pouco úteis.
- Na prova Lua, o benefício deve sobreviver ao wrapper e à sincronização. Se desaparecer, reduzir overhead ou encerrar a avaliação; não prosseguir baseado apenas no gráfico upstream.
- Um recurso exclusivo útil pode justificar o plugin mesmo sem ganho de velocidade, mas exige um caso de uso concreto e decisão de escopo explícita. Não trocar silenciosamente a justificativa de performance por uma lista de recursos.
- Se não houver vantagem suficiente ou o custo de manutenção/coexistência for desproporcional, manter apenas a solução atual e arquivar os resultados.

## Validação e preservação dos clientes atuais

| Frente | Reuso e entrega necessária |
| --- | --- |
| Backend atual | Preservar CTest OFF/ON e igualdade de traces em `compare-rigid.cmake`; manter contratos Lua e fachada sem alterações |
| Fluidos/Ratworks | Executar `run-lua-contracts.py`, `run-ratworks.py` e casos de emissão/filtro/contorno/depth como regressão quando build/helper/loader compartilhados mudarem; não testar Ratworks com `box2d3` |
| Novo solver | Reutilizar cenários rígidos e collector; adicionar cobertura apenas para lacunas de shapes/joints/sensores/CCD da API escolhida |
| API nova | Testar ownership entre mundos/plugins, IDs reciclados, destruição corpo→shapes/joints, world→todos, GC, dupla remoção, eventos pendentes e erros Lua |
| Coexistência | Nenhum plugin, atual OFF/ON, novo e ambos; require em ambas as ordens, símbolos, instâncias independentes, troca de cena e recusa de vínculo visual duplo |
| Threading futuro | Scheduler/finish, teardown com tasks, ausência de acesso a Lua nos workers, redução de overhead e repetibilidade com contagens diferentes |
| Documentação | Exemplos antigos continuam no plugin atual; exemplos novos exercitam apenas a API nova e deixam claros os recursos ausentes |

Os contratos `legacyApi`, `riseTime`, `fixtureMaskBits`, `convexPolygon`, lifetime, queries de partículas e render de fluidos permanecem onde estão. `getVersion()` do plugin atual continua reportando sua versão; o novo reportará 3.1.1. Não alterar o retorno antigo para sugerir atualização da engine física de todos os clientes.

**F, cliente inspecionado:** Ratworks usa `box2dLiquidFun` em `logic/physics.lua`; `logic/rat.lua` depende de categoria `0x0002`, máscara de fluido que exclui ratos, `setActive(false)`, conversão por escala, emissão gradual, lifetime e fluido 3D planar independente do lifetime do rato. Esses contratos reforçam a decisão de mantê-lo em 2.4.1.

**F, inventário documental anterior:** em cada branch externa (`master` e `pt-br`), `box2d.rst` contém 135 blocos Lua (45 candidatos a cena, 90 fragmentos), e `box2LiquidFun.rst` contém 47 (11 candidatos, 36 fragmentos). A classificação de `inventory.py` é heurística e não significa execução aprovada. Reutilizar `run-examples.py` para regressão do plugin atual quando a integração compartilhada mudar; não converter os exemplos para 3.x.

Usar ASan/UBSan em Linux/macOS quando disponíveis, MSVC ASan na configuração suportada, e TSan quando existir concorrência. RSS isolado não prova ausência de leaks. Validar Debug e Release, lifecycle prolongado e trocas de cena com marcadores de sucesso/exit code; testar o vínculo visual no renderer real conforme a [skill engine-testing](../.agents/skills/engine-testing/SKILL.md).

## Plataformas e requisitos

**F:** upstream 3.1.1 declara Windows/Linux/Mac; fontes reconhecem Android/iOS e x86/ARM. Biblioteca requer C17; C++20 é dos samples, enquanto wrapper/engine continuam C++17. CMake upstream 3.22 é inferior ao mínimo 3.25.1 da engine. SSE2/NEON dependem da CPU, AVX2 é opcional e existe modo sem SIMD. CI Windows da tag usa x64, não certifica Win32. Nenhum alvo novo foi compilado ou executado nesta análise.

| Prioridade/alvo | Evidência e compilação a exigir | Execução a exigir |
| --- | --- | --- |
| P0 Linux x86_64 | Desktop declarado; GCC/Clang, C17+C++17, SIMD e scalar diagnóstico, sanitizers | Benchmark comparável e depois Lua/lifecycle/coexistência/render |
| P0 Windows MSVC Win32 | Fontes reconhecem x86, CI upstream x64; verificar `.c`, atomics, alinhamento, SSE2 e ausência de truncamento de IDs | Execução Win32 nativa, benchmark, DLLs nas duas ordens e scene teardown |
| P0 macOS arm64 | Desktop e NEON; AppleClang, módulos/archives e sanitizers | Benchmark nativo, Metal e integração Lua |
| P1 Android arm64-v8a | NDK r29/API 24 conforme projeto, PIC/visibilidade e loader embedded | Device físico, pause/resume, memória e orçamento térmico |
| P1 iOS arm64 | Xcode, device/simulator separados, archives sem símbolos duplicados; deployment target da engine | Device assinado, lifecycle e render; simulator não substitui device |
| P1 Windows x64/MinGW | MSVC x64 e MinGW em jobs separados, exports/runtime e link | Execução nativa de cada configuração, não só cross-build |
| P1 macOS Intel | AppleClang x86_64/SSE2; universal se distribuído | Hardware Intel, não somente Rosetta |

A triagem de desempenho pode começar em Linux. Compilar Win32 cedo para identificar bloqueio de plataforma antes do binding completo. Distribuição nos P0 depende de evidência nos três; destinos P1 só são declarados suportados depois de compilar e executar a integração nativamente. Signed MAS permanece uma validação de distribuição separada.

Preservar opções de matemática do upstream, incluindo `-ffp-contract=off` onde aplicável; auditar flags globais/fast-math. Determinismo 3.x não equivale a trajetória idêntica à 2.4 nem a rollback determinism; Lua, ordem de criação e scheduler também precisam de política explícita.

## Etapas, esforço e critérios de avanço

Escala relativa para uma pessoa: P = dias; M = aproximadamente uma a duas semanas; G = várias semanas. As faixas são estimativas, não compromissos. Acesso a hardware e extensão da API têm impacto grande.

| Etapa | Entrega verificável | Gate | Esforço; confiança |
| --- | --- | --- | --- |
| 0. Preparar comparação | Tags/hashes, workloads comuns, budgets e tolerâncias; baseline 2.4.1 OFF preservada | Configurações reproduzíveis e qualidade definida antes dos números | P; alta |
| 1. Medir solver rígido | Resultados nativos 2.4.1 OFF versus 3.1.1; uma thread primeiro; compilação Win32 inicial | Avançar apenas com ganho útil ou caso de recurso concreto; parar se não justificar custo | M; média |
| 2. Definir API mínima e isolamento | Exemplos propostos, ownership, eventos, unidades/stepping; prova de link/carregamento separado e conjunto | Sem dependência de emulação legada ou fork amplo; decidir custo de coexistência | P–M; média |
| 3. Protótipo Lua/engine | Uma cena rígida equivalente, corpos/shapes básicos/eventos/remoção e vínculo visual | Benefício sobrevive ao wrapper; memória/lifecycle corretos e plugin antigo sem regressão | M; média |
| 4. Completar escopo aprovado | Subconjunto documentado de joints/queries/materiais e API nova, testes negativos e exemplos | Nenhum método anunciado sem suporte; fechar comportamento público antes de release | G; média-baixa |
| 5. Scheduler opcional | Comparação 1/2/4 workers, custo total de CPU e teardown seguro | Só incluir se houver ganho relevante sobre modo serial; não bloqueia entrega serial útil | M–G; baixa |
| 6. Validar e distribuir | P0 completo, P1 por destino, docs EN/PT, build opcional e versão da engine | Suporte anunciado apenas onde validado; novo plugin pode ser desabilitado independentemente | M–G; média-baixa |

As etapas 0 e 1 são a próxima investigação proposta, ainda não executada. Não iniciar automaticamente a implementação completa após esta atualização documental. O esforço é menor que uma migração compatível com fluidos, mas continua incluindo uma nova API pública e manutenção permanente de duas integrações.

## Alternativas e custo de manutenção

| Alternativa | Custo/benefício | Direção |
| --- | --- | --- |
| Manter apenas 2.4.1 | Menor manutenção, atende clientes atuais; não oferece recursos 3.x | Resultado válido se medições não justificarem o novo plugin |
| Plugin 3.x independente, API própria e sem fluidos | Usa capacidades nativas, reduz adaptação; adiciona bindings, testes, docs e matriz de builds | Opção em avaliação |
| Substituir plugin atual preservando tudo | Exige port de fluidos, compatibilidade de callbacks/defaults e joints ausentes | Retirado do plano ativo |
| Novo plugin com shim integral da API 2.x | Reintroduz custos e limitações da migração | Fora do escopo |
| Dois mundos sincronizados para misturar fluidos 2.4 e rígidos 3.x | Contatos/forças e lifecycle complexos; custo e estabilidade desconhecidos | Fora do escopo |

A manutenção inclui acompanhar releases 3.x, validar API C, scheduler e plataformas, conservar os contratos 2.4 existentes e manter guias claros de escolha. A seleção do plugin por um jogo novo é explícita. Suporte simultâneo não significa que todos os jogos devam carregar os dois.

## Isolamento, rollback e documentação

Experimentos em targets/branch próprios e fontes fixadas; builds `build/box2d3-eval/<baseline|v3>/<debug|release|asan>` com `MBM_OUTPUT_ROOT` absoluto por perfil. Baseline ON e OFF também precisam de raízes distintas quando usados para regressão. MSVC usa `OutDir`/`IntDir` separados. Não misturar `LUA_CPATH`, caches ou DLLs. Resultados, logs, CSVs e screenshots ficam em `build/` ou `/tmp`.

Benchmarks iniciais executam cada solver em processo separado; a prova de coexistência é uma atividade posterior deliberada. Rollback do experimento é retirar o target/flag novo; o backend antigo não é substituído. Na futura distribuição, manter `USE_BOX2D3` opcional e reverter mudanças compartilhadas isoladamente se necessário. Um jogo já escrito para a API 3.x não faz fallback automático para 2.4: rollback desse jogo exige sua versão anterior ou adaptação explícita. Não persistir IDs nativos nem migrar estado interno de solver ao vivo.

Documentação futura:

- `docs/box2d.md`: explicar escolha entre atual com fluidos e novo rígido, com links para APIs distintas.
- `docs/lua-api.md`: adicionar seção própria para `box2d3`; manter seções 10/11 corretas para 2.4.1, seus defaults e exemplos.
- Índice de símbolos, build CMake/MSVC/embedded e game template: registrar novo módulo opcional, lifecycle e limites de suporte.
- `/home/michel/mbm-documentation`: adicionar página própria, por exemplo `doc/box2d3.rst`, em `master` (inglês) e `pt-br` (português), atualizando navegação e versões. Preservar `box2d.rst` e `box2LiquidFun.rst` como documentação do plugin atual, sem mudar a descrição dele para solver C/3.x.
- Explicar que não há fluidos, compatibilidade automática nem interação entre mundos das duas versões; documentar eventos, substeps, unidades e joints realmente expostos.
- Atualizar `include/version/version.h` quando houver funcionalidade entregue, não nesta alteração de plano.

## Questões a resolver nos gates

1. Qual carga concreta de um jogo novo se beneficia, e quanto tempo de solver/frame ela economiza em cada P0?
2. Qual configuração de substeps atende à qualidade com custo comparável às iterações atuais?
3. Qual API mínima entrega esse benefício sem recriar toda a superfície histórica?
4. Como definir passo fixo, pause, sincronização visual e ownership sem atualização dupla?
5. Coexistência dinâmica e estática é segura sem patch de alto custo ao upstream?
6. Win32 mantém desempenho/correção adequados e o ganho sobrevive a Lua/render?
7. Multithreading agrega valor suficiente para manter scheduler e testes adicionais?

A decisão pendente é se vale construir e manter um plugin rígido novo. A permanência de Box2D 2.4.1 e dos fluidos atuais deixa de depender do resultado dessa avaliação.
