# Editor de Pintura de Textura sobre Mesh 3D

Este documento descreve o comportamento implementado pelo editor
`editor/mesh_texture_paint_editor.lua`.

## Objetivo e escopo

O editor permite pintar uma textura UV de uma mesh 3D sem modificar a geometria
original. A textura de origem permanece somente leitura durante a sessão. As
alterações são mantidas em uma camada RGBA transparente, exibidas em um preview
privado e incorporadas a uma nova textura durante a exportação.

O editor trabalha com o frame 1, o canal UV 0 e a textura primária baseada em
arquivo de cada material. A mesh precisa possuir UVs válidos e pelo menos uma
textura primária editável.

O editor está disponível no launcher Lua de Linux, macOS e Windows (projeto de
desenvolvimento), além de poder ser aberto diretamente pelo script Lua.

## Abertura e seleção do alvo

Ao abrir uma MSH, o editor:

- valida a mesh e os seus subsets do frame 1;
- extrai posições, índices, UVs e triângulos de cada subset;
- agrupa os subsets pela textura primária normalizada;
- disponibiliza a seleção da textura no painel de controles;
- mantém a lista de subsets que usam a textura selecionada;
- constrói as ilhas UV, as bordas das ilhas e a contagem de sobreposições UV;
- cria uma cópia privada da mesh para o preview;
- carrega a textura selecionada sem alterar o arquivo original.

O painel pode limitar picking, pintura e overlay UV ao subset selecionado e também
pode limitar essas operações a uma ilha UV selecionada. `Ctrl` + clique direito
seleciona a ilha UV sob o cursor sem iniciar uma pintura.

## Modos de pintura

### Paint

Pinta a cor RGBA configurada sobre a camada transparente usando coordenadas UV.
O raio, a dureza e a opacidade são configuráveis. A forma do dab pode ser
circular ou quadrada.

No dab quadrado, a cobertura é amostrada no footprint projetado sobre a view e
depois convertida para UV. A máscara recorta a área que não pertence à geometria,
evitando que uma única diagonal da triangulação produza um polígono irregular ou
lacunas dentro do quadrado.

### Erase

Remove somente a contribuição da camada de pintura. A textura base reaparece no
preview e permanece inalterada no arquivo de origem.

### Pick color

O clique direito sobre uma superfície visível captura a cor composta naquele
ponto para uso posterior no modo `Paint`.

### Project/Stencil

Projeta a cor configurada pela câmera sobre a superfície visível. O modo usa a
forma circular ou quadrada selecionada, respeita oclusão e permite `Paint
through` para aplicar a contribuição em todas as superfícies atingidas pelo raio
da câmera.

### Surface 3D

Consulta triângulos próximos ao ponto atingido em espaço do mundo e constrói
samples em UV usando uma base tangente da superfície. O método acompanha a
curvatura e pode atravessar seams UV quando os triângulos pertencem à mesma
região espacial. A forma circular usa distância radial; a forma quadrada usa a
métrica quadrada na base tangente.

### Stamp

O modo `Stamp` possui duas fontes:

- trecho capturado da textura composta atual com clique direito;
- imagem raster externa carregada pelo painel.

Para uma imagem externa, o editor abre uma janela centralizada que concentra a
imagem e os controles. Enquanto essa janela está aberta, mouse, zoom e teclado
não são encaminhados para a view 3D.

A seleção externa oferece:

- forma quadrada ou circular;
- seleção por arrasto sobre a imagem;
- campos editáveis de X, Y, largura e altura;
- preview da seleção sobre checkerboard e alpha;
- confirmação por `Usar seleção`;
- cancelamento por `Cancelar` ou `Esc`.

A seleção é copiada para um buffer RGBA privado. A imagem externa não é alterada.
O carimbo preserva alpha, proporção e orientação. Escala e rotação são aplicadas
ao redor do centro do recorte e são independentes do raio do dab de destino.
Imagens externas preservam os pixels nativos do recorte. Para carimbos capturados
da view, o limite padrão é 257 amostras por lado e o limite configurável é 513
amostras por lado; a dimensão também é limitada pelo diâmetro do pincel.

## Controles de autoria

O painel de controles oferece:

- textura alvo;
- subset e ilha UV;
- modo de pintura;
- forma circular ou quadrada;
- raio;
- dureza;
- opacidade;
- cor RGBA;
- resolução da captura do carimbo da view;
- escala e rotação do carimbo;
- `Paint through`;
- limite por subset;
- limite por ilha UV;
- padding de seams;
- undo e redo.

O padding de seams usa uma edição adiada. Durante o arrasto, somente o valor
pendente muda. A dilatação, a composição e o rebuild do preview ocorrem quando o
slider é desativado após a edição.

## Viewport e preview 2D

O viewport 3D permite orbitar com o botão esquerdo, deslocar com o botão central
e aproximar ou afastar com zoom. O botão direito seleciona a origem do carimbo ou
inicia uma pintura conforme o modo ativo.

O painel de textura exibe a composição atual da textura e pode desenhar:

- triângulos UV da seleção;
- bordas das ilhas UV;
- limites das ilhas filtradas;
- indicadores de UVs sobrepostos.

O desenho do overlay UV possui limites de triângulos e bordas para controlar o
custo do ImGui. A mesh do preview e as texturas privadas são reconstruídas apenas
quando uma alteração exige atualização.

## Dados de pintura e desempenho

O rasterizador opera em buffers RGBA8 e mantém tiles sujos para composição,
undo/redo e atualização do preview. A pintura é processada em lotes cooperativos
quando necessário. Um stroke cancelado restaura os tiles capturados antes do
primeiro pixel alterado.

O histórico armazena snapshots dos tiles tocados por cada stroke. Operações de
câmera, seleção de janela e controles que não alteram pixels não entram no
histórico de pintura.

## Projetos e exportação

Um projeto `.meshpaint.lua` é acompanhado por uma camada `.paint.png`. O projeto
versionado armazena:

- mesh de origem;
- textura de origem;
- camada de pintura;
- textura exportada, quando aplicável;
- frame, subset, papel da textura e canal UV;
- modo de projeção;
- padding de seams;
- forma do pincel;
- filtros por subset e ilha UV.

Os caminhos gravados no projeto são absolutos. A abertura valida a versão, os
caminhos, as dimensões da camada, a mesh, a textura, o subset e a ilha UV antes
de substituir a sessão aberta.

A imagem externa, o recorte do carimbo, a escala e a rotação do carimbo não fazem
parte do arquivo do projeto. A fonte externa é liberada ao trocar de mesh,
textura ou projeto.

### Salvar textura

`Salvar textura` grava a composição da textura base com a camada de pintura em um
novo PNG. O arquivo de origem é protegido contra sobrescrita acidental.

### Salvar mesh como

`Salvar mesh como` cria uma cópia da MSH e um PNG com o mesmo nome-base. A cópia
recebe a nova referência de textura nos subsets que usam o alvo selecionado. A
mesh de origem, seus vértices, índices, UVs, animações e demais referências não
editadas permanecem preservados.

### Publicação de arquivos

As gravações preparam arquivos temporários no diretório de destino, publicam os
arquivos e mantêm backups durante a operação. Falhas reportadas durante a
publicação acionam restauração dos arquivos anteriores. Se a restauração também
falhar, o caminho do backup é retornado no erro. O mecanismo não representa uma
transação atômica contra queda de processo ou energia entre múltiplos arquivos.

## Módulos Lua

- `editor/mesh_texture_paint_editor.lua`: cena, callbacks, menus e janelas;
- `editor/mesh_texture_paint_model.lua`: snapshots de mesh, subsets, ilhas e
  sobreposições UV;
- `editor/mesh_texture_paint_pick.lua`: raios, interseção, barycentric, UV, BVH e
  samples de superfície;
- `editor/mesh_texture_paint_raster.lua`: buffers, tiles, dabs, blend, composição,
  dilatação, undo e carimbos;
- `editor/mesh_texture_paint_stamp.lua`: seleção, campos numéricos e recorte
  circular ou quadrado de fontes externas;
- `editor/mesh_texture_paint_layout.lua`: cálculo de layout da janela e da imagem
  usando as dimensões reais do backbuffer;
- `editor/mesh_texture_paint_project.lua`: validação, leitura e escrita do
  formato `.meshpaint.lua`;
- `editor/mesh_texture_paint_io.lua`: publicação, backups e restauração de
  arquivos.

## Limitações do editor

O estado implementado não oferece:

- edição de vértices, normais ou ossos;
- criação, desemaranhamento ou correção automática de UVs;
- pintura de normal, roughness, specular, emissive ou máscaras de material;
- múltiplas camadas independentes;
- clonagem, smudge, blur, preenchimento ou simetria;
- pressão ou inclinação de tablet;
- seleção de frames diferentes do frame 1;
- canais UV diferentes do canal 0;
- restauração automática de uma imagem externa usada como carimbo;
- garantia de equivalência visual para materiais ou shaders que não sejam
  representados pelo preview do editor.

Meshes sem textura primária baseada em arquivo ou sem UVs válidos são rejeitadas.
Texturas compartilhadas continuam sendo editadas como um alvo comum, portanto a
alteração afeta todos os subsets que usam a textura selecionada.

## Testes disponíveis

Os testes Lua relacionados ao editor estão em `src/test-lib/`:

- `mesh_texture_paint_io_test.lua`;
- `mesh_texture_paint_layout_test.lua`;
- `mesh_texture_paint_model_test.lua`;
- `mesh_texture_paint_pick_test.lua`;
- `mesh_texture_paint_project_test.lua`;
- `mesh_texture_paint_raster_test.lua`;
- `mesh_texture_paint_stamp_test.lua`.

Os smoke tests de preview, salvamento e mesh estão em:

- `mesh_texture_paint_preview_smoke.lua`;
- `mesh_texture_paint_save_checks.lua`;
- `mesh_texture_paint_mesh_smoke.lua`.

O benchmark do rasterizador está em `mesh_texture_paint_raster_benchmark.lua`.
