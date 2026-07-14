# Plano: `mannequin_editor.lua` (Marco 1) + roteiro dos Marcos 2-4

## Contexto

Objetivo final do usuário: carregar uma imagem de referência (foto ou textura/arte estilizada — não
precisa ser fotorrealista) de um personagem em T-pose, marcar pontos anatômicos sobre ela dentro do
próprio editor Lua/ImGui do mini-mbm, gerar um "manequim" 3D de primitivas (esferas nas juntas +
cilindros conectando-as) com UV derivado da própria imagem, e uma hierarquia de armature real o
bastante pra ser "animável" de fato. Esses dados são exportados como JSON e entregues a um script
Blender headless que constrói mesh+armature de verdade e exporta um FBX pronto para upload num
serviço de auto-rig (hoje Mixamo, mas a arquitetura não amarra nisso — ver "Desacoplamento do
Mixamo" abaixo). De lá, a animação baixada volta a entrar no `mesh_debug.lua` via o pipeline
"Import via Blender" que já existe.

Isso substitui duas tentativas anteriores nesta mesma conversa que **não** eram o que o usuário
tinha em mente: (1) um guia de texto dentro do `mesh_debug.lua` sobre o fluxo Mixamo manual, e (2)
um addon Blender (`editor/blender_body_adjust.py`, já criado e funcional) para redimensionar partes
do corpo via escala de ossos. Esses dois artefatos já existentes **não devem ser revertidos**:
- O guia de texto em `mesh_debug.lua` (`tMixamoGuideState`/`showMixamoGuideDialog()`) continua
  válido e deve ser mantido como caminho manual alternativo — o novo editor é um "plus", podendo
  virar o caminho principal no futuro quando estiver maduro o bastante, mas não substitui o guia
  agora.
- O addon `blender_body_adjust.py` tem um bloco de exportação FBX (limpeza de cena + centralização
  + export) que será **reaproveitado no Marco 3**.

Este plano foi revisado numa sessão de "grill" (metodologia da skill `grill-me`,
`.agents/skills/grill-me/SKILL.md`, agora também linkada em `.claude/skills/grill-me` pra ficar
visível ao Claude Code do mesmo jeito que já está pro Codex) que resolveu 7 pontos de mudança
levantados pelo usuário sobre o plano original. As decisões abaixo já refletem esse resultado.

---

## Desacoplamento do Mixamo (achado da sessão de grill, sem pergunta necessária)

A arquitetura já não amarra em Mixamo especificamente: o JSON do Marco 1 usa nomes de junta
genéricos (`chin`, `lShoulder`, `groin`, etc.), e o script do Marco 3 constrói o armature do zero a
partir desses nomes — sem depender de convenção de nomenclatura Mixamo (diferente de
`blender_body_adjust.py`, que sim faz matching de nomes `mixamorig:*`, mas esse é um addon
separado). Mixamo é só o serviço usado hoje pra transformar o FBX resultante em animação —
trocável no futuro por qualquer outro serviço de auto-rig sem tocar nos Marcos 1-3.

---

## Marco 1 — `editor/mannequin_editor.lua` (escopo deste plano, a implementar agora)

Nome do arquivo confirmado: **`mannequin_editor.lua`** (não mais `mannequin_from_photo.lua` — a
ferramenta não é mais restrita a fotos). Segue a convenção deste repo de `<substantivo>_editor.lua`
(`physic_editor.lua`, `tilemap_editor.lua`, `particle_editor.lua`, `scene_editor2d/3d.lua`).

### Decisões confirmadas nesta revisão

1. **Qualquer textura, não só foto.** Caso de teste sugerido pelo usuário: Mario (não será
   commitado no master, só pra validar a progressão do editor em arte estilizada/não-fotorrealista).
2. **Guia de texto do Mixamo em `mesh_debug.lua` permanece** como alternativa/plus — não é
   substituído por este editor agora.
3. **Heurísticas de profundidade/raio viram parâmetros editáveis por marcador na GUI**, em vez de
   constantes fixas assumindo proporção humana. Os valores "humanos" do plano original
   (`fTorsoDepthRatio=0.6`, raios como fração da largura do ombro etc.) viram só um preset inicial
   sugerido, não uma verdade fixa — necessário porque proporções não-humanas (Mario) quebrariam
   qualquer heurística fixa de corpo humano. Isso funde os pontos "GUI pra `tMarkerDefs`" e "suportar
   qualquer textura" do usuário no mesmo trabalho.
4. **Controles de câmera**: clique-esquerdo reservado pra marcar/arrastar pontos; órbita da câmera
   no **botão direito** (diferente do `scene_editor3d.lua`, que usa esquerdo) — decisão deliberada
   pra não arriscar mover a câmera sem querer durante a marcação.
5. **Sem exportação de `.msh` no Marco 1** — isso foi promovido a Marco 2 próprio (ver abaixo), já
   que depende de uma mudança real de core engine, não é só trabalho de editor Lua.
6. **Contorno/silhueta pra recortar fundo (pedido do usuário, ponto 7) fica fora do Marco 1** —
   confirmado que o propósito seria recortar a silhueta da textura pra não vazar fundo nas partes
   visíveis do manequim. Isso é um modo de interação genuinamente distinto (desenho 2D vs. o
   viewport 3D atual) — vira **Marco 4** (ver abaixo), a ser desenhado em detalhe só depois do
   Marco 1-3 validados ponta-a-ponta.

### Arquivo e requires
```lua
-- editor/mannequin_editor.lua
tImGui = require "ImGui"
tUtil  = require "editor_utils"   -- drawOrbitGizmo, dirFromOrbit/orbitFromDir, showMessage, tLang
```

### Estruturas de estado principais
```lua
camera3d = nil
cam3d = { azimuth=0.3, elevation=0.35, distance=900, fx=0, fy=0, fz=0 }
tCam3dMove = { forward=0, right=0 }

tImages = {
    front = { path=nil, tex=nil, w=0, h=0 },
    side  = { path=nil, tex=nil, w=0, h=0, xOffset=-400, facing=1 },  -- lateral opcional
}

tMarkerDefs = { -- ordem fixa, 14 entradas
    {key='chin', label='Queixo', centerline=true},
    {key='lShoulder', label='Ombro E'}, {key='rShoulder', label='Ombro D'},
    {key='lElbow', label='Cotovelo E'}, {key='rElbow', label='Cotovelo D'},
    {key='lWrist', label='Pulso E'}, {key='rWrist', label='Pulso D'},
    {key='groin', label='Virilha', centerline=true},
    {key='lKnee', label='Joelho E'}, {key='rKnee', label='Joelho D'},
    {key='lHandTip', label='Ponta Mão E'}, {key='rHandTip', label='Ponta Mão D'},
    {key='lFootTip', label='Ponta Pé E'}, {key='rFootTip', label='Ponta Pé D'},
}
-- cada marcador agora carrega seus próprios parâmetros editáveis via GUI (ponto 3 acima),
-- em vez de ler direto de uma tabela de constantes fixas:
tMarkers = {}   -- [key] = {front={px,py}, side={px,py}, worldPos={x,y,z}, radius=, depthRatio=}
sArmedMarker = nil

tJoints = {}    -- 14 marcados + 4 sintéticos (spine1, shoulderCenter, lAnkle, rAnkle), rebuilt a cada mudança
iMannequinRebuildGen = 0
tMannequinHandles = { spheres={}, bones={} }
```

### Convenção de coordenadas (decisão-chave que simplifica tudo)
1 unidade de mundo = 1 pixel da imagem frontal. Imagem frontal fica no quad `Z=0`,
`X∈[0,frontW], Y∈[0,frontH]` (`worldY = frontImgHeight - pixelY`). Isso faz a UV de qualquer vértice
front-facing ser trivial: `u = worldX/frontW`, `v = 1 - worldY/frontH`.

### Skeleton de callbacks (mesma estrutura de `scene_editor3d.lua`)
```
onInitScene()  -- camera3d = mbm.getCamera('3d'); setFar; setLightEnabled/setAmbientLight/setDirectionalLight('3d',...); applyCam3d
onLoop(delta)  -- drawMainPanel() [inclui GUI de marcadores + raio/profundidade]; tUtil.drawOrbitGizmo(cam3d,{size=110}); updateCam3dKeyboardMovement(delta); applyCam3d(cam3d)
onTouchDown/Move/Up(key,x,y)  -- esquerdo = marcar/arrastar; ver "Marcação"
onTouchZoom(zoom)   -- copiar/adaptar de scene_editor3d.lua:3717-3737 (dois samples de mbm.to3d)
onKeyDown/onKeyUp(key)  -- WASD -> tCam3dMove, copiar/adaptar de scene_editor3d.lua:3740-3793
```

## Carregamento e exibição das imagens

```lua
function loadImage(which)  -- 'front' | 'side'
    local f = mbm.openFile(sLastImagePath or '', 'png','jpg','jpeg','bmp','tga')
    if not f then return end
    sLastImagePath = f
    local w, h = getImagePixelSize(f)
    local tex = texture:new('3d', 0, 0, 0)   -- texture suporta '3d' (docs/lua-api.md linha 399)
    if not tex:load(f) then tUtil.showMessage(tLang.L('mannequin_image_load_failed')); return end
    local nativeW, nativeH = tex:getAABB(true)
    if nativeW and nativeW > 0 then tex:setScale(w/nativeW, h/nativeH, 1) end
    if which == 'front' then
        tex:setPos(w*0.5, h*0.5, 0)
        tImages.front = {path=f, tex=tex, w=w, h=h}
    else
        tex:setAngle(0, math.pi*0.5, 0)   -- rotaciona pro plano Y-Z
        tex:setPos(tImages.side.xOffset, h*0.5, w*0.5)
        tImages.side.path, tImages.side.tex, tImages.side.w, tImages.side.h = f, tex, w, h
    end
    rebuildMannequinPreview()
end
```

`getImagePixelSize(path)` — cadeia de fallback (o primeiro passo precisa ser validado no início da
implementação, é a única coisa que bloqueia tudo se falhar):
1. **Primário (verificar primeiro):** `tex:getAABB(true)` logo após `:load()`, antes de qualquer
   `:setScale()`. Não confirmado se `texture` (sem bloco de física, ao contrário de `mesh`) retorna
   tamanho nativo em pixels aqui — `scene_editor3d.lua:892-905` documenta que `getAABB` é
   physics-driven para objetos tipo `mesh`, mas `texture` não tem bloco de física.
2. **Fallback:** shell out pra `identify -format "%w %h"` do ImageMagick, reaproveitando
   `editor/imagick_cli_wrapper.lua` (`M.detectImageMagick()`, `getIdentifyExe`) — adicionar novo
   `M.getImageSize(path)` nesse arquivo, no formato de `M.getPsdLayerInfo`.
3. **Último recurso:** campos numéricos manuais no ImGui pra digitar largura/altura.

**Risco a validar cedo:** `mbm.openFile`/`texture:load()` aceitando caminho absoluto fora da pasta
do projeto — padrão já usado por `meshD:load(fileName)` em `mesh_debug.lua:2827`, nunca confirmado
especificamente pra `texture`.

## Marcação dos pontos + GUI de ajuste

Painel ImGui: uma linha por marcador (`label` + status `—`/`F`/`F+S` + botão "Marcar" que arma
`sArmedMarker` + campos numéricos de **raio** e **proporção de profundidade** editáveis diretamente
ali, com o preset humano como valor inicial sugerido + botão remover), botões "Carregar Imagem
Frontal"/"Carregar Imagem Lateral (opcional)", toggle de lado da imagem lateral
(`tImages.side.facing`), botão Exportar.

Resolução de clique — raio vs. quad finito, testado contra as duas imagens, o hit mais próximo vence
(mesma função `pickRayVsFiniteQuad` do plano original, inalterada).

`onTouchDown` (botão **esquerdo**, só se `not tImGui.GetWantCaptureMouse()`): tenta pegar marcador
existente sob o cursor primeiro; senão, se `sArmedMarker` setado, resolve o clique contra as duas
imagens via `mbm.getPickRay` + `pickRayVsFiniteQuad`, usa o hit mais próximo.

`onTouchMove`: mesmo hit-test restrito ao quad de origem do drag; `onTouchUp`: solta o drag, auto-
arma o próximo marcador sem dado frontal.

**Botão direito**: orbita a câmera (`cam3dGetPos`/`applyCam3d`, adaptado de
`scene_editor3d.lua:473-515`).

Sem foto lateral: profundidade da junta fica em `Z=0`; só a seção transversal do cilindro usa o
`depthRatio` editável do marcador. Com foto lateral: centerline lateral = média do X-pixel de Queixo
e Virilha marcados na foto lateral; `Z = (sidePixelX - centerlineSidePixelX) * facing` pros demais
pontos; Y final é a média entre frontal e lateral quando ambos existem.

## Geração do manequim ao vivo

Hierarquia (14 marcados + 4 sintéticos, nunca clicados) — igual ao plano original:

| Junta | Pai | Nota |
|---|---|---|
| `groin` | — (raiz) | marcado |
| `spine1` | `groin` | sintético: `lerp(groin, shoulderCenter, 0.5)` |
| `shoulderCenter` | `spine1` | sintético: `midpoint(lShoulder, rShoulder)` |
| `chin` | `shoulderCenter` | marcado; cabeça+pescoço = 1 osso só no Marco 1 |
| `lShoulder`/`rShoulder` | `shoulderCenter` | marcado |
| `lElbow`/`rElbow` | `lShoulder`/`rShoulder` | marcado |
| `lWrist`/`rWrist` | `lElbow`/`rElbow` | marcado |
| `lHandTip`/`rHandTip` | `lWrist`/`rWrist` | marcado |
| `lKnee`/`rKnee` | `groin` | marcado |
| `lAnkle`/`rAnkle` | `lKnee`/`rKnee` | sintético: `lerp(knee, footTip, 0.85)` |
| `lFootTip`/`rFootTip` | `lAnkle`/`rAnkle` | marcado |

Novo helper de primitiva (mesmo idioma de `unitSphereVerts`, `scene_editor3d.lua:1673-1697`):
```lua
local function unitCylinderVerts(radiusTop, radiusBottom, height, radialSegments)
    radialSegments = radialSegments or 10
    local verts = {}
    local function push(x,y,z) table.insert(verts,x); table.insert(verts,y); table.insert(verts,z) end
    for i = 0, radialSegments - 1 do
        local a1 = (i/radialSegments) * math.pi*2
        local a2 = ((i+1)/radialSegments) * math.pi*2
        local x1b,z1b = math.cos(a1)*radiusBottom, math.sin(a1)*radiusBottom
        local x2b,z2b = math.cos(a2)*radiusBottom, math.sin(a2)*radiusBottom
        local x1t,z1t = math.cos(a1)*radiusTop,    math.sin(a1)*radiusTop
        local x2t,z2t = math.cos(a2)*radiusTop,    math.sin(a2)*radiusTop
        push(x1b,0,z1b); push(x2b,0,z2b); push(x2t,height,z2t)
        push(x1b,0,z1b); push(x2t,height,z2t); push(x1t,height,z1t)
    end
    return verts
end
```

Orientar um osso entre dois pontos A→B: `dir = normalize(B-A)`, `height = |B-A|`,
`azimuth = atan2(dir.x, dir.z)`, `elevation = asin(dir.y)` (mesma convenção esférica já usada em
`cam3d`/`dirFromOrbit`/`orbitFromDir`) → `handle:setAngle(...)`. **Ordem/sinais exatos dos eixos de
Euler não estão documentados — validar empiricamente antes de integrar no loop real.** Caso comum
sem foto lateral (tudo em `Z=0`): a orientação vira um simples `setAngle(0,0,anguloZ)` — implementar
esse caso trivial primeiro.

UV: vértices em espaço de mundo, classificados front/back comparando `worldZ` com o eixo do osso.
Front-facing: `u=worldX/frontW`, `v=1-worldY/frontH`. Back-facing com foto lateral: inverte pra
pixel lateral, mesma fórmula com `sideW/sideH`. Back-facing sem foto lateral: reusa a fórmula
frontal (consequência natural, não caso especial — é o fallback "UV frontal estendida"). Cada
vértice guarda `uvSource: "front"|"side"`.

Esferas das juntas: `unitSphereVerts()` puro, nickname fixo único
`'mannequin_marker_sphere_unit'`. **Cilindros dos ossos precisam de nickname único a cada rebuild**
(cache global de `shape:create()` por nickname, documentado em `docs/lua-api.md:697-717` e
`scene_editor3d.lua`~1719-1780):
```lua
iMannequinRebuildGen = iMannequinRebuildGen + 1
local nick = 'mannequin_bone_' .. boneName .. '_' .. iMannequinRebuildGen
handle:create(verts, uvs, nick)
```

## Formato do arquivo de handoff (JSON) — consumido pelo Marco 3

Sem lib JSON no repo — `editor/gimp_cli_wrapper.lua:997-1025` (`M.writeJsonMeta`) é o padrão
estabelecido de escrita manual (escapador `jsonStr` + `string.format` + `io.open`).

```json
{
  "schemaVersion": 1,
  "sourceImages": {
    "front": { "path": "/abs/path/front.png", "width": 1024, "height": 1536 },
    "side":  { "path": "/abs/path/side.png",  "width": 900,  "height": 1536, "facing": 1 }
  },
  "joints": [
    { "name": "groin", "parent": null, "x": 512.0, "y": 780.0, "z": 0.0, "radius": 42.0 },
    { "name": "lShoulder", "parent": "shoulderCenter", "x": 340.0, "y": 1280.0, "z": 0.0, "radius": 21.0 }
  ],
  "mesh": {
    "vertices": [
      { "x": 340.0, "y": 1280.0, "z": 21.0, "u": 0.354, "v": 0.166, "uvSource": "front", "owner": "lShoulder" }
    ],
    "subsets": [
      { "name": "bone_lShoulder_lElbow", "owner": "lShoulder", "indices": [0,1,2, 0,2,3] }
    ]
  }
}
```

`joints[].radius` já resolvido (heurística editável ou foto lateral). `owner` em vértice E subset
(redundante de propósito) permite ao Marco 3 fazer vertex-group com peso 100% por osso-dono sem
re-derivar a relação a partir da geometria. Exportar via `mbm.saveFile(title, filter)`, filtro
sugerido `'*.mannequin.json'`.

## Reaproveitado vs. novo (Marco 1)

**Copiado/adaptado**: `cam3dGetPos`/`applyCam3d`/`updateCam3dKeyboardMovement`
(`scene_editor3d.lua:473-515`), dolly de `onTouchZoom` (`scene_editor3d.lua:3717-3737`), WASD
(`scene_editor3d.lua:3740-3793`), `unitSphereVerts` (`scene_editor3d.lua:1673-1697`), padrão de
nickname único por rebuild, idioma "construir verts locais, transladar/orientar pro mundo"
(`physic_editor.lua:630-666`), `M.detectImageMagick()`/`getIdentifyExe`
(`imagick_cli_wrapper.lua:88-179`) + novo `M.getImageSize(path)`, escritor JSON manual
(`gimp_cli_wrapper.lua:997-1025`).

**Requerido direto**: `require "editor_utils"` (`tUtil.drawOrbitGizmo`, `tUtil.showMessage`, `tLang`).

**Genuinamente novo**: `unitCylinderVerts`, classificação front/back + UV dupla-fonte,
`pickRayVsFiniteQuad`, GUI de marcador com raio/profundidade editáveis, todo o modelo de dados de
marcador, derivação de juntas sintéticas, escritor do JSON.

## Riscos a validar empiricamente na primeira sessão de implementação (ordem de prioridade)

1. `texture:new('3d',...):load(caminhoAbsoluto)` com caminho fora da pasta do projeto.
2. `tex:getAABB(true)` após `:load()` — dimensão nativa confiável pra `texture` puro?
3. Direção do flip de `v` na textura do preview (cosmético, resolver cedo).
4. Ordem/sinais de composição de Euler em `handle:setAngle(ax,ay,az)`.

## Verificação end-to-end (Marco 1)

1. Rodar `./mini-mbm editor/mannequin_editor.lua`, carregar uma imagem de teste (foto real e, em
   separado, algo estilizado tipo Mario — sem commitar a imagem), confirmar quad no tamanho certo.
2. Marcar os 14 pontos, confirmar visualmente o manequim semi-transparente sobreposto à imagem,
   acompanhando cada marcador ao arrastar.
3. Ajustar raio/profundidade de um marcador pela GUI e confirmar que o manequim reflete a mudança
   sem precisar arrastar o ponto.
4. Testar botão direito orbitando sem mover marcador; clique-esquerdo marcando/arrastando sem
   orbitar.
5. Carregar imagem lateral, marcar os mesmos pontos, confirmar mudança plausível de profundidade e
   `uvSource: "side"` no JSON exportado nas partes de trás.
6. Exportar o JSON, inspecionar manualmente os 14+4 joints, hierarquia, vértices e subsets.

---

## Marco 2 — persistência de armature no formato nativo (`SECTION_FRAME_SKINNED`)

**Não é runtime de animação por esqueleto.** Confirmado por exploração de código: não existe
nenhum rastro de skinning/bone-matrix/skeleton em `src/render/`, `include/render/`, nem em
`animation.h/.cpp` — a engine hoje é 100% frames estáticos pré-calculados. Este marco serve só pra
dar ao **editor** um jeito de salvar/recarregar seu próprio estado de trabalho (joints + mesh) no
formato nativo `.msh`, como diagnóstico de evolução — não pra fazer o `MESH` da engine animar por
esqueleto em jogo.

- `SECTION_FRAME_SKINNED = 11` **já está reservado** em `include/core_mbm/header-mesh.h:564` e
  documentado em `docs/mesh-v11-format.md:103-105`: *"reserved now specifically so that when bones
  ship later, they get a new section type, not a new file-format version"*. **Não é preciso
  inventar um novo id** (`SECTION_FRAME_BONES`/`SECTION_FRAME_ARMATURE` não são necessários) — só
  falta desenhar o payload (`docs/mesh-v11-format.md:337`: *"payload layout — not designed yet, only
  the type id is reserved"*).
- Payload proposto: espelha o schema JSON do Marco 1 (lista de joints com nome, posição, pai,
  raio; vértices com posição+UV+owner; subsets com owner+índices), serializado no formato binário
  de seção do v11 (mesmo padrão de `SECTION_ANIMATION`/`SECTION_FRAME_STATIC` em
  `mesh-manager.cpp`).
- Trabalho de C++: leitura/escrita em `src/core_mbm/mesh-manager.cpp`, respeitando a diretriz de
  PIMPL/header-hygiene do `AGENTS.md`/`CLAUDE.md` (não vazar estado mutável novo em headers
  públicos) e atualizando `docs/core-pimpl-status.md` quando o trabalho mudar essa fronteira.
- Novo binding Lua em `meshDebug` (native class, `src/lua-wrap/render-table/mesh-debug-lua.cpp`),
  algo como `addBone`/`getBone`/`getTotalBone`, espelhando o padrão já existente de
  `addAnim`/`getAnim`.
- No `mannequin_editor.lua`: botões "Exportar .msh (diagnóstico)" / "Recarregar do .msh" usando
  esse novo binding.

Este marco só faz sentido depois que a geração do manequim (Marco 1) já existe — não há o que
persistir antes disso. Detalhamento fino do payload binário fica para quando este marco for
efetivamente planejado (fora do escopo de implementação agora).

---

## Marco 3 — script Blender headless (constrói armature+mesh reais, exporta FBX)

- Lê o **JSON do Marco 1** como entrada (confirmado — não o `.msh` do Marco 2, que é só para
  round-trip do editor; não existe parser Python de `.msh` v11 hoje e criar um seria redundante).
- Constrói o armature do zero via `bpy.data.armatures.new()` + `edit_bones.new()` a partir da lista
  de joints (nomes genéricos, não amarrados a convenção Mixamo).
- Constrói a mesh a partir dos vértices/UV/subsets recebidos.
- Vertex groups com peso 100% pro osso-dono (usando o campo `owner`), em vez de automatic weights
  do Blender — mais confiável já que a relação vértice→osso já é conhecida.
- Reaproveita o bloco de limpeza de cena + centralização + export FBX já existente em
  `blender_body_adjust.py` (`MIXAMO_OT_prepare_and_export`).
- Invocação headless: reaproveita o padrão de `blender_cli_wrapper.lua` (detecção do executável,
  `buildBakeCmd`-style command builder) + o padrão de coroutine/polling de progresso já usado em
  `mesh_debug.lua`'s `blenderImportCoroutine` — precisa de uma nova função tipo
  `M.buildMannequinCmd(...)` no wrapper.
- Saída: FBX pronto pra upload manual num serviço de auto-rig (Mixamo hoje, qualquer outro no
  futuro — ver "Desacoplamento do Mixamo" acima).

Detalhamento fino (assinatura exata do script Python, flags CLI) fica para quando este marco for
efetivamente planejado.

---

## Marco 4 (futuro, não detalhado agora) — "ajuste de textura" / recorte de silhueta

Modo de interação 2D separado (distinto do viewport 3D dos Marcos 1-3) pra desenhar um
contorno/silhueta sobre a imagem carregada e recortar o fundo que vazaria nas partes visíveis do
manequim (afeta a textura que acaba mapeada nas UVs geradas pelo Marco 1). Confirmado como
necessidade real pelo usuário, mas deliberadamente fora de escopo até o fluxo Marco 1-3 estar
validado ponta-a-ponta — só nesse ponto ficará claro o desenho de interação certo (ferramenta de
laço/caneta 2D? threshold automático de cor de fundo? etc.).

---

### Arquivos críticos (Marco 1)
- `/home/michel/mini-mbm/editor/mannequin_editor.lua` (novo)
- `/home/michel/mini-mbm/editor/scene_editor3d.lua` (câmera, `unitSphereVerts`, convenção de cor/alpha, pitfall de nickname)
- `/home/michel/mini-mbm/editor/editor_utils.lua` (`tUtil.drawOrbitGizmo`, `dirFromOrbit`/`orbitFromDir`, `tLang`)
- `/home/michel/mini-mbm/editor/physic_editor.lua` (padrão de primitiva 3D + ray test, base pro `pickRayVsFiniteQuad`)
- `/home/michel/mini-mbm/editor/imagick_cli_wrapper.lua` (fallback de dimensão de imagem)
- `/home/michel/mini-mbm/editor/gimp_cli_wrapper.lua` (padrão de escrita manual de JSON)
- `/home/michel/mini-mbm/docs/lua-api.md` (referência de API texture/shape/pick-ray)

### Arquivos críticos (Marcos 2-3, referência futura)
- `/home/michel/mini-mbm/include/core_mbm/header-mesh.h` (`SECTION_FRAME_SKINNED = 11`, já reservado)
- `/home/michel/mini-mbm/docs/mesh-v11-format.md` (formato v11, payload do `SECTION_FRAME_SKINNED` ainda não desenhado)
- `/home/michel/mini-mbm/src/core_mbm/mesh-manager.cpp` (leitura/escrita de seções v11)
- `/home/michel/mini-mbm/src/lua-wrap/render-table/mesh-debug-lua.cpp` (bindings `meshDebug`)
- `/home/michel/mini-mbm/docs/core-pimpl-status.md` (atualizar se o Marco 2 mudar a fronteira PIMPL)
- `/home/michel/mini-mbm/editor/blender_body_adjust.py` (bloco de export FBX a reaproveitar no Marco 3)
- `/home/michel/mini-mbm/editor/blender_cli_wrapper.lua` + `mesh_debug.lua` (padrão de invocação headless/coroutine a reaproveitar no Marco 3)
