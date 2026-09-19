# Image Mesh Editor

Editor offline para extrudar regiões de uma imagem em módulos 3D com relevo.
Disponível a partir da versão 7.217.0. O estado atual corresponde às etapas 1 e 2
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
2. Na área da imagem, escolha **Retângulo**, **Elipse / círculo** ou **Polígono**.
   Arraste para criar retângulos/elipses; clique nos pontos e use **Concluir
   polígono** para fechar uma forma. Igualar largura e altura do recorte seleciona
   um círculo na imagem; iguale também as dimensões no mundo para gerar um volume
   circular. Polígonos côncavos são aceitos; cruzamentos, auto-contato e furos não.
3. Para várias peças regulares, abra **Criar regiões em grade** e informe colunas,
   linhas, margens simétricas e espaçamento em pixels. A grade adiciona regiões,
   sem substituir as existentes. Ajuste recortes individuais se o atlas for irregular.
4. Use **Selecionar / mover** para arrastar uma peça; arraste seu canto inferior
   direito para redimensionar um retângulo/elipse. Em polígonos, arraste os pontos.
   A área **Propriedades** permite editar recorte, forma e coordenadas normalizadas
   dos pontos, além de inserir/remover pontos. Confirme com **Aplicar**.
5. Selecione várias peças com Ctrl+clique. **Duplicar** e **Excluir** operam sobre
   a seleção. Os parâmetros de geração em **Aplicar** afetam todas as selecionadas;
   nome, recorte, forma e pontos afetam somente a peça principal.
6. **Editar padrões do projeto** altera os valores herdados. Peças podem ter
   sobrescritas próprias; **Usar padrões do projeto** limpa as sobrescritas da seleção.

O projeto suporta até 256 regiões e guarda as últimas 40 operações para
**Desfazer/Refazer** (Ctrl+Z/Ctrl+Y). Um arraste confirmado é uma operação. Escape
cancela um contorno ainda em desenho ou um arraste em andamento.

## Relevo e prévia

Largura/altura no mundo, espessura, amplitude de relevo, resolução, inversão,
transição da borda e orçamento de geometria são independentes. A frente aponta
para -Z; a origem fica no centro do volume básico. A altura vem da luminosidade,
que também contém sombras/manchas: a ferramenta não reconstrói semanticamente o
objeto da imagem.

A prévia mostra contagens finais, permite orbitar com arraste, ajustar distância
com a roda e intensidade da luz direcional. Modificações nas propriedades só
entram no projeto após **Aplicar**. A prévia fica indisponível se a geração falhar,
com o motivo apresentado; ela não mantém uma peça antiga como se fosse atual.

O editor não regenera nem serializa a malha a cada frame. Contornos desenhados
são armazenados em cache; alterações confirmadas ou seleção disparam geração.
A textura da prévia é atualizada quando geometria, câmera ou luz mudam e é
reutilizada em repouso. A geração de uma peça ainda é síncrona.

## Salvar, relocalizar e exportar

**Salvar projeto** (Ctrl+S) grava `.imesh`, contendo versão, imagem, regiões e
parâmetros. É um arquivo Lua de dados seguindo o padrão dos editores, carregado
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
e fundo usam o recorte original, e as laterais esticam a borda. Pintura de altura,
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
engine sozinho não comprova sucesso. O teste do editor deve produzir os três
marcadores de projeto, UI/lote/repouso e gestos de entrada. Os gestos são simulados
nas funções de entrada ImGui, passando pelo canvas real.

A referência visual dos 12 painéis pode ser reproduzida com:

```sh
MBM_IMAGE_MESH_SOURCE=/home/michel/Downloads/mesh-tile-experiment/f1-c002ae9c-img01.png \
timeout -s KILL 25 bin/debug/linux_x86/mini-mbm --scene src/test-lib/image_mesh_wall_smoke.lua --disable_select_monitor --nosplash -w 1200 -h 800
```

Esse fixture exige a imagem de referência 1344 x 768; os demais testes criam seus
próprios dados. A parede fica visível por 10 segundos, exporta os 12 módulos e salva
`/tmp/image-mesh-stage2-export/reference.imesh`, que pode ser aberto no editor.
