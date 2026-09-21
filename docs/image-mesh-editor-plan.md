# Plano: Image Mesh Editor

Data: 2026-09-19

Status: implementação iniciada; etapas 1 e 2 concluídas em Linux/OpenGL ES em 2026-09-19. As seções de escopo descrevem o destino planejado. Consulte o registro de execução ao final para distinguir o que já está disponível.

## Objetivo

Criar um editor totalmente offline para gerar módulos 3D por extrusão de regiões de imagens, com relevo determinístico, sem depender de IA ou serviços pagos. O uso principal é montar cenários com peças posicionadas lado a lado, priorizando a aparência frontal, mas permitindo inspecionar e usar laterais e fundo.

Referência inicial: `/home/michel/Downloads/mesh-tile-experiment/f1-c002ae9c-img01.png`, imagem com 12 painéis organizados em 4 colunas e 3 linhas. Esse caminho é uma referência local de desenvolvimento, não uma dependência do editor ou dos testes distribuídos.

A unidade de trabalho será uma região da imagem com contorno, nome e parâmetros próprios. O projeto comportará peças retangulares, circulares, irregulares e, em etapa posterior, peças com furos.

## Limites da abordagem

- Luminosidade não equivale necessariamente a profundidade: sombras, reflexos, manchas e riscos também influenciam a imagem. O resultado será uma interpretação ajustável de relevo, não uma reconstrução do objeto original.
- O relevo frontal terá uma altura por posição da superfície. Cavidades com saliências sobrepostas e outras formas que exigem múltiplas profundidades na mesma posição ficam fora desse modelo.
- Uma abertura desenhada na textura não vira automaticamente um furo. Furos reais precisam de contornos internos e paredes próprias.
- A imagem original será preservada. Correções de altura serão dados separados do projeto.

## Fluxo de trabalho

1. Abrir uma imagem.
2. Criar regiões manualmente ou por divisão em grade.
3. Ajustar os contornos de cada região.
4. Definir dimensões, espessura, relevo e texturas.
5. Conferir o resultado na prévia 3D.
6. Salvar o projeto editável.
7. Exportar uma peça ou todas as peças como malhas da engine.

Cada região poderá herdar os parâmetros gerais do projeto e sobrescrever valores específicos. Seleção múltipla permitirá aplicar ajustes a várias peças. Informar apenas a quantidade de peças não determina seus recortes; o modo em grade terá linhas, colunas, margens e espaçamento.

## Seleção e contornos

O editor básico incluirá:

| Forma | Controles |
|---|---|
| Retângulo | Mover e redimensionar |
| Círculo e elipse | Centro e raios |
| Polígono | Adicionar, mover e remover pontos; aceitar contornos côncavos |
| Grade | Linhas, colunas, margens e espaçamento; ajustes individuais após criação |

Uma etapa posterior adicionará contornos internos para furos reais, desenho livre convertido em polígono e seleção automática por transparência ou cor de fundo.

A seleção automática produzirá contornos editáveis e deverá distinguir o fundo externo dos detalhes escuros internos. Contornos cruzados, degenerados, furos fora da peça e interseções inválidas receberão diagnósticos claros. Nenhuma geometria inválida deverá ser exportada silenciosamente.

## Espessura e relevo

Forma, espessura e relevo serão independentes:

- Forma: silhueta da peça.
- Espessura: volume básico da extrusão.
- Relevo: deslocamento da superfície frontal dentro do contorno.

Fontes de altura: luminosidade, canal de cor selecionado ou imagem separada. Para um mapa separado, o editor deverá definir explicitamente o alinhamento com a imagem e a região selecionada.

Controles planejados:

- Amplitude e deslocamento da altura.
- Inversão entre claro e escuro.
- Pontos de preto e branco, contraste e curva de resposta.
- Suavização e limites mínimo e máximo.
- Altura fixa no contorno e largura da transição até o interior.

A conversão básica usa uma intensidade normalizada e ajustada para calcular o deslocamento. Valores uniformes e intervalos de intensidade nulos precisam de comportamento definido, sem divisão por zero. O gerador deverá impedir que o relevo atravesse o fundo e produza espessura inválida, inclusive quando houver relevo nas duas faces.

Uma etapa posterior incluirá pintura de altura para elevar, rebaixar, suavizar e achatar áreas. As alterações serão salvas separadamente da textura original e participarão do histórico de desfazer/refazer.

## Laterais, fundo e texturas

| Superfície | Opções planejadas |
|---|---|
| Frente | Textura do recorte original |
| Laterais | Faixa da borda esticada, textura repetida ou cor uniforme |
| Fundo | Plano ou com relevo copiado; textura normal, espelhada ou própria |

O espelhamento da textura e a cópia do relevo serão opções independentes. Para contornos irregulares, a faixa lateral acompanhará o perímetro. Furos terão paredes internas.

A exportação preservará as coordenadas de textura e incluirá os arquivos necessários. Recortes exportados terão margem de proteção para reduzir vazamento de pixels vizinhos durante a filtragem. Normais e separações entre faces deverão preservar a iluminação pretendida nas quinas e na superfície de relevo.

## Geometria, orçamento e encaixe

A triangulação respeitará o contorno e terá subdivisões internas para representar o relevo. Triangular somente os vértices da borda não é suficiente. A escolha do algoritmo deverá considerar contornos côncavos, futura inclusão de furos e preservação do contorno durante refinamento e simplificação.

O editor mostrará resolução, contagem final de vértices e triângulos e limites de exportação. Os limites considerarão frente, fundo, laterais e duplicações necessárias para textura e iluminação. Um orçamento insuficiente para preservar o contorno será reportado, sem degradar silenciosamente a forma.

A simplificação deverá preservar silhueta, furos, fronteiras de textura e bordas de encaixe. A compatibilidade com os limites de índices e buffers da engine deverá ser verificada antes de definir os máximos expostos pela interface.

Recursos para montagem de cenários:

- Dimensões em unidades da engine.
- Origem configurável, incluindo centro e base.
- Orientação padronizada.
- Ajuste a uma grade.
- Presets de dimensões e bordas.
- Prévia com cópias vizinhas para identificar frestas.

A altura fixa na borda ajuda no encaixe geométrico. Ela não garante continuidade visual das texturas, que deverá ser conferida na prévia.

## Interface e projeto editável

A interface terá área da imagem com contornos, prévia 3D, lista de peças e propriedades da seleção. A prévia permitirá orbitar, aproximar, alterar a luz e alternar entre textura, material neutro, mapa de altura e malha de arames. O material neutro ajudará a distinguir volume real de sombras pintadas.

O projeto salvará versão de formato, referências às imagens, regiões, parâmetros, correções de altura e configurações de exportação. Usará caminhos relativos quando possível e permitirá localizar novamente imagens movidas. O estado salvo deverá permitir reproduzir as malhas, sem depender de identificadores transitórios de textura ou objetos de renderização.

Recursos de edição: desfazer/refazer, duplicação de regiões, seleção múltipla e presets. A persistência seguirá os padrões dos editores existentes, com validação de versão e de dados ao reabrir.

## Desempenho

- O editor parado não deverá recalcular malhas, reler imagens, serializar projetos ou reenviar buffers sem alterações.
- Alterações marcarão apenas os dados dependentes como pendentes de atualização.
- Ajustes contínuos usarão prévia reduzida ou atualização após breve pausa; a exportação usará a qualidade final.
- Processamento pesado deverá manter a interface responsiva, com progresso e cancelamento quando necessário.
- Resultados de operações antigas não poderão substituir resultados de parâmetros mais recentes.
- Criação e atualização de recursos gráficos respeitarão as restrições de thread da engine.

## Arquitetura e integração

Separação proposta:

1. Gerador em C++ independente da interface: processamento de imagem, altura, triangulação, fechamento do volume e diagnósticos.
2. API Lua própria: geração, parâmetros e resultados utilizáveis por scripts e pelo editor.
3. Editor Lua/ImGui: seleção, edição, prévia e persistência.
4. Exportação: aproveitar o caminho existente de malhas v11, preservando texturas e normais.

A inspeção inicial identificou pontos a avaliar durante a prova técnica:

- `include/render/shape-mesh.h`: geometria indexada para prototipagem/prévia.
- `src/lua-wrap/render-table/mesh-debug-lua.cpp`: operações de vértices, índices, simplificação e salvamento v11.
- `editor/articulated_sprite_geometry.lua`: referência de triangulação; adequação para esta geração 3D ainda não validada.
- `editor/editor_utils.lua` e `editor/lang/language.lua`: infraestrutura e localização dos editores.

O uso de `meshDebug` para construção, inspeção ou exportação poderá ser interno; a geração terá uma API própria. Assinaturas e localização definitiva dos novos arquivos serão definidas após a prova técnica, evitando ampliar desnecessariamente os cabeçalhos públicos.

A implementação deverá preservar PIMPL e encapsulamento, manter módulos Lua coesos, integrar o editor aos launchers desktop pertinentes e fornecer textos em português e inglês com pontuação compatível com o atlas ImGui. Ao entregar a funcionalidade, atualizar a versão da engine e a documentação correspondente.

## Etapas de entrega

| Etapa | Entrega | Critério de conclusão |
|---|---|---|
| 1 — Prova técnica | Painel do exemplo com relevo, laterais, fundo e exportação | Malha exportada carrega na engine com textura, orientação e iluminação corretas; caminho de geração e exportação validado |
| 2 — Editor básico | Retângulos, elipses, polígonos côncavos, grade, parâmetros básicos, projeto editável, desfazer/refazer e exportação em lote | Gerar os 12 painéis, reabrir o projeto e montar uma parede; validar também uma peça circular e uma côncava |
| 3 — Controle artístico | Pintura de altura, presets, opções completas de textura/fundo e prévia de encaixe | Corrigir relevo localmente, preservar as correções ao reabrir e conferir módulos vizinhos |
| 4 — Contornos avançados | Furos, desenho livre e seleção automática | Exportar peças com aberturas reais e paredes internas, sem triângulos preenchendo os furos |
| 5 — Otimização e acabamento | Simplificação validada, limites finais e refinamento de progresso/cancelamento | Respeitar orçamento ou explicar inviabilidade; manter editor responsivo e sem reconstruções em repouso |

Os limites de recursos e a atualização somente quando houver mudanças são requisitos desde o início; a etapa 5 aprofunda a otimização. O editor básico não será limitado a retângulos.

## Validação

Usar a skill `engine-testing` para execução de scripts e verificação real na engine. Usar `doc-drift-check` ao alterar bindings ou documentação vinculada à implementação.

Cobertura prevista:

- Contornos convexos, côncavos, elípticos e, quando suportados, furos.
- Rejeição de contornos degenerados, cruzamentos e entradas inválidas.
- Imagens uniformes, transparentes, com detalhe fino e mapas de altura separados.
- Relevo invertido, limites de altura e espessura mínima válida.
- Geometria sem triângulos degenerados, normais invertidas ou frestas indesejadas. A verificação de fechamento considerará duplicações de vértices por UV e normal.
- Orçamento final incluindo todas as faces e duplicações.
- Salvamento, reabertura e reprodução determinística do resultado.
- Exportação individual e em lote, com carregamento posterior das malhas e texturas.
- Prévia sob iluminação e com material neutro.
- Editor em repouso sem reconstrução, carga de imagens ou uploads contínuos.
- Cancelamento e descarte de resultados obsoletos quando houver tarefas assíncronas.

O primeiro marco visual será montar uma parede 3D com os 12 painéis da imagem de referência. Os testes automatizados usarão imagens pequenas e sintéticas reproduzíveis, sem depender do arquivo externo em Downloads. Verificações em outros backends serão registradas conforme a disponibilidade real de cada plataforma.


## Registro de execução — 2026-09-19

### Etapa 1: prova técnica concluída em Linux/OpenGL ES

Implementado:

- Gerador C++ CPU-only em `src/core_mbm/image-mesh.cpp`, com interface em `include/core_mbm/image-mesh.h`.
- API Lua `mbm.generateImageMesh(imagePath, options)`, retornando um novo objeto de autoria `meshDebug` e relatório de geometria/altura; erros de processamento retornam `nil, mensagem`.
- Recorte retangular, altura por luminância interpolada, inversão, amplitude, espessura, resolução, borda fixa e transição linear.
- Frente e fundo subdivididos, laterais fechadas, normais suavizadas na frente e separadas nas quinas, material fosco e UVs na imagem original.
- Orçamento de vértices/triângulos incluindo todas as faces, validação de parâmetros e limite de tamanho da imagem.
- Exportação v11 pelo método `asset:save`, preservando normais e UVs gerados.
- Versão da engine atualizada para 7.216.0; fontes adicionados ao projeto Visual Studio. Windows não foi executado nesta etapa.

Validação realizada:

- Build Debug dos targets `mini-mbm` e `testLib`.
- `src/test-lib/image_mesh_smoke.lua`: fechamento geométrico com arestas orientadas em pares, volume positivo, normais unitárias, UVs, inversão, bordas fixas, recortes, repetibilidade, rejeição de entradas inválidas, exportação/reabertura e objeto renderizado.
- Carregamento do arquivo exportado pelo `testLib` C++, com encerramento automático.
- Prévia do primeiro painel da imagem de referência em vista oblíqua, com textura e iluminação, conferida visualmente. Grade de 64 x 64 células: 9.474 vértices e 16.896 triângulos incluindo fundo/laterais.
- Correção do brilho especular inicial: material padrão do gerador agora é fosco, para não saturar a textura na prévia.

Reprodução do teste sintético a partir da raiz do repositório:

```sh
cmake -S . -B build -DPLAT=Linux -DUSE_ALL=1 -DCMAKE_BUILD_TYPE=Debug -DUSE_TEXTURE_MISSING_DIALOG=0
cmake --build build --target mini-mbm testLib -j 6
timeout -s KILL 15 bin/debug/linux_x86/mini-mbm --scene src/test-lib/image_mesh_smoke.lua --disable_select_monitor --nosplash -w 640 -h 480
timeout -s KILL 15 bin/debug/linux_x86/testLib 3 /tmp/mbm_image_mesh_smoke.msh 3d
```

O teste Lua deve imprimir os marcadores `IMAGE MESH GEOMETRY / VALIDATION / ROUNDTRIP OK` e `IMAGE MESH RENDER OK`; somente o código de saída da engine não basta para verificar erros Lua. Os arquivos sintéticos e exportados ficam em `/tmp`.

Prévia reproduzível do painel de referência (encerra após 10 segundos):

```sh
MBM_IMAGE_MESH_SOURCE=/home/michel/Downloads/mesh-tile-experiment/f1-c002ae9c-img01.png \
MBM_IMAGE_MESH_X=41 MBM_IMAGE_MESH_Y=27 \
MBM_IMAGE_MESH_WIDTH=233 MBM_IMAGE_MESH_HEIGHT=216 \
timeout -s KILL 20 bin/debug/linux_x86/mini-mbm --scene src/test-lib/image_mesh_preview.lua --disable_select_monitor --nosplash -w 960 -h 720
```

`MBM_IMAGE_MESH_OUTPUT` pode alterar o destino padrão `/tmp/mbm_image_mesh_preview.msh`; `MBM_IMAGE_MESH_RELIEF` altera a amplitude. Sem variáveis de recorte, a prévia usa a imagem inteira.

Limites ao final da etapa 1 (histórico; atualizados pela etapa 2 abaixo):

- Ainda não há interface de editor. A próxima entrega é a etapa 2, incluindo elipses e polígonos côncavos, além dos retângulos.
- A geração é síncrona e retangular; furos, suavização configurável, canais alternativos, pintura e mapas separados ainda não estão implementados.
- A textura original é referenciada, não copiada. Exportação portátil com recortes e margens permanece pendente.
- O fundo é plano e repete as coordenadas frontais; as laterais esticam os texels da borda. As opções artísticas completas continuam na etapa 3.
- A prévia de um painel valida a prova técnica; a parede com os 12 painéis continua sendo o marco da etapa 2.
- Os contratos e limites da API implementada estão em `docs/lua-api.md`, seção de geração de malhas por imagem.


### Etapa 2: editor básico concluído em Linux/OpenGL ES

Disponível em `editor/image_mesh_editor.lua`, com módulos separados para modelo,
canvas e persistência. Guia de uso: [Image Mesh Editor](image-mesh-editor.md).

Entregue:

- Retângulos, elipses/círculos e polígonos simples côncavos. O gerador C++ valida
  contornos, triangula e refina com pontos compartilhados; gera fundo e laterais
  fechados respeitando a forma. Polígonos não suportam furos nesta etapa.
- Seleção e desenho sobre a imagem, movimento, redimensionamento, edição de
  pontos por arraste ou coordenadas, inserção e remoção de pontos.
- Grade com margens e espaçamento, seleção múltipla, duplicação e exclusão.
- Parâmetros gerais e sobrescritas por região. Aplicação de parâmetros de geração
  às regiões selecionadas; nome/recorte/forma pertencem à região principal.
- Histórico de 40 operações, desfazer/refazer, projeto versionado `.imesh`, caminhos
  relativos quando a imagem está sob a pasta do projeto e localização de imagem
  movida com conferência das dimensões.
- Prévia 3D com órbita, distância e intensidade da luz. Reconstrução somente após
  edição confirmada ou seleção; render target atualizado apenas após mudanças de
  geometria, câmera ou luz, mantendo a textura da prévia em repouso. A orientação
  vertical do render target 3D foi corrigida na exibição ImGui/OpenGL ES.
- Exportação individual e em lote. O lote processa uma peça por frame, pode ser
  cancelado entre peças e relata falhas. Arquivos existentes no lote são rejeitados
  individualmente; arquivos já concluídos permanecem após cancelamento.
- Integração aos launchers Linux, macOS e Windows, aliases/atalhos e localização
  inglês/português. Versão 7.217.0. Macro `MBM_IMAGE_MESH_LUA_API` removido.

Validação:

- Build Debug e teste sintético da API, incluindo volume, fechamento orientado,
  normais, recortes, elipses, concavidade, orientação invertida do contorno,
  colinearidade e rejeição de cruzamentos/orçamentos inválidos.
- `image_mesh_model_test.lua`: modelo, grade, herança, histórico e caminhos.
- `image_mesh_editor_smoke.lua`: execução do editor real, projeto salvo/reaberto,
  imagem relocalizada, parâmetros em seleção múltipla, lote de 14 peças reaberto,
  repouso sem reconstruções e sem render target ativo. Eventos ImGui simulados
  exercitam desenho de retângulo/elipse/polígono, movimento, redimensionamento e
  arraste de ponto; não equivalem a uma revisão manual de todos os controles.
- `image_mesh_wall_smoke.lua`: os 12 painéis da referência exportados e carregados
  como módulos adjacentes. Parede e prévia côncava conferidas visualmente no GLES.
- Teste C++ `testLib` carrega as malhas exportadas. Outras plataformas não foram
  executadas; o registro nos projetos/launchers não é evidência de teste nelas.

Artefatos locais da referência: `/tmp/image-mesh-stage2-export/reference.imesh`
e `reference_1.msh` a `reference_12.msh` na mesma pasta. São reproduzíveis pelo
fixture visual descrito no guia, dependem da imagem original e não fazem parte
permanente do repositório.

Próxima entrega: etapa 3 (pintura de altura, presets, texturas/fundo e prévia de
encaixe). Continuam pendentes furos, seleção automática, mapas de altura separados,
empacotamento portátil de texturas e otimização/simplificação da etapa 5. O lote é
cooperativo entre peças; a geração de uma peça ainda é síncrona e não cancelável.


### Revisão da interface após uso (7.218.0)

A organização atual segue o `texture_packer.lua`: uma lateral **Regiões** reúne
ferramentas, lista e propriedades da seleção. As janelas separadas de imagem,
propriedades e prévia 3D foram removidas. O checkbox **Modo de edição** alterna
entre imagem/contornos e a mesh selecionada na cena principal da engine.

O canvas usa `texture`/`line` e callbacks `onTouch*`, com zoom, deslocamento,
seleção e edição dos contornos. A visualização usa a câmera 3D da cena, sem RTT.
A geração é adiada durante edição e acontece ao entrar na visualização, selecionar
ou aplicar alterações nesse modo. Em repouso, não há reconstruções de malha ou
buffers dos contornos. Os registros anteriores sobre RTT descrevem a versão
inicial da etapa 2, substituída por esta revisão.

Os testes de projeto/lote foram mantidos e os gestos migraram de consultas ImGui
simuladas para callbacks da cena. Foram verificadas alternância dos modos,
órbita, desenho/movimento/redimensionamento/pontos e ausência de reconstruções
em repouso no Linux/OpenGL ES.


### Controles de diagnóstico e câmera 2D (7.219.0)

Adicionadas à direita as janelas Câmera 3D / 2D e Luz, reaproveitando o gizmo de
órbita e as conversões de direção de `editor_utils.lua` usados pelo Mesh Debug.
A câmera oferece navegação e edição numérica; a luz tem controles independentes
por espaço (direcional 3D e pontual 2dw). A imagem e seus contornos migraram de
2ds para 2dw. Arrastar com botão direito/central ou com a ferramenta Mover câmera
move a câmera 2D; zoom altera a escala de visualização dos objetos, sem alterar
os recortes. Pan não reconstrói os buffers de contorno. Luz e câmera 3D não
regeneram a mesh. Esta entrega adiciona instrumentos para investigar o relevo;
não altera o algoritmo de amostragem nem declara resolvida a suspeita visual.


### Primitivas e iluminação restrita ao 3D (7.220.0)

A janela Luz agora só aparece na visualização 3D; controles e estado de luz 2dw
foram removidos. A imagem de entrada permanece sem iluminação. O registro da
7.219.0 acima descreve a interface anterior, substituída por esta revisão.

O painel Regiões oferece Adicionar forma com combo de retângulo, círculo, elipse,
triângulo e polígono regular (3..32 lados), dimensões em pixels e botão Adicionar.
As formas são criadas como regiões existentes do modelo, selecionadas na cena,
com movimento, alças/pontos, dimensões numéricas, histórico e persistência.
Todos os presets usam a extrusão já implementada; não há alteração no algoritmo
CPU nem no formato dos projetos. Testes cobrem criação, limites, undo/redo,
extrusão de cada preset, persistência e ocultação da luz durante edição.


### Redimensionamento e transparência das laterais (7.220.1)

Alças maiores (16..40 pixels), com crescimento pelo zoom e margem de clique de
6 pixels. O arraste usa a variação desde o clique para não saltar quando iniciado
fora do centro da alça, inclusive próximo ao limite da imagem.

A ausência visual das laterais foi reproduzida no atlas original: os triângulos
estavam presentes, mas amostravam transparência nas margens do recorte. O gerador
mantém a geometria fechada e passa a usar um texel próximo de máxima opacidade
para cada segmento de parede que atravessa transparência. Frente/fundo permanecem
iguais. A consulta é construída sob demanda em tempo linear, com até 64 MiB de
memória adicional no limite de 16 megapixels, apenas durante a geração.

Regressões verificam as seis direções de faces, UVs laterais com margem alfa,
exportação/recarga e alças ampliadas sem salto. Conferência visual com o atlas
original mostrou as paredes antes invisíveis. Exportações anteriores exigem
regeneração para incorporar os novos UVs.


### Hit testing fora do centro (7.220.2)

O projeto de reprodução revelou dois problemas na entrada: uma rejeição de toda
a faixa direita, inclusive da área livre sob a câmera no modo de edição, e mistura
de coordenadas lógicas dos callbacks com pixels de framebuffer. O editor passa a
bloquear apenas áreas ocupadas pela interface via WantCaptureMouse e limites da
tela. A conversão para pixels é feita nos callbacks; as escalas X/Y são tratadas
separadamente no canvas, nas alças e no posicionamento de primitivas.

A regressão testa o projeto salvo, escalas 1x/2x/assimétrica, alça à direita fora
do centro, redimensionamento/undo e captura da interface. Cópias temporárias do
código anterior falham nos casos que agora passam.


### Arraste contextual com o botão esquerdo (7.221.0)

Em Selecionar / mover, o clique inicial decide a ação: alça edita a região,
interior de forma move a forma, espaço vazio move a câmera 2D. A decisão é
mantida durante o arraste. Ctrl continua alternando a seleção sem iniciar pan
sobre formas; a captura da interface mantém prioridade. As ferramentas de
desenho e o pan por botão direito/central permanecem disponíveis.

A regressão de entrada cobre pan em vazio dentro/fora da imagem, movimento de
forma sem mover a câmera, seleção com Ctrl após undo e escalas distintas.
Pan não modifica o projeto nem reconstrói buffers de contornos.


### Zoom no cursor em edição (7.222.0)

A roda ajusta a escala da imagem e a posição da câmera 2D em conjunto, mantendo
o ponto sob o cursor fixo na tela. Usa coordenadas físicas do ImGui e compensa
as escalas X/Y da câmera. Os limites de zoom não provocam deslocamento adicional.
O zoom numérico e o enquadramento continuam disponíveis no painel da câmera.

A regressão verifica aproximação/afastamento após pan, escalas 1x/2x/assimétrica,
limites de zoom e captura da interface, sem editar o projeto ou regenerar a mesh.


### Círculos, proporções e expansão de polígonos (7.223.0)

- Elipses usam leque inicial central antes do refinamento compartilhado; polígonos
  genéricos mantêm ear clipping para suportar concavidade.
- O editor preserva por padrão a proporção do recorte entre 2D e 3D, inclusive em
  projetos antigos. A opção pode ser desmarcada para usar dimensões livres. É uma
  opção do editor: a API continua recebendo largura/altura explícitas.
- Arrastar vértices além do recorte expande seus limites e renormaliza os pontos,
  preservando os demais vértices em pixels. O limite passa a ser a imagem.
- Regressões cobrem leque central, fechamento/normais/volume, proporções da mesh
  do módulo 5 e expansão superior/inferior com undo nas três escalas de tela.


### Wireframe na cena 3D (7.224.0)

O painel Regiões oferece wireframe no modo de visualização, alternando superfície
e arestas completas da mesh. As linhas são geradas sob demanda e reutilizadas
em alternâncias e movimentos de câmera. Trocar a geometria invalida o cache;
a edição 2D oculta a visualização. Exportação e projeto não são modificados.

Arestas coincidentes são unificadas e percorridas em sequências limitadas de
linhas, evitando conexões falsas entre componentes e um draw call por triângulo.
A regressão integrada cobre visibilidade, cache, troca de módulo e modos.


### Diagnóstico e simplificação dos orçamentos (7.225.0)

O gerador informa recurso excedido, quantidade mínima já necessária e limite
efetivo, em todos os caminhos de orçamento (grade, contorno e refinamento).
O editor apresenta a falha no alto de Regiões, com módulo e orientação para
reduzir densidade, sem expor localização de assert em Lua.

Orçamento de vértices permite tetos menores que os 65.535 fixos da engine.
O teto de triângulos no editor passa a ser automático: 2 vezes esse orçamento,
um limite conservador, não uma previsão da contagem final. Projetos antigos
continuam legíveis; o maxTriangles armazenado deixa de restringir o editor.
A API mantém seu parâmetro independente para consumidores existentes.

Regressões cobrem os limites de ambos os recursos, refinamento de elipse acima
da capacidade da engine, orçamento derivado e recuperação da prévia via undo.


### Feedback de salvamento e contagem de faces (7.226.0)

Salvar confirma o sucesso em overlay com nome do arquivo e duração de quatro
segundos, reiniciada a cada salvamento. O painel Regiões mostra o total de faces
triangulares da seleção em formato compacto (K decimal), com totais exatos de
faces/vértices no tooltip. Vale para edição 2D e visualização 3D.

A edição 2D consulta a geração CPU sob demanda e mantém relatórios por módulo
em cache até mudanças no projeto. Arrastes não recalculam continuamente, e
contagens antigas não são exibidas durante edição de geometria ou após erro.
Regressões cobrem confirmação repetida, formatação, seleção, invalidação e idle.


### Detecção de sulcos e geometria adaptativa (7.227.0)

Entregues controles por módulo de limiar, inversão, duas alturas, transição,
suavização preservando bordas e tolerância de erro. Prévia 2D de alturas e
sobreposição azul usam o mesmo processamento CPU da geração, sobre a imagem
na cena da engine. O botão de prévia permite avaliar ajustes antes de Aplicar.

O modo adaptativo é opt-in, preservando projetos anteriores. Refina por erro
amostrado, melhora triângulos por flips locais, alinha arestas às transições
e simplifica o verso plano. Os limites de geometria continuam obrigatórios;
Colunas/Linhas delimitam densidade e amostragem. Pincéis e furos não fazem parte
desta etapa. Não há reconstrução semântica de profundidade.

Testes: fechamento, normais, volume, orçamento, pequenos triângulos nas transições,
retângulos/elipses/polígonos côncavos, mapas sem orçamento de mesh, cache, prévia
de rascunho, salvamento/carregamento e alternância 2D/3D. No projeto real, os
módulos 1/2/5 passaram em varredura dos limiares 0,3/0,45/0,6. O círculo passou
de 31.392 triângulos no preset anterior a 13.586 na configuração documentada.


### Preservar enquadramento ao aplicar (7.227.1)

Regenerar o mesmo módulo mantém órbita, distância e foco 3D. A distância de
enquadramento é atualizada apenas como referência para Resetar visão. Outro
módulo ou projeto mantém o enquadramento automático. A identificação da vista
permanece válida após falhas de geração para permitir corrigir parâmetros sem
perder a comparação. Regressão cobre Aplicar, undo/redo, modos e recuperação.

### 7.228.0 - Simplificação opcional após geração

- Ajustes por módulo/padrões: proporção, preservar detalhes e limiar de fronteira,
  usando o mesmo algoritmo e intervalos do painel Frames do Mesh Debug.
- Execução assíncrona com progresso e proteção dos controles durante a operação.
- Prévia, wireframe, estatísticas e exportações usam o resultado final. Falhas não
  exportam a geometria original como se a redução tivesse sido concluída.
- Compatibilidade com projetos anteriores, histórico e persistência dos ajustes.
- Contagens do frame gerado preenchidas antes de salvar, permitindo simplificação
  diretamente em memória. Teste de integração cobre exportação, lote e recuperação.


### Etapa 3a - Pintura manual de altura (7.234.0)

Entregues quatro pincéis (elevar, rebaixar, nivelar e suavizar), cursor na cena,
raio/intensidade/altura alvo, histórico por arraste, cancelamento e limpeza com
undo. Traços normalizados persistem por módulo, são duplicados com ele e alimentam
a mesma altura na prévia, geração adaptativa, simplificação e exportação. A imagem
fonte permanece intacta; a borda fixa continua prevalecendo. O mapa atualiza ao
soltar o mouse, com geometria adiada durante a pintura e sem reconstrução em repouso.

Verificação: `image_mesh_paint_smoke.lua` cobre os quatro modos, bordas, números
inválidos, escopo do contorno, histórico, cancelamento, persistência, exportação e
repouso. Os testes existentes de modelo/API/editor continuam aplicáveis.

A etapa 3 ainda não está toda concluída: restam opções adicionais de textura/fundo
e prévia de encaixe entre módulos. Os presets foram entregues na etapa 3b abaixo. As etapas
4 e o restante do acabamento da etapa 5 continuam pendentes.


### Correção da fidelidade da pintura (7.234.1)

Alinhamento adaptativo passa a consultar a altura corrigida. Após os ajustes de
diagonais, refinamento local verifica os pixels pintados e a borda do traço,
independentemente da densidade geral, mantendo fechamento e orçamento. O projeto
continua guardando traços editáveis; não há exportação/releitura intermediária de
PNG. Sem modo adaptativo, a resolução continua sendo explícita.

Regressão `image_mesh_paint_refine_smoke.lua`: retoques menores que a célula da
mesh com tolerância global alta, retângulo/elipse/polígono côncavo, altura amostrada,
fechamento, repetibilidade, pintura sem efeito, depressões, preenchimento entre pixels, preservação de normais
dos patamares e rejeição por orçamento.

Na configuração analisada de `project-1-pintado.imesh` (módulo 2, 30 x 30,
tolerância 0,26, simplificação desligada), o resultado passou de 3.590 para
4.686 triângulos. Nos 39 pixels alterados, o erro absoluto médio entre a superfície
interpolada e o mapa diagnóstico de 8 bits caiu de 0,756579 para 0,0365014 unidades;
o máximo caiu de 4,422234 para 0,1480951. A leitura do mapa inclui arredondamento
para 8 bits; essas medidas são desse projeto, não uma garantia global. Comparação
visual com material neutro verificou a preservação dos patamares fora da pintura.


### Etapa 3b - Presets reutilizáveis (7.235.0)

Entregue: captura dos parâmetros exibidos, aplicação aos módulos selecionados ou
aos padrões do projeto, atualização, renomeação e exclusão com histórico.
Contornos e pintura manual já confirmados são preservados. Presets são salvos
no projeto; arquivos `.imeshpreset` permitem exportação e importação entre
projetos. Projetos anteriores continuam carregando sem migração.

Verificação: `image_mesh_presets_smoke.lua` cobre seleção múltipla, isolamento
dos módulos não selecionados, preservação de contorno/pintura, validação,
desfazer/refazer, salvar/reabrir e importação entre projetos, além da interface
ImGui e ausência de trabalho contínuo em repouso.

Próximas pendências da etapa 3: opções adicionais de textura/fundo e prévia de
encaixe entre módulos. Etapas 4 e acabamento restante da etapa 5 seguem pendentes.

### Etapa 3c - Controle do verso (7.236.0)

Entregue: fundo plano ou com relevo copiado da frente e espelhamento horizontal
independente da textura do verso. A cópia inclui pintura e bordas, preserva
normais dos patamares e usa orçamento para a triangulação completa do verso.
Opções na seção **Textura e fundo**, persistidas em projetos/presets, aplicáveis
à seleção múltipla e exportação. Os padrões anteriores permanecem válidos.

Verificação: `image_mesh_back_smoke.lua` cobre retângulo, elipse e polígono
côncavo, geração regular/adaptativa, fechamento, espessura, normais, independência
entre UVs/geometria, limites, pintura, persistência/presets e exportação.

O bloco de texturas/fundo ainda não está completo. Próximas entregas: textura
própria no verso, repetição/cor uniforme nas laterais e empacotamento portátil
com margens de proteção. Prévia de encaixe segue pendente na etapa 3.

### Extensão do verso - sem fundo e recorte UV (7.237.0)

Entregue: **Sem fundo**, removendo geometria traseira e seu custo do orçamento,
e **Plano + remap UV**, com retângulo independente na imagem original.
Contorno roxo acompanha a forma frontal, com seleção própria para mover e
redimensionar, limites da imagem, histórico por gesto e persistência por módulo.
Espelhamento também é refletido no desenho de contornos assimétricos.
Presets preservam recortes UV existentes dos módulos.

Verificação: `image_mesh_back_uv_smoke.lua` cobre retângulo/elipse/polígono,
modos regulares/adaptativos, ausência de faces traseiras, preservação de
frente/laterais, UVs, espelhamento, limites, input de movimento/redimensionamento,
câmera em espaço vazio, cancelamento, histórico, presets, persistência e exportação.

Textura de outro arquivo, opções de repetição/cor das laterais e exportação
portátil permanecem pendentes. Remap UV seleciona pixels da mesma imagem.

### Etapa 3d - Texturas laterais e faixa interna (7.238.0)

Entregues os quatro modos: borda esticada, cor uniforme, textura repetida e
faixa interna do próprio contorno. A faixa possui limite geométrico, campo em
pixels e alça verde no canvas, sem alterar o relevo. Repetição possui controles
independentes no perímetro/profundidade, com divisões geométricas contabilizadas
no orçamento e propagadas às faces adjacentes para preservar fechamento.

Cor/repetição separam paredes em outro material; prévia, comparação, wireframe,
simplificação e exportação foram adaptados a múltiplos subsets. Persistência de
projetos/presets inclui caminhos relativos de textura. O modo antigo permanece
como padrão. Consultas de contorno e linhas só mudam quando necessário.

`image_mesh_sides_smoke.lua` verifica formas convexas/côncavas, geração regular
e adaptativa, UVs, fechamento, materiais, simplificação, limites, exportação,
alça, histórico, persistência, caminhos relativos, wireframe e repouso.

Próximas pendências: textura do verso de outro arquivo, empacotamento portátil
com margens e prévia de encaixe entre módulos.

### Correção da textura repetida padrão (7.238.1)

Selecionar textura repetida sem outro arquivo usa o recorte original, com UVs
limitados aos centros dos pixels desse recorte. Arquivos externos continuam
opcionais e validados; o editor permite voltar ao recorte original.

### Inversão da faixa lateral (7.239.0)

Checkbox exclusivo da faixa interna permite trocar os UVs das extremidades
frente/verso da lateral. Mantém o sentido anterior por padrão e persiste em
projetos/presets; não altera contorno, geometria nem normais.

### Fundo com cor sólida (7.240.0)

Quinto modo de fundo: plano opaco com seletor RGB, persistido em projetos/presets
e exportado como material `#RRGGBBFF`. Três subsets preservam a independência
das texturas frontal, traseira e lateral, inclusive na simplificação.

### Textura externa do fundo (7.241.0)

Entregue o sexto modo: fundo plano com arquivo de imagem independente e
espelhamento horizontal. Imagem inteira ajustada à forma, UVs nos centros dos
pixels e fallback para recorte original quando não há arquivo escolhido.
Projetos/presets preservam caminhos relativos; exportação referencia a imagem.
Testes cobrem formas regulares/côncavas, quatro modos laterais, UVs, geometria,
espelhamento, simplificação, exportação e persistência.

Próximo milestone: exportação portátil com texturas e margens de proteção.
Depois: prévia de encaixe entre módulos.

### Exportação portátil (7.242.0)

Entregue exportação individual/em lote de `.msh` + PNGs adjacentes, nomes
relativos, recortes pelos UVs finais e margem replicada de 4 pixels. Materiais
sólidos não exigem PNG; geometria/normais preservadas. Colisões recusadas e
limpeza dos arquivos criados por módulo em caso de falha. Sem atlas compartilhado
ou deduplicação entre módulos nesta entrega. Próximo milestone: prévia de encaixe.

### Ajuste da exportação portátil (7.243.0)

Sobrescrita habilitada para exportação portátil individual/em lote, preparando
novos arquivos antes da substituição e restaurando backups em falhas normais.
Padrão: imagem inteira em PNG, dimensões/pixels/UVs preservados, sem margem.
Recorte com margem e remapeamento de UVs tornou-se checkbox opcional no menu
Arquivo, desmarcado no início da sessão.

### Compartilhamento de texturas na exportação (7.244.0)

Lotes reutilizam cada imagem de origem entre módulos e materiais; cada PNG é
gerado apenas uma vez e referenciado pelo mesmo nome. Recortes compartilham
quando origem e limites de UV coincidem. O registro de compartilhamento só
é atualizado depois do commit bem-sucedido do módulo. O cache dura apenas
o lote, sem trabalho adicional em repouso nem exclusão de arquivos antigos.

### Prévia de encaixe entre módulos (7.245.0)

Entregue montagem 3D com todos os módulos, grade por maior dimensão, número
de colunas, espaçamento X/Y, posição numérica linha/coluna, profundidade e
visibilidade por módulo. Planos frontais de base alinhados. Reutiliza luz,
câmera e wireframe, respeitando simplificação e múltiplos materiais.
Disposição temporária, sem alterar/salvar/exportar parâmetros de geração.
Posições e seleção não regeneram geometria; reconstrução preserva câmera.
Prévias e arquivos temporários são liberados ao sair/abrir outro projeto.

`image_mesh_assembly_smoke.lua` valida disposição, alinhamento em profundidade,
simplificação, wireframe, seleção sem regeneração, câmera, arquivos temporários
e ausência de trabalho em repouso. Conferência visual com project-1.imesh.
Arraste 3D, rotação individual e persistência da montagem ficam fora desta entrega.

### Etapa 4a - Furos manuais (7.246.0)

Entregues contornos internos por módulo (até 16 x 128 pontos), retângulo/círculo
inicial e desenho poligonal manual. Edição de vértices e movimento no canvas,
validação de contenção/cruzamentos, histórico e persistência. Triangulação da
frente/verso respeita aberturas e gera paredes internas; refinamento acompanha
todos os perímetros, sem paredes nas pontes internas da triangulação.

Compatível com simplificação, modos de fundo/laterais, orçamento e exportação.
A faixa UV interna permanece externa; paredes dos furos usam borda esticada
nesse modo. Fundo adaptativo de peças furadas reutiliza a triangulação frontal.
Testes cobrem fechamento, característica de Euler, área, normais, 16 furos,
furos côncavos, limites, mapas, edição, histórico, presets e salvamento.

Próximas pendências da etapa 4: desenho livre e seleção automática de contornos.

### Etapa 4b - Desenho livre (7.248.0)

Entregue traçado contínuo com mouse para módulos e furos. Prévia fechada antes
da confirmação, redução de pontos por tolerância em pixels da imagem, contagem
e validação de limites/cruzamentos. Resultado armazenado como polígono comum,
sem novo formato de projeto; preserva edição de vértices, histórico e exportação.

Até 4096 amostras por traçado e 128 vértices finais. Redução somente ao soltar
ou alterar tolerância; nenhum processamento contínuo em repouso.
`image_mesh_freehand_smoke.lua` cobre contorno côncavo, furo, redução, rejeição,
cancelamento, histórico, persistência, exportação e GUI em repouso.

Próxima pendência da etapa 4: seleção automática de contornos.

### Robustez das ligações entre furos (7.248.1)

A triangulação conecta os furos por ordem geométrica, evitando iniciar por
um furo oculto dos vértices externos por outros contornos. Em vértices
repetidos pelas ligações, verifica também o setor interno correspondente.
Nenhum contorno é movido, fundido ou simplificado por essa correção.
Regressão inclui 500 combinações/ordens, aberturas próximas e traços estreitos,
com verificação de fechamento e característica de Euler.

### Etapa 4c - Seleção automática (7.249.0)

Entregue seleção por transparência ou exclusão de cor de fundo, com captura da
cor na imagem, limiares e escolha de componente conectado por clique. Destino
explícito de novo módulo ou furo; cavidades internas não criam aberturas implícitas.
Prévia e redução de pontos antes da confirmação; saída no formato poligonal
existente, compatível com histórico, edição, persistência e exportação.

Leitura CPU de RGBA sob demanda pela nova `mbm.readImagePixels`. Busca por
corrotina cancelável, limitada a 262.144 células com passo informado e opção
de recorte para maior precisão. Limites e ambiguidades recebem diagnósticos.
Nenhuma detecção ou decodificação em repouso.

`image_mesh_auto_smoke.lua` cobre alfa/cor, cavidades, captura, módulos/furos,
cancelamento, mudança de projeto, persistência, exportação e GUI em repouso.
A etapa 4 está entregue. Próxima revisão: pendências de progresso/cancelamento
e limites finais da etapa 5, preservando as otimizações já implementadas.

### Relevo desenhado por áreas (entregue em 7.250.0)

Motivação: no `project-2.imesh`, as cores e sombras da textura não representam
necessariamente profundidade. A pintura de altura existente permite retoques
com pincel, mas não oferece áreas nomeadas cujos contornos e alturas possam ser
alterados posteriormente como objetos separados. As áreas editáveis complementam
a pintura e a seleção automática desde a versão 7.250.0.

#### Primeira entrega

- Escolher a origem da altura: **Imagem** (comportamento atual), **Manual**
  (plano de altura base ajustável) ou **Imagem + áreas manuais**. A textura
  continua sendo usada normalmente nos três modos.
- Desenhar regiões de relevo dentro de cada módulo usando retângulo, elipse,
  polígono ou desenho livre. São máscaras de altura; não criam módulos nem
  aberturas atravessando a peça.
- Atribuir uma altura-alvo a cada região, na escala normalizada de 0 a 1 já
  usada pelo mapa de alturas. O parâmetro Relevo converte essa escala em
  unidades da mesh. No modo Manual, uma base intermediária permite desenhar
  tanto áreas elevadas quanto sulcos abaixo dela, sem atravessar o verso.
- Controlar a largura da transição entre a altura vizinha e a altura-alvo;
  largura zero representa uma mudança abrupta no campo de alturas, cuja
  aproximação geométrica continua sujeita à resolução e ao orçamento.
- Listar regiões com nome, visibilidade/habilitação, altura e ordem. Permitir
  selecionar, mover, redimensionar, editar vértices, duplicar e excluir.
  Regiões posteriores prevalecem nas sobreposições; mostrar essa ordem na GUI.
- Visualizar contornos coloridos sobre a textura e o mapa de alturas resultante
  antes de aplicar à mesh. Identificar região selecionada e altura-alvo.

#### Integração e compatibilidade

Ordem implementada: obter a altura da imagem ou do plano base; compor as regiões
manuais; aplicar os retoques de pincel existentes; aplicar a restrição da borda
externa. Furos continuam recortando a superfície sem rebaixar a vizinhança.
As regiões de altura são limitadas à área útil do módulo; o exterior e os furos
não recebem geometria. Projetos antigos mantêm os resultados atuais.

Salvar as regiões por módulo, em coordenadas normalizadas, com histórico de
desfazer/refazer. Preservá-las ao mover/redimensionar e duplicar o módulo.
Prévia 2D, geração 3D, refinamento adaptativo, simplificação e exportação devem
consultar o mesmo campo de alturas composto. Regiões manuais precisam participar
do refinamento local, como a pintura, sem depender somente das cores da imagem.
Alterações só atualizam dados dependentes; não processar máscaras em repouso.

#### Critérios de validação

- Reproduzir no painel do projeto 2 uma moldura elevada, painel central em
  altura menor e uma canaleta, independentemente das cores da textura.
- Verificar alturas e transições, ordem de sobreposição, recorte contra o módulo
  e furos, pintura posterior, borda fixa, orçamento e ausência de degenerações.
- Conferir persistência, histórico, duplicação, prévia/exportação coerentes e
  preservação dos projetos existentes.

Uma entrega posterior poderá acrescentar **sulcos por linha**, com largura,
altura-alvo e edição do trajeto. A primeira entrega usa áreas fechadas; canais
estreitos podem ser desenhados como regiões. A primeira entrega foi priorizada antes do acabamento pendente da etapa 5.

Entrega 7.250.0: modos Imagem/Manual/Misto, áreas editáveis com altura e transição,
composição no campo de alturas, refinamento local, histórico, persistência e
exportação. Testes de backend e editor em `src/test-lib/image_mesh_areas*_smoke.lua`.

### Observação de desempenho - geração manual (2026-09-21)

Investigação sem mudança de algoritmo, motivada pela pausa inicial percebida na GUI.
Medição no build Linux Debug, com uma leitura do `project-2.imesh` do usuário:
`module_001`, recorte 437 x 341, duas áreas, modo Manual, grade 24 x 24,
adaptativo ligado e simplificação desligada. Arquivos de saída somente em `/tmp`.
Os números abaixo são tempo de CPU (`os.clock`), não latência de quadro medida.

| Operação | Tempo de CPU | Triângulos |
|---|---:|---:|
| Geração completa, três chamadas iguais | 2,354 / 2,313 / 2,229 s | 2.342 |
| Mapa de alturas, três chamadas iguais | 0,805 / 0,765 / 0,764 s | — |
| Salvar a mesh já gerada | 0,005–0,006 s | — |
| Geração só da base, sem áreas/pincel | 0,174 s | 396 |
| Geração com áreas, adaptativo desligado | 0,174 s | 16.968 |

A comparação localiza o custo adicional principalmente no caminho adaptativo com
áreas, mas não discrimina seus estágios internos. Desligá-lo não é uma otimização
equivalente: muda a triangulação e a fidelidade dos detalhes. Não houve uma grande
redução de custo entre a primeira e as demais gerações idênticas.

`updateStatisticsImpl` gera a mesh completa em modo de edição quando não há
estatísticas em cache. `changed()` invalida esse cache ao confirmar alterações.
A troca para 3D passa por `rebuildImpl`, que pode gerar novamente o mesmo módulo:
o cache das estatísticas conserva o relatório, não a geometria. A chamada nativa
`generateImageMesh` é síncrona; a coroutine que a envolve não permite desenhar a GUI
enquanto o C++ está executando. A simplificação já tem processamento assíncrono,
mas não resolve essa pausa anterior. Isso explica uma pausa ao calcular os dados,
seguida de fluidez em repouso; ajustes confirmados ainda podem repetir o custo.

Pendência de otimização, sem classificar a observação como defeito:

1. Avaliar reaproveitar a geometria gerada para estatísticas na prévia 3D, com
   invalidação correta por módulo/opções, limites de memória e atenção à comparação
   original/simplificada e à rotação aplicada pelo editor.
2. Se a pausa continuar relevante, instrumentar os estágios do adaptativo e avaliar
   geração assíncrona com progresso/cancelamento, mantendo operações de GPU na thread
   apropriada. Essa entrega se relaciona ao acabamento de responsividade da etapa 5.

### Etapa 5 - Reaproveitamento da geometria das estatísticas (7.251.0)

Entregue o primeiro item da investigação acima. O editor mantém uma única mesh
completa, correspondente à última geração bem-sucedida para estatísticas, com
identificador de módulo e revisão do projeto. A prévia 3D do mesmo módulo/revisão
consome esse resultado sem repetir geração, rotação ou simplificação. A estatística
numérica continua disponível após o consumo; o cache não cresce com o número de módulos.

Quando há simplificação, a original já orientada para o editor fica em um arquivo
temporário, sem criar uma prévia GPU durante a contagem de faces. Ao entrar em 3D,
a comparação é montada a partir dela e da mesh simplificada. O temporário é removido
ao consumir, substituir ou invalidar o cache, inclusive após falhas. Mudanças
confirmadas, histórico, instalação de projeto e encerramento invalidam o resultado.
Uma entrada de outro módulo nunca é utilizada como se fosse a atual.

Exportação e montagem de módulos continuam usando geração independente: a exportação
portátil pode modificar texturas/UVs, portanto não recebe a mesh mutável do cache.
Nenhuma busca, cópia ou reconstrução de geometria foi adicionada ao editor em repouso.
O primeiro processamento continua síncrono; geração assíncrona com progresso e
cancelamento permanece pendente.

Validação: `image_mesh_cache_smoke.lua` verifica contagem de chamadas ao gerador,
vértices/normais/UVs da prévia, comparação original/simplificada, câmera, wireframe,
invalidação por histórico, troca de módulo, isolamento da exportação, erros e limpeza.
Regressões `image_mesh_simplify_smoke.lua` e `image_mesh_areas_editor_smoke.lua` passaram.
Build Linux Debug atualizado. Na mesma configuração do projeto 2, sem simplificação,
a estatística levou 2,320 s de CPU; a entrada em 3D levou 0,012 s e manteve o total
em uma única chamada ao gerador. Medição de CPU, não uma garantia de latência geral.

### Contagem de faces sob demanda em edição (7.251.1)

O reaproveitamento da prévia não eliminava o cálculo síncrono após cada arraste:
`changed()` invalidava a contagem e o próximo `onLoop` gerava novamente a mesh.
A geração automática para estatísticas foi removida do modo de edição. Adicionar,
mover, redimensionar, aplicar propriedades e usar o histórico deixam a contagem
pendente; **Calcular faces** solicita explicitamente a geração completa. A entrada
em 3D também gera/atualiza a contagem e continua usando o cache quando disponível.
Nenhuma estimativa antiga é apresentada como uma contagem atual.

A GUI distingue cálculo pendente de processamento em andamento. O botão confirma
propriedades pendentes e explica no tooltip que a contagem exata pode demorar.
A prévia do mapa de alturas mantém seu processamento próprio; esta correção elimina
a geração 3D implícita para contar faces, não torna todo processamento assíncrono.

Validação Linux Debug: `image_mesh_statistics_smoke.lua` move a área três vezes por
callbacks de mouse intercalados com frames e verifica zero chamadas ao gerador;
o botão gera uma vez, a prévia 3D reutiliza a mesh, alterações deixam a contagem
pendente e entrar em 3D atualiza o resultado. Regressões de cache/comparação e editor
de áreas passaram. Build atualizado para 7.251.1.


### Etapa 5 - Geração assíncrona com progresso e cancelamento (7.252.0)

Entregue para a geração da mesh: `mbm.startImageMesh` cria uma tarefa com cópia
própria dos parâmetros, contornos, áreas, pincel e caminhos. Um worker executa o
mesmo gerador CPU da API síncrona. A thread principal consulta estado/progresso e
só recebe o asset completo. Operações de Lua, GPU, prévia e exportação ficam na
thread principal. A API síncrona continua disponível para os consumidores atuais.

Checkpoints de cancelamento cobrem composição de áreas, filtros, pintura,
triangulação, alinhamento, refinamento e montagem final. Progresso por etapa é
estimado, não uma previsão de tempo restante. Cancelamento é cooperativo; decoder
e alocações indivisíveis podem terminar antes de o pedido ser observado.

O editor mostra uma janela de progresso com Cancelar/Escape fora dos controles
bloqueados da edição. Uma geração cancelada não alimenta a contagem/cache, não
sobrescreve exportações e interrompe o restante do lote. Na prévia individual, a
anterior é preservada até a nova estar pronta e fica explicitamente identificada
se a nova geração for cancelada ou falhar. O cache 7.251.0 continua sendo consumido
sem iniciar outra tarefa. A contagem permanece sob demanda, conforme 7.251.1.

O worker tem propriedade exclusiva do resultado e dados imutáveis de entrada;
leitura do resultado acontece após publicação atômica e join. O coletor solicita
cancelamento e aguarda o worker, sem thread solta acessando Lua após encerramento.
Nenhuma regeneração, cópia de mesh ou upload contínuo foi adicionado ao idle.

Validação Linux Debug: paridade exata de vértices/normais/UVs e materiais com a
API síncrona, cópia de arrays/caminhos, progresso monotônico, frames durante a
geração, exclusão de workers simultâneos, cancelamento, falhas, coleta e retry em
`image_mesh_async_smoke.lua`. `image_mesh_async_editor_smoke.lua` verifica a janela,
botão Cancelar, prévia anterior, retry, isolamento de exportação e limpeza. No
projeto 2, a conferência visual registrou 253 frames durante a geração.

Limites desta entrega: mapa de alturas/PNG continua síncrono; a simplificação
conserva sua tarefa existente sem o novo cancelamento; gravação e carregamento da
prévia permanecem na thread principal. A montagem de vários módulos conserva seu
tratamento anterior de falhas, sem restauração de uma montagem parcialmente refeita.
A revisão final de limites/diagnósticos da etapa 5 continua pendente.

### Etapa 5 - Revisão de limites e diagnósticos (7.253.0)

Entregue a revisão do orçamento de geometria na GUI. O painel mostra contagens e
percentuais antes da simplificação, resultado final quando simplificado e aviso
quando pelo menos 90% de um recurso foi utilizado. Os limites são registrados no
relatório da geração, para não confundir inputs ainda não aplicados com o orçamento
que produziu aquela mesh. Relatórios pendentes, em processamento ou de prévias
anteriores não são apresentados como resultados atuais.

A tradução de falhas agora cobre tanto estimativas mínimas do refinamento quanto
contagens das emendas laterais. O segundo formato não era reconhecido pelo editor.
Somente recursos excedidos aparecem como falha. A mensagem identifica a etapa e
sugere ajustes, explicando que simplificar posteriormente não contorna o orçamento
do gerador. Não foram modificados os limites ou a triangulação do backend.

O painel usa somente o relatório em memória: não gera, serializa ou varre meshes
em repouso. Validação: teste puro `image_mesh_budget_test.lua` cobre os dois formatos,
etapas, idiomas, recursos dentro do limite, contagem antes/depois da simplificação,
limites aplicados e supressão de dados antigos. Regressões de estatísticas e geração
assíncrona verificam o fluxo real do editor.

Com esta entrega fica concluída a revisão de orçamento/diagnósticos pendente da
etapa 5. Permanecem as limitações de responsividade já registradas em 7.252.0
(mapa de alturas, IO e cancelamento da simplificação); não são eliminadas por este
painel. Sulcos por linha continuam como extensão futura das áreas manuais.

### Controle artístico - Canais de altura (7.254.0)

Entregue a escolha de canal prevista em Fontes de altura: luminosidade, vermelho,
verde, azul e alfa. O campo de alturas compartilhado pelo gerador e pelas prévias
consulta o canal antes dos filtros e do mapeamento de sulcos. Manual ignora essa
seleção; Misto usa o canal como base antes das áreas e retoques. A textura e seus
UVs permanecem os mesmos. Luminosidade mantém a fórmula anterior por padrão.

A GUI apresenta o controle somente em Imagem/Misto, com tooltip sobre alfa e
aplicação dos ajustes. Projetos, presets e histórico conservam a escolha. A tarefa
assíncrona copia o enum junto com o restante das opções. A mudança não introduz
novas gerações nem varreduras em repouso.

Validação: `image_mesh_channels_smoke.lua` cobre valores conhecidos por canal,
mapa/mesh, RGB sem alfa, inversão, duas alturas, Manual/Misto, parâmetros inválidos,
cópia assíncrona, persistência, presets, histórico e padrão de projetos antigos.
Mapa de alturas em arquivo separado e sulcos por linha continuam pendentes como
extensões distintas; esta entrega não os implementa.
