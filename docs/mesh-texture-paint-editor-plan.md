# Plano: Editor de Pintura de Textura sobre Mesh 3D

Status: pronto para revisão via PR. O milestone vertical atual já abre MSH, faz picking
3D, pinta uma camada UV, mantém preview privado, salva projetos Lua, exporta
textura composta e MSH derivada, mostra a revisão 2D com overlay UV e oferece
projeção de cor sólida pela câmera em superfícies visíveis e padding configurável
para seams, destaca bordas das ilhas UV e avisa sobre sobreposição de UVs;
o modo opcional de pintar através e o modo experimental `Surface 3D` também
estão disponíveis; o milestone de carimbo por imagem externa está implementado
na primeira versão e foi aceito após validação visual nos backends previstos.
A forma
quadrada agora usa uma máscara projetada no caminho normal e uma máscara
tangente no caminho `Surface 3D`; a validação visual em meshes reais continua
sendo necessária.

Milestone de salvamento seguro e abertura transacional implementado: projeto Lua + camada PNG e exportação
MSH + PNG são preparados antes da publicação, com restauração dos arquivos
anteriores quando uma operação de I/O falha. A exportação usa uma cópia da fonte,
preservando os vínculos privados do preview. A abertura agora valida arquivos,
dimensões, frame, textura, subset e ilha UV antes de substituir a sessão atual.
Limites e evidências estão nas seções 8 e 10; falhas durante a publicação física
continuam sendo diagnosticadas pelo mecanismo de recuperação de arquivos.

Validação de plataforma: Linux/OpenGL ES, macOS/Metal e Windows com DirectX 9 e
DirectX 11 estão concluídos e aceitos para o escopo atual.

Requisitos de autoria adicionados: o cursor não oferece mais o modo artificial de
círculo sempre voltado para a câmera; a forma do dab é escolhida entre círculo e
quadrado. O carimbo usa a mesma ferramenta para uma fonte capturada da view ou
uma imagem externa. A fonte externa é selecionada em uma sobreposição ImGui sobre
o viewport, com escala e rotação independentes; “tatuagem” é apenas o caso de uso
de aplicar essa fonte em uma mesh.

## Estado para entrega via PR

Esta revisão representa um editor funcional para pintura não destrutiva de uma
textura UV de uma mesh 3D. O escopo está adequado para entrega ao `master` e
inclui:

- abertura de MSH, seleção de frame/subset/textura/canal UV e preview privado;
- `UV Brush`, `Project/Stencil`, `Surface 3D`, borracha, picking de cor e carimbo;
- formas circular e quadrada com máscaras coerentes no cursor, rasterização normal,
  projeção e `Surface 3D`;
- padding de seams com aplicação adiada até o fim do arrasto do slider;
- filtro por subset e ilha UV, destaque de bordas UV e aviso de UVs sobrepostos;
- undo/redo, cancelamento de stroke e preview incremental por regiões sujas;
- salvamento transacional de projeto, camada PNG, textura composta e MSH derivada;
- carimbo por imagem externa centralizada, seleção quadrada/circular, alpha,
  escala proporcional e rotação;
- resolução de captura da view com limite padrão de 257 e máximo de 513 amostras
  por lado; imagens externas preservam a resolução nativa do recorte.

### Evidências desta revisão

- A pintura normal quadrada foi validada sem lacunas no quad dividido por diagonal.
- `Surface 3D` quadrado foi validado em mesh e apresentou o footprint esperado.
- O comportamento do padding em seams foi comparado com valores 0 e 6 e produziu
  a expansão esperada.
- A importação de imagem externa, seleção do recorte e rotação do carimbo foram
  validadas manualmente.
- No Windows, a importação externa em resolução nativa do monitor e em resolução
  menor foi aceita: a imagem permanece inteira na view, e a confirmação do
  recorte (forma, confirmar e cancelar) permanece totalmente visível após a
  seleção.
- Os testes Lua de IO, modelo, picking, projeto, rasterização e carimbo passaram.
- A checagem de sintaxe dos módulos Lua e `git diff --check` passaram.

### Follow-ups que não bloqueiam este PR

- decidir e implementar a persistência/restauração automática da imagem externa e
  do recorte, sem incorporar a imagem ao arquivo `.meshpaint.lua`;
- ampliar o editor para múltiplas camadas, canais de material e pincéis avançados.

O PR não deve prometer esses follow-ups como parte do estado atual. Eles permanecem
registrados como evolução posterior do editor.

## 1. Objetivo

Criar um editor independente, baseado em Lua e ImGui, para retocar a textura de
uma mesh 3D diretamente sobre a superfície renderizada. O caso inicial é abrir um
personagem humano, orbitar até o rosto e pintar uma tatuagem, uma correção de cor
ou a remoção de um pequeno detalhe.

O editor deve manter a relação entre três coisas:

1. o ponto atingido na superfície 3D;
2. as coordenadas UV do triângulo atingido;
3. os pixels da textura material que será exportada.

A geometria da mesh não é alterada pela pintura. A textura de cor/base original é
somente leitura; a pintura acontece em uma camada transparente de trabalho que é
composta sobre ela no preview e achatada apenas durante a exportação. Os demais
dados da mesh são preservados.

## 2. Escopo inicial

### Incluído no MVP

- Abrir uma MSH com `meshDebug` e criar uma prévia privada no viewport.
- Inspecionar frames, subsets, materiais e texturas primárias referenciadas pela
  mesh.
- Selecionar explicitamente um alvo de pintura identificado por
  `(frame, subset/material, texture_role, uv_channel)`.
- Manter a textura original bloqueada e criar uma camada transparente de pintura.
- Orbitar, deslocar e aproximar a câmera 3D.
- Obter um raio de seleção com `mbm.getPickRay` e atingir o triângulo visível
  mais próximo.
- Mostrar o cursor do pincel sobre a superfície atingida.
- Pintar com cor, opacidade, raio e dureza ajustáveis no modo `UV Brush`.
- Projetar uma cor ou imagem simples a partir da câmera no modo `Project/Stencil`.
- Apagar a contribuição da camada de pintura, revelando a textura base original.
- Aplicar padding configurável em texels transparentes próximos da pintura para
  reduzir bleeding em seams e mipmaps.
- Desfazer e refazer por operação de pincel, sem reverter a câmera ou controles de
  visualização.
- Atualizar a prévia da mesh somente depois de uma alteração real nos pixels.
- Exportar uma nova imagem PNG e uma nova MSH apontando para ela.
- Salvar um projeto textual versionado para reabrir a fonte, a textura de trabalho
  e as configurações do editor.
- Recusar assets sem UVs válidos ou sem uma textura editável, com mensagem clara.

### Fora do MVP

- Esculpir ou mover vértices.
- Pintar pesos esqueléticos, normais ou posições de ossos.
- Criar UVs, desemaranhar a mesh ou corrigir seams automaticamente.
- Pincel de clonagem, smudge, blur e filtros complexos.
- Camadas múltiplas, máscaras e composição não destrutiva avançada.
- Pintura 3D simultânea em várias texturas de material.
- Pintura de normal, roughness, specular, emissive ou outros canais PBR.
- Pintura por pressão de tablet.
- Pintura em animação deformada ou em vários frames com uma única pincelada.
- Exportação de um formato de textura diferente de PNG.

Esses recursos podem ser adicionados depois, mas não devem contaminar o primeiro
contrato de dados nem esconder as limitações de UV da mesh.

## 3. Decisões de produto

### 3.1 Pintura de textura, não vertex color

O resultado deve ser uma alteração dos pixels da textura escolhida. Vertex color
não é suficiente para uma tatuagem detalhada e não existe garantia de que todos os
materiais/shaders da engine a interpretem da mesma forma.

### 3.2 Camada de pintura

O arquivo de trabalho deve manter a textura base original e uma camada RGBA8
transparente na mesma resolução. O preview compõe as duas imagens; o exportador
gera uma textura achatada ou, quando suportado pelo material, preserva a camada
como uma referência separada.

O pincel é configurado em pixels da textura, mas também exibe o tamanho projetado
no viewport. Assim, o usuário sabe tanto a precisão do arquivo quanto o tamanho
aparente na superfície. A borracha limpa a camada de pintura e não altera a
textura base.

### 3.3 Identidade do alvo

Uma textura pode ser usada por vários subsets e um material pode possuir papéis
distintos. O alvo ativo deve ser tratado como uma tupla estável:

```text
(frame, subset/material, texture_role, uv_channel)
```

O MVP limita a edição a uma textura de cor/base por vez, mas o modelo deve guardar
essa identidade desde o início para não misturar pinturas quando a mesma imagem
aparecer em mais de um material ou frame.

### 3.4 Mapeamento do pincel

O MVP oferece três modos complementares:

- `UV Brush`: converte o hit para UV e rasteriza o pincel nessa região. Para o
  dab quadrado, a área visível é amostrada por raios no footprint projetado e
  cada hit é convertido para UV; isso evita que um único quadrado UV apareça
  como um polígono apenas por causa da triangulação local;
- `Project/Stencil`: projeta uma cor ou imagem pela câmera, respeitando a face
  visível por padrão, com opção explícita de pintar através;
- `Surface 3D`: coleta triângulos próximos em espaço do mundo e rasteriza seus
  pontos mais próximos em UV, reduzindo a dependência da projeção da câmera.
  A máscara é avaliada por texel dentro dos triângulos. O círculo usa distância
  euclidiana na superfície; o quadrado usa a métrica `max(abs(tangente),
  abs(bitangente))` em uma base tangente derivada do triângulo atingido.
  Assim, a forma não é reduzida a um círculo quando o usuário escolhe quadrado.

Quando uma textura é compartilhada por vários subsets, o painel oferece `Limitar
ao subset`. Com o filtro ativo, picking, pintura, Surface 3D, Project/Stencil e
overlay UV usam somente os triângulos do subset escolhido; o estado é persistido
no projeto Lua. O painel também oferece `Limitar a ilha UV`; ilhas são separadas
por seams e só conectam triângulos que compartilham posição e UV.

Um pincel de superfície 3D que acompanha a curvatura e atravessa seams será
avaliado depois do MVP. Triplanar fica reservado para materiais repetitivos e não
é requisito para tatuagens ou decals localizados.

### 3.5 Superfície visível por padrão

Uma pincelada começa pelo triângulo mais próximo atingido pelo raio. O editor não
deve pintar através do rosto ou do corpo por acidente. A versão inicial pode
oferecer uma opção explícita `Pintar através` para atingir superfícies posteriores,
mas ela deve começar desligada e ser claramente indicada no cursor.

### 3.6 Preservação dos originais

Salvar deve sugerir novos nomes, por exemplo:

```text
character.msh       -> character_painted.msh
character_base.png  -> character_base_painted.png
character_painted.meshpaint.lua
```

O editor não deve sobrescrever a MSH ou a imagem de origem sem uma confirmação
explícita. A MSH exportada deve manter a geometria, índices, UVs, animações,
materiais e demais referências não editadas.

## 4. Fluxo de uso proposto

1. O usuário escolhe **Mesh Texture Paint** no launcher ou abre o script
   `editor/mesh_texture_paint_editor.lua` diretamente.
2. Em **Arquivo > Abrir mesh**, seleciona uma MSH.
3. O editor lista frames, subsets/materiais, papéis de textura e o canal UV
   associado. O usuário seleciona uma textura de cor/base; os demais canais
   aparecem como somente leitura no MVP.
4. A mesh aparece no viewport 3D com iluminação de inspeção e opção de exibir
   wireframe, UV seams, backfaces e a camada de pintura.
5. O usuário escolhe `UV Brush` ou `Project/Stencil`, orbita até a região desejada,
   configura cor/imagem, raio, dureza e opacidade, e arrasta o botão direito sobre
   a superfície. `Pintar através` permanece desligado por padrão.
6. Cada stroke é aplicado somente à camada transparente e aparece na composição
   do preview após o upload da região alterada. O botão esquerdo continua reservado
   para órbita quando o cursor não está sobre uma janela ImGui.
7. A borracha remove a contribuição da camada e revela a textura original.
8. **Ctrl+Z** e **Ctrl+Y** desfazem/refazem strokes completos. A alteração só entra
   no histórico quando o arrasto termina. `Esc` cancela a pincelada, restaura seus
   tiles e não cria uma entrada no histórico.
9. O usuário pode abrir o painel **UV/Texture** para revisar o resultado no plano,
   visualizar a área atingida e localizar seams ou ilhas sobrepostas.
10. **Salvar textura** compõe base e camada e escreve o PNG de trabalho. **Salvar
    mesh como...** grava uma MSH e exporta automaticamente, no mesmo diretório e
    com o mesmo nome-base, o PNG que ela referencia. **Salvar projeto** preserva a
    base, a camada e as escolhas para continuar depois.

## 5. Interface do editor

### Viewport 3D

- órbita no botão esquerdo quando nenhuma ferramenta de pintura está capturando o
  mouse;
- deslocamento no botão central;
- zoom na roda;
- pincel no botão direito;
- marcador do ponto de hit, normal aproximada e raio projetado;
- indicação visual distinta para hit válido, textura sem UV e face posterior;
- modos opcionais de textura final, albedo plano, wireframe e UV seams;
- iluminação apenas de prévia, sem alterar o material salvo.

### Painel de pintura

- textura selecionada, material/subset, frame e canal UV;
- camada base bloqueada e camada de pintura ativa;
- modo: UV Brush, Project/Stencil ou Borracha;
- imagem/cor do stencil, quando aplicável;
- fonte do carimbo: trecho da textura atual ou textura externa;
- escala e rotação do carimbo externo, independentes do raio do dab;
- limite padrão de 257 amostras por lado para carimbos capturados da view,
  com limite configurável de 513 (equivalente simétrico de uma referência 512);
  a captura ainda é limitada pelo diâmetro real do pincel; carimbos importados
  preservam a resolução nativa do recorte selecionado;
- forma do pincel: círculo ou quadrado;
- cor RGBA;
- raio em pixels;
- opacidade;
- dureza/falloff;
- dilatação de seams;
- o slider de dilatação mostra o valor pendente durante o arrasto, mas só aplica
  a dilatação, recomposição e atualização da prévia após o fim da edição;
- toggle Pintar através;
- toggle Limitar ao subset selecionado;
- Undo/Redo e contador de alterações;
- aviso quando a textura é compartilhada por mais de um subset ou por mais de um
  asset.

### Painel UV/Texture

- preview da textura editada em 2D;
- overlay opcional dos triângulos UV da mesh selecionada, com orçamento limitado
  de desenho para não elevar a CPU durante a edição parada e destaque automático
  das bordas das ilhas UV;
- overlay do último hit e da área do pincel;
- zoom e pan independentes;
- opção de mostrar apenas a ilha UV selecionada;
- destaque da ilha UV sob o cursor e seleção direta pela viewport com
  `Ctrl + botão direito`, sem pintar;
- indicação de texels que são compartilhados por ilhas sobrepostas.
- opção de visualizar base, camada de pintura ou composição final.

### Fonte externa do carimbo

- comando para carregar uma textura raster externa e abri-la centralizada na view;
- janela ImGui sobre toda a área do viewport, que captura mouse, zoom e teclado
  enquanto a fonte está sendo escolhida; nenhum evento é encaminhado à view 3D;
- seleção por quadrado ou círculo, com confirmação, cancelamento e Escape;
- prévia da seleção sobre a imagem, incluindo alpha e checkerboard;
- cópia do recorte para uma fonte de carimbo privada, sem alterar a textura
  externa;
- escala independente do raio do dab, preservando a proporção do recorte; a
  escala representa a extensão completa da imagem selecionada, incluindo o
  alpha transparente que o usuário manteve no recorte;
- rotação em graus ao redor do centro da fonte, preservada ao aplicar na mesh;
- aplicação do carimbo na superfície usando os UVs do hit, com recorte, alpha,
  proporção e orientação preservados;
- reutilização do modo `Stamp` para tatuagens, decals e logos, sem criar um modo
  especial de “tatuagem”;
- limpeza da fonte externa ao trocar de mesh, textura ou projeto;
- persistência opcional do caminho da textura e do recorte no projeto, sem copiar
  a imagem inteira para o arquivo `.meshpaint.lua`.

A forma da seleção da fonte não substitui a forma do pincel. O recorte pode ser
circular ou quadrado; o dab de destino também pode ser circular ou quadrado, com
a máscara de alpha do recorte preservada. Assim, o usuário pode selecionar um
dragão inteiro, ajustar escala para o pescoço e girá-lo sem alterar a imagem
externa.

### Arquivo e projeto

- Abrir mesh;
- Abrir projeto `.meshpaint.lua`;
- Salvar textura;
- Salvar mesh como;
- Salvar projeto;
- Recarregar fonte e descartar alterações;
- exportar texturas portáveis quando a MSH depender de caminhos externos.

Todos os textos renderizados pelo ImGui devem respeitar a regra de pontuação ASCII
segura da engine.

## 6. Arquitetura técnica proposta

### 6.1 Editor e módulos Lua

O editor deve seguir o padrão dos demais editores Lua: script de cena independente,
`editor_utils`, `lang.language`, callbacks globais da engine e `dpCall` para chamadas
protegidas.

Estrutura sugerida:

```text
editor/mesh_texture_paint_editor.lua  -- cena, menus, janelas e callbacks
editor/mesh_texture_paint_model.lua   -- estado, seleção e dirty flags
editor/mesh_texture_paint_raster.lua  -- camadas, dabs, blend, dilatação e tiles
editor/mesh_texture_paint_pick.lua    -- raios, triângulos, barycentric e UV
editor/mesh_texture_paint_project.lua -- formato .meshpaint.lua versionado
editor/mesh_texture_paint_io.lua      -- publicação e recuperação dos arquivos
editor/mesh_texture_paint_preview.lua -- textura privada e atualização da mesh
```

O script principal deve manter poucas variáveis locais no chunk. Helpers de
geometria, rasterização e persistência devem ficar em módulos, respeitando o limite
de 200 locais do Lua 5.4 e facilitando testes puros.

### 6.2 Reuso da engine existente

O plano parte das capacidades já presentes no repositório:

- `meshDebug:loadMeshPreview` para uma prévia privada e substituível;
- `meshDebug:getTotalFrame`, `getTotalSubset`, `getTotalVertex`, `getVertex`,
  `getIndex` e `getTexture` para construir o snapshot de picking;
- `meshDebug:setTexture` e `meshDebug:save` para aplicar a referência exportada;
- `mbm.getPickRay` para converter o cursor em raio 3D;
- `mbm.readImagePixels` e `mbm.writeImagePixels` para o buffer e o export PNG;
- `textureInfo:reload` ou slots temporários privados para atualizar a prévia;
- `tImGui.Image` para a revisão 2D da textura.

O modelo deve distinguir `sourceTexture`, `paintLayer` e `compositedTexture`. A
primeira é imutável durante a sessão; a segunda recebe os strokes; a terceira é
um cache de preview/exportação. Nenhuma operação de borracha deve escrever na
fonte original.

O editor não deve usar o objeto de jogo compartilhado nem alterar texturas do cache
global. Arquivos temporários devem ser privados, ter nomes únicos e ser liberados
no fechamento ou na troca de asset.

### 6.3 Picking e conversão para UV

Na abertura da mesh, o editor constrói um snapshot somente leitura por frame/subset:

- posições dos vértices;
- UVs;
- índices ou triângulos implícitos de listas sem índice;
- normal opcional;
- textura primária, papéis de material, canal UV e identidade do alvo;
- limites espaciais.

Para cada stroke, o raio é testado contra uma estrutura espacial em cache. O hit
retorna triângulo, distância, coordenadas barycentric e UV interpolado. O primeiro
protótipo pode usar uma grade/BVH Lua para validar o contrato; a implementação de
produção deve medir o custo com meshes grandes e migrar a interseção para C++ caso
o picking deixe de ser responsivo.

O hit deve rejeitar triângulos sem UV finita. Winding, backface culling e tolerância
de interseção precisam ser centralizados em um módulo, pois a mesma convenção será
usada no overlay do viewport e nos testes.

### 6.4 Rasterização da pincelada

Cada stroke é uma sequência de dabs amostrados ao longo do movimento do mouse e
é gravado apenas na camada de pintura. A composição com a fonte ocorre no preview
e na exportação. Cada dab contém:

- centro em UV;
- raio na textura;
- cor e alfa;
- dureza;
- forma (`circle` ou `square`);
- modo de composição;
- triângulo/material de origem;
- modo de mapeamento (`uv` ou `project`);
- identificador do stroke.

No `UV Brush`, `Erase`, `Stamp` e na cobertura UV de `Project/Stencil`, a forma
`circle` usa distância radial e `square` usa a maior distância axial. Para
`Paint` e `Erase` quadrados, a cobertura é amostrada no footprint projetado e
convertida para UV antes da rasterização. O modo `Surface 3D` usa distância
euclidiana para círculo e distância L-infinito em uma base tangente local para
quadrado. Uma área fora da geometria real continua sendo corretamente recortada;
o editor não deve preencher espaço que não pertence à mesh.

No MVP, o `UV Brush` circular é rasterizado no espaço UV local do triângulo
atingido; o quadrado e o `Project/Stencil` usam a projeção da câmera para
produzir contribuições UV. Todos devem tratar explicitamente os efeitos
conhecidos:

- distorção do círculo em UV quando a superfície está inclinada;
- seams que interrompem a pintura;
- ilhas UV sobrepostas que alteram mais de uma região física;
- resolução insuficiente da textura para detalhes menores que um texel;
- triângulos muito pequenos ou degenerados.

Após rasterizar, o editor deve dilatar a camada nas bordas das ilhas UV afetadas.
O raio de dilatação precisa ser configurável e limitado ao tile/ilha relevante,
evitando uma varredura completa da textura a cada dab.

Quando uma cobertura for construída por vários samples projetados, o raio UV de
cada sample deve ser derivado do jacobiano UV local e do espaçamento na tela,
com sobreposição suficiente para não deixar lacunas. Um raio fixo baseado apenas
na quantidade de samples produz padrões tracejados em texturas com densidade UV
alta ou não uniforme.

Depois da prova de conceito, comparar a qualidade de duas estratégias adicionais:

1. **UV local**: simples e previsível, adequada ao MVP em UVs bem abertas;
2. **projeção por câmera/stencil**: adequada a decals e tatuagens localizadas, mas
   dependente de resolução, oclusão e continuidade da projeção;
3. **superfície 3D**: coleta triângulos próximos ao ponto, projeta a influência na
   superfície e rasteriza cada contribuição UV; melhor em áreas curvas, mas mais
   cara e sensível a seams.

O pincel circular usa distância radial e falloff. O pincel quadrado usa a maior
distância entre os eixos locais do dab, mantendo o mesmo raio, dureza e opacidade.
Cursor, pintura normal, `Surface 3D`, borracha, clipping, alpha blend, tiles sujos
e Undo/Redo devem compartilhar a mesma definição de forma; a forma não pode
alterar a textura base.

A decisão deve ser baseada em fixtures de rosto, mão e personagem completo, não em
uma esfera sintética isolada.

### 6.5 Buffer e atualização da prévia

O buffer de trabalho deve permanecer em memória entre strokes. Nunca se deve reler
e redecodificar a imagem a cada movimento do mouse.

A atualização mínima aceitável é por regiões sujas:

1. rasterizar apenas os texels do retângulo afetado;
2. registrar tiles sujos;
3. atualizar a prévia após o dab ou após um pequeno lote de dabs;
4. evitar `writeImagePixels`, recarregamento e alocação de toda a imagem em cada
   frame ocioso.

Controles que acionam dilatação global, como o padding de seams, devem separar o
valor exibido do valor aplicado. Durante o arrasto, apenas o valor pendente muda;
`Raster.dilate`, composição e reload da textura só podem começar depois que o
item ImGui for desativado após a edição.

O plano trata o desempenho por tiles como gate da Fase 0, não apenas como melhoria
posterior. Se o caminho Lua com tabelas de bytes não sustentar uma textura
4096x4096 com arrasto contínuo, criar uma operação nativa de rasterização em tiles.
Essa operação deve receber um buffer ou handle privado, devolver as regiões sujas
e não expor estado mutável novo no header público sem necessidade.

Para o preview, usar um PNG temporário privado recarregado somente quando houver
alteração confirmada. Se a latência for perceptível, a segunda etapa deve permitir
upload de sub-região ou textura editável nativa; não mascarar o problema fazendo
rebuild contínuo da mesh.

## 7. Formato do projeto `.meshpaint.lua`

O formato deve ser textual, versionado e conter somente uma tabela Lua literal.
O carregador usa um ambiente vazio, valida o resultado e não permite que o
projeto acesse os globais do editor. O arquivo deve referenciar arquivos, não
incorporar a MSH ou a textura inteira.

Campos mínimos:

```lua
return {
    version = 1,
    source_mesh = "...",
    source_texture = "...",
    paint_layer = "...",
    working_texture = nil,       -- cache achatado opcional
    frame = 1,
    subset = 1,
    texture_role = "base_color",
    uv_channel = 0,
    projection_mode = "uv_brush",
    dilation_radius = 0,
}
```

O projeto também pode guardar preferências de viewport, pincel e stencil. A camada
de pintura é a fonte persistente das alterações; `working_texture` é apenas um
cache achatado que pode ser regenerado. O projeto não deve guardar cache de GPU,
BVH, jobs ativos ou o histórico completo de pixels.

Caminhos relativos devem ser usados quando os arquivos estiverem sob uma raiz
comum do projeto. A implementação atual grava caminhos absolutos para manter o
primeiro ciclo determinístico. O carregamento valida versão, chaves conhecidas,
caminhos, existência da camada e compatibilidade de dimensões antes de alterar
o estado atual.

## 8. Histórico, cancelamento e atomicidade

- Um stroke completo é uma unidade de Undo.
- A composição do preview é um job cooperativo por fases; `Esc` pode cancelá-lo
  entre fases, preservando a última prévia válida e as pinturas pendentes.
- Para texturas grandes, o histórico deve guardar tiles anteriores e posteriores,
  não duas cópias completas da camada.
- `Ctrl+Z` deve restaurar pixels da camada de pintura e marcar composição e preview
  como sujos.
- Salvar textura escreve primeiro em um temporário privado no diretório do destino.
- Salvar projeto prepara a camada PNG e o arquivo Lua antes de substituir qualquer
  destino. Retorno `false`/`nil` de um escritor, além de exceções, cancela a gravação.
- Salvar MSH usa uma cópia candidata do snapshot da fonte em memória, altera apenas
  as referências da textura selecionada e valida a candidata. A MSH e seu PNG são
  preparados antes da publicação, sem modificar os materiais do preview.
- Ao substituir arquivos existentes, o editor mantém backups privados até concluir
  todo o conjunto. Uma falha de publicação restaura os arquivos anteriores e não
  atualiza os caminhos salvos nem limpa a indicação de alterações pendentes.
- Se a restauração também falhar, o backup é preservado e seu caminho aparece no
  diagnóstico. Essa garantia cobre erros retornados pelas operações de I/O; não é
  uma transação de filesystem resistente à queda do processo/energia, nem oferece
  publicação simultânea dos dois arquivos para leitores externos.
- Trocar mesh, textura ou projeto deve cancelar a operação em andamento e liberar
  o preview privado antes de remover temporários.
- Abrir projeto deve concluir a validação de todos os arquivos e da identidade do
  alvo (mesh, frame, textura, subset e ilha UV) antes de `loadMesh`; falhas de
  validação não podem substituir a sessão corrente.
- Fechar o editor com alterações deve oferecer salvar, descartar ou cancelar.

## 9. Fases de implementação

### Fase 0 - Spike de viabilidade

- Escolher três fixtures: plano UV, rosto com seams e personagem com várias
  texturas/subsets.
- Carregar a MSH, extrair UVs e fazer picking do triângulo mais próximo.
- Pintar um dab em uma camada transparente e validar a conversão barycentric -> UV.
- Projetar um stencil simples a partir da câmera e comparar o resultado com o modo
  UV local.
- Verificar dilatação em seams e o comportamento de ilhas UV sobrepostas.
- Medir custo de rasterização e atualização da prévia em 1024, 2048 e 4096 px.
- Decidir se o rasterizador Lua é suficiente ou se a Fase 1 já exige raster C++ em
  tiles.

Saída: nota de decisão, limites medidos, política de camadas, limites de projeção e
uma lista de meshes incompatíveis.

### Fase 1 - MVP editável

- Criar o script registrado no launcher.
- Implementar abertura de MSH, seleção do alvo de textura e viewport 3D.
- Implementar textura base bloqueada, camada de pintura e composição de preview.
- Implementar picking, UV Brush, Project/Stencil, borracha, cor, opacidade, raio e
  dureza.
- Implementar padding de seams, destaque das bordas UV e aviso para UVs
  sobrepostos, com limite de segurança na análise de meshes densas.
- Implementar preview privado, dirty flags e Undo/Redo por stroke.
- Implementar UV/Texture view.
- Implementar exportação PNG, MSH e projeto `.meshpaint.lua`.
- Adicionar strings em `editor/lang/language.lua` em inglês e pt-BR.

### Fase 2 - Robustez de autoria (escopo deste PR parcialmente concluído)

- Pintura por superfície 3D para reduzir artefatos em curvaturas (primeira versão
  experimental implementada; ainda requer validação em meshes com partes muito
  próximas).
- Máscara por material (os filtros por subset e ilha UV já estão disponíveis).
- Forma de dab circular ou quadrada implementada, com cursor e rasterização
  coerentes; a escolha é persistida no projeto. A forma quadrada deve ser
  validada tanto na projeção normal quanto na base tangente de `Surface 3D`.
- Carimbo por textura externa implementado: seleção ImGui centralizada de recorte
  circular ou quadrado, fonte privada reutilizável, captura de eventos, escala
  proporcional e rotação.
- Captura de carimbo da view usa limite de 257 amostras por lado por padrão e
  aceita até 513 amostras por lado para áreas grandes; o limite continua sujeito
  ao diâmetro do pincel e ao custo quadrático da amostragem.
- Recuperação de gravação após erro de I/O implementada para projeto + camada,
  MSH + textura e PNG isolado. Abertura de projeto valida os arquivos e o alvo
  antes de substituir a sessão atual; projetos com textura, subset, ilha ou frame
  incompatíveis são rejeitados sem alterar a pintura aberta. Undo/Redo por tiles,
  cancelamento de pincelada e cancelamento do preview já estão disponíveis.
- Múltiplas camadas de pintura, máscaras e composição normal, alpha e multiplicação.
- Importação/exportação de um conjunto de texturas relacionadas.

### Fase 3 - Extensões de material

- Pintura de normal, roughness/specular/emissive/mask quando o material e o shader
  suportarem esses canais.
- Pincéis de clonagem, smudge, blur e preenchimento.
- Simetria por eixo e espelhamento por espaço UV.
- Suporte a pressão/tilt quando a camada de input da plataforma fornecer esses
  dados.
- Pintura em poses de animação com decisão explícita sobre espaço bind ou espaço
  deformado.

## 10. Validação e testes

### Testes puros Lua

- Conversão barycentric para UV em triângulos orientados de todas as formas.
- Interseção de raio com triângulos, backface culling e escolha do hit mais próximo.
- Rasterização de círculo, falloff, alpha blend, camada transparente e borracha não
  destrutiva.
- Rasterização equivalente para dabs circulares e quadrados, mantendo dureza,
  alpha, clipping e tiles sujos.
- Pintura e borracha normais quadradas em um quad dividido por diagonal, com
  confirmação de que a máscara projetada não vira um polígono por causa de uma
  única triangulação.
- Pintura normal quadrada em uma fixture plana deve permanecer sólida dentro do
  footprint, sem lacunas entre os samples projetados.
- `Surface 3D` circular e quadrado na mesma fixture plana e em uma fixture
  inclinada, verificando a métrica radial versus L-infinito tangente.
- Projeção de stencil, oclusão e modo Pintar através.
- Dilatação de seams e composição base + camada.
- Clipping de dabs nas bordas da textura.
- UV seams, ilhas sobrepostas e triângulos degenerados.
- Undo/Redo de vários strokes e descarte de histórico após novo stroke.
- Leitura/escrita do projeto, rejeição de versão e caminhos inválidos.

### Smoke tests do editor

- Abrir uma fixture MSH com textura RGBA e pintar um dab conhecido.
- Confirmar que a textura exportada tem pixels esperados e dimensões originais.
- Reabrir a MSH exportada e verificar que geometria, UVs, índices e animações não
  mudaram.
- Confirmar que a imagem original permanece byte a byte intacta.
- Trocar textura, cancelar uma operação e fechar o editor sem vazamento de
  temporários.
- Carregar uma textura externa, cancelar e confirmar recortes circular e quadrado,
  aplicar o carimbo com escala e rotação e confirmar que a fonte externa não foi
  alterada nem recebe eventos da view durante a seleção.
- Manter a CPU quase ociosa quando o editor estiver parado, sem rasterização,
  reload ou rebuild contínuo.

### Validação visual e de plataforma

Evidência do milestone de salvamento:

- `src/test-lib/mesh_texture_paint_io_test.lua`: arquivos existentes e novos,
  falhas de escrita/renomeação, restauração e retenção do backup quando a própria
  recuperação falha. O teste também simula a recusa de renomear sobre um destino
  existente, como ocorre no Windows; isso não substitui validação nativa nele.
- `src/test-lib/mesh_texture_paint_project_test.lua`: persistência do formato Lua
  e rejeição de dados inválidos.
- `src/test-lib/mesh_texture_paint_preview_smoke.lua`, com
  `mesh_texture_paint_save_checks.lua`: pintura renderizada, gravação e reabertura
  de PNG/projeto/MSH, preservação de vértices, UVs, animação e material não editado,
  fontes intactas, sessão preservada após falhas simuladas de I/O e rejeição de
  projetos inválidos (textura, subset, frame, ilha UV e dimensões). O preview
  continua sem rebuild/upload durante os frames ociosos; o segundo stroke usa o
  dab quadrado e a reabertura confirma a forma persistida no projeto. O smoke
  deve também comparar a área pintada normal quadrada com a circular e exercitar
  `Surface 3D` quadrado.

Status de execução dos backends:

- Linux/OpenGL ES: validado.
- macOS/Metal: validado.
- Windows/DirectX 9 e DirectX 11: validado e aceito; o fluxo de importação de
  imagem externa foi verificado em resolução nativa e menor, incluindo a
  visibilidade da imagem e dos controles de confirmação do recorte.

Casos aceitos no Windows nesta revisão:

- textura sem alpha e com alpha parcial;
- UV invertido e seams;
- múltiplos subsets e preview render-to-texture;
- reload da textura sem rebuild contínuo da mesh.

Pendências de autoria que seguem depois do gate de backend:

- persistir e restaurar a descrição do recorte sem incorporar a imagem ao projeto;
- ampliar a matriz de validação de carimbos com alpha, UV invertido, seams,
  escala e rotação entre os backends, sem bloquear o estado aceito desta revisão;
- decidir se a persistência deve reabrir automaticamente a imagem externa ou
  apenas oferecer a descrição para uma nova seleção explícita.

## 11. Riscos e decisões em aberto

| Risco | Impacto | Mitigação planejada |
|---|---|---|
| UVs ruins ou ausentes | A pintura não representa a superfície corretamente | Diagnóstico de UV e bloqueio de assets sem UV válido |
| Seams visíveis | Um stroke pode parar ou saltar entre ilhas | Dilatação no MVP, overlay de seams, modo superfície e máscara explícita |
| Ilhas UV sobrepostas | Uma alteração afeta regiões físicas inesperadas | Aviso, seleção de ilha e opção de limitar ao triângulo |
| Texturas grandes | Travamentos durante rasterização/upload | Gate de tiles na Fase 0, orçamento por frame e raster nativo se necessário |
| Borracha destrutiva | Perda da textura base e dificuldade de edição | Fonte bloqueada e borracha limitada à camada de pintura |
| Textura compartilhada | Alteração acidental em outros assets | Copiar para textura de trabalho e exportar referências novas |
| Muitos frames/subsets | Ambiguidade sobre qual imagem deve ser editada | MVP restrito a uma textura selecionada; política posterior explícita |
| Diferença entre preview e runtime | Tatuagem pode parecer diferente no jogo | Preview usa material real quando possível e documenta limitações de shader |
| Backends diferentes | Reload e alpha podem divergir | Fixtures de smoke e matriz mínima de Linux, Windows e macOS |
| Fonte externa do carimbo | Recorte pode perder alpha, orientação ou escala | Buffer privado RGBA, overlay modal com captura de eventos, preview com checkerboard, escala proporcional, rotação e fixtures de recorte |
| Forma do dab | Cursor e pixels podem representar formas diferentes, especialmente entre UV e superfície | Compartilhar a mesma máscara de forma entre cursor, raster, projeção tangente e testes |

Decisões que devem ser fechadas na Fase 0:

1. rasterização Lua, nativa ou híbrida e se tiles nativos já são necessários no
   MVP;
2. representação persistente da camada: PNG RGBA, conjunto de tiles ou formato
   próprio;
3. canal UV padrão e como expor canais adicionais se a API da mesh os fornecer;
4. política para textura compartilhada entre subsets;
5. se o editor aceita somente frame 1 ou permite selecionar frames com texturas
   independentes;
6. limites do `Project/Stencil` para o primeiro release.

## 12. Critério de pronto do MVP

O MVP estará pronto quando um usuário conseguir abrir uma MSH real com UVs,
selecionar o alvo de textura de cor do rosto, pintar uma tatuagem contínua usando
UV Brush ou Project/Stencil, apagar parte dela sem alterar a textura base,
desfazer/refazer o stroke, salvar uma nova PNG e uma nova MSH, reabrir o resultado
e verificar que a geometria e a animação originais continuam intactas.

Além disso, o editor deve permanecer sem trabalho caro quando estiver ocioso,
preservar os arquivos de origem por padrão e informar claramente quando um
resultado é limitado por UVs, seams, resolução ou material compartilhado.

## 13. Benchmark de referência

Esta revisão do plano foi confrontada com fluxos documentados de ferramentas
atuais:

- [Unreal Mesh Paint](https://dev.epicgames.com/documentation/unreal-engine/activating-and-using-mesh-paint-mode-in-unreal-engine)
  separa vertex color, weights, texture color e pintura direta de texturas. Sua
  referência também trata pintura por instância, canal UV, recurso de textura,
  seleção de canais e seam painting.
- [Unreal Mesh Paint Tool Reference](https://dev.epicgames.com/documentation/unreal-engine/mesh-paint-tool-reference-in-unreal-engine)
  documenta dilatação de seams, brushes customizados, visualização de canais e
  configuração de textura por canal UV.
- [ArmorPaint Manual](https://armorpaint.org/manual) usa pintura GPU, camadas,
  visualização 2D com wireframe UV, modos UV/project/triplanar, X-Ray, simetria,
  máscaras e raio de dilatação para seams.
- [Blender Texture Slots](https://docs.blender.org/manual/en/5.4/sculpt_paint/texture_paint/tool_settings/texture_slots.html)
  confirma o modelo de selecionar uma imagem/slot associado ao UV ativo para a
  pintura.
- [Substance Material Projections](https://helpx.adobe.com/substance-3d-stager/desktop/features/material-projections.html)
  documenta a diferença entre projeção UV e triplanar e a dependência de UVs para
  decals localizados.

Essas referências não são requisitos para transformar a mini-mbm em uma suíte de
texturização completa. Elas servem para justificar as decisões mínimas de camada
não destrutiva, projeção, seams, identidade do alvo e atualização incremental.
