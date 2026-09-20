# Image Mesh Editor

Editor offline para extrudar regiões de uma imagem em módulos 3D com relevo.
Disponível a partir da versão 7.217.0; interface reorganizada na 7.218.0 e controles de diagnóstico na 7.219.0 e adição de formas na 7.220.0. O estado atual corresponde às etapas 1 e 2
[do plano](image-mesh-editor-plan.md).

## Abrir

Escolha **Image Mesh Editor / Editor de Mesh por Imagem** no launcher ou execute,
a partir da raiz do repositório:

```sh
bin/debug/linux_x86/mini-mbm --scene editor/image_mesh_editor.lua --disable_select_monitor --nosplash -w 1440 -h 900
```

Requer build com Lua e ImGui. O comando normal não possui prazo de encerramento.
Linux/OpenGL ES foi validado; integração de launcher para Windows/macOS também
está presente, mas não foi executada nessas plataformas.

## Criar e editar peças

1. Use **Arquivo > Abrir imagem**. O projeto usa uma imagem de até 16 megapixels.
2. Na lateral **Regiões**, mantenha **Modo de edição** marcado e escolha **Retângulo**, **Elipse / círculo** ou **Polígono**.
   Arraste para criar retângulos/elipses; clique nos pontos e use **Concluir
   polígono** para fechar uma forma. Igualar largura e altura do recorte seleciona
   um círculo na imagem; **Preservar proporção da imagem**, ativo por padrão,
   mantém essa proporção no volume. Polígonos côncavos são aceitos; cruzamentos, auto-contato e furos não.
3. Para várias peças regulares, abra **Criar regiões em grade** e informe colunas,
   linhas, margens simétricas e espaçamento em pixels. A grade adiciona regiões,
   sem substituir as existentes. Ajuste recortes individuais se o atlas for irregular.
4. Use **Selecionar / mover** para arrastar uma peça; arraste seu canto inferior
   direito para redimensionar um retângulo/elipse. Em polígonos, arraste os pontos:
   o recorte cresce ao ultrapassar sua borda, até o limite da imagem, mantendo
   os demais pontos nas mesmas posições.
   A própria lateral **Regiões** mostra as propriedades da seleção. Abra
   **Recorte e contorno na imagem** para editar recorte, forma e coordenadas
   normalizadas dos pontos, além de inserir/remover pontos. Confirme com **Aplicar**.
5. Selecione várias peças com Ctrl+clique. **Duplicar** e **Excluir** operam sobre
   a seleção. Os parâmetros de geração em **Aplicar** afetam todas as selecionadas;
   nome, recorte, forma e pontos afetam somente a peça principal.
6. **Editar padrões do projeto** altera os valores herdados. Peças podem ter
   sobrescritas próprias; **Usar padrões do projeto** limpa as sobrescritas da seleção.

### Adicionar formas pelo painel

Em **Modo de edição**, use **Adicionar forma** na lateral Regiões:

1. Escolha o tipo no combo: **Retângulo**, **Círculo**, **Elipse**, **Triângulo**
   ou **Polígono regular** (3 a 32 lados).
2. Informe largura/altura em pixels, ou o diâmetro para o círculo.
3. Clique em **Adicionar forma**. A região nasce perto do centro da área visível,
   limitada à imagem, e fica selecionada com a ferramenta Selecionar / mover.
4. Arraste para mover. Retângulos/elipses têm alça de tamanho no canto; polígonos
   têm pontos editáveis. Em **Recorte e contorno na imagem**, ajuste X/Y e
   largura/altura numericamente e confirme com **Aplicar**.
5. Desative o modo de edição para gerar e inspecionar a extrusão.

Todos esses tipos são extrudáveis agora. Círculos são elipses inicialmente com
largura/altura iguais, inclusive nas dimensões de mundo; triângulos e polígonos
regulares são contornos editáveis. Edições posteriores podem deformar essas
formas (por exemplo, transformar um círculo em elipse). A criação participa do
histórico, salvamento e exportação como qualquer região desenhada.

O projeto suporta até 256 regiões e guarda as últimas 40 operações para
**Desfazer/Refazer** (Ctrl+Z/Ctrl+Y). Um arraste confirmado é uma operação. Escape
cancela um contorno ainda em desenho ou um arraste em andamento.

## Cena principal e modos

A janela **Regiões** fica à esquerda, reunindo ferramentas, lista e propriedades.
À direita ficam **Câmera 3D / 2D** e **Luz**.
Ao selecionar outra peça, suas propriedades aparecem nessa lateral. **Volume e
relevo** contém os ajustes principais; **Resolução e limites de geometria** agrupa
os parâmetros de densidade e orçamento.

- **Modo de edição marcado:** a cena mostra a imagem e os contornos, renderizados
  por objetos `texture` e `line` em **2dw**, centrados na origem do mundo.
  Na ferramenta **Selecionar / mover**, arrastar com o botão esquerdo sobre
  uma forma move a forma; arrastar em espaço vazio (dentro ou fora da imagem)
  move a câmera 2D. Alças têm prioridade para editar tamanho/pontos. A ação é
  escolhida ao pressionar e mantida até soltar. As ferramentas de desenho
  continuam criando formas. Botão direito ou central também move a câmera. A ferramenta **Mover câmera** permite esse arraste com o botão
  esquerdo. A roda ajusta o zoom da imagem e dos contornos mantendo o ponto sob
  o cursor fixo na tela, inclusive após mover a câmera. As coordenadas originais
  dos recortes são preservadas.
  **Enquadrar imagem** restaura zoom e posição. O canto inferior direito e os
  pontos do polígono são alças de edição. As alças têm 16 a 40 pixels de lado,
  crescem com o zoom e aceitam cliques até 6 pixels além da borda. O redimensionamento
  preserva a distância inicial do clique para evitar saltos.
- **Modo de edição desmarcado:** a cena mostra somente a mesh 3D selecionada.
  Arraste com o botão esquerdo para orbitar e use a roda para ajustar a distância.
  Ative **Wireframe (arestas dos triângulos)** no painel **Regiões** para mostrar
  somente as arestas da mesh, incluindo frente, verso e laterais. Desmarque para
  voltar à superfície com textura. É uma opção de inspeção: não altera o projeto
  nem a exportação. As linhas ficam em cache até a geometria mudar.
  A luz fica na janela à direita. Selecionar outra região atualiza a mesh mostrada.

A troca de modo cancela desenhos e arrastes ainda não confirmados. Valores nos
campos precisam de **Aplicar** antes da troca. Cliques na interface não iniciam
manipulação da cena. A área livre abaixo da câmera à direita também recebe
cliques e arrastes quando a janela de luz está oculta. Desde 7.220.2, a seleção
converte a entrada do mouse para pixels da tela e trata separadamente as escalas
X/Y, mantendo as alças alinhadas ao redimensionar/escalar a janela.
Não existem janelas separadas de imagem, propriedades ou
prévia, nem render target intermediário para a mesh.

## Controles de diagnóstico

**Câmera 3D / 2D** acompanha o modo do editor: seus botões também alternam entre
edição 2D e visualização 3D. No modo 2D, edite X/Y, ajuste o zoom ou use
**Resetar visão** para enquadrar a imagem. Em 3D, use o gizmo de órbita compartilhado
com Mesh Debug, distância, posição/centro da órbita e reset. A posição numérica
representa a órbita antes do deslocamento paralelo usado para centrar a mesh
entre as laterais.

**Luz** aparece somente na visualização 3D: habilitar/desabilitar, cor ambiente,
cor da luz e direção pelo gizmo ou vetor numérico. **Resetar luz** restaura os
padrões 3D da engine. A imagem de entrada em 2dw permanece sem iluminação;
não há controles de luz 2D. Esses ajustes são de inspeção, não alteram alturas,
não regeneram a mesh e não são salvos no projeto ou exportados.

Para investigar relevo, reduza a luz ambiente e varie a direção da luz em 3D,
além de orbitar a câmera. Isso ajuda a distinguir sombras da iluminação de marcas
já pintadas na textura; não comprova por si só que a amostragem de altura está correta.

## Detectar sulcos e acompanhar as formas

Na seleção, abra **Relevo e sulcos**. Os controles são salvos por módulo e podem
ser herdados dos padrões do projeto. Projetos antigos mantêm a geração anterior
até habilitar **Seguir formas da imagem (adaptativo)**.

1. Em edição 2D, escolha **Sulcos detectados (azul)** ou **Mapa de alturas** em
   **Visualização da imagem**. O mapa aparece sobre o recorte selecionado na cena
   da engine; o restante da imagem continua visível e as alças ficam por cima.
2. Ajuste **Limiar dos sulcos**. Intensidades abaixo dele, após inversão e
   suavização, são interpretadas como regiões baixas. **Inverter** troca a
   interpretação claro/escuro. Azul representa a classificação, não furos.
3. No **Mapa de alturas**, use **Duas alturas (barras / sulcos)** para nivelar partes elevadas e fundos,
   com uma rampa controlada pela **Largura da transição**. A amplitude **Relevo**
   define a diferença de profundidade no mundo. O mapa de alturas mostra valores
   normalizados, incluindo a atenuação de borda, antes dessa amplitude.
4. **Suavização preservando bordas** aplica de zero a quatro passes locais para
   reduzir manchas. Ela não separa automaticamente sombras de profundidade real.
5. Clique **Pré-visualizar ajustes** após mudar os controles para conferir o
   rascunho, sem modificar o projeto. **Aplicar** confirma os parâmetros; use
   **Salvar projeto** para gravá-los em disco. **Imagem original** remove o mapa.
6. Volte para **Imagem original** ou para o modo 3D para ativar a geometria
   adaptativa e ajustar sua tolerância; aplique. Use wireframe para
   inspecionar as arestas criadas ao longo das transições.

O combo aparece antes dos ajustes de **Relevo e sulcos**. Nesse grupo, os
controles sem efeito na prévia selecionada ficam ocultos, preservando seus valores:

| Ajuste | Mapa de alturas | Sulcos detectados (azul) |
|---|---|---|
| Inversão e suavização | Sim | Sim |
| Limiar dos sulcos | Com **Duas alturas** | Sim |
| Duas alturas | Sim | Não |
| Largura da transição | Com **Duas alturas** | Não |
| Geometria adaptativa e tolerância | Só na mesh 3D | Só na mesh 3D |

Em **Imagem original**, no modo 3D ou ao editar os padrões, os controles de geração
voltam a aparecer conforme suas dependências. A tolerância exige geometria adaptativa;
o limiar também orienta o alinhamento das arestas adaptativas mesmo sem **Duas alturas**.
A imagem original não é processada. Os grupos de volume e resolução continuam sendo
configurações de geração, independentemente da prévia selecionada.

A geometria adaptativa amostra o erro de interpolação do relevo, refina onde é
necessário, melhora a forma dos triângulos e insere arestas nas transições da
imagem processada. O verso plano usa só vértices da borda. **Tolerância de erro
do relevo** é uma fração da amplitude e orienta o refinamento; **Colunas/Linhas**
limitam a densidade e a amostragem. Não é uma garantia de erro máximo em cada pixel:
detalhes menores que a escala de amostragem podem exigir maior resolução.

O mapa é temporário, fica em cache e não altera a textura original ou a exportada.
Zoom e pan reposicionam o mapa sem refazer processamento. Arrastes ocultam o mapa
antigo; após confirmar, ele é atualizado. Trocar módulo ou vista também atualiza
o cache. A prévia 2D continua funcionando quando o orçamento impede gerar a mesh.
A máscara ainda não tem pintura manual; pincéis e furos reais permanecem etapas futuras.

No teste com o módulo 2 do projeto de exemplo, a configuração adaptativa com
limiar 0,45, transição 0,12, suavização 2 e densidade 48 x 48 produziu 13.586
triângulos; a configuração anterior salva (50 x 50) produzia 31.392. A redução e
a aparência dependem da imagem e dos parâmetros, não são metas fixas do gerador.

## Relevo e prévia

**Preservar proporção da imagem** calcula a altura no mundo a partir da largura
usando `(h - 1) / (w - 1)` do recorte (intervalos mínimos de um pixel). Assim,
o contorno 3D mantém a proporção do desenho 2D. Isso também vale ao abrir projetos
antigos sem essa opção; desmarque-a e aplique para usar largura/altura independentes.
Espessura, amplitude de relevo, resolução, inversão, transição da borda e orçamento
de geometria continuam independentes. Círculos/elipses começam com triângulos
radiais a partir do centro e recebem subdivisões para amostrar o relevo. A frente aponta
para -Z; a origem fica no centro do volume básico. A altura vem da luminosidade,
que também contém sombras/manchas: a ferramenta não reconstrói semanticamente o
objeto da imagem.

Em **Resolução e limites de geometria**, **Orçamento de vértices** permite impor
um teto menor para módulos leves, até o limite fixo de 65.535 da engine. Ele não
é uma contagem alvo nem aumenta a capacidade dos índices de 16 bits. O orçamento
de triângulos é calculado automaticamente como `2 * maxVertices`: um teto
conservador para as formas fechadas e sem furos deste gerador, não uma previsão
da contagem final. Valores antigos de `maxTriangles` no projeto deixam de limitar
a geração no editor; a API direta ainda aceita esse orçamento independente.

Se a geração exceder um limite, o alto do painel **Regiões** informa o módulo,
o recurso excedido, a quantidade mínima já necessária e seu limite. As contagens
incluem frente, verso e laterais; não são totais finais quando o refinamento foi
interrompido. Reduza **Colunas/Linhas** ou **Segmentos da elipse**; um orçamento
menor de vértices pode ser aumentado até 65.535.

O alto do painel **Regiões** mostra as faces triangulares do módulo selecionado
em formato compacto, por exemplo `4K` ou `4.2K` (`K = 1000`), na edição 2D e na
visualização 3D. O tooltip mostra os totais exatos de vértices e triângulos,
incluindo frente, verso e laterais. Durante um arraste a contagem fica pendente;
ao confirmar a alteração ela é recalculada. Uma geração inválida mostra seu erro
em vez de manter uma contagem antiga.

Ao aplicar alterações ao mesmo módulo, o editor preserva órbita, distância e
foco da câmera 3D, inclusive após undo/redo ou recuperação de erro. O enquadramento
automático ocorre ao visualizar outro módulo ou abrir um projeto. **Resetar visão**
no painel da câmera continua disponível para reenquadrar conforme o tamanho atual.

A prévia permite orbitar com arraste, ajustar distância
com a roda e intensidade da luz direcional. Modificações nas propriedades só
entram no projeto após **Aplicar**. A prévia fica indisponível se a geração falhar,
com o motivo apresentado; ela não mantém uma peça antiga como se fosse atual.

O editor não regenera nem serializa a malha a cada frame. Contornos desenhados
são armazenados em cache e os buffers de linhas só são reconstruídos quando suas
entradas mudam. No modo de edição, a contagem exata é calculada pela geração CPU
uma vez por módulo consultado; os relatórios ficam em cache até uma alteração
confirmada no projeto, incluindo undo/redo. Não há exportação ou upload de mesh
3D para contar faces. A prévia GPU fica pendente até entrar na visualização.
Nesse modo, alterações confirmadas ou seleção disparam geração;
orbitar ou ajustar a luz não regenera a mesh. A cena desenha normalmente a cada
frame, sem reconstruir geometria em repouso. A geração de uma peça ainda é síncrona.

## Salvar, relocalizar e exportar

**Salvar projeto** (Ctrl+S) grava `.imesh`, contendo versão, imagem, regiões e
parâmetros. Ao concluir, uma mensagem temporária de quatro segundos confirma o
salvamento e mostra o nome do arquivo. Salvar novamente reinicia sua duração. É um arquivo Lua de dados seguindo o padrão dos editores, carregado
com ambiente vazio e validado. O histórico de desfazer e recursos GPU não são
salvos. Imagens sob a pasta do projeto usam referência relativa; outras mantêm
o caminho recebido pelo diálogo. Ao reabrir com imagem ausente, **Localizar
imagem** permite escolher uma substituta com as mesmas dimensões.

**Exportar mesh selecionada** escreve uma `.msh` v11. **Exportar todas as meshes**
usará a pasta escolhida e nomes com ID da região, por exemplo `001_module_001.msh`.
O lote processa uma peça por frame e pode ser cancelado entre peças. Falhas são
relatadas, arquivos existentes são preservados e os já exportados permanecem.

**As malhas ainda referenciam a imagem original; a textura não é empacotada.**
Mantenha a imagem acessível nos caminhos de assets ao carregar as malhas. Frente
e fundo usam o recorte original. As laterais esticam a borda quando ela é opaca.
Desde 7.220.1, segmentos da lateral que atravessam transparência usam um pixel
próximo de maior opacidade dentro do recorte, evitando paredes invisíveis quando
existem pixels opacos na imagem. Frente/fundo mantêm a transparência original.
Meshes exportadas anteriormente precisam ser geradas/exportadas novamente para
receber a correção. Pintura de altura,
furos, mapas separados e opções adicionais de textura pertencem às próximas etapas.

A API de geração e seus limites estão em [Lua API](lua-api.md#image-based-mesh-generation).

## Verificação reproduzível

```sh
bin/debug/linux_x86/lua-5.4.1.exe src/test-lib/image_mesh_model_test.lua
mkdir -p /tmp/image-mesh-stage2-export
timeout -s KILL 20 bin/debug/linux_x86/mini-mbm --scene src/test-lib/image_mesh_smoke.lua --disable_select_monitor --nosplash -w 640 -h 480
timeout -s KILL 25 bin/debug/linux_x86/mini-mbm --scene src/test-lib/image_mesh_editor_smoke.lua --disable_select_monitor --nosplash -w 1440 -h 900
```

Use um build com `-DUSE_TEXTURE_MISSING_DIALOG=0` para testes automáticos. Confira
os marcadores `IMAGE MESH ... OK` e ausência de erros Lua; o código de saída da
engine sozinho não comprova sucesso. O teste do editor deve produzir os sete
marcadores de projeto, UI/lote/repouso, gestos de entrada, modos da cena, controles de câmera/luz e primitivas/extrusão/luz 2D desativada e alça ampliada sem salto. Os gestos
são simulados nos callbacks `onTouch*` da engine, passando pelo canvas real; isso
não substitui uma conferência manual com mouse físico.

A referência visual dos 12 painéis pode ser reproduzida com:

```sh
MBM_IMAGE_MESH_SOURCE=/home/michel/Downloads/mesh-tile-experiment/f1-c002ae9c-img01.png \
timeout -s KILL 25 bin/debug/linux_x86/mini-mbm --scene src/test-lib/image_mesh_wall_smoke.lua --disable_select_monitor --nosplash -w 1200 -h 800
```

Esse fixture exige a imagem de referência 1344 x 768; os demais testes criam seus
próprios dados. A parede fica visível por 10 segundos, exporta os 12 módulos e salva
`/tmp/image-mesh-stage2-export/reference.imesh`, que pode ser aberto no editor.


Teste de coordenadas de entrada (escala 1x, 2x, escalas X/Y distintas e faixa direita):

```sh
timeout -s KILL 15 bin/debug/linux_x86/mini-mbm --scene src/test-lib/image_mesh_input_scale_smoke.lua --disable_select_monitor --nosplash -w 1440 -h 900
```

O teste cria seus dados por padrão. Para reproduzir um projeto salvo, defina
`MBM_IMAGE_MESH_PROJECT=/caminho/project.imesh`; o arquivo é somente lido.
O marcador esperado é `IMAGE MESH INPUT SCALE / RIGHT COLUMN / PROJECT / LEFT DRAG OK`.
