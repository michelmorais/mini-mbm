# Plano: Editor "Mannequin from Photo" (Marco 1) + visão geral do Marco 2

## Contexto

Objetivo final do usuário: baixar/tirar uma foto de uma pessoa em T-pose, marcar pontos anatômicos
sobre essa foto dentro do próprio editor Lua/ImGui do mini-mbm, gerar um "manequim" 3D de
primitivas (esferas nas juntas + cilindros conectando-as) com UV derivado da própria foto, exportar
esses dados (vértices + hierarquia de armature) para o Blender, que constrói a mesh+armature de
verdade e exporta um FBX pronto para upload no mixamo.com. De lá, a animação baixada volta a entrar
no `mesh_debug.lua` via o pipeline "Import via Blender" que já existe.

Isso substitui duas tentativas anteriores que **não** eram o que o usuário tinha em mente: (1) um
guia de texto dentro do mesh_debug.lua sobre o fluxo Mixamo manual, e (2) um addon Blender
(`editor/blender_body_adjust.py`, já criado e funcional) para redimensionar partes do corpo via
escala de ossos. Esses dois artefatos já existentes **não devem ser revertidos** — o addon Blender
em particular tem um bloco de exportação FBX (limpeza de cena + centralização + export) que será
**reaproveitado no Marco 2** deste plano.

O trabalho está deliberadamente dividido em dois marcos, confirmado com o usuário:

- **Marco 1** (escopo deste plano, a ser implementado agora): a ferramenta Lua dentro do mini-mbm —
  carregar foto(s), marcar pontos, gerar preview 3D semi-transparente com UV, exportar um JSON de
  handoff. Testável e visível sozinho, sem depender do Blender.
- **Marco 2** (fora do escopo de implementação agora, só descrito em alto nível aqui): script
  Python headless que lê o JSON do Marco 1, constrói o armature+mesh de verdade no Blender
  (`bpy.data.armatures.new`, `edit_bones.new`, mesh a partir dos vértices+UV recebidos, vertex
  groups com peso 100% por osso-dono em vez de automatic weights), e exporta FBX — reaproveitando o
  bloco de export já existente em `blender_body_adjust.py` e o padrão de invocação
  headless/coroutine já existente em `blender_cli_wrapper.lua` + `mesh_debug.lua`.

---

## Marco 1 — decisões confirmadas com o usuário

1. **Profundidade (Z)**: foto lateral é opcional. Se fornecida, deriva Z real a partir dela. Se não,
   assume uma proporção padrão por segmento (torso mais raso que largo, membros ~circulares) — é só
   pra ficar "plausível o bastante" para o checklist do Mixamo, não uma reconstrução anatômica
   exata.
2. **Marcadores**: 14 pontos — Queixo, Ombro E/D, Cotovelo E/D, Pulso E/D, Virilha, Joelho E/D,
   Ponta da Mão E/D, Ponta do Pé E/D (igual à imagem de referência do usuário, tipo meshy.com).
3. **Controles de câmera**: como clique-esquerdo fica reservado para marcar/arrastar pontos, a
   órbita da câmera passa para **botão direito** (diferente do `scene_editor3d.lua`, que usa
   esquerdo) — decisão deliberada para não arriscar mover a câmera sem querer durante a marcação.
4. **Sem `.msh`**: Marco 1 não salva um `.msh` de debug (o usuário já apontou que seria inútil para
   o objetivo real). Só o preview 3D ao vivo no editor + o JSON de handoff.

---

## Arquivo novo

`/home/michel/mini-mbm/editor/mannequin_from_photo.lua` — script standalone, mesma categoria de
`scene_editor3d.lua`/`mesh_debug.lua` (rodado via `./mini-mbm editor/mannequin_from_photo.lua`),
`snake_case` (convenção de arquivos Lua deste repo — `kebab-case` é só para C++).

### Requires
```lua
tImGui = require "ImGui"
tUtil  = require "editor_utils"   -- drawOrbitGizmo, dirFromOrbit/orbitFromDir, showMessage, tLang
```

### Estruturas de estado principais
```lua
camera3d = nil
cam3d = { azimuth=0.3, elevation=0.35, distance=900, fx=0, fy=0, fz=0 }
tCam3dMove = { forward=0, right=0 }

tPhotos = {
    front = { path=nil, tex=nil, w=0, h=0 },
    side  = { path=nil, tex=nil, w=0, h=0, xOffset=-400, facing=1 },
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
tMarkers = {}   -- [key] = {front={px,py}, side={px,py}, worldPos={x,y,z}}
sArmedMarker = nil

tJoints = {}    -- 14 marcados + 4 sintéticos (spine1, shoulderCenter, lAnkle, rAnkle), rebuilt a cada mudança
iMannequinRebuildGen = 0
tMannequinHandles = { spheres={}, bones={} }
```

### Convenção de coordenadas (decisão-chave que simplifica tudo)
1 unidade de mundo = 1 pixel da foto frontal. Foto frontal fica no quad `Z=0`, `X∈[0,frontW]`,
`Y∈[0,frontH]` (`worldY = frontPhotoHeight - pixelY`). Isso faz a UV de qualquer vértice
front-facing ser trivial: `u = worldX/frontW`, `v = 1 - worldY/frontH`.

### Skeleton de callbacks (mesma estrutura de `scene_editor3d.lua`)
```
onInitScene()  -- camera3d = mbm.getCamera('3d'); setFar; setLightEnabled/setAmbientLight/setDirectionalLight('3d',...); applyCam3d
onLoop(delta)  -- drawMainPanel(); tUtil.drawOrbitGizmo(cam3d,{size=110}); updateCam3dKeyboardMovement(delta); applyCam3d(cam3d)
onTouchDown/Move/Up(key,x,y)  -- ver "Marcação" abaixo
onTouchZoom(zoom)   -- copiar/adaptar de scene_editor3d.lua:3717-3737 (dois samples de mbm.to3d)
onKeyDown/onKeyUp(key)  -- WASD -> tCam3dMove, copiar/adaptar de scene_editor3d.lua:3740-3793
```

## Carregamento e exibição das fotos

```lua
function loadPhoto(which)  -- 'front' | 'side'
    local f = mbm.openFile(sLastPhotoPath or '', 'png','jpg','jpeg','bmp','tga')
    if not f then return end
    sLastPhotoPath = f
    local w, h = getImagePixelSize(f)
    local tex = texture:new('3d', 0, 0, 0)   -- texture suporta '3d' (docs/lua-api.md linha 399)
    if not tex:load(f) then tUtil.showMessage(tLang.L('mannequin_photo_load_failed')); return end
    -- forçar o quad a exatamente w x h unidades de mundo
    local nativeW, nativeH = tex:getAABB(true)
    if nativeW and nativeW > 0 then tex:setScale(w/nativeW, h/nativeH, 1) end
    if which == 'front' then
        tex:setPos(w*0.5, h*0.5, 0)
        tPhotos.front = {path=f, tex=tex, w=w, h=h}
    else
        tex:setAngle(0, math.pi*0.5, 0)   -- rotaciona pro plano Y-Z
        tex:setPos(tPhotos.side.xOffset, h*0.5, w*0.5)
        tPhotos.side.path, tPhotos.side.tex, tPhotos.side.w, tPhotos.side.h = f, tex, w, h
    end
    rebuildMannequinPreview()
end
```

`getImagePixelSize(path)` — cadeia de fallback (o primeiro passo precisa ser validado no início da
implementação, é a única coisa que bloqueia tudo se falhar):
1. **Primário (verificar primeiro):** `tex:getAABB(true)` logo após `:load()`, antes de qualquer
   `:setScale()`. Não confirmado se `texture` (sem bloco de física, ao contrário de `mesh`) retorna
   tamanho nativo em pixels aqui — `scene_editor3d.lua:892-905` documenta que `getAABB` é
   physics-driven para objetos tipo `mesh`, mas `texture` não tem bloco de física, então pode se
   comportar diferente.
2. **Fallback:** shell out para `identify -format "%w %h"` do ImageMagick, reaproveitando o padrão
   de `editor/imagick_cli_wrapper.lua` (`M.detectImageMagick()`, `getIdentifyExe`) — adicionar um
   novo `M.getImageSize(path)` nesse arquivo, no mesmo formato de `M.getPsdLayerInfo`.
3. **Último recurso:** campos numéricos manuais no ImGui pra digitar largura/altura.

Arranjo 3D: foto frontal em `Z=0`; foto lateral (se houver) rotacionada 90° em Y, em
`X = tPhotos.side.xOffset` (padrão -400), longe da frontal. O manequim é gerado direto nesse mesmo
espaço de coordenadas — sem transform extra de alinhamento necessário.

**Risco a validar cedo:** `mbm.openFile`/`texture:load()` aceitando caminho absoluto fora da pasta
do projeto (ex: `/home/michel/Pictures/Screenshots/...`) — padrão já usado por
`meshD:load(fileName)` em `mesh_debug.lua:2827`, mas nunca confirmado especificamente pra `texture`.

## Marcação dos pontos

Painel ImGui: uma linha por marcador (`label` + status `—`/`F`/`F+S` + botão "Marcar" que arma
`sArmedMarker` + botão remover), botões "Carregar Foto Frontal"/"Carregar Foto Lateral (opcional)",
toggle "lado da foto lateral" (Esquerda/Direita, define `tPhotos.side.facing`), botão Exportar.

Resolução de clique — raio vs. quad finito, testado contra as duas fotos, o hit mais próximo vence:

```lua
function pickRayVsFiniteQuad(ox,oy,oz, dx,dy,dz, quad)
    -- quad = {planeNormal={x,y,z}, planePoint={x,y,z}, right={x,y,z}, up={x,y,z}, w=, h=}
    local denom = dx*quad.planeNormal.x + dy*quad.planeNormal.y + dz*quad.planeNormal.z
    if math.abs(denom) < 1e-6 then return nil end
    local t = ((quad.planePoint.x-ox)*quad.planeNormal.x + (quad.planePoint.y-oy)*quad.planeNormal.y
             + (quad.planePoint.z-oz)*quad.planeNormal.z) / denom
    if t < 0 then return nil end
    local hx,hy,hz = ox+dx*t, oy+dy*t, oz+dz*t
    local localX = (hx-quad.planePoint.x)*quad.right.x + (hy-quad.planePoint.y)*quad.right.y + (hz-quad.planePoint.z)*quad.right.z
    local localY = (hx-quad.planePoint.x)*quad.up.x    + (hy-quad.planePoint.y)*quad.up.y    + (hz-quad.planePoint.z)*quad.up.z
    if localX < 0 or localX > quad.w or localY < 0 or localY > quad.h then return nil end
    return t, localX, localY
end
```

`onTouchDown` (botão **esquerdo**, `key==0`, só se `not tImGui.GetWantCaptureMouse()`):
1. Tenta pegar um marcador já existente sob o cursor primeiro (`findMarkerSphereUnderScreenPoint`,
   via `mbm.getPickRay` + teste de distância até cada esfera).
2. Senão, se `sArmedMarker` setado: pega `mbm.getPickRay(x,y)`, testa contra os dois quads via
   `pickRayVsFiniteQuad`, usa o hit mais próximo (`t` menor) pra decidir se foi clique na foto
   frontal ou lateral, chama `setMarkerFromClick(sArmedMarker, 'front'|'side', localX, localY)`.

`onTouchMove`: repete o mesmo hit-test restrito ao quad de origem do drag (nunca troca de quad no
meio do gesto), atualiza a posição do marcador seguro e chama `rebuildMannequinPreview()`.

`onTouchUp`: solta o drag; se foi uma colocação nova (não drag de existente), auto-arma o próximo
marcador da lista ainda sem dado frontal (conveniência pra marcar em sequência sem clicar "Marcar"
toda hora).

**Botão direito**: orbita a câmera (`cam3dGetPos`/`applyCam3d`, copiado/adaptado de
`scene_editor3d.lua:473-515` — só a origem do gesto muda de esquerdo pra direito).

**Heurística de profundidade padrão (sem foto lateral)** — só afeta a seção transversal do
cilindro, não a posição da junta (a posição fica em `Z=0` se não há foto lateral):
```lua
tDefaultRadiusRatio = {  -- multiplicado por shoulderWidth = |RShoulder.x - LShoulder.x|
    chin=0.15, shoulderCenter=0.12, spine1=0.16, groin=0.20,
    lShoulder=0.10, rShoulder=0.10, lElbow=0.075, rElbow=0.075, lWrist=0.05, rWrist=0.05,
    lHandTip=0.04, rHandTip=0.04, lKnee=0.11, rKnee=0.11, lAnkle=0.075, rAnkle=0.075,
    lFootTip=0.06, rFootTip=0.06,
}
fTorsoDepthRatio = 0.6   -- achatamento largura->profundidade do torso
fHeadDepthRatio  = 0.85
-- membros: profundidade 1.0 (circular)
```

Com foto lateral: centerline lateral = média do X-pixel de Queixo e Virilha marcados *na foto
lateral* (pontos de linha-central deveriam ficar quase no mesmo X num perfil real). Pra qualquer
outro ponto marcado na lateral: `Z = (sidePixelX - centerlineSidePixelX) * facing`. Y final é a
média entre frontal e lateral quando ambos existem.

## Geração do manequim ao vivo

Hierarquia (14 marcados + 4 sintéticos, nunca clicados):

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
`cam3d`/`dirFromOrbit`/`orbitFromDir`) → `handle:setAngle(...)`. **A ordem/sinais exatos dos eixos
de Euler não estão documentados — validar empiricamente com um cilindro de teste apontado em
algumas direções conhecidas antes de integrar no loop real.** No caso comum do Marco 1 (sem foto
lateral, tudo em `Z=0`), a direção de todo osso fica no plano XY e a orientação vira um simples
`setAngle(0,0,anguloZ)` — implementar/testar esse caso trivial primeiro, adicionar o caso geral
3-eixos depois.

Achatamento do torso: `handle:setScale(1,1,fTorsoDepthRatio)` aplicado em espaço local do osso antes
do transform de posição/ângulo.

UV: gerar vértices em espaço de mundo, classificar front/back comparando `worldZ` do vértice com o
eixo do osso naquela altura. Front-facing: `u=worldX/frontW`, `v=1-worldY/frontH`. Back-facing com
foto lateral: inverter `worldZ` pra pixel lateral e usar a mesma fórmula com `sideW/sideH`.
Back-facing sem foto lateral: **reusa a fórmula frontal** — isso não é caso especial, é consequência
natural de `u` nunca depender de `worldZ`, e É o fallback "UV frontal espelhada/estendida" pedido.
Cada vértice guarda também `uvSource: "front"|"side"` (necessário no Marco 2 pra saber a qual imagem
aquele UV se refere).

Esferas das juntas: reusar `unitSphereVerts()` puro, nickname fixo único
`'mannequin_marker_sphere_unit'` (geometria unitária constante, só varia `:setPos`/`:setScale` por
instância — igual ao padrão de `scene_editor3d.lua:1767`).

**Cilindros dos ossos precisam de nickname único a cada rebuild** — vértices diferem por
osso/frame (`radiusTop`/`radiusBottom`/`height` diferentes), então o cache global por nickname do
`shape:create()` (documentado em `docs/lua-api.md:697-717` e no comentário de
`scene_editor3d.lua`~1719-1780) vai servir geometria velha se o nome for reaproveitado:
```lua
iMannequinRebuildGen = iMannequinRebuildGen + 1
local nick = 'mannequin_bone_' .. boneName .. '_' .. iMannequinRebuildGen
handle:create(verts, uvs, nick)
```
`rebuildMannequinPreview()` destrói todos os handles de osso anteriores antes de recriar, seta
`:setColor(r,g,b,0.55)` pro visual semi-transparente (mesma convenção de alpha de
`tSceneMarkerColor`, `scene_editor3d.lua:1647,1792`).

## Formato do arquivo de handoff (JSON)

Não existe lib JSON no repo — `editor/gimp_cli_wrapper.lua:997-1025` (`M.writeJsonMeta`) já é o
padrão estabelecido de escrita manual de JSON (escapador `jsonStr` + `string.format` + `io.open`) —
seguir exatamente esse padrão aqui.

```json
{
  "schemaVersion": 1,
  "sourcePhotos": {
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

`joints[].radius` é o raio já resolvido (heurística ou foto lateral) — o que o Marco 2 precisa pra
dimensionar o osso/envelope. `owner` em vértice E em subset (redundante de propósito) permite ao
Marco 2 fazer vertex-group com peso 100% por osso-dono sem precisar re-derivar a relação a partir da
geometria. Exportar via `mbm.saveFile(title, filter)` (mesmo padrão de `particle_editor.lua:285`,
`font_maker.lua:208`), filtro sugerido `'*.mannequin.json'`.

## Reaproveitado vs. novo

**Copiado/adaptado** (funções locais não exportadas nos arquivos-fonte, mesma situação de
`mesh_debug.lua`/`scene_editor3d.lua` já duplicarem código de câmera entre si):
- `cam3dGetPos`, `applyCam3d`, `updateCam3dKeyboardMovement` — `scene_editor3d.lua:473-515`
- Dolly de `onTouchZoom` (dois samples de `mbm.to3d`) — `scene_editor3d.lua:3717-3737`
- Acumulação WASD em `onKeyDown`/`onKeyUp` — `scene_editor3d.lua:3740-3793`
- `unitSphereVerts` — `scene_editor3d.lua:1673-1697`, usada sem modificação
- Padrão de nickname único por rebuild — `docs/lua-api.md:697-717`, `scene_editor3d.lua`~1719-1780
- Idioma "construir verts locais, transladar/orientar pro mundo" — `physic_editor.lua:630-666`
  (`boxCorners`/`boxTriangleFaces`), adaptado pra cilindro
- `M.detectImageMagick()`/`getIdentifyExe` — `editor/imagick_cli_wrapper.lua:88-179`, + novo
  `M.getImageSize(path)`
- Escritor JSON manual — `editor/gimp_cli_wrapper.lua:997-1025`

**Requerido direto, sem adaptação**: `require "editor_utils"` pra `tUtil.drawOrbitGizmo`,
`tUtil.showMessage`, `tLang`.

**Genuinamente novo**: `unitCylinderVerts`, classificação front/back + fórmula de UV dupla-fonte,
`pickRayVsFiniteQuad` (o `rayHitsAABB` existente em `physic_editor.lua:676` é teste de caixa, não de
plano limitado — função nova, mas da mesma família), todo o modelo de dados de marcador, derivação
de juntas sintéticas, heurística de profundidade padrão, escritor do JSON.

## Riscos a validar empiricamente na primeira sessão de implementação (em ordem de prioridade)

1. `texture:new('3d',...):load(caminhoAbsoluto)` com caminho fora da pasta do projeto — funciona?
   (assunção herdada de `meshD:load`, nunca confirmada especificamente pra `texture`)
2. `tex:getAABB(true)` após `:load()` — retorna dimensão nativa em pixels pra um `texture` puro, ou
   é physics-driven/não-confiável como é documentado pra `mesh`? Decide se o fallback ImageMagick é
   primário na prática.
3. Direção do flip de `v` na textura do preview (cosmético, mas resolver cedo pra checagens visuais
   confiáveis)
4. Ordem/sinais de composição de Euler em `handle:setAngle(ax,ay,az)`, necessário pra fórmula geral
   de orientação de osso quando há foto lateral (fora do plano XY)

## Verificação end-to-end

1. Rodar `./mini-mbm editor/mannequin_from_photo.lua` (ou binário debug equivalente), carregar uma
   foto frontal de teste, confirmar que o quad aparece do tamanho/proporção certos na cena 3D.
2. Marcar os 14 pontos, confirmar visualmente que o manequim de esferas+cilindros semi-transparente
   aparece sobreposto à foto e acompanha cada marcador ao arrastar.
3. Testar botão direito orbitando a câmera sem mover nenhum marcador; testar clique-esquerdo
   marcando/arrastando sem orbitar a câmera.
4. Carregar uma foto lateral, marcar os mesmos pontos nela, confirmar que a profundidade (Z) do
   manequim muda de forma plausível e que UVs back-facing passam a usar a foto lateral
   (`uvSource: "side"` no JSON exportado).
5. Exportar o JSON, inspecionar manualmente que os 14+4 joints, hierarquia de pais, vértices e
   subsets estão presentes e com valores plausíveis.

### Arquivos críticos
- `/home/michel/mini-mbm/editor/mannequin_from_photo.lua` (novo)
- `/home/michel/mini-mbm/editor/scene_editor3d.lua` (câmera, `unitSphereVerts`, convenção de cor/alpha, pitfall de nickname)
- `/home/michel/mini-mbm/editor/editor_utils.lua` (`tUtil.drawOrbitGizmo`, `dirFromOrbit`/`orbitFromDir`, `tLang`)
- `/home/michel/mini-mbm/editor/physic_editor.lua` (padrão de primitiva 3D + ray test, base pro `pickRayVsFiniteQuad`)
- `/home/michel/mini-mbm/editor/imagick_cli_wrapper.lua` (fallback de dimensão de imagem)
- `/home/michel/mini-mbm/editor/gimp_cli_wrapper.lua` (padrão de escrita manual de JSON)
- `/home/michel/mini-mbm/docs/lua-api.md` (referência de API texture/shape/pick-ray)
- `/home/michel/mini-mbm/editor/blender_body_adjust.py` (bloco de export FBX a reaproveitar no Marco 2)
- `/home/michel/mini-mbm/editor/blender_cli_wrapper.lua` + `mesh_debug.lua` (padrão de invocação headless/coroutine a reaproveitar no Marco 2)
