# Plano de normal mapping 3D

Status: preparação CPU, persistência opcional e API C++/Lua implementadas.
Caminhos estáticos OpenGL ES, DirectX 11, DirectX 9 SM3 e Metal implementados; controles de
convenção/intensidade no Mesh Debug disponíveis em 7.319. DirectX 11 validado
no Windows em 7.320, DirectX 9 em 7.321 e Metal/macOS Apple M4 em 7.323.

Primeira entrega: normal mapping de malhas estáticas em GLES, DX9, DX11 e Metal,
com propriedades editáveis e persistentes. Skinning LBS/DQS, deformações dinâmicas
e geração tardia de bases em runtime ficam para uma entrega posterior. As seções
futuras deste plano registram contratos e cenários, não requisitos de conclusão
da primeira entrega. A matriz de backends efetivamente executados deve ser declarada.

### Progresso da etapa 1

- Implementado `src/core_mbm/private/normal-map-preparation.*`, sem API pública
  adicional ou dependência de contexto gráfico. Integrado ao salvamento Mesh Debug
  e ao carregamento runtime síncrono/assíncrono quando faltam dados preparados.
- MikkTSpace incorporado sem alterações, na revisão
  `3e895b49d05ea07e4c2133156cfa94369e19e409`, com licença e origem em
  `third-party/mikktspace/`. Geração e importação são políticas explícitas.
- Preparação opcional por subset. Ausência de solicitação, normais ou UVs produz
  resultado vazio. Arrays inconsistentes, índices inválidos e valores não finitos
  são rejeitados sem publicar resultado parcial.
- Tangentes por canto preservam sinais em UVs espelhadas. Lotes privados mantêm o
  índice do vértice fonte e índices locais de 16 bits, com divisão entre triângulos
  quando necessário. A cópia de influências por esse remapeamento está implementada
  como operação CPU privada; o consumo no desenho esquelético ainda será integrado.
- Listas, strips e fans são convertidos para triângulos preservando a ordem e o
  winding. Triângulos sem base utilizável recebem tangente zero com sinal zero;
  o futuro shader deverá usar a normal da malha nesse caso.
- Verificação CPU pelo comando `testLib --normal-map-preparation-tests`: plano,
  normais curvas, costura espelhada, subsets opcionais, topologias, tangentes
  importadas, entradas inválidas, degeneração e particionamento de 66 mil vértices.
  CMake e projetos Visual Studio incluem os fontes; execução validada em Linux.

- Implementada a seção opcional `SECTION_NORMAL_MAP_TANGENTS` (14, versão 1),
  sem modificar os layouts existentes. Dados privados por frame preservam tangentes,
  sinal, remapeamento, lotes e assinatura da geometria fonte. Leitores de runtime
  e autoria validam o conteúdo independentemente da ordem das seções.
- `saveV11` prepara somente quando necessário, reutiliza dados válidos e refaz a
  base quando a geometria muda. Sem mapa nem preparação retida, não emite seção.
  Cópia/remoção de frames preserva/reindexa o cache; mudanças estruturais de subsets
  o invalidam. A extração de runtime copia a preparação pela fronteira privada.
- `testLib --normal-map-persistence-tests` cobre round-trip com/sem compressão,
  bases importadas, assinatura após edição, seções ausentes/reordenadas/duplicadas,
  versões inválidas, referências inválidas, NaN, todos os prefixos truncados,
  UV compartilhada, cópia/remoção de frame e geometria indexada.

- Implementada a seção opcional `SECTION_NORMAL_MAP_MATERIALS` (15, versão 1),
  com convenção de origem `+Y`/`-Y` e intensidade finita não negativa por frame/subset.
  Defaults `+Y` e `1`; configurar propriedades não atribui mapa nem prepara tangentes.
- APIs C++/Lua `getNormalMapSettings` / `setNormalMapSettings` disponíveis para
  autoria e material compartilhado em runtime. Valores inválidos não alteram estado.
  Cópia, remoção e reordenação preservam a associação; mesclar subsets com propriedades
  diferentes é recusado, sem mutação. Não há novos controles de UI neste incremento.
- Round-trip com compressão, ausência/defaults, valores inválidos, seções corrompidas,
  cópia/remoção/reordenação/mesclagem, extração indexada e loaders síncrono/assíncrono
  cobertos pelos testes em Linux/OpenGL ES, incluindo execução da API Lua.
  Preparação, persistência e regressão esquelética passaram; execução em outros
  backends permanece pendente. Alterar apenas propriedades preserva os bytes das tangentes
  importadas. A versão 7.313 identifica esta entrega.

- API C++/Lua `prepareNormalMap` por frame/subset com políticas `preserve`,
  `generate` e `import`. Preparação antecipada não atribui textura. `preserve`
  reutiliza dados válidos e gera os ausentes/obsoletos; `generate` substitui a base
  selecionada; `import` recebe tangentes por canto de triângulo expandido.
  Bases válidas dos demais subsets são preservadas. Falhas não publicam preparação
  parcial. Relatório informa lotes, vértices, triângulos sem base e reutilização.
- Mesh Debug aceita frame/subset `0` para todos, com relatório agregado e falhas
  identificadas por frame/subset (7.315). Sucessos parciais mantêm um Undo único
  para toda a ação. Normais geométricas ausentes devem ser geradas antes das tangentes.
- Mesh Debug oferece ação explícita de preservar/recalcular por subset, com o
  snapshot de Undo compartilhado com transformações. O painel mostra o resultado
  da última ação, não executa preparação nem varredura de geometria por frame.
- Image Mesh Editor oferece `normalMapPrecompute`, desativado por padrão, nas
  propriedades. Participa do projeto, seleção múltipla e histórico; prepara após
  simplificação na geração compartilhada com Mesh Debug e persiste na exportação.
- O adaptador de dados intermediários de Mesh Debug aceita `normalMapPolicy`,
  `normalMapPrecompute` e `subset.cornerTangents`. Tangentes fornecidas exigem uma
  escolha explícita de importar ou recalcular. Importar junto com pós-processamento
  de UV/geometria é recusado; recalcular usa a geometria resultante no salvamento.
  O produtor Blender direto ainda não extrai/publica `cornerTangents`; a importação
  de uma base externa está disponível pela API/adaptador, não por um novo seletor
  de arquivo na UI. Arquivos MSH já preparados continuam preservados ao carregar.
- Versão 7.314: testes CPU de preparação/persistência e regressão esquelética,
  API Lua, projeto/Undo/Redo/exportação, importação explícita e painel ImGui
  executados no Linux/OpenGL ES. O teste de painel simula a ação no binding e mede
  20 frames ociosos; não substitui teste manual de cliques.

- Versão 7.317: `normal_map::remapSkinWeights` constrói pesos transitórios por lote
  usando `sourceVertices`. Valida os pesos canônicos uma vez, a associação ao frame,
  os índices locais e as referências fonte. Preserva ID do esqueleto, paleta e os
  quatro slots de influência, incluindo sentinelas vazias. Erros não publicam saída
  parcial. Sem lotes preparados, não exige esqueleto nem aloca pesos derivados.
- Prova CPU: costura espelhada duplica influências sem alterar a malha fonte;
  deformar por LBS/DQS antes ou depois do remapeamento produz as mesmas posições
  e normais. A preparação dos atributos GPU resolve a mesma paleta. Particionamento
  de 66 mil vértices mantém a associação dos pesos nos dois lotes de 16 bits.
  Contagem/frame inválidos, referências fora do intervalo, pesos vazios e NaN são
  rejeitados. Essas provas não executam shaders de skinning com tangentes.
- Prova dos consumidores de autoria: quad com quatro vértices produz seis vértices
  preparados, mantendo ponteiros, contagens e índices fonte usados por seleção.
  Os payloads das seções de geometria e física permanecem idênticos após exportação;
  reload reutiliza a base de seis vértices sem substituir os quatro da autoria.
  A cena de extração cobre também essa fixture e duas instâncias do mesmo asset,
  inclusive após preparar novamente uma cópia de edição destacada.
- Build `testLib`/`mini-mbm`, preparação, persistência, fundação esquelética e cena
  de extração passaram em Linux/OpenGL ES 3.2. A seleção foi validada pela API de
  dados usada pelo editor, sem automatizar cliques. O helper de pesos ainda não é
  chamado pelo render loop: sua integração e a deformação das tangentes pertencem
  ao caminho de renderização, sem regeneração por frame.

### Progresso da etapa 2 — versão 7.318

- Upload único dos lotes preparados para buffers privados OpenGL ES, preservando
  buffers de autoria. Atributo tangente e base em espaço de visão nos shaders
  padrão e `lit textured.ps`; normal por inversa transposta, ortogonalização da
  tangente, sinal do determinante e fallback finito para base/transformação inválida.
- Normal perturbada usada no difuso/especular, com convenção e intensidade por
  subset. Intensidade zero e remoção do mapa retornam ao desenho fonte.
  Slots materiais são transferidos ao carregar, incluindo ausência explícita por
  subset para impedir vazamento. Não há geração ou upload contínuos no render loop.
- Corrigido o contexto de iluminação do render-to-texture e sua matriz de visão;
  estado anterior é restaurado ao sair. Corrigida a cor direcional do shader
  embutido, que consultava a primeira luz pontual. Mantida sua diferença intencional:
  3D direcional no recurso embutido versus direcional + pontuais no shader gerado.
- Teste `normal-map-render-test.lua` passa com IB e VB: mapa neutro/detalhe,
  intensidade zero, remoção, convenções equivalentes, luz pontual, subsets mistos,
  rotação/escala não uniforme, escala negativa, vertex shader personalizado legado,
  HUD sem iluminação e normal map 2dw. Intensidades maiores que 1 usam coeficientes
  limitados antes da normalização para evitar overflow em mediump.
  Diferença média em canais RGBA de 8 bits: neutro `0.0350`, detalhe `1.9351`,
  intensidade zero/remoção/convenções equivalentes `0`, pontual `2.4017`.
  A metade da imagem sem mapa permanece idêntica. Não é teste de paridade entre GPUs.
- `module_001.msh` em Downloads foi apenas lido e capturado com câmera oblíqua e
  luz rasante: diferença média `0.8674` com seu mapa original e `0.0367` com mapa
  neutro. As imagens confirmaram detalhe de superfície ausente no baseline.
- Build, preparação, persistência, fundação esquelética e extração de autoria
  passaram em Linux/Mesa OpenGL ES 3.2. ES2/Android, perda real de contexto,
  DirectX e Metal ainda precisam de execução própria. Os recursos derivados têm
  liberação e recriação pelo carregamento; não se declara teste de perda de contexto.
- Limites: assets esqueléticos usam o caminho anterior; atribuição tardia sem base
  previamente carregada e regeneração após alterações arbitrárias de geometria
  permanecem na etapa 4. Atualização dinâmica descarta buffers derivados obsoletos.
  Importador externo de tangentes permanece pendente. Controles de convenção e
  intensidade no Mesh Debug foram acrescentados em 7.319. Materiais sem mapa não passam a exigir tangentes.

Para reproduzir as comparações visuais (diretório de saída fora do repositório):

```sh
mkdir -p /tmp/mini-mbm-normal-render
timeout -s KILL 20 bin/debug/linux_x86/mini-mbm \
  --scene src/test-lib/normal-map-render-test.lua \
  --disable_select_monitor --nosplash -w 320 -h 240
MBM_NORMAL_MAP_TEST_VB=1 timeout -s KILL 20 bin/debug/linux_x86/mini-mbm \
  --scene src/test-lib/normal-map-render-test.lua \
  --disable_select_monitor --nosplash -w 320 -h 240
```

Exigir o marcador `NORMAL MAP VISUAL PASS`, além do término do processo.
DirectX 11 concluído no incremento 7.320, DirectX 9 SM3 em 7.321 e Metal/macOS em 7.323 abaixo.
A exposição de convenção/intensidade no Mesh Debug precede esse incremento;
a integração esquelética/dinâmica da etapa 4 fica fora da primeira entrega.
Versão 7.312 identifica a entrega de persistência das tangentes.

Para reproduzir a integração em Linux, gerar as fixtures e executar:

```sh
MBM_NORMAL_MAP_FIXTURE_DIR=/tmp/mini-mbm-normal-map-fixtures \
  bin/debug/linux_x86/testLib --normal-map-persistence-tests
MBM_NORMAL_MAP_FIXTURE_DIR=/tmp/mini-mbm-normal-map-fixtures \
  timeout -s KILL 15 bin/debug/linux_x86/mini-mbm \
  --scene src/test-lib/normal-map-runtime-test.lua \
  --disable_select_monitor --nosplash -w 320 -h 240
MBM_NORMAL_MAP_FIXTURE_DIR=/tmp/mini-mbm-normal-map-fixtures \
  timeout -s KILL 20 bin/debug/linux_x86/mini-mbm \
  --scene src/test-lib/normal-map-authoring-test.lua \
  --disable_select_monitor --nosplash -w 800 -h 600
```

A cena `normal-map-runtime-test.lua` verifica leitura síncrona/assíncrona,
rejeição dos arquivos inválidos e extração de uma malha indexada. A regressão
específica de extração pode ser executada após gerar as mesmas fixtures:

```sh
MBM_NORMAL_MAP_FIXTURE_DIR=/tmp/mini-mbm-normal-map-fixtures \
  timeout -s KILL 15 bin/debug/linux_x86/mini-mbm \
  --scene src/test-lib/normal-map-readback-test.lua \
  --disable_select_monitor --nosplash -w 320 -h 240
```

### Extração não indexada no OpenGL ES — concluída em 7.316

- [x] Reproduzida a falha de `loadDebugFromMemory` com `no-map.msh`.
  O caminho VB consultava os buffers compartilhados exclusivos de IB e não
  reconstruía os subsets. O caminho IB também deduzia contagens incorretas pelo
  maior índice global, perdendo vértices não usados e alterando intervalos.
- [x] Extração usa os buffers VB por subset e os intervalos originais de autoria;
  IB preserva a contagem completa e os índices globais. Leituras validam tamanho,
  resultado do mapeamento e do unmap; bindings anteriores são restaurados.
  O mapeamento usa acesso de leitura em ES3 ou `EXT_map_buffer_range` junto de
  `OES_mapbuffer` em ES2. A antiga leitura de memória mapeada apenas para escrita
  era indefinida segundo a [especificação EXT_map_buffer_range](https://registry.khronos.org/OpenGL/extensions/EXT/EXT_map_buffer_range.txt).
  Sem suporte à leitura, a operação informa falha; não usa o acesso inválido.
- [x] Slots extras de material de autoria são copiados dos subsets runtime, junto
  dos dados opcionais de normal map. Isso não serializa overrides de textura
  armazenados apenas nos estágios do shader por `setMaterialTexture`.
- [x] Nova cena de regressão com 11 casos: IB/VB, com/sem mapa, tangentes importadas,
  múltiplos subsets/frames, vértice não usado e loaders síncrono/assíncrono.
  Compara geometria, índices, texturas e propriedades na extração, extração repetida
  e salvamento comprimido/reabertura. Resultado: `NORMAL MAP READBACK PASS`.
  Comparação adicional dos payloads descomprimidos das seções 14/15 confirmou
  bytes idênticos entre os arquivos fonte e extraídos nos 11 casos.
- [x] Build `mini-mbm`/`testLib` e suítes CPU de preparação/persistência passaram
  em Linux/Mesa OpenGL ES 3.2. Forçar Mesa ES2 falhou na criação do contexto X11
  (`num_configs > 0`), antes do teste; ES2/Android e Windows/Metal seguem pendentes.

Esta pendência de extração está resolvida no backend validado. A prova CPU de
consumidores de autoria e remapeamento de influências foi acrescentada em 7.317,
conforme o progresso acima. O caminho estático OpenGL ES passou a aplicar normal maps em 7.318.

## 1. Problema original e evidências da análise inicial

Na análise inicial, o shader padrão não aplicava `TextureNormal` na iluminação 3D.
Assim, o arquivo podia conter
o mapa corretamente e ainda assim produzir a mesma imagem com e sem essa textura.
O caminho estático OpenGL ES foi corrigido em 7.318; os demais backends seguem pendentes.

| Constatação | Evidência no repositório | Consequência |
|---|---|---|
| O Image Mesh Editor cria o preview como `mesh:new('3d')` | `editor/image_mesh_editor.lua`, criação do preview e `onInitScene` | O preview segue a iluminação 3D |
| A presença do mapa é habilitada apenas em `lightMode == 2` | `shader-opengl_es.cpp`, `shader-directx9.cpp`, `shader-directx11.cpp`, `shader-metal.mm`, em `src/core_mbm/` | O modo 3D não ativa o mapa |
| O cálculo 3D usa a normal do vértice | Shaders gerados nesses backends e recursos `shader-resource-*` | Alterar somente `HasNormalMap` não resolve |
| Existem slots tipados e binding de normal map no estágio 2 | `include/core_mbm/texture-role.h`, `MESH_MBM::getMaterialTexture` / `setMaterialTexture` | Reutilizar o armazenamento e o slot atuais |
| O formato de vértice expõe posição, normal e UV, sem tangente | `include/core_mbm/shader.h`, `FVF_PROVIDE_BY_ENGINE` | A base tangente precisa ser gerada ou reconstruída |
| A documentação descreve normal mapping em `2dw` | `docs/light.md`, Material Texture Slots | Falta suporte 3D; não é evidência de corrupção do asset |

A inspeção de `/home/michel/Downloads/module_001.msh` encontrou um frame com
535 vértices, um subset, normais e UVs. O slot normal (role 2) aponta para
`/home/michel/Downloads/download.png`, existente e visualmente compatível com
um normal map. Os checksums das seções passaram. Esse arquivo é uma reprodução
local; os testes versionados devem usar fixtures próprias e independentes de Downloads.

## 2. Resultado esperado e escopo

Uma malha 3D com normais, UVs e mapa no slot normal deve usar a normal resultante
na iluminação difusa e especular, tanto direcional quanto pontual. O detalhe deve
acompanhar a superfície quando o objeto, a câmera ou a luz se movem.

Nesta primeira entrega, cobrir malhas estáticas em OpenGL ES, DirectX 9, DirectX 11
e Metal, incluindo translação, rotação e escala do objeto. A conclusão deve declarar
a matriz efetivamente testada. Buffers dinâmicos e animação esquelética CPU/GPU
LBS/DQS pertencem à evolução posterior.

Preservar o comportamento de `2dw`, `2ds`, materiais sem mapa e shaders unlit.
Normal map e dados de tangentes são opcionais. Sem normal map, o asset usa as
normais dos vértices na iluminação 3D, sem exigir geração ou armazenamento de
tangentes. Permitir preparação antecipada explícita para materiais que receberão
mapas durante o jogo; essa opção não deve atribuir uma textura normal ao material.
PBR, parallax, displacement e consumo dos mapas specular/AO/emissive ficam fora
deste trabalho. Normal mapping altera a iluminação, não a geometria ou a silhueta.

## 3. Decisão arquitetural

Preparar e armazenar tangentes durante a importação/exportação do asset. Preservar
tangentes importadas quando válidas e compatíveis com o bake, ou gerá-las a partir
de posição, normal, UV e topologia com MikkTSpace. A política de importar ou
recalcular deve ser explícita e reproduzível; não substituir tangentes válidas
silenciosamente. O fluxo principal será:

`Modelo + texturas -> importação/validação -> asset preparado -> leitura/upload -> renderização`

Quando houver normal map ou preparação antecipada explícita, o asset preparado
deve persistir tangente e sinal de orientação, além da geometria
de renderização e do remapeamento necessários para preservar costuras. Resolver
a separação entre representação de autoria e representação preparada na etapa 1.
No runtime, manter os dados em `Impl` / `BackendData` / estruturas privadas de backend,
sem expor containers ou armazenamento mutável adicional nos headers públicos.
Reconstruir a bitangente segundo a convenção escolhida e transformar a normal
amostrada do espaço tangente para o mesmo espaço usado pelas luzes.

Geração sob demanda atende edição, geometria procedural e materiais com normal map
sem tangentes preparadas, inclusive na atribuição posterior de mapas. Usar a mesma
implementação de preparação, independentemente da data de criação do asset.
Assets com tangentes preparadas válidas não devem executar MikkTSpace ao carregar.
Remover o mapa deixa de usar a base; tangentes já preparadas podem permanecer no asset.

Persistir os dados em seções opcionais do MSH, com payload versionado e associação
explícita aos frames/subsets. A ausência dessas seções é válida; não caracteriza
asset legado, corrupção ou necessidade de migração. A etapa 1 define os tipos e
layouts e integra sua leitura aos loaders de runtime e autoria. Os layouts
existentes não precisam receber campos obrigatórios para esta funcionalidade.
Não incluir projeto de compatibilidade histórica, migração ou exportação para
engines antigas. A política atual de rejeitar seções desconhecidas não será
alterada por este trabalho.

| Alternativa | Benefício | Limitação | Decisão proposta |
|---|---|---|---|
| Tangentes importadas/geradas e persistidas na preparação | Base explícita alinhada ao bake; carregamento sem geração | Evolução do asset, remapeamento e integração com skinning | Solução principal |
| Tangentes geradas sob demanda | Atende materiais que precisam da base e geometria procedural | Custo de preparação quando os dados necessários estiverem ausentes ou mudarem | Caminho complementar |
| Base reconstruída por derivadas no fragment shader | Menos alterações de geometria; acompanha posições deformadas | Derivadas opcionais em GLES2, indisponíveis em `ps_2_0`; base pode divergir do bake | Não usar como única correção multiplataforma |
| Copiar o tratamento `2dw` para `3d` | Alteração pequena | Interpreta normal tangente como normal de iluminação | Rejeitada |

GLES2 requer a extensão `OES_standard_derivatives` para derivadas; HLSL `ddx`
não está disponível em `ps_2_0`. A seleção DX9 atual depende das capacidades do
dispositivo, em `core-manager-directx9.cpp`. Tangentes explícitas evitam essa
dependência, mas os limites de atributos, interpoladores e instruções ainda
precisam ser verificados nos perfis suportados.

Referências: [Khronos OES_standard_derivatives](https://registry.khronos.org/OpenGL/extensions/OES/OES_standard_derivatives.txt),
[Microsoft HLSL ddx](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-ddx).

A [interface oficial do MikkTSpace](https://github.com/mmikk/MikkTSpace/blob/master/mikktspace.h)
retorna tangentes por canto de face e alerta contra sobrescrever resultados usando
os índices originais. A implementação deve preservar descontinuidades e fixar
uma revisão da dependência, mantendo sua licença. Compatibilidade com um bake
também depende da triangulação e da convenção de interpolação/reconstrução.

## 4. Contratos a estabelecer antes da implementação dos shaders

- Convenção canônica de cálculo: normal map em espaço tangente, dados lineares,
  `+Y` no canal verde em relação à base UV adotada. Não deduzir a convenção pelo
  backend. Incluir nesta entrega propriedades persistentes por material/subset
  para convenção de origem (`+Y` / `-Y`) e intensidade do normal map.
- Normalizar a convenção de origem para a canônica exatamente uma vez, na
  decodificação pelo shader: multiplicar o componente tangencial Y pelo sinal da
  convenção de origem. Persistir a convenção de origem sem transformar os pixels;
  salvar/reimportar não aplica conversão. Este contrato será consumido na etapa 2.
  Uma textura compartilhada não pode ser alterada globalmente por uma opção de material.
- Intensidade: valor finito e não negativo; `0` equivale à normal sem detalhe,
  `1` à intensidade original. Definir escala dos componentes tangenciais seguida
  de normalização e tratamento seguro de vetores degenerados. Quando as propriedades
  opcionais estiverem ausentes, assumir `+Y` e intensidade `1` no caminho 3D;
  esses defaults não atribuem um normal map. Preservar o caminho `2dw`
  existente; aplicar essas propriedades a ele exigiria uma mudança separada.
- Mapa neutro deve reproduzir a normal geométrica dentro da tolerância de
  quantização. Não aplicar conversão sRGB ao normal map. Conferir o cache de
  texturas se a mesma imagem puder ser usada com papéis diferentes. Classificar
  normal maps na importação e definir decodificação, compressão e mipmaps coerentes
  com dados vetoriais; não depender de amostragem RGB bruta se houver canais empacotados.
- Espelhamento de UV, escala negativa e escala não uniforme precisam de regras
  explícitas para orientação e transformação da base. Auditar a transformação
  atual das normais por `mvMatrix`; para transformações gerais a normal requer
  inversa transposta. Não corrigir apenas a tangente deixando espaços incoerentes.
- Normais/UVs ausentes, UVs degeneradas e transformações singulares devem produzir
  fallback finito para a iluminação existente, nunca NaN ou desaparecimento.
- Distinguir textura normal atribuída de textura neutra usada como fallback de
  binding; disponibilidade da base também participa da decisão de aplicar o mapa.
- Shaders personalizados preservam seu contrato atual. A participação no novo
  normal mapping exige entradas compatíveis; não reescrever código arbitrário.

## 5. Etapas de implementação

### Etapa 1 — Preparação de assets, persistência e prova da base tangente

Criar um preparador compartilhado entre importação/exportação, editor e geração sob demanda
de runtime, com importação de tangentes válidas ou geração MikkTSpace. Usar fixtures
pequenas: plano, superfície curva, UV espelhada, costura e triângulo degenerado.
Definir uma representação preparada de renderização com
mapa de vértice renderizado para vértice fonte. Dividir vértices quando as bases
por canto diferirem, sem mudar os índices públicos da malha de edição.

Preservar subsets, winding e topologias suportadas. Para strips/fans, definir a
triangulação interna sem alterar a representação de autoria. Tratar crescimento
além do limite de índices de 16 bits por particionamento interno ou outra solução
compatível com os backends; nunca truncar índices. Duplicar influências esqueléticas
usando o mesmo remapeamento.

Validar extração por `MESH_MBM_DEBUG::loadDebugFromMemory`, física, seleção,
exportação e caches compartilhados: nenhum consumidor pode passar a interpretar
índices de renderização como índices de autoria. Essa validação é condição para
avançar com a arquitetura proposta.

Especificar e implementar leitura/escrita versionadas para tangentes, sinal,
remapeamento e propriedades de material. Definir a associação com frames/subsets
e influências esqueléticas, validação de contagens/valores, limites de tamanho,
identificação da revisão do preparador e detecção de dados derivados obsoletos.
Seções opcionais ausentes permitem geração somente quando houver necessidade da
base; payloads presentes e corrompidos devem ser rejeitados com diagnóstico.

Conectar os importadores/exportadores e salvamento dos editores, inclusive Image
Mesh Editor e Mesh Debug. Resolver defaults das propriedades opcionais e seu
round-trip. O salvamento sem normal map nem preparação antecipada não deve criar
textura normal, tangentes ou remapeamento exclusivamente para normal mapping.
Separar preparação CPU de upload GPU também no carregamento assíncrono.

Critério de saída: round-trip preserva tangentes, costuras e materiais; asset
com tangentes preparadas carrega sem gerá-las; sem mapa não exige tangentes;
com mapa e sem base preparada gera os dados necessários sob demanda.

### Etapa 2 — Caminho estático completo em OpenGL ES

Integrar leitura dos dados opcionais/geração sob demanda, cache, upload, atributo tangente,
transformação da base e leitura de `TextureNormal` no shader 3D, incluindo convenção
e intensidade por material. Usar a normal perturbada em todos os termos
difusos/especulares. Cobrir shaders padrão gerados e recursos de iluminação
embutidos, preservando suas diferenças intencionais.

Atualizar a seleção de variantes e chaves dos caches de shaders conforme os novos
atributos. Verificar limites com e sem atributos de skinning. Destruição, perda
de contexto e restauração devem recriar também os recursos derivados.

Critério de saída: fixture A/B automatizada e reprodução local do `module_001.msh`
com luz rasante, mostrando diferença atribuível ao mapa e neutralidade do mapa plano.

### Controles de material antes da paridade — 7.319

Mesh Debug expõe convenção +Y/-Y e intensidade não negativa no painel Normal map.
A seleção de frame/subset aceita 0 para todos; valores mistos são indicados e a
aplicação explícita substitui ambos os valores. Usa snapshot Undo, invalida o
preview e persiste pela seção opcional 15. Não atribui textura nem solicita nova
preparação de tangentes. Leituras agregadas ficam em cache até seleção/edição mudar.

### Progresso da etapa 3 — DirectX 11, versão 7.320

- Upload único de lotes estáticos para buffers privados, com publicação somente
  após sucesso de todas as alocações. Geometria fonte e seu layout permanecem
  separados; liberação e atualização dinâmica descartam os recursos derivados.
- Shaders gerados e `lit textured.ps` com VS gerado consomem tangente em espaço
  de visão, inversa transposta para normal, sinal do determinante, convenção e
  intensidade por subset. Ambos preservam iluminação direcional e pontual DX11.
  Perfis continuam `vs_4_0`/`ps_4_0`; não houve aumento de Shader Model.
- Tangentes usam input slot 2; constantes privadas de normal mapping usam PS b3.
  Desenho fonte recebe tangente zero com stride zero. Subsets sem mapa, intensidade
  zero, shaders legados e assets esqueléticos preservam seus caminhos anteriores.
- Corrigida extração DX11 de VB: subsets e intervalos fonte eram omitidos. Extração
  IB/VB também preserva o nome `default` quando não há textura difusa carregada.
- Executado no Windows/MSVC Debug x86, DirectX 11 Feature Level 11_0:
  preparação CPU, persistência, fundação esquelética, API runtime síncrona/assíncrona,
  extração de autoria e suíte DX11 (16/16, incluindo 58 shaders embutidos).
- Comparações visuais IB/VB: `NORMAL MAP VISUAL PASS`. Diferença média RGBA:
  neutro `0.0350`, detalhe `1.9351`, intensidade zero/remoção/convenções equivalentes
  `0`, luz pontual `2.4017`, transformação com escala não uniforme `8.0145`.
  Subsets mistos, escala negativa, HUD, 2dw e VS legado também passaram.
- `Downloads/module_001.msh` apenas lido: captura oblíqua confirmou relevo com o
  mapa original. Diferença média `5.4924` versus intensidade zero; mapa neutro
  `0.1483`. Câmera/luz diferem da captura Linux; não é uma medida de paridade entre GPUs.
  Smoke de 3 segundos com esse asset passou na camada de depuração e na validação
  de ciclo de vida dos recursos DirectX 11.
- Sem validação de perda real de dispositivo, hardware de feature level inferior,
  skinning com tangentes ou regeneração dinâmica. DX9 e Metal ainda sem implementação.

Reprodução no PowerShell, após build DX11 de `mini_mbm` e `libTest`:

```powershell
New-Item -ItemType Directory -Force build/normal-dx11 | Out-Null
$env:MBM_NORMAL_MAP_RENDER_DIR = "$PWD/build/normal-dx11"
& platform-msvs/Debug/mini_mbm.exe --scene src/test-lib/normal-map-render-test.lua --disable_select_monitor --nosplash
$env:MBM_NORMAL_MAP_TEST_VB = '1'
& platform-msvs/Debug/mini_mbm.exe --scene src/test-lib/normal-map-render-test.lua --disable_select_monitor --nosplash
Remove-Item Env:MBM_NORMAL_MAP_TEST_VB
$env:MBM_DIRECTX11_VALIDATE = '1'
$env:MBM_NORMAL_MAP_TEST_LIGHTING = '1'
& platform-msvs/Debug/libTest.exe 3 "$env:USERPROFILE/Downloads/module_001.msh" 3d
Remove-Item Env:MBM_DIRECTX11_VALIDATE
Remove-Item Env:MBM_NORMAL_MAP_TEST_LIGHTING
```

Exigir `NORMAL MAP VISUAL PASS` nas duas execuções; o smoke C++ deve informar
sucesso das validações de debug-layer e resource-lifecycle. A variável
`MBM_DIRECTX11_VALIDATE` habilita essas validações também nos smokes temporizados.
`MBM_NORMAL_MAP_TEST_LIGHTING` ativa iluminação 3D antes de carregar a malha,
exercitando o desenho com tangentes em vez do shader sem iluminação.

### Progresso da etapa 3 — DirectX 9, versão 7.321

- Implementado caminho estático com `vs_3_0`/`ps_3_0`, shaders gerados e
  `lit textured.ps`, iluminação direcional/pontual e material por subset. Reutiliza
  bases preparadas, sem gerar tangentes ou subir geometria continuamente.
- Correção da avaliação anterior: `core-manager-directx9.cpp` já seleciona perfis
  pelas capacidades da GPU. Os defaults textuais SM2 não eram o perfil efetivo
  desta máquina, que seleciona SM3. Não houve aumento global de perfil.
- Lotes privados em `D3DPOOL_MANAGED`, publicação após upload completo e liberação
  com o buffer. Atributo de tangente no stream 1; desenho fonte usa tangente zero.
  Inversa transposta, sinal de reflexão, fallback de base degenerada, convenção e
  intensidade seguem os contratos GLES/DX11. Skinning continua sem tangentes.
- Cache de shaders distingue a variante de normal mapping; reuso e recompilação
  da mesma instância testados. Buffers temporários de compilação são liberados
  também nas falhas. Perfis sem suporte recebem diagnóstico uma vez por contexto
  e mantêm a geração anterior de shader, sem promessa de iluminação SM2 funcional.
- Medição pelo teste `--directx9-normal-map-shader-test`: shader gerado SM3 com
  orçamento padrão de quatro luzes usa aproximadamente **232 slots PS / 38 VS**.
  Forçar SM2 no shader geométrico anterior já falha por excesso de registradores
  temporários; o shader simples sem iluminação compila. Esse teste força o perfil
  no compilador, não emula uma GPU SM2. Resultado: `DIRECTX9 NORMAL MAP SHADER PASS`.
- Corrigidos o scaffolding do VS gerado para `lit textured.ps` e o desenho VB
  `MODE_DRAW_TRIANGLES`, antes enviado incorretamente como triangle strip.
- Implementada extração estática DX9 para Mesh Debug (antes era um stub): buffers
  fonte managed permitem leitura, preservando subsets e índices, inclusive vértices
  não usados e o nome difuso `default`. Buffers derivados continuam write-only e
  não substituem autoria. Extração dinâmica write-only permanece sem suporte.
- Windows/MSVC Debug x86, GPU com SM3: preparação CPU, persistência, fundação
  esquelética, API runtime síncrona/assíncrona e extração passaram. Regressão GPU
  de skinning passou nos quatro casos sintético/Lorekeeper, LBS/DQS.
- Comparações IB/VB: `NORMAL MAP VISUAL PASS`, diferença média RGBA neutro `0.0711`,
  detalhe `1.8103`, zero/remoção/convenções equivalentes `0`, ponto `2.2720`,
  escala não uniforme/rotação `7.8486`. Subsets mistos, reflexão, HUD, 2dw,
  intensidade extrema e VS legado também passaram.
- `Downloads/module_001.msh` apenas lido e capturado: diferença de detalhe `5.5170`
  versus intensidade zero e mapa neutro `0.1514`. Inspeção visual confirmou relevo.
- Não foram validados GPU física SM2, perda real de dispositivo, runtime DX9 de
  depuração, normal mapping esquelético ou regeneração dinâmica. No ambiente de
  teste, criar dispositivo DX9 exigiu execução fora do sandbox.

Reprodução após build DX9 de `libTest` e `mini_mbm`:

```powershell
New-Item -ItemType Directory -Force build/normal-dx9 | Out-Null
& platform-msvs/Debug/libTest.exe --directx9-normal-map-shader-test
$env:MBM_NORMAL_MAP_RENDER_DIR = "$PWD/build/normal-dx9"
& platform-msvs/Debug/mini_mbm.exe --scene src/test-lib/normal-map-render-test.lua --disable_select_monitor --nosplash
$env:MBM_NORMAL_MAP_TEST_VB = '1'
& platform-msvs/Debug/mini_mbm.exe --scene src/test-lib/normal-map-render-test.lua --disable_select_monitor --nosplash
Remove-Item Env:MBM_NORMAL_MAP_TEST_VB
```

Exigir os marcadores `DIRECTX9 NORMAL MAP SHADER PASS` e `NORMAL MAP VISUAL PASS`;
o erro de compilação SM2 no teste de perfis é uma medição esperada, não um erro SM3.

### Progresso da etapa 3 — Metal/macOS, versão 7.323

- Upload único e transacional de lotes preparados para buffers privados de
  posição/normal/UV, tangentes e índices de 16 bits. Buffers fonte permanecem
  independentes; liberação e atualização dinâmica descartam os derivados.
- Shaders gerados e `lit textured.ps` com VS gerado usam base em espaço de visão,
  inversa transposta, sinal do determinante, convenção e intensidade por subset.
  Tangentes são lidas pelo vertex ID no slot 20; parâmetros usam slot 21 em VS/PS,
  sem colisão com iluminação 4–18 nem paleta esquelética 19. Não há preparação ou
  upload de geometria por frame. A chave existente de cache inclui FVF/luz/skinning.
- Corrigido scaffolding de iluminação do VS gerado para `lit textured.ps` e uso
  de `DirectionalColor` nesse recurso. O recurso preserva iluminação 3D apenas
  direcional; o shader gerado combina direcional e pontuais, como no GLES.
- Extração Metal corrigida para preservar subsets VB, intervalos fonte IB e
  vértices não usados. Buffers derivados nunca substituem geometria de autoria.
- macOS/Apple M4, Debug, `MTL_DEBUG_LAYER=1`: comparações IB e VB com
  `NORMAL MAP VISUAL PASS`. Diferença média RGBA: neutro `0.0349`, detalhe `1.9350`,
  intensidade zero/remoção/convenções equivalentes `0`, luz pontual `2.4019`,
  rotação/escala não uniforme `8.0144`. Subsets mistos, reflexão, HUD, 2dw,
  intensidade extrema e VS legado passaram, sem erros de validação Metal.
- Preparação CPU, persistência, fundação esquelética, runtime síncrono/assíncrono
  e extração de autoria passaram. Paridade GPU esquelética passou nos quatro casos
  sintético/Lorekeeper LBS/DQS; shaders de pintura com DQS compilaram.
- `Downloads/module_001.msh` e texturas apenas lidos. Nova cena reutilizável
  `normal-map-asset-test.lua` captura original, intensidade zero e mapa neutro.
  Diferença média de detalhe `3.5187`, neutro `0.1096`; inspeção das capturas confirma
  relevo. Câmera/luz diferem das execuções Windows/Linux: não é medida de paridade
  entre GPUs. Smoke C++ temporizado também executado com iluminação ativada.
- Não validados iOS, outras GPUs Metal, perda real de dispositivo/contexto nem
  custos comparativos de memória/carregamento/frame. Normal mapping esquelético
  e regeneração dinâmica permanecem fora desta entrega.

Reprodução no macOS após build Debug com `USE_TEXTURE_MISSING_DIALOG=0`:

```sh
mkdir -p /tmp/mini-mbm-normal-render
MTL_DEBUG_LAYER=1 bin/debug/arm64/mini-mbm \
  --scene src/test-lib/normal-map-render-test.lua \
  --disable_select_monitor --nosplash -w 600 -h 600
MTL_DEBUG_LAYER=1 MBM_NORMAL_MAP_TEST_VB=1 bin/debug/arm64/mini-mbm \
  --scene src/test-lib/normal-map-render-test.lua \
  --disable_select_monitor --nosplash -w 600 -h 600
MTL_DEBUG_LAYER=1 MBM_NORMAL_MAP_TEST_MESH="$HOME/Downloads/module_001.msh" \
  MBM_NORMAL_MAP_RENDER_DIR=/tmp/mini-mbm-normal-render bin/debug/arm64/mini-mbm \
  --scene src/test-lib/normal-map-asset-test.lua \
  --disable_select_monitor --nosplash -w 600 -h 600
MTL_DEBUG_LAYER=1 MBM_NORMAL_MAP_TEST_LIGHTING=1 \
  bin/debug/arm64/testLib 3 "$HOME/Downloads/module_001.msh" 3d
```

Exigir `NORMAL MAP VISUAL PASS` e `NORMAL MAP ASSET PASS`, além de ausência de
mensagens de erro da validação Metal. Em execução automatizada, impor também
um timeout externo (as cenas encerram ao concluir as comparações). O sandbox
usado nesta sessão não expõe a GPU; os testes gráficos exigiram execução fora dele.

### Etapa 3 — Paridade estática de DirectX 9, DirectX 11 e Metal

DirectX 11, DirectX 9 SM3 e Metal/macOS concluídos. Suporte à iluminação SM2 permanece fora
do incremento DX9 validado; iOS requer validação própria. Reutilizar a interface
privada `normal-map-upload.h` e os contratos de material. Esta etapa não inclui
skinning nem atualização arbitrária de geometria.

Portar layouts/declarations, bindings, constantes e shaders, mantendo a mesma
convenção matemática. Em Metal, respeitar os slots já ocupados pelas constantes
e pela paleta esquelética. Em DX9, testar os perfis realmente suportados, inclusive
o orçamento de instruções com múltiplas luzes.

Quando um dispositivo não suportar a variante, manter a renderização anterior e
emitir diagnóstico uma vez por contexto/variante, sem repetição por frame. Registrar
a limitação; fallback não conta como validação do efeito nesse dispositivo.

### Etapa 4 — Evolução futura: animação, edição e invalidação

Fora da primeira entrega. Dinâmico não significa apenas skinning:

| Situação | Contrato previsto |
|---|---|
| Objeto inteiro move, gira ou escala | Caminho estático; transformar a base sem gerar novas tangentes |
| Skinning CPU/GPU LBS/DQS | Deformar normais e tangentes com a pose de referência e preservar orientação |
| Vértices/normais alterados por código, física ou deformador | Atualizar a base afetada quando a geometria mudar |
| UVs/topologia alteradas | Invalidar remapeamento e base; preparar novamente antes de consumir |
| Morph targets/blend shapes, caso incorporados à engine | Atualizar normais/tangentes junto da deformação; não presumir suporte atual |
| Frames de geometria | Manter base por frame, com cache e invalidação por fonte |
| Normal map atribuído após carregar asset sem base | Preparar e enviar uma vez quando necessário |

Manter dados fonte separados dos buffers derivados e especialização nos backends.
As interfaces atuais não prometem consumo dinâmico: GLES descarta a base GPU em
`updateDynamic`; assets esqueléticos continuam no caminho anterior. Não adicionar
reconstrução por frame ao caminho estático como preparação para esse trabalho.

Transformar a base junto com as normais/posições nos caminhos GPU LBS/DQS e CPU.
Respeitar as restrições existentes para escalas/paletas; validar orientação após
deformação e a coerência entre os caminhos CPU e GPU.

Invalidar dados derivados quando posição, normal, UV ou topologia de autoria mudar.
Ao salvar/exportar após edição, atualizar também a representação preparada persistida.
Mudanças apenas de intensidade ou convenção não devem regenerar tangentes.
Em animação esquelética, reutilizar a base da pose de referência e deformá-la com
a pose; não executar MikkTSpace a cada frame. Em geometria dinâmica arbitrária,
recalcular somente quando os dados necessários mudarem. Em animações de frames,
preparar/persistir por frame de geometria que necessite da base; gerar/cachear no
runtime apenas quando a base for necessária e os dados preparados não existirem.

Cobrir atribuição/remoção tardia de mapa via `setMaterialTexture`, troca de frame,
reload do preview, carregamento assíncrono e múltiplas instâncias do mesmo asset.
A primeira atribuição pode preparar dados uma vez quando eles estiverem ausentes;
o loop de renderização não deve fazer reconstrução ou busca completa da malha continuamente.

### Etapa 5 — Editor, documentação e entrega

Atualizar o diagnóstico do Mesh Debug para distinguir mapa atribuído, mapa efetivo
em 3D e fallback. Validar o preview no Image Mesh Editor e oferecer uma comparação
temporária com/sem mapa, sem alterar o asset salvo apenas para visualizar.
Convenção/intensidade já estão expostas no Mesh Debug com persistência e Undo.
A ampliação desses controles ao Image Mesh Editor e os demais diagnósticos são
incrementos posteriores; não bloqueiam a paridade estática desta entrega.
Mostrar a política de importar/recalcular tangentes e diagnósticos de preparação,
para que incompatibilidades com o bake possam ser identificadas.

Auditar caminhos alcançados por `onLoop`: cálculos e leituras de diagnóstico devem
ser armazenados e atualizados por mudanças. Textos ImGui com pontuação ASCII.

Atualizar `docs/light.md`, `docs/core-pimpl-status.md` e a documentação esquelética
conforme os contratos implementados. Documentar em `docs/lua-api.md` as operações
de autoria/runtime que expuserem convenção e intensidade. Atualizar a documentação
do MSH com layouts das seções opcionais, versões dos payloads e defaults na ausência
de propriedades; distinguir dados persistidos de dados gerados sob demanda.
Incrementar `MBM_VERSION` na entrega funcional.

## 6. Validação e critérios de aceite

A tabela registra a matriz completa de evolução. Para a primeira entrega, excluir
os casos CPU/GPU LBS/DQS, deformação procedural em runtime e atribuição tardia sem
base previamente carregada. Preparação offline de malha procedural e atribuição
posterior com base carregada continuam cobertas no caminho estático. Edição aqui
é autoria seguida de salvar/recarregar; não promete atualização dinâmica da base.


| Caso | Resultado exigido |
|---|---|
| Sem mapa / mapa neutro / mapa com detalhe | Baseline preservado / equivalente dentro da quantização / diferença visível e mensurável |
| Sem mapa nem tangentes / mapa com tangentes / mapa sem tangentes | Sem geração ou exigência de tangentes / leitura sem MikkTSpace / geração sob demanda |
| Preparação antecipada / atribuição posterior / remoção do mapa | Base opcional sem textura atribuída / prepara se necessário / deixa de usar a base |
| Malha procedural | Preparação somente quando o material precisar da base |
| Tangentes importadas / calculadas | Política respeitada; comparação com bake de referência e triangulação fixa |
| Salvar, reabrir e reimportar | Tangentes, sinal, remapeamento, convenção e intensidade preservados |
| Convenções `+Y` e `-Y` equivalentes | Mesmo relevo após uma única conversão; sem modificar outros materiais que compartilham a textura |
| Intensidade `0`, `1` e maior que `1` | Sem detalhe / detalhe original / detalhe ampliado, sem NaN |
| Payload novo inválido / versão não suportada | Rejeição clara; sem fallback que esconda corrupção |
| Luz direcional e pontual, difuso e especular | Todos usam a normal perturbada |
| Rotação do objeto e movimento de câmera/luz | Relevo permanece preso à superfície |
| UV espelhada, costura, escala negativa/não uniforme | Orientação consistente; sem inversões indevidas |
| UV degenerada, normais/UVs ausentes | Fallback finito, sem crash ou NaN |
| Subsets com e sem mapa no mesmo draw sequence | Estado não vaza entre subsets/objetos |
| CPU/GPU LBS e DQS, pausa e troca de animação | Base acompanha a deformação; CPU/GPU comparáveis |
| Edição, reload, troca de textura, contexto recriado | Cache invalidado/restaurado corretamente |
| `2dw`, `2ds`, unlit e shader personalizado legado | Comportamento anterior preservado |
| Instâncias repetidas e editor parado | Sem geração/upload contínuo de tangentes; cache reutilizado |

Usar `testLib` para fixtures C++ e uma cena Lua temporária para integração do
preview. Seguir `engine-testing`: timeout, monitor picker desabilitado quando
aplicável e build de testes com `USE_TEXTURE_MISSING_DIALOG=0`.

Comparar capturas determinísticas com tolerância, sem exigir igualdade de pixels
entre GPUs. Medir custo de carregamento, memória e frame antes/depois; separar
custo fixo de preparação do custo de shading. Fixtures devem incluir mapa de
direção conhecida, além de textura artística, para detectar sinais trocados.

Executar GLES no ambiente Linux disponível; DX9/DX11 em Windows e Metal em
macOS/iOS com validação de API. Android GLES2 precisa de verificação própria.
Compilar um backend não substitui validar sua renderização. Os testes executados
estão registrados no progresso das etapas 1–3. Android ES2, iOS e comparações
controladas entre GPUs permanecem pendentes.

## 7. Riscos e limites da decisão

Os maiores riscos são a persistência dos dados opcionais, a integração entre
vértices preparados e consumidores da malha original, e os limites de índices.
Também é necessário manter a base coerente nos caminhos de deformação e em todos
os shaders de iluminação. Por isso a etapa 1 precede mudanças extensas de backend.

A direção acordada é preparar e persistir tangentes quando necessárias ou
explicitamente solicitadas, com geração sob demanda em runtime. O layout exato,
as APIs e os custos ainda precisam ser validados na prova.
Compatibilidade com outras ferramentas depende de convenções, triangulação, normais
e base tangente coerentes com o bake; não promete imagem final idêntica entre engines.
Uma dificuldade de implementação não deve converter silenciosamente o caminho
principal em geração a cada carregamento ou em derivadas sem suporte equivalente.
