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

# Unificação de Box2D e LiquidFun: viabilidade e plano

Data: 2026-10-13. Estado: **baseline, port nativo e integração Lua/CMake implementados;
Ratworks validado automaticamente e testado pelo usuário; marcos 4 e 5 parciais.
Performance, plataformas restantes e remoção do legado pendentes**.
Marco atual: [migração e consolidação](box2d-liquidfun-migration.md).
Continuação: [destruição de corpos e validação prolongada](box2d-liquidfun-lifecycle.md).
Medições ampliadas: [custo nativo OFF/ON e escalabilidade](box2d-liquidfun-performance.md).
Preparação para Windows: [revisão e limpeza Linux](box2d-liquidfun-linux-cleanup.md).
Validação Windows: [MSVC Win32 ON/OFF, OpenGL ES, DirectX11 e DirectX9](box2d-liquidfun-windows.md).
Os contratos, Ratworks e cenário prolongado passaram no escopo registrado;
x64 real, MinGW, sanitizadores Windows e aceitação geral da migração seguem pendentes.
Validação macOS: [Apple Silicon/Metal e Xcode](box2d-liquidfun-macos.md).
Revisão integrada e próxima entrega: [aceitação de jogos e exemplos](box2d-liquidfun-desktop-review.md).
Matriz de plataformas: [regressão](box2d-liquidfun-platforms.md).
Cliente de referência: [Ratworks](box2d-liquidfun-ratworks.md).
Marco anterior: [integração Lua e perfis de build](box2d-liquidfun-integration.md).
Resultados e reprodução: [baseline de compatibilidade](box2d-liquidfun-baseline.md).
Base inspecionada: mini-mbm `e29aa3cd`; Ratworks `c52d991`, em
`/home/michel/Ratworks`. Ambos estavam sem alterações locais no início da análise.
Complemento: exemplos de `/home/michel/mbm-documentation`, revisão `b1b41b6`,
inspecionados para inclusão no escopo de migração e aceitação.

## 1. Recomendação

É tecnicamente plausível manter **uma única base de física rígida Box2D 2.4.1,
com o módulo de partículas do LiquidFun portado e opcional na compilação**.
Isso é um port com manutenção própria, não uma atualização de pacote pronta.
Recomenda-se começar por medição e uma prova de conceito isolada, mantendo os
plugins atuais disponíveis até passar pelos critérios de compatibilidade.

A necessidade de unificação é principalmente compatibilidade e manutenção.
Não há evidência, nesta análise, de que a mera presença do LiquidFun imponha um
solver pesado aos mundos sem fluidos. O código já evita executar esse solver
quando não existem sistemas de partículas. Um build sem partículas ainda é útil
para excluir código, estado e dependências dessa funcionalidade.

Ratworks deve ser o cliente de referência para fluidos. Os jogos já publicados
com Box2D 2.4.1, conforme informado pelo autor, devem continuar sendo a referência
para preservar a física rígida. Ratworks sozinho não cobre joints nem todos os
contratos de colisão utilizados pela engine.

## 2. Versões e opções externas

| Identificação | Verificação | Consequência |
| --- | --- | --- |
| `third-party/box2d-2.4.1` | `src/common/b2_settings.cpp` declara `{2,4,1}` | É a base local a preservar, inclusive suas adaptações |
| `third-party/box2d-liquid-fun-2.3.1` | `Box2D/Common/b2Settings.cpp` declara Box2D `{2,3,0}` e LiquidFun `1.1.0` | `2.3.1` é o nome local da pasta/variável CMake; não identifica a versão declarada pelo código |
| LiquidFun oficial | Release mais recente listada: 1.1.0, baseada em Box2D 2.3.0; repositório arquivado em 13/02/2026 | Não há release oficial 2.4.1 para simplesmente substituir a dependência |
| Box2D oficial | Já possui linha 3.x; 2.4.1 é a última release da linha 2 | Escolher 2.4.1 é uma decisão de compatibilidade, não de usar a linha mais recente |

Fontes locais: [versão Box2D](../third-party/box2d-2.4.1/src/common/b2_settings.cpp),
[versões LiquidFun](../third-party/box2d-liquid-fun-2.3.1/Box2D/Common/b2Settings.cpp)
e [CMake raiz](../CMakeLists.txt), variáveis `BOX2DLIB_VERSION` e
`BOX2D_LIQUID_FUN_LIB_VERSION`.
Fontes oficiais: [releases LiquidFun](https://github.com/google/liquidfun/releases),
[release Box2D 2.4.1](https://github.com/erincatto/box2d/releases/tag/v2.4.1)
e [releases Box2D](https://github.com/erincatto/box2d/releases).

LiquidFun estende a física **2D**. Neste plano, as referências a "Box3D" na ideia
inicial são entendidas como Box2D; nem Bullet3D nem simulação volumétrica entram
no escopo. `is3d=true` no Ratworks seleciona a apresentação visual do fluido, não
transforma a simulação de partículas em 3D.

| Alternativa | Avaliação |
| --- | --- |
| Continuar com os dois plugins e melhorar LiquidFun | Menor risco imediato; mantém duplicação e dois comportamentos físicos. É o fallback durante o port |
| Usar somente LiquidFun atual | Unifica à custa de regredir a base rígida dos jogos existentes; não recomendado |
| Portar partículas para a Box2D 2.4.1 local | Recomendado para a prova de conceito; preserva a base rígida e assume manutenção explícita do port |
| Adotar um fork comunitário | Só após auditar origem, licença, base exata, testes, plataformas e manutenção. A pesquisa não estabeleceu um candidato C++ 2.4.1 validado para esta engine |
| Migrar tudo para Box2D 3.x | Projeto separado: a API da linha 3 muda substancialmente e não resolve automaticamente o port de partículas |

A migração 3.x tem seu próprio [guia oficial](https://github.com/erincatto/box2d/blob/main/docs/migration.md).
Não combinar essa migração com a consolidação proposta.

## 3. Evidências sobre o custo atual

Esta seção registra a inspeção estática inicial; as medições e execuções posteriores
estão no [relatório de baseline](box2d-liquidfun-baseline.md). As linhas
abaixo distinguem comportamento observado de hipóteses que precisam de medição.

| Questão | Evidência local | Conclusão |
| --- | --- | --- |
| Só compilar LiquidFun executa simulação? | Cada plugin compila seu backend; a atualização depende de um mundo criado e atualizado | Disponibilidade do plugin não significa execução do solver |
| Mundo sem fluidos paga solver de partículas? | `b2World::Step` percorre `m_particleSystemList`; a lista começa nula | Não executa `b2ParticleSystem::Solve`; permanece o teste da lista e o estado adicional do mundo |
| Sistema existente, mas vazio, custa algo? | `b2ParticleSystem::Solve` retorna quando `m_count == 0`; o wrapper ainda percorre `lsFluid` | Custo residual por sistema e memória retida; diferente de nunca ter criado fluidos |
| Pausar um fluido elimina todo trabalho? | `Solve` trata lifetimes/zombies antes de testar `m_paused`; o wrapper ainda chama `update_fluid` | Não equivale a remover o sistema; não prometer custo zero nem congelamento completo de lifetime |
| Quantas iterações de partículas são usadas? | Wrapper chama `Step` com três argumentos; esse overload encaminha `particleIterations=1` | Não há seleção adaptativa automática nesse caminho; iterações de velocidade/posição são controles diferentes |
| Há trabalho fora do solver? | `update` chama `emitFluid`, `update_fluid` e, quando houve emissão, `update_uv_fluid`, antes do `Step` | Medir emissão, cópia visual e renderização separadamente da física |
| `resizeParticleData` aloca sempre? | `FLUID_GROUP::resizeParticleData` só aumenta buffers quando `new_size > size_particle_array` | Não aloca a cada chamada; emissão gradual crescente pode causar várias realocações e cópias |

Evidências: [mundo LiquidFun](../third-party/box2d-liquid-fun-2.3.1/Box2D/Dynamics/b2World.cpp)
(`Step`, `Init`), [overloads de Step](../third-party/box2d-liquid-fun-2.3.1/Box2D/Dynamics/b2World.h),
[solver de partículas](../third-party/box2d-liquid-fun-2.3.1/Box2D/Particle/b2ParticleSystem.cpp)
(`Solve`), [wrapper](../plugins/box2d-liquid-fun-lua/box-2d-liquid-fun-wrap.cpp)
(`update`, `createRenderizableFluid`, `update_fluid`, `update_uv_fluid`) e
[buffers visuais](../src/core_mbm/particle-control.cpp) (`resizeParticleData`).

O custo sem partículas precisa ser comparado entre builds da **mesma base 2.4.1**.
Comparar apenas os plugins atuais mistura a extensão com diferenças entre os
solvers rígidos 2.3 e 2.4. Também não se deve inferir uma melhoria de FPS apenas
porque a versão mudou.

### 3.1 O que Ratworks já revela

Em `logic/physics.lua`, o jogo cria `box2dLiquidFun:new()` e corpos estáticos a
partir dos marcadores. Em `logic/rat.lua`, cada rato possui um corpo dinâmico,
rotação bloqueada, filtro de colisão e velocidade controlada pelo jogo.
`beginEating` desativa o corpo e dispara `releaseFluid`.

A emissão usa círculo de raio visual 5, raio físico de partícula `0.03`,
`lifetime=5`, textura, posicionamento em 3D e máscara que exclui a categoria dos
ratos. `logic/level-config.lua` configura `riseTime=1.5`. O wrapper usa escala
inicial 10; posições e tamanhos visuais não devem ser confundidos com o raio físico.

Cada `createFluid` cria **outro `b2ParticleSystem`**. `Rat:destroy()` destrói o
corpo/modelo, mas deixa o fluido com o mundo. Expirar partículas não remove
automaticamente `INFO_FLUID`, seu sistema e a capacidade dos buffers visuais.
No wrapper, a destruição explícita passa por `destroyFluid`/`safeDestroyFluid`.
Logo, emissões repetidas na mesma cena podem acumular sistemas vazios: hipótese
concreta de custo crescente a medir, não prova do gargalo observado pelo autor.

Outro ponto de gameplay: partículas de sistemas diferentes não interagem
diretamente entre si. Por inferência, emissões de ratos distintos não formam um
único líquido interagente no modelo atual, embora possam afetar os mesmos corpos.
Esse comportamento é descrito no [guia oficial de partículas](https://google.github.io/liquidfun/Programmers-Guide/html/md__chapter11__particles.html).
Compartilhar um sistema pode resolver essa necessidade, mas muda comportamento,
propriedades e ownership; não deve ser uma otimização transparente no port inicial.

Investigar, em ordem: número de sistemas vivos/vazios; partículas e contatos;
custo do solver; emissão gradual; cópias/realocações; envio de buffers e desenho.
Não pausar física apenas por estar fora da câmera: o fluido é parte do puzzle.

## 4. Compatibilidade: mais do que copiar Particle/

O solver acessa detalhes internos do mundo, allocators, filtros/listeners e corpos.
LiquidFun também amplia shapes com `ComputeDistance` e callbacks com partículas.
Copiar somente a pasta `Particle/` não completa a integração.

| Área | Diferença observada / trabalho necessário |
| --- | --- |
| Mundo e passo | Portar criação/destruição/lista de sistemas, chamadas do solver e campos de timestep; preservar ordem de execução e caminho rígido 2.4.1 |
| Shapes | Portar cálculo de distância e colisão partícula-fixture sem restaurar algoritmos rígidos antigos |
| Chain/edge | 2.4.1 tem `b2ChainShape::Clear()` e `CreateChain(vertices,count,prev,next)`; LiquidFun usa dois argumentos e `SetPrevVertex`/`SetNextVertex`. Validar ghost vertices, orientação e contato nas extremidades |
| Callbacks/queries | Integrar filtros, destruição, AABB, raycast e contatos de partículas; manter assinatura, retorno e semântica dos callbacks Lua existentes |
| Userdata | A cópia local 2.4.1 já tem `b2Body::GetUserData()`/`SetUserData(void*)`; não tratá-la como upstream intocado nem sobrescrever essas adaptações |
| Ativação de corpos | Wrapper rígido usa `SetEnabled`; LiquidFun usa `SetActive`; manter o contrato Lua `setActive`, inclusive aplicação enfileirada |
| Joints de mola | Rígido expõe `get/setStiffness` e `get/setDamping`; LiquidFun expõe `get/setFrequencyHz` e `get/setDampingRatio`. Não são simples aliases numéricos |
| Criação de joints | O wrapper 2.4.1 ainda aceita opções `frequencyHz`/`dampingRatio` e converte com `b2LinearStiffness`/`b2AngularStiffness`; preservar esse comportamento |
| Rope | LiquidFun usa `b2RopeJointDef`; o plugin 2.4.1 implementa `rope` com `b2DistanceJointDef.maxLength`. Testar folga, limites, anchors, get/setLength e tipo reportado |
| Ciclo de vida | Preservar remoções diferidas, Lua GC, grupos destruídos, reemissão após expiração, pausa, troca de cena e validade dos handles |
| Memória e plataformas | Portar buffers, allocators, tipos, alinhamento e caminhos SIMD/NEON; testar tamanhos de objetos no allocator e referências internas com sanitizers |

Sobre "ClearShape": não foi encontrado esse identificador nos dois plugins.
A diferença concreta encontrada é em **ChainShape**, inclusive o método `Clear`.
Se o nome se referia a outra operação, localizar seu call site antes de especificar
uma adaptação. Não presumir um método Lua inexistente.

Referências: [joints rígidos](../plugins/box2d/physics-box2d-joint-lua.cpp),
[criação rígida](../plugins/box2d/physics-box-2d-lua.cpp),
[joints LiquidFun](../plugins/box2d-liquid-fun-lua/physics-box2d-liquid-fun-joint-lua.cpp),
[criação LiquidFun](../plugins/box2d-liquid-fun-lua/physics-box-2d-liquid-fun-lua.cpp),
[chain 2.4.1](../third-party/box2d-2.4.1/include/box2d/b2_chain_shape.h),
[chain LiquidFun](../third-party/box2d-liquid-fun-2.3.1/Box2D/Collision/Shapes/b2ChainShape.h),
[userdata local](../third-party/box2d-2.4.1/include/box2d/b2_body.h),
[callbacks LiquidFun](../third-party/box2d-liquid-fun-2.3.1/Box2D/Dynamics/b2WorldCallbacks.h)
e [API Lua, seções 10–11](lua-api.md).

O port também deve preservar as melhorias recentes: máscara de fixtures
(`0b074599`), emissão gradual (`6e36b189`) e contorno convexo (`e29aa3cd`).
Copiar apenas o código original do LiquidFun perderia contratos que Ratworks já usa.

## 5. Arquitetura proposta

### 5.1 Uma base, dois perfis de compilação

Os arquivos exclusivos do LiquidFun continuarão no repositório: implementação e
headers de partículas, grupos, buffers e auxiliares necessários ao port. Uma
organização possível é `src/particle/` na árvore física consolidada, com headers
correspondentes e bindings Lua de fluido separados dos bindings rígidos. O layout
exato será definido na prova de conceito; não implica manter uma segunda cópia
completa de Box2D nem conservar a pasta antiga `box2d-liquid-fun-2.3.1` para sempre.

Os arquivos comuns, como mundo, timestep, shapes e callbacks, terão pontos de
integração condicional. Desabilitar partículas no build exclui suas fontes e os
trechos condicionais, mas não apaga os arquivos do checkout. Separar fisicamente
os arquivos facilita manutenção; a exclusão real do binário depende do CMake e
das definições consistentes descritas abaixo.

Desde 7.365, o CMake raiz oferece os seguintes perfis (com `USE_LUA=1`):

| Configuração | Resultado |
| --- | --- |
| `USE_BOX2D=1`, `USE_BOX2D_PARTICLES=OFF` | Plugin rígido 2.4.1; sem fontes/estado do solver e sem métodos Lua de fluido |
| `USE_BOX2D=1`, `USE_BOX2D_PARTICLES=ON` | Mesmo backend, com extensão e bindings de partículas; sistemas criados sob demanda |
| `USE_BOX2D=0`, `USE_BOX2D_PARTICLES=ON` | Erro de configuração (Lua também é obrigatória) |
| `USE_ALL=1` | Partículas ON por padrão; OFF explícito ou já persistido no cache é respeitado |
| `USE_BOX2D=0`, `USE_BOX2D_LIQUID_FUN=1`, partículas OFF | Backend legado de comparação; não usar USE_ALL |

`MBM_BOX2D_PARTICLES=0/1` é propagado pelo target Box2D a suas unidades e consumidores.
OFF exclui fontes de partículas e bindings de fluido. Os arquivos continuam no checkout.
O backend antigo é desabilitado quando Box2D está habilitado; a fachada
`box2dLiquidFun` encaminha para a biblioteca única e acusa a opção ausente em OFF.
Usar artefatos separados para comparar perfis: os diretórios de saída padrão da engine
são compartilhados entre diretórios de build do mesmo tipo/plataforma.

No perfil completo, manter criação sob demanda por mundo. Não adicionar teste por
fixture/corpo no caminho rígido apenas para decidir se há fluidos. Uma lista vazia
ou um teste por mundo é suficiente para evitar o solver de partículas; medir o
custo residual. O perfil sem partículas deve excluir esse caminho na compilação.

### 5.2 API e carregamento

`require "box2d"` deve continuar funcionando com o contrato rígido atual.
No perfil completo, ele oferece `createFluid` e a consulta
`supportsParticles()`.
No perfil sem partículas, a consulta deve indicar ausência e a compatibilidade
`box2dLiquidFun` deve falhar com diagnóstico claro, sem fallback silencioso.

Manter temporariamente `require "box2dLiquidFun"`, seus nomes globais e as
variantes `luaopen_lib*` por uma fachada sobre **o mesmo backend compilado uma vez**.
A fachada precisa adaptar joints antigos de frequência/razão de amortecimento
com conversão física, tratar gear separadamente e preservar os retornos Lua.
Aliases diretos de frequência para stiffness alterariam as unidades e o jogo.
Se não for possível reproduzir um contrato, documentar migração explícita.

Um símbolo `luaopen_box2dLiquidFun` dentro de `box2d.so` não basta para o loader
encontrar outro nome de módulo. Planejar um shim Lua, `package.preload` ou módulo
encaminhador, incluindo os carregadores estáticos. A ordem dos dois `require`s
não pode duplicar factories, subscriptions ou instâncias do backend.

Antes deste marco, os dois plugins compilavam cópias completas da física. Agora o
CMake exclui o backend antigo sempre que `USE_BOX2D` está habilitado e constrói a
fachada sobre o target único. A árvore antiga permanece apenas para builds isolados
de comparação; a validação dos links estáticos continua no marco de plataformas.
Cobrir também projetos Visual Studio, exports Windows, Android e entry points
estáticos iOS/MAS.

Referências: [CMake rígido](../plugins/box2d/CMakeLists.txt),
[CMake LiquidFun](../plugins/box2d-liquid-fun-lua/CMakeLists.txt),
[CMake raiz](../CMakeLists.txt), [link da engine](../src/CMakeLists.txt)
e [entry points LiquidFun](../plugins/box2d-liquid-fun-lua/box2d-liquid-fun-lua.cpp).

### 5.3 Melhorias de fluido separadas do port

Depois de medir, considerar exposição de `particleIterations`, estatísticas de
sistemas/partículas e limites de orçamento. Não confundir `particleIterations`
com `staticPressureIterations`, que já tem binding e controla outra etapa.
Manter o padrão inicial de uma iteração para não alterar custo e comportamento
silenciosamente. Timestep fixo/substeps são uma decisão separada; o wrapper atual
passa `delta * multiplyStep`.

Para sistemas vazios, preferir reutilização ou liberação explícita. Autodestruir
um fluido só porque sua contagem chegou a zero quebraria `fluid:add()` posterior,
handles Lua e emissão pendente. Uma futura política automática precisa ser opt-in,
considerar filas de emissão e definir invalidação de handles.

Para compartilhar líquido entre ratos, desenhar ownership por reservatório/material,
emissões por grupo e compatibilidade de raio/densidade/pausa. Isso muda a API e a
interação física; deve ter sua própria cena de aceitação. Também avaliar capacidade
geométrica dos buffers para reduzir realocações durante `riseTime`, conforme perfil.

## 6. Plano de execução e pontos de decisão

| Etapa | Entrega verificável | Condição para avançar |
| --- | --- | --- |
| 0. Baseline | Inventário das diferenças locais contra tags oficiais; contratos Lua, exemplos de mbm-documentation e métricas dos plugins atuais | Snapshots reproduzíveis, exemplos classificados, uso dos jogos existentes inventariado e gargalo dividido por etapa |
| 1. Prova de conceito nativa | Uma base 2.4.1 com partículas opcionais; recipiente, fluido e corpo dinâmico, sem mudar o plugin de produção | OFF preserva rígidos; ON conserva contato/impulso bidirecional, queries, destruição e lifetimes; sanitizers sem falhas |
| 2. Integração Lua | Métodos de fluido no backend único, fachada antiga, contratos de joints e melhorias recentes | Testes de API/ownership passam nas duas formas de carregar; nenhuma segunda cópia física no binário |
| 3. Cliente Ratworks | Cenário reduzido reproduzível na engine e execução do jogo com o candidato | Emissão, filtros, pausa, lifetime, apresentação e longa duração aprovados; custo medido |
| 4. Plataformas e regressão | Builds OFF/ON e testes nos alvos suportados; jogos rígidos e exemplos adaptados de mbm-documentation testados | Nenhuma regressão bloqueante de física, API, renderização ou performance; efeitos dos exemplos preservados |
| 5. Consolidação | Uma árvore física e um wrapper principal; remoção do backend antigo; guia de migração e documentação externa atualizada | Comparação final aprovada, fallback registrado e artefatos antigos não necessários em produção |

O esforço é substancial, concentrado nas etapas 1–2. Não estimar prazo fechado a
partir apenas da leitura: a etapa 1 deve produzir o orçamento de manutenção e a
decisão de continuar. Interromper a consolidação se o port exigir trocar grandes
partes do solver rígido 2.4.1 por código 2.3, se houver regressões sem explicação
ou se faltar capacidade de validar os jogos existentes. Nesse caso, manter os
plugins e entregar as melhorias de LiquidFun isoladamente.

Não remover a árvore antiga antes da etapa 5. Durante comparação, usar executáveis
separados para os backends: evita colisões de símbolos e contaminação de medidas.
Não chamar isso de implementação final unificada enquanto ambos forem necessários.

### 6.1 Pendência de memória: LSan no processo gráfico

Atualização da etapa 5: controles de índices, duplicação ODR, runtime da fachada
e erros Lua corrigidos; contratos ON/OFF passaram com sanitizadores completos
no ambiente Mesa. A alocação externa de 208 bytes foi simbolizada e permanece
acompanhada no [relatório de consolidação](box2d-liquidfun-migration.md).
A continuação em 7.366.2 aprovou 75 ciclos do cenário reduzido em dez minutos
e o Ratworks real por 60 s com sanitizadores completos, após corrigir também
uma leitura de corpo já liberado. Abaixo fica o diagnóstico histórico e o critério
de aceitação; os demais drivers/plataformas ainda não foram aprovados.

Diagnóstico inicial:
Na integração Lua, os perfis ON e OFF reportaram **72.920 bytes em quatro
alocações diretas**: 72.704 e 208 bytes em módulos ainda sem símbolos e duas
alocações de 4 bytes em `BUFFER_GL::initializeIndexBufferControl`
(`src/core_mbm/shader.cpp`). A igualdade entre perfis não elimina o vazamento,
nem permite atribuir os módulos desconhecidos ao driver ou à extensão.

- Reproduzir com cena gráfica mínima, rígidos OFF/ON e fluidos ON, com LSan ativo.
- Simbolizar as duas pilhas desconhecidas e identificar ownership e teardown.
- Corrigir a liberação dos controles de índices após confirmar o ciclo de vida.
- Reexecutar a matriz e o cenário prolongado; registrar bytes, pilhas e versões.
- Critério: nenhuma alocação própria sem liberação; eventual problema externo deve
  ter origem comprovada e acompanhamento explícito. Não usar supressão global ou
  `detect_leaks=0` como evidência de aprovação de memória.

Evidência inicial: [relatório da integração](box2d-liquidfun-integration.md).
O cenário prolongado da etapa 3 também deve registrar RSS, mas RSS estável não
substitui LSan e não encerra esta pendência.

A instrumentação da engine inteira na etapa 3 também encontrou duplicação de
`tinyfd_needs` (tinyfiledialogs) entre executável e `libcore_mbm.so`, impedindo
inicialização com a checagem ODR padrão do ASan. Acrescentar à tarefa a revisão do
link de tinyfiledialogs em Linux e a repetição com checagem ODR habilitada. A rodada
diagnóstica com somente essa checagem desabilitada manteve LSan ativo e confirmou
os 72.920 bytes na regressão Lua. Não considerar essa rodada aprovação integral
dos sanitizers.

## 7. Matriz de validação e performance

### 7.1 Medições comparáveis

Comparar quatro variantes: **A** Box2D atual; **B** LiquidFun atual; **C** candidato
2.4.1 com partículas OFF; **D** mesmo candidato ON. Comparações A/C isolam regressão
do port rígido; C/D sem sistemas isolam overhead opcional; B/D com partículas
comparam a extensão. Não interpretar A/B como medida pura de overhead de fluidos.

| Cenário | O que verifica |
| --- | --- |
| Mundo vazio; depois corpos rígidos sem criar fluidos | Custo mínimo, sleeping, sensores, filtros e ausência de alocações de partículas |
| Pilhas, projéteis rápidos e todos os joints usados pelos jogos | Estabilidade, CCD, limites, motores, callbacks e regressão rígida |
| Um sistema vazio; depois 100 e 1.000 sistemas vazios | Custo por sistema e memória retida após emissões |
| Mesmo total de partículas em 1 e em muitos sistemas | Separar custo por sistema de custo por partícula; registrar que a interação física difere |
| 1.000, 5.000 e 10.000 partículas em recipiente e contra corpos móveis | Escalabilidade, contatos, pressão e interação bidirecional; ajustar escala ao dispositivo |
| Emissão instantânea versus gradual | Picos de CPU, alocações, preservação da quantidade emitida |
| Fluido pausado; mundo pausado; delta zero | Emissão, lifetime, atualização visual e retomada, sem confundir os três estados |
| Emitir, expirar, reemitir e destruir por 10 minutos | Acúmulo de sistemas/buffers, handles inválidos e custo crescente |
| Ratworks completo e cenário reduzido sem assets externos | Aceitação real e reprodução independente do jogo |

Usar Release, mesmo compilador/flags, timestep e entradas reproduzíveis, VSync
desligado para medidas e aparelho identificado. Separar aquecimento de amostragem;
sugestão inicial: 10 segundos de aquecimento, 60 de coleta e cinco repetições.
Registrar mediana/p95/p99 em milissegundos, não apenas FPS: `Step` total, solver de
partículas, emissão, sincronização visual, CPU de desenho e GPU quando disponível.
Registrar memória/capacidade, alocações, sistemas ativos/vazios, partículas e contatos.

Critério inicial proposto, não resultado: nenhuma regressão repetível superior a
3% na mediana ou 5% no p95 do passo rígido para A/C e C/D sem partículas. Em tempos
muito pequenos, calibrar também tolerância absoluta conforme resolução e ruído do
ambiente. Fixar o critério antes de avaliar o candidato. Para fluidos, exigir ausência
de regressão estatisticamente distinguível e definir orçamento em ms no aparelho
alvo do Ratworks após a baseline; não inventar meta universal de partículas/FPS.

Atualização: a [coleta nativa ampliada](box2d-liquidfun-performance.md) executou
cinco repetições dos perfis OFF/ON/legado e uma confirmação OFF/ON, com afinidade
de CPU, sem observar regressão rígida repetível nos cenários medidos. Inclui
1/100/1.000 sistemas vazios e até 10.000 partículas. Permanecem pendentes A/C
contra upstream intocado, etapas do wrapper/renderização, memória/alocações,
contatos com corpos móveis na carga ampliada e orçamento no aparelho alvo.

### 7.2 Funcionalidade e plataformas

Continuação macOS em 2026-10-13: [relatório Xcode/Metal](box2d-liquidfun-macos.md).
O build unificado permite disponibilizar rígidos e fluidos no mesmo link estático
MAS, com os dois nomes Lua; a validação distingue link, runtime desktop e as
pendências de execução MAS assinada/iOS.

Reutilizar [fluid_filter_smoke.lua](../src/test-lib/fluid_filter_smoke.lua),
[fluid_rise_smoke.lua](../src/test-lib/fluid_rise_smoke.lua),
[fluid_convex_smoke.lua](../src/test-lib/fluid_convex_smoke.lua) e
[fluid_depth_smoke.lua](../src/test-lib/fluid_depth_smoke.lua). São cobertura
existente a executar, não testes considerados aprovados por esta análise.

Adicionar testes nativos para shapes/chain/edge, impulso de fluido em corpo móvel,
filtros, queries e destruição. Testar Lua para joints, unidades, retornos, ordem de
carregamento, GC, uso após destruição e reemissão em grupo expirado. Comparações
físicas precisam de tolerâncias e invariantes de gameplay; não exigir identidade
bit a bit entre o antigo solver 2.3 e o candidato 2.4.1.

Executar OFF/ON em Linux, Windows (MSVC e MinGW), macOS, Android e iOS; incluir
link estático iOS/MAS e visual de partículas nos backends de render relevantes.
ASan/UBSan em host para o port nativo e wrappers. Validar em Android físico o
custo térmico e CPU, além dos resultados do desktop. Antes de executar engine,
Lua ou testLib, seguir a skill [engine-testing](../.agents/skills/engine-testing/SKILL.md).

### 7.3 Exemplos de mbm-documentation como contrato de aceitação

Requisito do autor: os exemplos existentes de Box2D e fluidos devem ser adaptados
quando necessário e produzir o **mesmo efeito físico e visual pretendido** quando
testados. Esse conjunto complementa Ratworks e os jogos publicados e é obrigatório
para concluir a migração.

Inventário inicial no repositório externo, relativo a `/home/michel/mbm-documentation`:

| Fonte | Conteúdo localizado | Papel na migração |
| --- | --- | --- |
| `doc/box2d.rst` | 135 blocos `code-block:: lua`; exemplos de corpos, forças, impulsos, filtros, queries, contatos e joints | Contrato da API rígida e referências de comportamento |
| `doc/box2LiquidFun.rst` | 45 blocos `code-block:: lua`; criação, flags/grupos, adição, pausa, propriedades, lifetime, destruição, forças, queries e raycast | Contrato da extensão, incluindo efeitos não usados hoje por Ratworks |
| `doc/_static/` | GIFs de joints e colisões; `fluid-showcase.gif`, `fluid-1.gif` a `fluid-7.gif`, exemplos de adição e `fluid_particle.png` | Referências visuais e assets para reproduzir os exemplos |
| `doc/locales/pt_BR/LC_MESSAGES/box2d.po` e `box2LiquidFun.po` | Catálogos de tradução, com `.mo` correspondentes | Sincronizar traduções afetadas pelo procedimento do projeto |
| `doc/introduction.rst`, `plugin.rst`, `editors.rst` e `index.rst` | Referências a Box2D/LiquidFun | Revisar nomes, instalação, disponibilidade e navegação após consolidar |

A contagem é de blocos de código, **não de cenas executáveis independentes**.
Há tabelas e chamadas parciais que dependem de contexto. Na etapa 0, catalogar cada
bloco por arquivo/seção/identificador estável, dependências, tipo (cena ou fragmento),
backend original, interações de entrada, resultado esperado e teste associado.
Extrair cenas completas ou montar um harness mínimo para os fragmentos, mantendo
rastreabilidade até o exemplo publicado. Não executar fragmentos isolados como se
fossem programas completos.

Procedimento de comparação:

1. Executar cada caso reproduzível no backend atual correspondente, com os assets
   corretos, timestep, duração e entradas registrados. Guardar capturas/vídeos e
   métricas funcionais; usar os GIFs publicados como referência adicional.
2. Registrar exemplos já quebrados ou divergentes da documentação como problemas
   prévios. Resolver a baseline de intenção antes de atribuir a diferença ao port;
   não aceitar um erro antigo silenciosamente como comportamento desejado.
3. Adaptar imports, chamadas e conversões de unidades necessárias ao candidato.
   Preservar geometria, gravidade, interação, intensidade das forças e efeito dos
   joints/fluidos. Ajustes de parâmetros precisam de justificativa; não enfraquecer
   o cenário ou trocar seu efeito para fazer o teste passar.
4. Comparar versões com mesma câmera, escala, textura e sequência de entradas.
   Definir por caso tolerâncias de trajetória, limites, frequência/amortecimento,
   contenção, emissão e lifetime. Exigir equivalência perceptível e funcional,
   sem comparação pixel a pixel de uma simulação que pode divergir numericamente.
5. Rodar casos rígidos com partículas OFF e ON; fluidos com ON. Cobrir a fachada
   antiga enquanto suportada e a API recomendada nos exemplos atualizados.
6. Atualizar os `.rst`, traduções afetadas e instruções de build; validar o build
   da documentação e seus links/assets. Só atualizar GIFs após verificar que o
   resultado continua representando o mesmo efeito, sem encobrir regressões.

A atualização de `mbm-documentation` inclui explicitamente os branches `master`
e `pt-br`. As notas de build/fachada, exemplos e referências afetadas devem ser
revisados em ambos, respeitando a organização e o procedimento de tradução de cada
branch; atualizar os catálogos `pt_BR` em `master` não substitui essa revisão.
Em 2026-10-13, as notas de `box2d.rst` e `box2LiquidFun.rst` foram também
aplicadas ao working tree de `pt-br`, em `/home/michel/mbm-documentation-pt-br`,
com traduções nos dois catálogos `.po` e `.mo` gerados pelo Sphinx. O build HTML
com `language=pt_BR` passou e as notas em português foram verificadas na saída;
o conteúdo original e os exemplos foram preservados. As alterações de `master`
permanecem intactas. Ambas as atualizações estão sem commit/publicação. Registrar
as revisões finais de ambos antes de considerar encerrada a documentação externa.

Critério de conclusão: todos os casos catalogados devem ter resultado registrado,
com a adaptação e evidência de equivalência quando aplicável. Caso não reproduzido,
asset ausente ou diferença sem explicação permanece pendência, não aprovação.
As execuções mínimas da suíte devem ser automatizáveis; cenas interativas precisam
de entradas reproduzidas ou roteiro explícito de validação visual.

## 8. Documentação, manutenção e limites desta análise

Na implementação, manter manifesto da origem do código, revisões upstream,
adaptações locais e testes do port. Preservar avisos de licença de cada fonte.
Atualizar `docs/lua-api.md`, `docs/lua-plugin-symbols.md`, contextos de jogo,
CMake/Visual Studio e exemplos; sincronizar AGENTS/Copilot quando suas instruções
mudarem. Bump de `include/version/version.h` somente ao entregar funcionalidade,
não nesta proposta. Se otimizações de buffers alterarem limites públicos/PIMPL do
core, atualizar também `docs/core-pimpl-status.md`.

Ratworks e mbm-documentation foram consultados somente para leitura nesta análise.
A etapa 0 extraiu e executou exemplos externos em um harness local, sem alterar
os arquivos publicados. A adaptação à nova API e a equivalência visual permanecem
pendentes. O plano não altera o jogo nem
assume que sua cena atual cobre todas as necessidades futuras de fluidos.
A análise verificou caminhos e contratos concretos no código e consultou fontes
oficiais. A baseline posterior acrescenta builds, testes de runtime e medições
nativas. O port e sua integração Lua/CMake foram testados inicialmente no Linux;
as continuações [Windows](box2d-liquidfun-windows.md) e
[macOS/Xcode](box2d-liquidfun-macos.md) acrescentam evidências e limites específicos.
Equivalência visual completa, regressão de todos os jogos e os alvos restantes
ainda estão pendentes. Portanto,
**viabilidade estrutural é favorável, ganho de performance e equivalência física
permanecem por demonstrar**.
