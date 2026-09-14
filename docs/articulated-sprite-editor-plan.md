# Plano: editor de sprites com animação articulada 2D

Data: 2026-09-14.

Status: planejamento aprovado em conversa; implementação ainda não iniciada.
Este documento registra as decisões de produto e propõe uma sequência de execução.
As etapas e nomes de arquivos sugeridos não representam funcionalidades já entregues.

## Objetivo e limites

Criar um novo editor Lua para recortar peças de imagens, montar um personagem 2D,
definir pivôs e hierarquia de subsets e criar animações articuladas, como andar,
correr e pular. Disponibilizar o editor por padrão no menu principal de desenvolvimento.

Usar o sistema de animação articulada existente na engine, com subsets rígidos.
Não incluir skinning, criação de ossos, pesos por vértice, LBS ou DQS neste trabalho.
A triangulação define a geometria do recorte, não deformações por ossos.
Exportar `.spt` com dados articulados, sem substituir as animações por quadros
pré-calculados. A prévia no editor e a reprodução no jogo devem usar o comportamento
articulado da engine.

Referência visual fornecida: `/home/michel/Downloads/boy-parts.png`.
Esse caminho é uma referência local de trabalho, não uma dependência do editor.
Recortar pode excluir uma manga desenhada no tronco, mas não reconstrói pintura
oculta na imagem original. Pintura/reconstrução da imagem não integra este escopo.

## Base existente confirmada

- [sprite.cpp](../src/render/sprite.cpp): `SPRITE` oferece reprodução, pausa,
  retomada e seek de animação articulada, atualizando e renderizando suas partes.
- [header-mesh.h](../include/core_mbm/header-mesh.h): as estruturas articuladas v11
  representam identificação de partes, frame/subset, pai, nome, pivô, clipes,
  tracks e chaves de posição, rotação e escala, com parâmetros de interpolação.
- [mesh-manager.cpp](../src/core_mbm/mesh-manager.cpp): grava e lê
  `SECTION_ARTICULATED_PARTS` e `SECTION_ARTICULATED_ANIMATION`.
  `renderArticulatedStatic` percorre os subsets na ordem do frame; a profundidade
  também deve ser considerada na validação visual.
- Não há canal articulado específico de reordenação de subsets nas estruturas
  examinadas. O plano usa a ordem editável por frame, sem criar esse canal.

Referências de implementação e documentação a consultar durante a execução:

- [Sprite Maker](../editor/sprite_maker.lua): geometria, subsets, imagens e `.spt`.
- [Mesh Debug](../editor/mesh_debug.lua) e
  [editor articulado existente](../editor/skeletal_animation_editor.lua): montagem,
  pivôs, hierarquia, timeline, clipes e edição articulada; separar esse fluxo do skinning.
- Editor de física: localizar os controles de retângulo/triângulos e
  círculo/triângulos citados pelo usuário e reaproveitar suas convenções quando adequadas.
- [Animação articulada](articulated-animation.md),
  [formato v11](mesh-v11-format.md) e [API Lua](lua-api.md).

## Decisões de interface e fluxo

### Imagens e montagem

- Aceitar várias imagens de origem desde a primeira versão.
- Cada subset mantém referência à imagem de origem utilizada.
- Oferecer área da imagem original para seleção/recorte e área de montagem para
  posicionar peças, ajustar pivôs e animar.
- Incluir opção para mostrar/ocultar a imagem original.
- Selecionar um retângulo da imagem como região inicial de criação do recorte.
- Permitir mover as peças para suas posições iniciais, separando coordenadas da
  textura, geometria local, pivô e transformação na montagem.

### Recortes e triangulação

Dentro do retângulo selecionado, oferecer recorte por alfa ou formas base.
Permitir usar a forma diretamente ou combiná-la com o alfa: nesse caso, aproveitar
somente as regiões visíveis da imagem dentro da forma.

| Forma | Comportamento acordado |
|---|---|
| Retângulo | Começar com 2 triângulos; permitir aumentar ou reduzir a quantidade. |
| Círculo | Começar com 5 triângulos, mínimo de referência solicitado para essa forma. |
| Cápsula ("cilindro" 2D) | Segmento central e extremidades arredondadas; controlar segmento e extremidades independentemente ou de forma vinculada para preservar sua circularidade. |
| Rosca | Contorno externo e interno; interior sem geometria visível. Ajustar tamanho e deslocamento do contorno interno, inicialmente centralizado. |

- Tratar a quantidade de triângulos como orçamento aproximado, ajustável para mais
  ou para menos; mostrar o total efetivamente gerado.
- Priorizar a preservação do contorno em vez de prometer um total exato que destrua detalhes.
- No recorte por alfa com regiões desconectadas, oferecer:
  - um subset por região, opção padrão;
  - todas as regiões em um único subset.
- Oferecer **Preservar furos**, ativado inicialmente, para regiões transparentes internas.
- Incluir nesta versão a edição manual: mover, adicionar e remover pontos dos
  contornos, inclusive contornos internos, e editar os vértices da geometria.
- Editar o recorte preserva pivô, posição de montagem e animações da peça.
- Alterar parâmetros não descarta silenciosamente ajustes manuais. **Gerar novamente**
  é uma ação explícita que substitui a geometria, mantém pivô/montagem/animações e
  pode ser desfeita para recuperar o recorte anterior.

### Partes, hierarquia e frames

- Representar cada peça articulada como subset, com identidade estável, nome, pivô
  e referência ao pai, conforme o sistema existente.
- Trocar o pai preserva posição, rotação e escala visuais na pose inicial,
  recalculando a transformação relativa ao novo pai.
- Essa preservação foi acordada para a pose inicial; não pressupõe preservar toda
  a trajetória mundial das animações após mudar a hierarquia.
- Permitir reordenar subsets dentro de cada frame. Não adicionar chaves específicas
  para alterar a ordem dos subsets durante a reprodução.
- Importar `.spt` existentes e recuperar geometria e dados articulados presentes.
- Preservar todos os frames importados e editar um frame por vez, por meio de seletor.
- Oferecer duas ações de duplicação:
  - **Duplicar peça**: copiar recorte e montagem, sem copiar chaves;
  - **Duplicar com animação**: incluir as chaves da peça.
- Oferecer duas ações de exclusão, ambas reversíveis:
  - **Excluir peça**: manter filhos, vinculá-los ao pai da peça removida e preservar
    sua pose inicial visual; se não houver pai, torná-los raízes;
  - **Excluir hierarquia**: remover a peça e seus descendentes.

### Animação

- Seguir o editor articulado existente para timeline, interpolação, reprodução e
  propriedades dos clipes, adaptando os controles para o uso 2D.
- Permitir criar e editar animações nomeadas, visualizar a reprodução e percorrer
  a timeline, usando os canais já suportados pela engine.
- **Auto Key desligado inicialmente**: gravar chaves por ação explícita; quando
  ativado, alterações animáveis gravam chaves no instante selecionado.
- Separar edição da pose inicial de edição da pose de animação, para evitar que
  ajustes temporários sejam confundidos com alterações da montagem.
- Incluir **onion skin**, desligado inicialmente, mostrando poses anterior e
  seguinte com transparência.

### Persistência

- Salvar projeto editável e exportar `.spt` são operações distintas.
- O projeto conserva informações de autoria: imagens de origem, regiões de
  seleção, parâmetros de formas, configurações de alfa, contornos e ajustes
  manuais, orçamento de triângulos, montagem e animações.
- Oferecer **copiar imagens utilizadas para uma pasta junto do projeto**, ativado
  por padrão, com caminhos relativos.
- O `.spt` conserva geometria e animação articulada usando o formato existente.
  Não assumir que as imagens externas ou os parâmetros de autoria estejam todos
  embutidos no binário; verificar a política de texturas na implementação.
- Ao abrir apenas um `.spt`, recuperar os dados presentes. Não inventar parâmetros
  originais de cápsula, rosca ou recorte por alfa que não estejam no arquivo.
  A geometria importada deve continuar editável.

## Sequência proposta de implementação

### 1. Mapear o reaproveitamento e os contratos

- [ ] Ler as skills `new-editor-tool` e `engine-testing`; aplicar `doc-drift-check`
  para contratos de API/formato que forem usados ou alterados.
- [ ] Mapear APIs Lua existentes para criar geometria, editar partes, pivôs,
  hierarquia, clipes, tracks, chaves e importar/exportar `.spt`.
- [ ] Verificar remapeamento de identidades ao reordenar, duplicar e excluir subsets.
- [ ] Localizar controles de formas/triangulação no editor de física.
- [ ] Verificar acesso a pixels/alfa e disponibilidade de triangulação com furos e
  componentes desconectados; identificar as lacunas reais antes de criar APIs.
- [ ] Definir transformações entre imagem, peça, pivô, pai e montagem, incluindo
  como representar a montagem inicial no formato existente.

### 2. Estrutura do editor e projeto

- [ ] Criar editor Lua independente; nome sugerido: `editor/articulated_sprite_editor.lua`.
- [ ] Extrair modelo, geometria, histórico e persistência para módulos coesos,
  evitando o limite de 200 variáveis locais do Lua.
- [ ] Implementar imagens múltiplas, seletor de imagem, áreas de origem/montagem e
  mostrar/ocultar imagem original.
- [ ] Implementar projeto versionado, salvamento/carregamento e cópia opcional de
  imagens, tratando nomes iguais de arquivos de diretórios diferentes.
- [ ] Implementar desfazer/refazer para operações de autoria; agrupar um arraste
  em uma operação, em vez de produzir uma entrada por frame.

### 3. Geometria de recortes

- [ ] Implementar seleção retangular e as quatro formas parametrizadas.
- [ ] Implementar contorno por alfa, separação/agrupamento de regiões e furos.
- [ ] Implementar interseção da forma com alfa, simplificação e triangulação.
- [ ] Implementar orçamento ajustável, total gerado e edição manual.
- [ ] Implementar regeneração explícita e reversível, preservando dados articulados.
- [ ] Validar entradas degeneradas e contornos inválidos, mantendo a última
  geometria válida quando a operação não puder ser concluída.

### 4. Montagem articulada

- [ ] Implementar seleção, movimento, rotação, escala, pivôs e parentesco.
- [ ] Preservar pose inicial visual ao trocar pai ou excluir peça sem descendentes.
- [ ] Implementar ordem de subsets, edição de um frame por vez, duplicações e exclusões.
- [ ] Garantir identidades novas nas duplicações e referências consistentes nas animações.

### 5. Timeline e prévia

- [ ] Reaproveitar comportamento de clipes, chaves e interpolação do editor existente.
- [ ] Implementar gravação explícita e Auto Key inicialmente desligado.
- [ ] Implementar reprodução, pausa, seek e onion skin inicialmente desligado.
- [ ] Verificar que a prévia corresponde à reprodução articulada exportada.

### 6. Binário, integração e documentação

- [ ] Importar `.spt` com múltiplos frames e dados articulados existentes.
- [ ] Exportar e reabrir `.spt`, preservando partes, pivôs, pais, clipes e chaves.
- [ ] Registrar no menu de desenvolvimento de Linux, macOS e Windows, conferindo
  também outros pontos de distribuição/listagem de editores aplicáveis.
- [ ] Adicionar textos em inglês e português em `editor/lang/language.lua`.
- [ ] Manter pontuação ASCII nos textos exibidos pelo ImGui.
- [ ] Documentar uso e limitações efetivamente entregues; atualizar referências
  de API/formato somente quando a implementação exigir.
- [ ] Atualizar `MBM_VERSION` ao entregar a funcionalidade, conforme o cabeçalho de versão.

## Validação e critérios de aceite

- [ ] Compilar os scripts com `loadfile()` usando o Lua fornecido pelo projeto.
- [ ] Exercitar geometria com furos, regiões desconectadas, cápsulas, roscas
  deslocadas, interseção com alfa e diferentes orçamentos de triângulos.
- [ ] Verificar pivô e montagem estáveis após editar/regenerar recorte e após desfazer.
- [ ] Testar troca de pai, duplicação, exclusão e reordenação sem referências quebradas.
- [ ] Testar Auto Key ligado/desligado e onion skin sem modificar as chaves existentes.
- [ ] Salvar e reabrir projeto com imagens de nomes iguais em diretórios diferentes;
  mover uma cópia do projeto com seus recursos e verificar caminhos relativos.
- [ ] Importar um `.spt` de múltiplos frames, editar um frame e verificar os demais.
- [ ] Exportar um personagem articulado e reproduzi-lo em um script independente,
  comparando pivôs, hierarquia, interpolação e sobreposição com a prévia do editor.
- [ ] Validar o fluxo visual com `boy-parts.png`, se disponível, e imagens sintéticas
  com alfa conhecido para não depender das características dessa imagem.
- [ ] Executar o editor real no Linux debug conforme `engine-testing`, com prazo
  de saída e diagnóstico de erros; verificar inclusão no menu de desenvolvimento.
- [ ] Auditar todos os caminhos por frame: carregamento de imagens, contornos,
  triangulação, serialização, uploads e reconstruções somente quando necessário.
  Onion skin deve atualizar quando suas entradas mudarem; editor parado não deve
  executar continuamente seek, scans completos ou recriação de geometria.
- [ ] Verificar ordenação/transparência nos backends disponíveis e registrar quais
  plataformas foram efetivamente testadas, sem presumir paridade não validada.

## Pontos técnicos a resolver durante a implementação

Estes itens não reabrem as decisões de produto; são verificações e escolhas de
implementação que dependem do código e dos testes:

- Algoritmo de triangulação/simplificação capaz de preservar furos e componentes.
- Representação dos parâmetros de formas e dos ajustes manuais no projeto.
- Política de texturas do `.spt` e exportação dos recursos necessários ao jogo.
- Preservação/remapeamento das referências articuladas ao editar frames/subsets.
- Comportamento em limites inválidos, como furo cruzando o contorno externo;
  evitar geometria inválida e fornecer diagnóstico claro.
- Reaproveitamento de helpers do editor existente sem acoplar o novo fluxo ao skinning.

Não adicionar automaticamente novos recursos de animação ou mudanças de formato
para contornar essas verificações. Priorizar os contratos existentes e registrar
qualquer limitação concreta encontrada antes de ampliar o escopo.
