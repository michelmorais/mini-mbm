# Gerador de Normal Map

Primeira entrega do [plano](normal-map-editor-plan.md), introduzida em 7.329.
Editor independente com módulos Lua compartilhados. O Image Mesh reutiliza esses
módulos e, desde 7.332, preserva o relevo geométrico ao acrescentar normal map.
Desde 7.336, o Image Mesh oferece **Detalhe residual (frente)**: uma aproximação
pela diferença entre a altura processada e a superfície da malha final. O módulo
Lua reutilizável `normal_map_residual.generate(image, vertices, indices, domain,
settings, tick)` rasteriza triângulos com índices Lua (base 1), preserva diferenças
assinadas em floats e devolve o mesmo resultado RGBA do gerador compartilhado.
`domain` informa `imageWidth`, `imageHeight`, origem do recorte `x/y`, dimensões
físicas `width/height` e multiplicador `scale` da altura normalizada; os vértices
contêm `u/v/z`. `settings` usa os ajustes de `height_map_source`, e `tick(progress)`
pode ceder execução para cancelamento cooperativo. UVs são os da imagem completa;
as dimensões de `image` são as do recorte processado. O módulo não depende da engine.
`height_map_source.blur` também aceita mapas de floats assinados;
`normal_map_generator.fromMap` permite escalas físicas diferentes por eixo.
Em 7.338, `normal_map_baker.generate(image, vertices, indices, corners, domain,
settings, tick)` acrescenta compensação na base tangente interpolada. Usa o mesmo
`domain`, com a orientação frontal do Image Mesh (X e Y decrescem com U e V),
vértices com `nx/ny/nz` e uma tangente `{x,y,z,sign}` por canto de triângulo.
As tangentes podem ser obtidas de `meshDebug:getNormalMapCorners(frame, subset)`.
O resultado contém `bytes`, `width`, `height` e `options`. O módulo é Lua puro,
com execução cooperativa por `tick`; não depende de ImGui ou da engine.
Desde 7.342, **Meta automatica de triangulos** fica em um painel próprio do Image
Mesh e funciona sem normal map. A redução QEM atende à meta; a busca de raios para
separação de detalhe só participa quando normal map residual e compensação estão
ativos. O ajuste manual de separação continua em **Normal map e tangentes**.

Desde 7.339, o Image Mesh oferece **Separacao de detalhe (px)**: raio opcional de
filtro na geometria, preservando a altura original no normal map residual.
A filtragem pesada roda no núcleo, reutilizável pela opção `geometryBlurRadius`
de `generateImageMesh`/`startImageMesh`; o projeto usa `normalMapGeometryBlur`.
O controle está disponível para fontes contínuas de imagem, manual e mista.
Desde 7.340, **Separacao automatica** busca um raio por meta de triangulos,
contando frente, verso e laterais após simplificação. Testa 0/1/2/4/8/16/32 px
e informa quando não consegue atingir a meta. Desde 7.341, ativa a simplificação
QEM implicitamente e calcula a redução pela meta, preservando as restrições
de detalhes/contornos. Raio, método e proporção manuais ficam preservados; consulte as limitações no [Image Mesh](image-mesh-editor.md#normal-map-relief).

## Uso

Selecione **Normal Map Generator / Gerador de Normal Map** no launcher desktop,
ou execute a partir da raiz do repositório:

```sh
./bin/debug/linux_x86/mini-mbm --scene editor/normal_map_editor.lua \
  --disable_select_monitor --nosplash -w 1180 -h 800
```

1. Abra uma imagem. O editor interpreta luminância ou um canal como altura.
2. Ajuste níveis, curva, inversão, suavização e intensidade.
3. Selecione a convenção `+Y/-Y` e o tratamento de bordas.
4. Compare original, altura, normais e superfície iluminada. Azimute e elevação
   movimentam a luz; a opção de normal map permite comparar com uma superfície plana.
5. Exporte PNG na resolução original. Salve um projeto `.normalmap` para guardar
   fonte e parâmetros e reproduzir a geração depois.

Os diálogos sugerem a pasta e o nome base da textura: `pedra.png` resulta em
`pedra_normal.png` para a imagem e `pedra.normalmap` para o projeto. A exportação
continua em PNG mesmo quando a fonte usa outra extensão. Projetos já abertos ou
salvos mantêm seu caminho como sugestão. Os nomes podem ser editados no diálogo.

O preview tem no máximo 384 pixels no maior eixo e usa amostragem pelo centro do
pixel mais próximo. Detalhes abaixo dessa resolução podem diferir da exportação.
O projeto referencia a imagem original; não a incorpora. Mover ou modificar a
fonte pode impedir a reabertura ou alterar o resultado. Quando a fonte está sob a
pasta do projeto, o arquivo armazena um caminho relativo.

A iluminação do preview é uma referência CPU sobre uma superfície plana, com
luz direcional difusa e ambiente. Não é uma simulação completa do material 3D da
engine. Nenhuma geometria, silhueta ou colisão é alterada. Cores escuras da fonte
podem produzir cavidades indesejadas; inspecione a altura antes de exportar.

## Módulos reutilizáveis

Todos ficam em `editor/`. Fonte, geração e persistência independem de ImGui e das
variáveis globais de uma cena; o painel recebe explicitamente UI, tradutor e opções.

| Módulo | Contrato |
|---|---|
| `height_map_source` | `image(rgba,w,h)` valida RGBA8; `settings(options)` valida e copia as opções; `build(image,options,limit,tick)` produz altura em linhas de floats compactados |
| `normal_map_generator` | `generate(image,options,limit,tick)` retorna resultado; `start(image,options,limit)` cria job cooperativo; `job(fn)` permite tarefas cooperativas auxiliares |
| `normal_map_preview` | `light(result,azimuth,elevation,enabled,tick)` ilumina as normais existentes; `new(temporaryPath)` cria um conjunto privado de slots GPU para a UI |
| `normal_map_panel` | `draw(ui,translate,options,processedHeight?)` altera opções e retorna se houve mudança; `processedHeight=true` omite canal/níveis/inversão já aplicados pela fonte; chamar dentro de uma janela ImGui aberta |
| `normal_map_project` | `save(path,source,options)` e `load(path)` persistem fonte/opções; load retorna fonte resolvida e opções validadas |
| `normal_map_editor` | Cena independente, com callbacks da engine; não é um módulo de lógica para importar em outros editores |

Exemplo de geração sem interface:

```lua
local Height = require 'height_map_source'
local Generator = require 'normal_map_generator'
local bytes, w, h = mbm.readImagePixels('/absolute/source.png')
assert(bytes, w)
local image = Height.image(bytes, w, h)
local job = Generator.start(image, {strength=2, blur=3, convention='+Y'})

-- No loop da ferramenta: executar apenas enquanto estiver processando.
job:step(0.006)
if job.state == 'completed' then
    local result = job.result
    assert(mbm.writeImagePixels('/absolute/normal.png',
        result.bytes, result.width, result.height))
    -- Consumir uma vez e remover o job do estado da ferramenta.
elseif job.state == 'failed' then
    error(job.error)
end
```

O resultado contém `bytes` (normal RGBA8), `heightBytes` (altura em cinza RGBA8),
`diffuse` (fonte na resolução de saída), `width`, `height` e `options` (snapshot).
Os jobs expõem `state`, `progress`, `result`, `error`, `step(seconds)` e `cancel()`.
Estados: `running`, `completed`, `failed`, `cancelled`. Não são threads: cedem
execução entre linhas, verificando o orçamento de CPU nesses pontos. Cancelar
descarta o coroutine e seu resultado. Trate a imagem de entrada como imutável.

## Convenções e filtros

- Imagens usam bytes RGBA de 0 a 255, linhas de cima para baixo, limite de
  16.777.216 pixels e 16.384 pixels por eixo no gerador. Alfa ausente na leitura vira 255.
- Luminância usa pesos 0,2126 / 0,7152 / 0,0722 nos canais codificados; não aplica
  conversão sRGB para luz linear.
- A altura é normalizada entre preto e branco, limitada a `[0,1]`, elevada à
  curva e opcionalmente invertida. Branco deve ser maior que preto.
- Suavização usa filtro box separável, ponderado por alfa. O raio é expresso em
  pixels da fonte, escalado e arredondado na resolução do preview.
- Normais usam diferenças centrais. Intensidade 1 representa amplitude de altura
  de `max(1,min(width,height)-1)/32` pixels na resolução de saída. Assim, a escala
  aproximada do relevo acompanha o tamanho da imagem; não é uma unidade de mundo.
- A normal aponta para `+Z`; `+Y` aponta para cima na imagem. `-Y` inverte apenas
  o canal verde. Ao atribuir ao material, use a mesma convenção para interpretá-lo.
- Pixels totalmente transparentes produzem normal neutra; ao derivar um pixel
  visível, vizinhos transparentes usam sua altura central. O alfa original é
  preservado. Semitransparência pondera a suavização, sem reduzir a altura em si.
- `clamp` replica a borda; `repeat` consulta o lado oposto. Repetição não transforma
  automaticamente uma fonte descontínua em textura sem costura.
- PNG exportado contém os vetores codificados, sem iluminação ou correção de cor.
  A normal neutra quantizada é aproximadamente `(128,128,255)`.

Opções aceitas: `channel` (`luminance/r/g/b/a`), `black` e `white` (`0..1`),
`curve` (`0.1..8`), `invert` (booleano), `blur` (`0..32`), `strength` (`0..16`),
`convention` (`+Y/-Y`) e `edge` (`clamp/repeat`).

## Persistência, recursos e custo

O projeto usa formato textual versionado, com caminho codificado em hexadecimal
e pares chave/valor. `save` exige caminho absoluto da fonte (como o seletor de
arquivos fornece), evitando reinterpretar caminhos relativos ao mover o projeto. Não executa Lua ao abrir; rejeita versões, chaves, valores
inválidos e arquivos acima de 16 KiB. Não persiste caches, jobs ou handles GPU.

O editor aguarda 150 ms após mudanças para iniciar a geração e cancela trabalhos
anteriores. Mudar a luz reutiliza a normal já gerada. Quatro slots temporários
(original, altura, normal e iluminado) são recarregados no lugar, evitando crescer
o cache a cada ajuste. Ao trocar a fonte/encerrar, libera suas imagens GPU e remove
os arquivos. Os pequenos objetos de cache permanecem sob propriedade da engine.

Use `preview:clear()` no encerramento. As operações `textureInfo:reload/release`
afetam a textura compartilhada; use apenas caminhos temporários privados nesse
fluxo. O editor mantém esses arquivos enquanto desenha seus respectivos previews.

A geração Lua é cooperativa e utiliza linhas compactadas para limitar o custo de
tabelas numéricas. Leitura de arquivo, concatenação final, codificação PNG e upload
são síncronos; imagens grandes ainda podem causar pausas nessas etapas. Exportação
em resolução original prioriza a conclusão do arquivo, sem prometer latência fixa.
Uma futura implementação nativa/GPU pode substituir o processamento preservando
o contrato dos módulos.

Uma medição local com o interpretador Lua Debug, imagem constante de 1024x1024 e
raio de suavização 8 consumiu aproximadamente 10,5 s de CPU, em 1.494 etapas
(maior etapa: 10 ms), com cerca de 33 MiB no heap Lua ao final. O tempo de parede
na UI será maior por causa do orçamento por frame; essa medição não inclui PNG,
upload nem memória nativa. Aceleração do processamento é uma prioridade antes de
tratar imagens grandes como um fluxo interativo.

## Validação

```sh
./bin/debug/linux_x86/lua-5.4.1.exe src/test-lib/normal_map_generator_test.lua
timeout -s KILL 15 ./bin/debug/linux_x86/mini-mbm \
  --scene src/test-lib/normal_map_launcher_smoke.lua \
  --disable_select_monitor --nosplash -w 1180 -h 800
timeout -s KILL 35 ./bin/debug/linux_x86/mini-mbm \
  --scene src/test-lib/normal_map_editor_smoke.lua \
  --disable_select_monitor --nosplash -w 1180 -h 800
```

O teste gráfico exige display e build com `USE_TEXTURE_MISSING_DIALOG=0`.
O teste de launcher usa `__onLoadScene` da engine e verifica a inicialização antes
de desenhar os painéis. A cena usa callbacks globais e não retorna uma tabela:
retornar uma tabela faria o launcher selecionar o contrato de cena por métodos.
Fixtures e PNG exportado ficam em `mini-mbm-normal-smoke` sob `TEMP`, `TMPDIR`
ou `/tmp` (nessa ordem) para inspeção.
Verifique os marcadores `NORMAL MAP LAUNCHER SMOKE PASS` e
`NORMAL MAP EDITOR SMOKE PASS` nos respectivos testes e a ausência de erros no log;
o código de saída da engine sozinho não comprova ausência de erros Lua.

Validado em Linux/GLES: compilação, altura constante, rampas X/Y, inversão,
transparência com/sem suavização, repetição, imagem 1x1, jobs independentes,
cancelamento, projeto, PNG sem perda, falha de escrita, recarga/liberação GPU,
preview renderizado e ausência de geração/upload durante oito segundos ociosos.
O teste também prepara uma malha 3D, aplica o PNG exportado e compara capturas com
normal mapping ativado e com intensidade zero para verificar seu consumo real.
A captura visual foi inspecionada; cliques e arrastes reais não foram automatizados.
Essa validação inicial não incluiu Windows, macOS/Metal ou plataformas móveis.

### Windows (MSVC Debug/Win32)

Validação de 2026-10-01, Visual Studio 2026, normal mapping habilitado:

| Verificação | OpenGL ES | DirectX 11 | DirectX 9 |
|---|---|---|---|
| Build do engine, ImGui e libTest | Passou | Passou | Passou |
| Inicialização pelo launcher | Passou | Passou | Passou |
| Gerador: PNG, preview, material 3D e oito segundos ociosos | Passou | Passou | Passou |
| Integração Image Mesh: residual, comparações, exportação e cancelamento | Passou | Passou | Passou |
| Integração MeshDebug: seleção, geração, desfazer e limpeza | Passou | Passou | Passou |

Os testes Lua puros do gerador, residual e baker passaram no interpretador 5.4.1
compilado das fontes incluídas no projeto. Preparação e persistência nativas,
argumentos UTF-8 do launcher e recursos GPU passaram; no DirectX 11, a camada de
debug também aprovou os recursos e seu ciclo de vida. A janela do gerador foi
inspecionada visualmente em DirectX 11. Cliques, arrastes e diálogos nativos não
foram automatizados. Os testes gráficos DirectX 9 precisaram do desktop real:
nesse ambiente, o sandbox não conseguiu criar o dispositivo gráfico.

O build OpenGL ES precisa vincular `libEGL.dll.lib` e `libGLESv2.dll.lib` no
projeto `core_mbm`; copiar as DLLs para a saída não substitui essa dependência.
Os testes nativos também usam duas funções do bridge privado de normal mapping
exportadas pela DLL. A conversão dos argumentos do launcher preserva o armazenamento
das strings UTF-8 até terminar o parsing, incluindo flags curtas e caminhos acentuados.

Para compilar com o Visual Studio mais recente instalado, em PowerShell na raiz:

```powershell
$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
& "$vs/MSBuild/Current/Bin/MSBuild.exe" platform-msvs/mini-mbm.sln `
  /p:Configuration=Debug /p:Platform=x86 /p:MbmBackend=OpenGLES `
  /p:MbmCoreFeatureDefines=AUDIO_ENGINE_PORT_AUDIO /m:4
```

O override de `MbmCoreFeatureDefines` desativa o diálogo de textura ausente apenas
nesse build de testes. Use `DirectX11` ou `DirectX9` em `MbmBackend` para os outros
backends; ao alternar, reconstrua os projetos dependentes ou use saídas e objetos
isolados. Não misture DLLs de backends diferentes.

```powershell
& ./platform-msvs/Debug/libTest.exe --launcher-args-test
& ./platform-msvs/Debug/mini_mbm.exe --scene src/test-lib/normal_map_editor_smoke.lua `
  --disable_select_monitor --nosplash -w 1180 -h 800
```

O smoke encerra sozinho e deve imprimir `NORMAL MAP EDITOR SMOKE PASS`. Execute
também `normal_map_launcher_smoke.lua`, `image_mesh_normal_map_smoke.lua` e
`mesh_debug_normal_generator_smoke.lua`, verificando seus marcadores de sucesso.
Para automação, acrescente um timeout externo de 45 s para o gerador, 135 s para
Image Mesh e 80 s para MeshDebug, encerrando somente o processo lançado pelo teste.
O caso de meta impossível do Image Mesh usa uma grade pequena para manter as sete
tentativas de raio viáveis no MSVC sem otimização; restaura a grade original antes
de verificar a preservação dos parâmetros manuais.

### macOS (Metal Debug/arm64)

Validação de 2026-10-01 em macOS 26.6.2, Apple M4, Retina 2x, AppleClang 21
e CMake 4.2.0, engine 7.344. Normal mapping habilitado, limite compilado de quatro
luzes e `MTL_DEBUG_LAYER=1` em todos os testes gráficos:

| Verificação | Resultado |
|---|---|
| Build do engine, ImGui e testLib | Passou |
| Lua puro: gerador, residual, baker, modelo Image Mesh e políticas de normais | Passou |
| Inicialização do gerador pelo launcher da engine | Passou |
| Gerador: PNG, reload/release GPU, material 3D e oito segundos ociosos | Passou |
| Image Mesh: residual/base tangente, laterais, comparações, meta automática, exportação, desfazer e cancelamento | Passou |
| MeshDebug: frames/subsets, `.msh`/`.imesh`, aplicação, falha de leitura, desfazer, limpeza e painel ocioso | Passou |
| Suíte runtime: preparação, persistência, recursos/pipelines Metal, leitura de pixels e renderização com/sem índices | Passou |

Os quatro smokes de editor terminaram com seus marcadores de sucesso, código 0
e sem diagnósticos de falha da validação Metal. O caso de cancelamento do Image Mesh
imprime `ime_generation_cancelled`; o caso de fonte inexistente do MeshDebug
imprime `Invalid image or more than 16 million pixels`. São falhas provocadas pelo
teste, que também verifica a preservação da malha anterior.

O PNG gerado e as capturas `material-mapped.png`/`material-flat.png` foram
inspecionados: o material consome o mapa e muda a iluminação em relação à
intensidade zero. Isso não valida toda a interface visual. Os smokes chamam as
operações reais dos editores programaticamente e desenham seus painéis; não
automatizam cliques, arrastes, atalhos nem diálogos nativos. A seleção no diálogo
inicial do aplicativo também não foi exercitada: o smoke de launcher verifica o
carregamento da cena por `__onLoadScene`.

Para reproduzir a partir da raiz:

```sh
cmake -S . -B build/macos_debug -DPLAT=MacOs -DUSE_ALL=1 \
  -DUSE_TEXTURE_MISSING_DIALOG=0 -DCMAKE_BUILD_TYPE=Debug
cmake --build build/macos_debug -j 8
```

Metal é o backend padrão. A configuração agora habilita Objective-C++ **depois**
de resolver esse padrão. Antes da correção, um build novo sem `-DUSE_METAL=1`
falhava ao configurar `normal-map-native-resource-tests.cpp`, cuja linguagem é
`OBJCXX`. Os avisos de compilação observados são de bibliotecas de terceiros.

O CMake atual gera o executável Lua isolado somente no Linux. No macOS, compile
o interpretador das fontes incluídas e vincule a biblioteca recém-construída:

```sh
cc -g -I third-party/lua-5.4.1 third-party/lua-5.4.1/lua.c \
  -L bin/debug/arm64 -llua-5.4.1 -Wl,-rpath,"$PWD/bin/debug/arm64" \
  -o /tmp/normal-map-lua-5.4.1
for test in normal_map_generator_test normal_map_residual_test normal_map_baker_test \
  image_mesh_model_test mesh_debug_normals_test; do
  /tmp/normal-map-lua-5.4.1 "src/test-lib/$test.lua" || break
done
```

Execute os smokes no desktop macOS. Este exemplo usa Python 3 para aplicar o
timeout externo sem depender do comando GNU `timeout`; guarda um log por teste
e exige o marcador, código de saída e ausência de diagnósticos fatais:

```sh
python3 - <<'PY'
import importlib.util, os, subprocess, tempfile
from pathlib import Path
spec = importlib.util.spec_from_file_location('runner', 'src/test-lib/run-normal-map-tests.py')
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)
output = Path(tempfile.mkdtemp(prefix='normal-map-macos-editors-'))
print('Logs:', output, flush=True)
env = dict(os.environ, MTL_DEBUG_LAYER='1')
cases = [
    ('normal_map_launcher_smoke', 'NORMAL MAP LAUNCHER SMOKE PASS', 20),
    ('normal_map_editor_smoke', 'NORMAL MAP EDITOR SMOKE PASS', 45),
    ('image_mesh_normal_map_smoke', 'IMAGE MESH NORMAL PASS', 135),
    ('mesh_debug_normal_generator_smoke', 'MESH DEBUG NORMAL GENERATOR PASS', 80),
]
for name, marker, seconds in cases:
    command = ['bin/debug/arm64/mini-mbm', '--scene', 'src/test-lib/' + name + '.lua',
               '--disable_select_monitor', '--nosplash', '-w', '1180', '-h', '800']
    with (output / (name + '.log')).open('wb') as log:
        result = subprocess.run(command, env=env, stdout=log,
                                stderr=subprocess.STDOUT, timeout=seconds)
    text = (output / (name + '.log')).read_text(errors='replace')
    failure = runner.verdict(result.returncode, text, marker)
    assert failure is None, (name, failure)
    assert 'Metal API Validation Enabled' in text, name
    print(name, 'PASS', flush=True)
PY
```

Para a suíte nativa/runtime, escolha um diretório de saída que ainda não exista:

```sh
python3 src/test-lib/run-normal-map-tests.py \
  --test-lib bin/debug/arm64/testLib --engine bin/debug/arm64/mini-mbm \
  --backend metal --normal 1 --lights 4 --require-native-validation \
  --output /tmp/normal-map-macos-runtime
```

O runner grava `report.json`, logs e capturas. Nesta sessão, os relatórios ficaram
em `/tmp/normal-map-macos-runtime-20261001` e
`/tmp/normal-map-macos-editors-20261001`. Esses diretórios são temporários.

Para a passada manual, abra `editor/normal_map_editor.lua`,
`editor/image_mesh_editor.lua` e `editor/mesh_debug.lua` com o mesmo executável:
verifique seleção/salvamento com caminhos acentuados, controles de normal map,
arrastes e posicionamento dos painéis em Retina, desfazer e reabertura dos arquivos
exportados. macOS/OpenGL ES, Intel, Release e plataformas móveis não foram
validados nesta sessão.

## Integração ao Image Mesh (entrega 2)

O Image Mesh reutiliza `normal_map_generator` e `normal_map_panel` através de
`image_mesh_normal_map.lua`. A altura vem do raster nativo do próprio Image Mesh,
sem repetir níveis/canais no painel compartilhado. O resultado é aplicado
à frente e às laterais por faixa ou textura repetida da malha 3D, com tangentes próprias de cada
superfície e iluminação do runtime. Consulte o
[fluxo, persistência e exportação](image-mesh-editor.md#normal-map-relief).

Teste da integração:

```sh
timeout -s KILL 135 ./bin/debug/linux_x86/mini-mbm \
  --scene src/test-lib/image_mesh_normal_map_smoke.lua \
  --disable_select_monitor --nosplash -w 1180 -h 800
```

Marcador esperado: `IMAGE MESH NORMAL PASS`. A mensagem
`ime_generation_cancelled` faz parte do caso de cancelamento intencional.

## Geração no MeshDebug (7.344)

O treenode **Gerar normal map** complementa o painel existente de material e
preparação de tangentes. Os controles são rascunhos: **Gerar e aplicar** inicia
um trabalho cooperativo, mantém a malha atual enquanto processa uma cópia e só
publica o resultado ao terminar. Cancelamento e falhas não aplicam a cópia.
O editor suspende suas outras operações durante esse trabalho; leitura de pixels,
salvamento PNG e preparação nativa de tangentes continuam com etapas síncronas.

Para entradas provenientes de **`.imesh`**, usa o projeto em memória e a região
selecionada, com a fonte/altura já configurada. Reutiliza os controles de habilitação,
suavização, intensidade, convenção, bordas, residual, compensação e separação manual,
além do painel de meta automática de triângulos. A região é reconstruída pelo mesmo
pipeline do Image Mesh, portanto edições feitas somente na malha são substituídas.
Os parâmetros aplicados atualizam os overrides da região em memória. O botão
**Salvar projeto Image Mesh aplicado como...** persiste o projeto explicitamente;
carregar ou aplicar não sobrescreve o `.imesh` de origem. Rascunhos não aplicados
não são incluídos nesse salvamento. Outras regiões permanecem inalteradas.

Para **`.msh`** e outras entradas de malha sem projeto Image Mesh, selecione uma
imagem de altura alinhada aos UVs e os frames/subsets (0 significa todos). Estão
disponíveis canal, níveis, curva, inversão, suavização, intensidade, convenção e
tratamento das bordas. O resultado é aditivo: não pressupõe uma altura original
nem permite residual/compensação de Image Mesh em uma malha arbitrária. Posições,
índices e UVs são preservados; os materiais selecionados recebem a textura normal
e suas tangentes são preparadas. A intensidade é gravada nos pixels, e a intensidade
do material resultante fica em 1. Os ajustes de material existentes continuam
independentes. **Exportar PNG gerado...** grava uma cópia do último mapa aplicado.
Esses rascunhos de fonte/parâmetros não são incorporados ao formato `.msh`.

O desfazer existente restaura a malha anterior e, para `.imesh`, os overrides da
região correspondentes àquela operação. Mapas gerados são privados à entrada e
mantidos enquanto ela existe, inclusive para suportar desfazer e materiais de
frames/subsets não substituídos. Para persistir a malha, salve/exporte também suas
texturas (por exemplo, **Save All to Folder**); o arquivo de malha referencia PNGs.
Remover a entrada, limpar a lista ou encerrar a cena libera os recursos privados.
Nenhuma geração, leitura de fonte ou gravação é disparada apenas por desenhar o painel.

O teste `src/test-lib/mesh_debug_normal_generator_smoke.lua` exercita o MeshDebug
real: seleção individual/todos, material com intensidade 1, tangentes, cancelamento,
falha de leitura, desfazer, projeto `.imesh` separado da origem, recarga de malha,
limpeza dos temporários e painéis ociosos. Validado em Linux/GLES e em
Windows/OpenGL ES/DX9/DX11 e macOS/Metal; cliques e arrastes reais não foram automatizados.
