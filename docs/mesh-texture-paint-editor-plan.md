# Plano: Editor de Pintura de Textura sobre Mesh 3D

Status: em implementação. O milestone vertical atual já abre MSH, faz picking
3D, pinta uma camada UV, mantém preview privado, salva projetos Lua, exporta
textura composta e MSH derivada, mostra a revisão 2D com overlay UV e oferece
projeção de cor sólida pela câmera em superfícies visíveis e padding configurável
para seams, destaca bordas das ilhas UV e avisa sobre sobreposição de UVs;
o modo opcional de pintar através e o modo experimental `Surface 3D` também
estão disponíveis; projeção por imagem continua nas próximas fases.

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

O MVP oferece dois modos complementares:

- `UV Brush`: converte o hit para UV e rasteriza o pincel nessa região;
- `Project/Stencil`: projeta uma cor ou imagem pela câmera, respeitando a face
  visível por padrão, com opção explícita de pintar através;
- `Surface 3D`: coleta triângulos próximos em espaço do mundo e rasteriza seus
  pontos mais próximos em UV, reduzindo a dependência da projeção da câmera.
  A máscara agora é avaliada por texel dentro dos triângulos, evitando círculos
  independentes e recortes causados pela triangulação. O modo representa uma
  distância na superfície, não uma forma circular fixa na tela.

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
- cor RGBA;
- raio em pixels;
- opacidade;
- dureza/falloff;
- dilatação de seams;
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
Cada dab contém:

- centro em UV;
- raio na textura;
- cor e alfa;
- dureza;
- modo de composição;
- triângulo/material de origem;
- modo de mapeamento (`uv` ou `project`);
- identificador do stroke.

No MVP, o `UV Brush` é rasterizado no espaço UV local do triângulo atingido e o
`Project/Stencil` usa a projeção da câmera para produzir contribuições UV. Ambos
devem tratar explicitamente os efeitos conhecidos:

- distorção do círculo em UV quando a superfície está inclinada;
- seams que interrompem a pintura;
- ilhas UV sobrepostas que alteram mais de uma região física;
- resolução insuficiente da textura para detalhes menores que um texel;
- triângulos muito pequenos ou degenerados.

Após rasterizar, o editor deve dilatar a camada nas bordas das ilhas UV afetadas.
O raio de dilatação precisa ser configurável e limitado ao tile/ilha relevante,
evitando uma varredura completa da textura a cada dab.

Depois da prova de conceito, comparar a qualidade de duas estratégias adicionais:

1. **UV local**: simples e previsível, adequada ao MVP em UVs bem abertas;
2. **projeção por câmera/stencil**: adequada a decals e tatuagens localizadas, mas
   dependente de resolução, oclusão e continuidade da projeção;
3. **superfície 3D**: coleta triângulos próximos ao ponto, projeta a influência na
   superfície e rasteriza cada contribuição UV; melhor em áreas curvas, mas mais
   cara e sensível a seams.

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
- Para texturas grandes, o histórico deve guardar tiles anteriores e posteriores,
  não duas cópias completas da camada.
- `Ctrl+Z` deve restaurar pixels da camada de pintura e marcar composição e preview
  como sujos.
- Salvar textura deve escrever primeiro em arquivo temporário e substituir o destino
  somente após a codificação concluir.
- Salvar MSH deve usar uma cópia candidata e publicar o arquivo apenas depois de a
  validação terminar.
- Trocar mesh, textura ou projeto deve cancelar a operação em andamento e liberar
  o preview privado antes de remover temporários.
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

### Fase 2 - Robustez de autoria

- Pintura por superfície 3D para reduzir artefatos em curvaturas (primeira versão
  experimental implementada; ainda requer validação em meshes com partes muito
  próximas).
- Máscara por material (os filtros por subset e ilha UV já estão disponíveis).
- Melhorias adicionais de recuperação após erro de I/O (Undo/Redo por tiles e
  cancelamento transacional de pincelada já estão disponíveis).
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
- Manter a CPU quase ociosa quando o editor estiver parado, sem rasterização,
  reload ou rebuild contínuo.

### Validação visual e de plataforma

- Linux/OpenGL ES no primeiro ciclo automatizado.
- Windows/DirectX 9 e DirectX 11 para validar reload, alpha e render target/cache.
- macOS/Metal para validar preview e caminhos de textura.
- Repetir testes com textura sem alpha, alpha parcial, UV invertido, seams e
  materiais com múltiplos subsets.

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
