# Image Mesh Editor

Editor offline para extrudar regiões de uma imagem em módulos 3D com relevo.
Disponível a partir da versão 7.217.0; interface reorganizada na 7.218.0 e controles de diagnóstico na 7.219.0 e adição de formas na 7.220.0. O estado atual inclui as etapas 1 e 2, pintura manual e presets reutilizáveis da etapa 3
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
os parâmetros de densidade e orçamento. **Volume e relevo** é recolhível e inclui
**Fixar altura da borda**.

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
   **Modo**. O mapa aparece sobre o recorte selecionado na cena
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
6. Entre no modo 3D para ativar a geometria
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

Em **Imagem original**, limiar, largura da transição, tolerância de erro e suavização
ficam ocultos, sem alterar seus valores. No modo 3D ou ao editar os padrões, os
controles de geração aparecem conforme suas dependências. A tolerância exige geometria adaptativa;
o limiar também orienta o alinhamento das arestas adaptativas mesmo sem **Duas alturas**.
A imagem original não é processada. Os grupos de volume e resolução continuam sendo
configurações de geração, independentemente da prévia selecionada.

A geometria adaptativa amostra o erro de interpolação do relevo, refina onde é
necessário, melhora a forma dos triângulos e insere arestas nas transições da
imagem processada. O verso plano usa só vértices da borda. **Tolerância de erro
do relevo** é uma fração da amplitude e orienta o refinamento; **Colunas/Linhas**
limitam a densidade e a amostragem. Não é uma garantia de erro máximo em cada pixel:
detalhes menores que a escala de amostragem podem exigir maior resolução.

Após o alinhamento, o gerador avalia trocas de diagonais pelo erro de altura
amostrado nas duas triangulações. Só aceita a troca quando ela reduz esse erro
sem piorar a qualidade mínima dos dois triângulos em XY (área em relação à soma
dos comprimentos quadráticos das arestas, nas dimensões do módulo). Arestas nos
limiares de alinhamento são preservadas. São até oito passes locais; não se trata
de uma otimização global nem de uma garantia adicional para a tolerância.
Sem **Duas alturas**, também são alinhados níveis próximos de preto e branco
(0,0001 e 0,9999), para capturar os extremos de rampas com patamares. Isso pode
acrescentar vértices e triângulos e continua sujeito ao orçamento da geração.
Com **Seguir formas da imagem** e **Duas alturas barra/sulcos**, as faces dos
patamares têm prioridade no cálculo das normais dos vértices compartilhados com
as transições. A classificação usa a altura mapeada antes da atenuação da borda,
portanto a inclinação desejada perto do contorno é mantida. Isso evita que paredes
íngremes inclinem indevidamente as normais dos topos e fundos dos sulcos.
Posições, UVs, índices e contagens permanecem iguais; não são criadas costuras de
vértices que impeçam a simplificação. Vértices sem faces de patamar continuam
usando a média das faces adjacentes, assim como os demais modos de geração.
O sombreamento das transições também muda, pois usa esses mesmos vértices.
A exportação mantém essas normais. Recalculá-las no Mesh Debug pela média comum
das faces substitui esse tratamento específico dos patamares.
Não há novo modo QUAD. Todo esse trabalho ocorre somente ao gerar a mesh.

O teste `src/test-lib/image_mesh_relief_smoke.lua` usa rampas verticais e horizontais
conhecidas e mede a interpolação dentro das faces, além das alturas dos vértices.
Ele exige erro amostrado abaixo de 0,03 unidade para amplitude 8; esse limite é
específico do teste, não uma promessa para qualquer imagem.

O mapa é temporário, fica em cache e não altera a textura original ou a exportada.
Zoom e pan reposicionam o mapa sem refazer processamento. Arrastes ocultam o mapa
antigo; após confirmar, ele é atualizado. Trocar módulo ou vista também atualiza
o cache. A prévia 2D continua funcionando quando o orçamento impede gerar a mesh.
A pintura manual de altura está disponível no grupo **Pintura de altura**; furos reais permanecem uma etapa futura.

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
para +Z na prévia e nos arquivos exportados pelo editor, compatível com a câmera
inicial do Mesh Debug; a origem fica no centro do volume básico. A altura vem da luminosidade,
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

## Simplificação após gerar

Desde 7.228.0, **Simplificar geometria > Simplificar após gerar** habilita uma etapa
opcional por módulo (ou nos padrões do projeto). **Aplicar** regenera a peça e
simplifica o frame inteiro com o mesmo algoritmo do Mesh Debug. Há um único frame
com um a três subsets, conforme os modos de textura lateral e fundo; todos participam da
operação. Não há seletores de escopo ou frames compartilhados.

- **Proporção de triângulos**: fração a manter, de 0,001 a 0,95; padrão 0,9.
  Por exemplo, 0,28 solicita aproximadamente 28% das faces de origem.
- **Preservar detalhes**: mesma penalização de colapsos em detalhes usada pelo Mesh Debug;
  habilitada por padrão.
- **Limiar de colapso de fronteira**: de 0 a 0,25, como no painel do Mesh Debug.
  Zero mantém as fronteiras abertas bloqueadas; valores maiores permitem reduzir
  arestas limpas de fronteira até essa fração da diagonal da mesh.

A prévia, wireframe, contagem de faces e exportações individuais/em lote usam o
resultado simplificado. O painel informa as contagens antes/depois e a estimativa
de triângulos para a proporção atual, usando a última contagem anterior à simplificação. Os ajustes são
salvos no `.imesh`, participam do histórico e ficam desabilitados em projetos antigos.
Desabilitar a etapa e aplicar recupera a geometria gerada, sem acumular simplificações.
Os mapas 2D continuam representando a imagem processada e não são simplificados.

Após gerar em 3D com simplificação habilitada, **Comparação** permite ativar
**Original e simplificada lado a lado**, compartilhando câmera e iluminação.
Ao ativar, a distância é ampliada se necessário para mostrar o par, mantendo a
orientação e o foco; ao desativar, volta à distância anterior.
As versões são posicionadas pelos limites reais no eixo X, com espaço de 15% da
maior largura. **Resetar visão** considera a largura do par. Desativar a comparação
centraliza novamente a versão simplificada.
Os checkboxes **Visualizar original** e **Visualizar simplificada** controlam cada
mesh separadamente; os labels indicam o lado conforme a câmera. A visibilidade
também se aplica ao wireframe e não altera a exportação. Fora da comparação,
a simplificada volta a aparecer normalmente. O painel mostra vértices e triângulos
antes/depois, redução percentual e os erros geométrico/relativo informados pelo
simplificador; esse erro não é uma avaliação visual da imagem.

As duas versões ficam em cache durante a prévia. Cada wireframe é criado apenas
quando solicitado pela primeira vez. A alternância não gera nem simplifica a mesh
novamente; os recursos e arquivos temporários são liberados ao substituir a prévia
ou encerrar a cena. A comparação não modifica parâmetros nem o histórico.
**Exportar** continua usando o resultado simplificado mesmo quando a original está
visível. No modo 2D as duas meshes ficam ocultas.

A simplificação roda em um worker da engine, com progresso. Durante essa etapa os
controles de edição ficam indisponíveis para impedir alterações na operação em curso;
a cena continua desenhando. Nenhum worker é iniciado novamente enquanto o editor está
ocioso. A geração inicial continua síncrona e precisa respeitar o orçamento de vértices.
Se as restrições de topologia impedirem a redução solicitada, o editor informa o erro;
não exporta silenciosamente a mesh original. O lote registra a falha e segue para a
próxima peça. Seu cancelamento ocorre entre peças.

## Salvar, relocalizar e exportar

**Salvar projeto** (Ctrl+S) grava `.imesh`, contendo versão, imagem, regiões, presets e
parâmetros. Salvar também valida e confirma os ajustes pendentes no painel, mesmo
sem pressionar **Aplicar**; se forem inválidos, o arquivo não é sobrescrito. Ao concluir, uma mensagem temporária de quatro segundos confirma o
salvamento e mostra o nome do arquivo. Salvar novamente reinicia sua duração. É um arquivo Lua de dados seguindo o padrão dos editores, carregado
com ambiente vazio e validado. O histórico de desfazer e recursos GPU não são
salvos. Imagens sob a pasta do projeto usam referência relativa; outras mantêm
o caminho recebido pelo diálogo. Ao reabrir com imagem ausente, **Localizar
imagem** permite escolher uma substituta com as mesmas dimensões.

As normais suavizadas usam a média das normais unitárias das faces vizinhas,
como **Recalcular todos** na tabela de normais do Mesh Debug. As separações entre
frente, verso e laterais são preservadas. O status **OK** indica concordância de
direção, não igualdade numérica com esse recálculo. Reexporte meshes antigas para
adotar essa ponderação; os arquivos existentes não são alterados automaticamente.

O editor rotaciona a geometria gerada em 180 graus no eixo Y, incluindo as normais,
antes de criar a prévia ou exportar (individualmente ou em lote). UVs e ordem dos
triângulos são preservados. A API `mbm.generateImageMesh` continua gerando a frente
em -Z. Arquivos já exportados não são modificados; reexporte para adotar a orientação +Z.

**Exportar mesh selecionada** escreve uma `.msh` v11. **Exportar todas as meshes**
usará a pasta escolhida e nomes com ID da região, por exemplo `001_module_001.msh`.
O lote processa uma peça por vez, aguardando sua simplificação opcional, e pode ser cancelado entre peças. Falhas são
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
mkdir -p /tmp/image-mesh-stage2-export /tmp/ime-simplify-batch
timeout -s KILL 20 bin/debug/linux_x86/mini-mbm --scene src/test-lib/image_mesh_smoke.lua --disable_select_monitor --nosplash -w 640 -h 480
timeout -s KILL 25 bin/debug/linux_x86/mini-mbm --scene src/test-lib/image_mesh_editor_smoke.lua --disable_select_monitor --nosplash -w 1440 -h 900
timeout -s KILL 40 bin/debug/linux_x86/mini-mbm --scene src/test-lib/image_mesh_simplify_smoke.lua --disable_select_monitor --nosplash -w 1440 -h 900
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

## Pintura de altura (7.234.0)

No modo de edição, selecione um módulo e abra **Pintura de altura**. Habilite
**Pintar módulo selecionado**: ajustes pendentes são aplicados e a imagem muda
para o mapa de alturas. Escolha **Elevar**, **Rebaixar**, **Nivelar** ou **Suavizar**.
O raio é exibido em pixels da imagem; a intensidade vai de 0,01 a 1.
Internamente, o raio é relativo ao menor lado para acompanhar o redimensionamento.
Nivelar oferece uma altura alvo entre 0 e 1 (multiplicada pelo relevo na mesh).

Arraste o botão esquerdo dentro do módulo. O círculo amarelo mostra o alcance;
ao soltar, o mapa recebe o traço completo. Cada arraste ocupa uma entrada no
histórico: Ctrl+Z/Ctrl+Y desfaz/refaz; Esc descarta o traço em andamento.
**Limpar pintura** também permite desfazer. Fora do módulo, o botão esquerdo
move a câmera. Escolher outra ferramenta desabilita a pintura para permitir
manipular as regiões normalmente.

A pintura é uma camada por módulo, salva em `heightEdits` no projeto e incluída
na duplicação, prévia 3D, simplificação e exportação. Coordenadas normalizadas
acompanham movimentação/redimensionamento do recorte. Alterar a detecção automática
reexecuta os mesmos traços sobre o novo mapa. Não modifica a imagem original.
A borda fixa ainda prevalece, e o contorno recorta a superfície final.
A sobreposição azul continua representando apenas os sulcos automáticos.

Com **Seguir formas da imagem (adaptativo)**, a pintura refina localmente a
geometria usando o campo final de alturas. Colunas/Linhas continuam definindo a
densidade geral; o retoque pode receber mais detalhe sem aumentar a grade inteira.
Sem o modo adaptativo, a grade continua seguindo a resolução escolhida. Há no máximo 4096
amostras por módulo; o espaçamento é calculado pela distância, sem depender da
frequência dos eventos do mouse. O backend limita o trabalho de pintura por geração
a 64 milhões de operações em pixels, com diagnóstico em caso de excesso.

Enquanto o pincel está habilitado, a contagem automática de faces fica adiada.
O mapa atualiza ao concluir o traço; a mesh é gerada ao voltar ao modo 3D ou
exportar. Em repouso, o editor não reexecuta a pintura nem reconstrói a geometria.


### Fidelidade dos retoques (7.234.1)

Os traços do projeto são reexecutados no campo de alturas em ponto flutuante.
A triangulação adaptativa consulta esse campo final para alinhar transições, e
uma etapa posterior refina as células efetivamente alteradas e sua vizinhança.
Ela verifica amostras a cada meio pixel, centros e meios de arestas, com alvo
local de erro de no máximo `min(tolerância configurada, 0,02)` do relevo. O piso
de subdivisão é 1/16 de pixel; esse alvo amostrado não é uma garantia de erro
contínuo em toda a superfície. A atenuação da borda fixa continua sendo aplicada.

Não é necessário salvar ou reler um PNG para obter esse resultado: o mapa exibido
e a mesh usam o mesmo campo, sem uma conversão adicional para 8 bits. Os traços
continuam sendo o formato editável do projeto, não uma lista de posições de vértices.
Pintura sem efeito não força novo refinamento. Os tetos de geometria continuam
valendo; orçamento insuficiente gera erro, em vez de omitir silenciosamente o retoque.
A simplificação opcional ainda pode aproximar detalhes depois da geração.


Na área corrigida, a interpolação usa os valores finais dos pixels pintados, com
transição local para o campo automático. Isso evita ondulações entre pixels
nivelados. Em Duas alturas, valores a menos de 0,0001 de 0 ou 1 são fixados no
patamar correspondente nessa área, em acordo com o cálculo de normais. O alinhamento
reexpressa a altura corrigida no domínio dos limiares, preservando o tratamento
dos patamares automáticos fora do retoque.

## Presets reutilizáveis (7.235.0)

No painel **Regiões**, abra **Presets de geração** após selecionar um módulo
ou habilitar **Editar padrões do projeto**.

- Digite um nome e use **Criar preset** para capturar os parâmetros exibidos,
  inclusive ajustes ainda não aplicados. Isso não gera uma mesh.
- Escolha um preset na lista. **Aplicar aos módulos selecionados** substitui os
  parâmetros dos módulos selecionados; no modo de padrões, o botão aplica aos
  padrões do projeto. A aplicação substitui os ajustes pendentes do painel e
  preserva nome, recorte, contorno e pintura já confirmados no projeto.
- **Atualizar com parâmetros exibidos** substitui o conteúdo do preset escolhido.
  Para renomeá-lo, digite o novo nome e pressione **Renomear**.
- **Excluir preset**, criação, atualização, renomeação, importação e aplicação
  participam do histórico do projeto (Ctrl+Z/Ctrl+Y).

O preset contém volume, dimensões 3D, preservação de proporção, relevo/sulcos,
resolução/orçamento e simplificação. Não contém imagem, contorno, traços de pintura,
câmera, luz ou modo de visualização. Com preservação de proporção habilitada,
a altura 3D é calculada a partir do recorte de cada módulo destinatário.

Os presets ficam dentro do arquivo `.imesh`: use **Salvar projeto** para persistir
as alterações. Projetos antigos sem presets continuam válidos. Há um limite de
128 presets por projeto; nomes devem ser únicos, não vazios e ter até 128 bytes.

**Exportar preset** grava o preset escolhido em `.imeshpreset`.
**Importar preset** adiciona esse arquivo ao projeto atual, permitindo reutilizar
configurações entre projetos. Importar não aplica automaticamente aos módulos;
nomes duplicados são rejeitados, sem sobrescrever o preset existente.
Esses arquivos usam dados Lua versionados, ambiente vazio e validação dos
parâmetros, com limite de 64 KiB.

A lista de nomes é atualizada apenas quando o projeto muda. Captura, aplicação,
serialização e acesso a arquivos ocorrem por ação do usuário, sem trabalho
contínuo de geração ou leitura em repouso.

## Textura e fundo: controle do verso (7.236.0)

A seção **Textura e fundo** permite escolher **Plano** ou **Copiar relevo frontal**
em **Geometria do fundo**, além de **Espelhar textura do fundo na horizontal**.
Pressione **Aplicar** e gire a câmera em modo 3D para inspecionar o verso.

O relevo copiado usa as alturas finais da frente, incluindo pintura e ajustes
da borda, crescendo para fora. A espessura em cada ponto passa a ser a espessura
base mais duas vezes o relevo local. Suas normais preservam a suavização e os
patamares da frente. A malha continua fechada, com quinas separadas das laterais.

O espelhamento é independente da geometria: altera apenas as coordenadas
horizontais da textura no verso, dentro do mesmo recorte. Frente e laterais
mantêm suas coordenadas anteriores.

Em geração adaptativa, o relevo copiado exige triangulação completa do verso,
em vez do fundo plano simplificado. Os limites de vértices/triângulos incluem
esse custo; um orçamento insuficiente é informado pelo gerador. A simplificação
opcional continua sendo aplicada depois da geração.

Ambas as opções participam da seleção múltipla, padrões do projeto, histórico,
presets e exportação. Projetos e presets anteriores usam fundo plano sem
espelhamento por padrão. Nenhuma dessas opções altera o mapa diagnóstico frontal.

Esta entrega cobre o verso plano/com relevo e o espelhamento. Texturas próprias,
textura repetida/cor uniforme nas laterais e exportação portátil com recortes
protegidos ainda estão pendentes; a imagem original continua sendo referenciada.

## Sem fundo e remapeamento UV (7.237.0)

**Geometria do fundo** agora também oferece:

- **Sem fundo**: gera a frente e as laterais, deixando a parte traseira aberta.
  Os vértices e triângulos exclusivos do verso deixam de contar no orçamento.
  Útil quando a câmera do jogo vê somente a frente e as laterais.
- **Plano + remap UV**: mantém o fundo plano, mas usa outro recorte da mesma
  imagem como textura. Inicialmente, o recorte coincide com a região frontal.

Para posicionar esse recorte, escolha **Plano + remap UV** e habilite
**Editar recorte do fundo** no modo de edição. Isso confirma os ajustes pendentes,
seleciona a imagem original e desabilita o pincel de altura.
O contorno roxo tem a mesma forma normalizada da frente; arraste seu interior
para mover e a alça inferior direita para redimensionar. Espaço vazio move a
câmera. O contorno UV recebe prioridade quando está sobreposto à frente.
A ferramenta também fica disponível no combo **Ferramenta** após aplicar o modo.

Os campos de posição/largura/altura permitem ajustes numéricos, confirmados com
**Aplicar**. **Restaurar recorte UV para a região frontal** repõe esses campos. Movimento e
redimensionamento ficam limitados à imagem. O desenho acompanha mudanças no
contorno frontal; polígonos assimétricos refletem também a opção de espelhamento
para mostrar os pixels realmente utilizados no verso.

O recorte roxo altera somente UVs: não altera a geometria frontal, o relevo,
a pintura nem as laterais. Não é um segundo módulo e não aparece na lista de
regiões. Seu retângulo é salvo em `region.backCrop={x,y,w,h}` no projeto,
com histórico por gesto e preservação ao duplicar módulos. Cada módulo conserva
sua posição UV ao receber um preset; presets guardam o modo, não coordenadas
específicas da imagem. As três escolhas de geometria especiais são exclusivas,
inclusive ao combinar padrões do projeto com ajustes individuais.

## Textura das laterais (7.238.0)

A seção **Textura das laterais** oferece quatro modos:

| Modo | Uso |
|---|---|
| **Borda esticada** | Comportamento anterior: estica os pixels do contorno ao longo da profundidade |
| **Cor uniforme** | Escolhe uma cor RGB opaca para todas as paredes |
| **Textura repetida** | Repete o recorte da imagem original por padrão, com outro arquivo opcional; ajusta repetições no perímetro inteiro e na profundidade. "Usar recorte da imagem original" remove a textura externa |
| **Faixa interna do contorno** | Usa a faixa da própria imagem entre o contorno original e um contorno interno |

Para a faixa, ajuste **Largura da faixa (px)** ou habilite
**Redimensionar contorno interno** em modo de edição. A alça verde altera apenas
o tamanho do contorno interno, sem deslocar o módulo. A textura lateral vai
do limite externo junto à frente até o interno junto ao verso. O ajuste não
modifica posições, índices ou normais. A faixa preserva a transparência da imagem.

O mínimo é 1 pixel. O máximo é calculado por contorno para evitar cruzamentos,
inversões de arestas e colapsos. Retângulos são reduzidos por lado; círculos
permanecem concêntricos; elipses mantêm os eixos com raios reduzidos. Polígonos
usam arestas deslocadas, sem criar furos ou dividir o contorno. Uma forma estreita
demais pode não aceitar uma faixa de 1 pixel; o editor informa essa limitação.
As medidas de UV usam centros de pixels: um recorte de 100 pixels cobre 99
intervalos entre centros.

Cor uniforme e repetição usam um segundo material. A prévia, comparação,
wireframe, simplificação e exportação percorrem ambos os materiais. A repetição
cria divisões nas UVs e na triangulação adjacente para funcionar sem depender
do modo global de endereçamento da textura; isso pode aumentar a contagem de
vértices e triângulos. Os limites continuam obrigatórios.

Todos os parâmetros entram nos padrões do projeto, seleção múltipla e presets.
Projetos antigos usam borda esticada. Caminhos de texturas são relativos ao
projeto/preset quando estão sob sua pasta e são resolvidos ao abrir/importar.
Se a textura estiver ausente, escolha novamente o arquivo nessa seção.
A exportação referencia a imagem escolhida; empacotamento portátil permanece
uma entrega separada. Cores uniformes não precisam de arquivo adicional.

A edição por alça tem desfazer/refazer por gesto e cancelamento com Esc.
Consultas de contorno e atualizações de linhas são armazenadas em cache;
o editor em repouso não recalcula o limite nem reconstrói a geometria.

No modo **Faixa interna do contorno**, **Inverter UV da faixa** troca o sentido
da textura ao longo da profundidade da lateral. Desmarcado (padrão): contorno
externo junto à frente e interno junto ao verso, continuando a frente em espelho.
Marcado: interno junto à frente e externo junto ao verso. A mudança afeta somente
os UVs laterais, sem alterar geometria, normais ou UVs das faces frontal/traseira.
Use **Aplicar** para atualizar; projetos e presets preservam a opção.

**Textura e fundo > Cor sólida** cria um fundo plano opaco com seletor RGB.
A mesh salva a cor como `#RRGGBBFF`, sem arquivo externo. Frente, fundo e
laterais têm materiais separados; funciona com os quatro modos laterais,
simplificação e exportação. Projetos/presets preservam modo e cor.

**Textura e fundo > Textura externa** cria um fundo plano com uma imagem
independente. **Escolher textura do fundo** seleciona o arquivo; a imagem inteira
é ajustada à forma do módulo (proporções diferentes podem esticar a textura).
O espelhamento horizontal permanece disponível. Sem arquivo escolhido, usa o
recorte original; **Usar recorte da imagem original** remove a escolha externa.
Aplique para atualizar. Arquivos inválidos ou acima de 16 milhões de pixels
são rejeitados na geração; a transparência da imagem é preservada.
Projetos/presets armazenam caminhos relativos. A exportação `.msh` referencia
a imagem e não a copia; empacotamento portátil continua pendente.

## Exportação portátil (7.244.0)

No menu Arquivo, **Exportar mesh selecionada + texturas...** e
**Exportar todas as meshes + texturas...** criam meshes e PNGs na mesma pasta.
Mova a pasta inteira para outro projeto. A exportação anterior continua disponível.

Por padrão, exporta a imagem inteira em PNG RGBA, preservando dimensões, pixels,
transparência e UVs originais. Não adiciona margem nem redimensiona a imagem:
texturas com dimensões em potência de dois conservam essas dimensões. A imagem
é decodificada e gravada como PNG; não se trata de cópia binária do arquivo original.

**Recortar texturas na exportação portátil** é opcional e começa desmarcado em
cada sessão. Quando marcado, cada material recebe a área coberta pelos UVs,
com margem de 4 pixels de borda replicada e UVs remapeados. Frente e verso no
mesmo material usam o retângulo que engloba ambos os recortes. Os recortes não
são arredondados para potências de dois. O lote captura a opção ao iniciar.

Geometria, normais e simplificação são preservadas. Cores sólidas permanecem
na mesh. No mesmo lote, módulos e materiais que usam a mesma imagem de origem
compartilham um único PNG e o mesmo nome de textura. Com recorte habilitado,
o compartilhamento exige também limites de UV idênticos. Arquivos de origem
diferentes não são comparados por conteúdo. Não há atlas compartilhado. Margens reduzem vazamentos por filtragem, sem garantia para
todos os níveis de mipmap. O `.msh` referencia os PNGs adjacentes; seus nomes
são limitados a 63 bytes pelo gravador.

A exportação portátil sobrescreve os arquivos de destino. Primeiro gera todos
os novos arquivos temporários do módulo; depois substitui os destinos mantendo
backups durante a operação. Falhas de geração preservam os arquivos anteriores;
falhas de substituição tentam restaurar os backups e informam se a restauração
falhar. Isso não é uma transação resistente a queda de energia. Módulos já
exportados no lote são preservados. Os arquivos do projeto não são alterados.

O compartilhamento vale para a exportação atual; uma exportação individual também
reutiliza a imagem entre seus materiais. Arquivos antigos que deixaram de ser
referenciados não são removidos automaticamente da pasta. Para conferir somente
os arquivos da nova exportação, escolha uma pasta vazia.

## Prévia de encaixe (7.245.0)

Em modo 3D, abra **Prévia de encaixe** e habilite **Mostrar módulos montados**.
A cena mostra todos os módulos com suas configurações atuais, incluindo
simplificação e materiais. A comparação original/simplificada fica substituída
pela montagem enquanto esse modo está ativo.

- **Colunas da grade** e **Organizar na grade** distribuem as peças na ordem do projeto.
- **Espaçamento X/Y** adiciona distância entre células. As células usam a maior
  largura/altura das peças; módulos menores podem deixar folgas mesmo com zero.
- Selecione um módulo na lista de Regiões para editar sua **Coluna**, **Linha**,
  **Deslocamento em profundidade** ou **Visualizar módulo**. Linhas/colunas começam
  em zero. Posições iguais permitem sobreposição para diagnóstico.
- **Enquadrar montagem** centraliza a câmera. Órbita, zoom, luz e wireframe
  continuam disponíveis. O plano frontal de base é alinhado em Z=0; os relevos
  continuam projetados para fora e o deslocamento de profundidade é adicional.

Esta entrega usa controles numéricos, sem arraste de peças na cena ou rotação
individual. O posicionamento é uma prévia da sessão: não altera as regiões,
não entra no histórico do projeto e não é salvo/exportado. Ao desabilitar,
a câmera anterior e a prévia individual são restauradas. Abrir outro projeto
limpa a montagem. Alternar para edição 2D apenas oculta a montagem.

Seleção, espaçamento, visibilidade e posições reutilizam as meshes prontas.
Alterações aplicadas ao projeto regeneram a montagem preservando a câmera;
wireframes são criados sob demanda. Não há geração, leitura de arquivos ou
reposicionamento contínuo em repouso. Falha de geração libera as prévias
parciais e apresenta o erro, sem repetir a geração a cada frame.

## Furos manuais (7.246.0)

Selecione o módulo e abra **Furos**. No modo de edição:

- **Adicionar furo** cria um retângulo ou círculo de 16 pontos em uma posição livre,
  buscando a partir do centro e mantendo o tamanho inicial. O novo furo fica selecionado.
  Se a busca não encontrar espaço, o editor orienta a mover/redimensionar os furos
  existentes ou usar **Desenhar furo**, sem alterar o projeto.
  A inclusão é recusada se não couber na peça ou sobrepuser outro furo; nesse
  caso, use desenho manual ou mova o furo existente antes de adicionar outro.
- **Desenhar furo** permite clicar os vértices de um polígono e **Concluir furo**.
- **Editar furos na cena** exibe alças nos vértices do furo selecionado. Arraste
  uma alça para mudar a forma ou arraste dentro do furo para movê-lo. Espaço
  vazio move a câmera. Escape cancela o gesto/desenho.
- **Remover furo** exclui apenas o furo selecionado.

Contornos rosa representam aberturas reais. Cada módulo aceita até 16 furos,
com 3..128 pontos por furo. Eles devem ficar estritamente dentro da peça, sem
contato, cruzamentos, sobreposição ou furos dentro de furos. Edições inválidas
são rejeitadas ao finalizar o gesto. Vértices consecutivos repetidos e áreas
degeneradas também são rejeitados.

As aberturas removem faces da frente/verso e geram paredes internas. Funcionam
com relevo, pintura, simplificação, todos os modos de fundo, montagem e exportação.
Fixar altura da borda afeta apenas o contorno externo. Os furos recortam o relevo
sem rebaixar sua vizinhança; suas paredes internas acompanham a altura local. Com furos, o fundo
usa a triangulação da frente e pode consumir mais orçamento que o fundo compacto.
Paredes internas usam o modo lateral selecionado; no modo **Faixa interna**,
estica-se a textura da borda do furo, sem aplicar o contorno verde externo.

Furos acompanham movimento/redimensionamento do módulo, participam de
desfazer/refazer e são salvos no `.imesh`. Presets de geração preservam os furos
existentes, sem copiá-los entre módulos. Não há detecção automática nesta etapa.

### Redimensionar furos circulares (7.247.0)

Furos criados como círculo têm **Manter forma circular/elíptica** habilitado.
O furo selecionado mostra três alças: direita ajusta a largura, inferior ajusta
a altura e inferior direita iguala os raios em pixels da imagem, formando um
círculo. O centro permanece fixo; arrastar o interior move o furo.

Desmarque para editar os vértices livremente. Ao marcar novamente, o editor
reconstrói a elipse usando os limites atuais. A opção acompanha o furo no
projeto e no histórico. Círculos/elipses regulares de 16 pontos de projetos
antigos são reconhecidos ao abrir. Sobreposição e saída do módulo continuam
sendo rejeitadas ao soltar a alça.

## Desenho livre de contornos (7.248.0)

Escolha **Contorno à mão livre** no seletor de ferramenta para criar um módulo.
Para uma abertura, selecione um módulo e use **Furos > Furo à mão livre**.
Arraste o botão esquerdo sobre a imagem e solte: o editor fecha o traçado e
apresenta uma prévia, sem modificar o projeto ou gerar a mesh nesse momento.

**Redução de pontos (px)** controla a tolerância de simplificação do traçado
em pixels da imagem, de 0,1 a 20 (padrão 1,5). Valores maiores removem detalhes.
A contagem mostra amostras capturadas e vértices finais. Ajuste e clique em
**Confirmar contorno**; um novo arraste substitui a prévia. **Cancelar** ou
Escape descarta o traçado.

O resultado é um polígono comum, com vértices editáveis, histórico, persistência,
relevo, simplificação da mesh e exportação já existentes. Furos também passam
pelas validações de contenção e sobreposição antes da confirmação.
Contornos que se cruzam, degenerados ou com mais de 128 vértices são recusados.
O traçado aceita até 4096 amostras; se exceder esse limite, precisa ser redesenhado.

A captura recebe eventos do mouse, e a redução só ocorre ao soltar ou alterar
a tolerância. A prévia parada não refaz o contorno nem regenera a geometria.
Esta ferramenta é manual: detecção automática de contornos permanece pendente.

## Seleção automática de contornos (7.249.0)

No seletor de ferramenta, escolha **Contorno automático**. Selecione o destino
(**Novo módulo** ou **Furo no módulo selecionado**) e o método:

- **Transparência**: pixels com alfa maior que o limiar pertencem à forma.
- **Cor de fundo**: além do alfa, exclui pixels próximos da cor escolhida.
  **Capturar cor de fundo** permite obter essa cor clicando na imagem.
  A tolerância usa a maior diferença entre canais RGB, de 0 a 255.

Clique dentro da forma desejada. A busca considera apenas a área conectada
ao ponto, por vizinhos horizontais/verticais. Cavidades internas não viram
furos automaticamente: o resultado é somente o contorno externo dessa área.
Para criar uma abertura, use explicitamente o destino de furo e confirme.

Revise a prévia, ajuste **Redução de pontos (px)** e confirme. Após a confirmação,
o polígono permite edição normal de vértices, histórico, salvamento e exportação.
Mudar o método, a cor ou os limiares descarta a prévia e exige outro clique.
**Cancelar**, Escape, troca de ferramenta/projeto ou desfazer descartam a busca
ou prévia pendente.

**Usar recorte do módulo selecionado** limita a busca para novos módulos;
furos sempre usam esse recorte. Sem módulo selecionado, usa-se a imagem inteira.
A detecção lê a imagem original, sem aplicar pintura de altura ou filtros de relevo.
Não há reconhecimento semântico: numa imagem opaca, a transparência sozinha
selecionará toda a área conectada do recorte.

Para limitar memória e trabalho, a grade de busca tem até 262.144 células.
Áreas maiores são amostradas com passo inteiro, exibido no resultado. Detalhes
menores que esse passo podem desaparecer; um recorte menor oferece maior precisão.
Há limites de 65.536 segmentos de borda, 4.096 cantos antes da redução e
128 vértices finais. Contornos ambíguos que se tocam ou se cruzam são recusados,
com orientação para ajustar a detecção ou desenhar manualmente.

A imagem é decodificada uma vez por clique. A busca cede execução a cada bloco
de trabalho para permitir cancelamento; depois de concluída não há releitura
da imagem, reconstrução ou processamento contínuo.

## Relevo desenhado por áreas (7.250.0)

Em **Relevos e sulcos**, escolha o **Modo de relevo**:

- **Imagem**: mantém a interpretação automática e os retoques de pincel anteriores.
- **Manual**: começa com a **Altura base (0-1)**. A textura não determina as alturas.
- **Imagem + áreas manuais**: mantém o relevo automático fora das áreas desenhadas.

Adicione retângulos/elipses ou desenhe polígonos/áreas à mão livre sobre o módulo.
A primeira área em modo Imagem ativa o modo misto e o refinamento adaptativo.
Selecione cada área na lista para definir nome, habilitação, altura-alvo e transição
interna em pixels. A altura é multiplicada pelo relevo de **Volume e relevo**;
por exemplo, base 0,4, área 0,8 e sulco 0,2 criam três níveis sem depender das cores.

**Editar áreas na cena** mostra contornos: verde para a selecionada, ciano para
as demais e cinza para desabilitadas. Arraste o interior para mover. Retângulos e
elipses têm alça inferior direita que preserva a forma; polígonos têm alças nos
vértices. Os campos de largura/altura redimensionam qualquer tipo pela caixa
envolvente. Uma área fica dentro do recorte do módulo, mas pode ultrapassar seu
contorno: a superfície e os furos recortam o resultado. Espaço vazio arrasta a câmera.

A lista indica a ordem; **Antes/Depois** alteram a sobreposição. Áreas posteriores
prevalecem. **Duplicar área** copia a área na mesma posição e seleciona a cópia;
**Remover área** e as demais operações participam do histórico. Propriedades são
confirmadas ao aplicar, salvar ou iniciar outra operação de edição.

A transição mistura a altura anterior com a nova, da borda para dentro da área.
Zero é uma mudança abrupta no mapa; a mesh continua limitada pelos pixels e pela
resolução. Pincel é aplicado depois das áreas, e a borda externa fixa por último.
Use **Visualização > Mapa de alturas** e **Pré-visualizar ajustes** para examinar as alterações
antes de **Aplicar**. No modo Manual, filtros da imagem e a visualização de sulcos
azuis ficam ocultos. O refinamento adaptativo continua disponível, assim como
simplificação, comparação e exportação.

Até 32 áreas por módulo, cada uma com 3–128 pontos; há ainda um limite de 64 milhões
de avaliações de arestas na rasterização. Áreas são persistidas no projeto,
copiadas com o módulo e preservadas em coordenadas normalizadas ao redimensioná-lo.
Presets guardam a origem/base e parâmetros de geração, não os contornos das áreas.
Projetos anteriores mantêm a origem Imagem. Sulcos por linha com largura editável
ficam para uma entrega posterior; por enquanto use áreas estreitas fechadas.

Validação: `image_mesh_areas_smoke.lua` cobre composição, alturas, transições,
ordem, pincel, furos e geração; `image_mesh_areas_editor_smoke.lua` cobre edição,
histórico, persistência, exportação, GUI e ausência de reconstruções em repouso.

Os controles de áreas ficam dentro de **Relevos e sulcos**, nos modos Manual e
Imagem + áreas manuais. A área selecionada mostra primeiro habilitação, altura-alvo,
altura resultante em unidades da mesh e diferença para a base manual. **Aplicar**
confirma os ajustes. Uma área desabilitada permanece salva, mas não altera a mesh;
o editor mostra um aviso explícito. Tamanho vertical em pixels dimensiona somente
o contorno na imagem, não a profundidade.

## Reaproveitamento da prévia (7.251.0)

Depois de calcular a contagem de faces, o editor conserva a última mesh gerada.
Entrar em 3D no mesmo módulo, sem confirmar novas alterações, reaproveita esse
resultado e sua simplificação. A comparação mantém a original correta.
O cache é limitado a um módulo e invalidado por alterações, histórico e troca de
projeto. Exportações permanecem independentes. A primeira geração ainda pode
pausar a interface; o reaproveitamento evita repetir esse trabalho ao entrar em 3D.

Desde 7.251.1, a contagem exata no modo de edição é **sob demanda**. Após alterações,
aparece **Faces: cálculo pendente**. Use **Calcular faces** quando precisar da
contagem; isso confirma propriedades pendentes e gera a mesh completa, podendo
levar alguns segundos. Entrar em 3D também atualiza a contagem. Arrastar áreas não
dispara mais geração 3D apenas para contar faces. O mapa de alturas, quando visível,
continua tendo sua atualização própria.
