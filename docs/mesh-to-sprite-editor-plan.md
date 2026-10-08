# Plano: editor de animação 3D para sprite 2D

Status: proposta de implementação, sem editor implementado nesta etapa.

## Objetivo e escopo

Criar um editor Lua/ImGui que amostre uma animação de mesh 3D, renderize suas poses em imagens e exporte um sprite `.spt` composto exclusivamente de quadros 2D texturizados. O resultado não dependerá de ossos, articulações ou da mesh original durante o jogo.

Casos obrigatórios:

- **Tango Way:** animação estática (sequência de frames da mesh); caminho do asset a confirmar, pois não foi localizado um arquivo com esse nome na árvore consultada. “Estática” é a categoria de animação por frames do engine, não apenas uma pose imóvel.
- `src/test-lib/Lorekeeper-walk.msh`: animação esquelética.
- `src/test-lib/ChompBot.msh`: animação articulada; selecionar a segunda animação, `bite`, conforme o requisito. A implementação deve conferir nome e índice ao carregar o arquivo, sem assumir que todo asset tem essa ordem.

O projeto salvo conterá a receita de captura e referências aos assets, sem precisar conter as imagens geradas. Ao reabrir, será possível reconstruir a prévia e recapturar os mesmos quadros. A reprodução exige que a mesh, suas texturas e demais dependências continuem disponíveis.

Nome proposto: **Mesh to Sprite**, com entrada `editor/mesh_to_sprite_editor.lua`. A primeira versão trabalhará com uma mesh, uma configuração de animação e uma direção de câmera por projeto. A configuração permite animação estática isolada ou combinada com uma animação articulada ou esquelética, conforme o runtime existente. Composição adicional além do que o engine já oferece, edição de ossos, captura automática de várias direções, sombras e pós-processamento ficam para etapas futuras. A conversão deve funcionar integralmente dentro do engine.

## Base existente verificada

Esta seção registra inspeção de código, não validação em execução. Os pontos em aberto abaixo precisam de protótipos antes da interface completa.

| Necessidade | Base no repositório | Consequência para o plano |
| --- | --- | --- |
| Enumerar e posicionar animações | [mesh-lua.cpp](../src/lua-wrap/render-table/mesh-lua.cpp) registra enumeração, reprodução, pausa e `seek` para esquelética e articulada; [animation-lua.cpp](../src/lua-wrap/render-table/animation-lua.cpp) e [common-methods-lua.cpp](../src/lua-wrap/common-methods-lua.cpp) fornecem os controles da animação estática | Adaptar as três categorias sem torná-las mutuamente exclusivas; preservar suas APIs e semânticas |
| Combinar com animação estática | [MESH::render](../src/render/mesh.cpp) obtém o frame da animação estática e passa esse índice ao caminho esquelético, articulado ou simples | Oferecer estática isolada, estática + articulada e estática + esquelética; respeitar a precedência esquelética sobre articulada, sem inventar mistura entre elas |
| Duração esquelética | `getSkeletalAnimationDuration` no mesmo binding | Usar metadados do clip, com intervalo de captura editável |
| Duração articulada | O binding inspecionado possui consulta de tempo, mas não equivalente direto de duração; [articulated_mesh_view.lua](../editor/articulated_mesh_view.lua) trabalha com `clip.duration` no modelo do editor | Verificar acesso aos metadados do binário; adicionar consulta mínima se necessária, sem estimar duração esperando a reprodução terminar |
| Captura isolada | [render-2-texture-lua.cpp](../src/lua-wrap/render-table/render-2-texture-lua.cpp) expõe criação, câmera, inclusão/remoção de objetos, cor de limpeza e gravação PNG | Usar render target dedicado, sem capturar a janela ou a interface |
| Câmera do alvo | Binding expõe posição, foco, up, ângulo, escala e planos near/far | Validar quais controles afetam a projeção 3D; não presumir que escala representa zoom ortográfico |
| Sprite binário | [sprite_maker.lua](../editor/sprite_maker.lua) usa `meshDebug`, frames/subsets, vértices, índices, texturas, `addAnim`, `setType('sprite')` e `save` | Reutilizar o contrato de exportação e validar o arquivo com o carregador real |
| Iluminação | [framework-lua.cpp](../src/lua-wrap/framework-lua.cpp) registra controles de luz e `getLightState` | Aplicar configuração explícita e restaurar estado compartilhado ao finalizar/cancelar |
| Convenções visuais e persistência | [editor_utils.lua](../editor/editor_utils.lua), Sprite Maker e editor articulado | Reutilizar painel esquerdo, mensagens, localização e serialização de dados |

## Experiência do editor

Usar a barra de menus, tema e comportamento de janelas existentes. **Projeto será um menu na barra superior**, com Novo, Abrir, Salvar e Salvar como e os atalhos correspondentes; não será uma seção recolhível no painel esquerdo.

O painel esquerdo, posicionado com `tUtil.setInitialWindowPositionLeft`, conterá as opções de trabalho:

1. **Fonte:** caminho da mesh, carregar/substituir asset e resolver referência ausente.
2. **Animação:** seleção da animação estática e opção de combiná-la com uma articulada ou esquelética disponível; nomes, frames, duração e modos existentes no engine. Incluir intervalo de captura, quantidade de quadros e duração/FPS de saída.
3. **Mesh:** posição, rotação e escala, restaurar transformação e visibilidade de partes quando suportada. Definir unidades na interface; converter graus para a unidade do engine na fronteira com a API.
4. **Aparência:** material/texturas e cor de fundo/transparência. Os controles de iluminação ficam no painel próprio de luz.
5. **Captura:** largura e altura por quadro, margem, pivô, filtro e qualidade suportados, estimativa de memória e botão Capturar/Cancelar.
6. **Saída:** nome da animação, destino `.spt`, exportação e estado de validade da captura.

Criar **dois painéis independentes de órbita**, seguindo `showCameraWindow` e `showLightWindow` de [mesh_debug.lua](../editor/mesh_debug.lua), reutilizando `tUtil.drawOrbitGizmo`:

- **Câmera:** gizmo orbital, posição XYZ, foco XYZ, distância, orientação/up, planos de corte, enquadramento e restaurar. Sincronizar edição numérica e órbita, como no Mesh Debug. Presets frontal/lateral/traseiro/isométrico podem ser atalhos para configurações explícitas.
- **Luz:** gizmo para direção da luz, habilitar iluminação, ambiente, cor e controles dos tipos disponíveis. Reutilizar `orbitFromDir`/`dirFromOrbit` e a interação do Mesh Debug. Para luz pontual, expor posição XYZ, raio e cor; para direcional, direção e cor. Foco é controle da câmera; não inventar posição/foco físico para luz direcional. Incluir restaurar e os demais controles já suportados.

Os painéis de câmera e luz devem poder ser usados simultaneamente e manter estados independentes. Seguir o posicionamento lateral direito do Mesh Debug, respeitando o tamanho físico da janela em HiDPI e reservando espaço para a prévia. A órbita da luz não altera a câmera, e a órbita da câmera não altera a luz.

À direita, mostrar prévia 3D e limite exato de captura. Abaixo, linha do tempo com play/pause, scrub, passo por quadro e miniaturas produzidas. Oferecer prévia 2D da sequência e, após exportar, do `.spt` efetivamente recarregado.

Grade, eixos, manipuladores e xadrez de transparência pertencem somente à interface. Nenhum deles pode entrar no PNG. Mouse e atalhos devem respeitar o foco do ImGui. Textos localizados em inglês e português; pontuação dos textos renderizados compatível com ASCII.

## Animação estática e combinação existente

A animação estática seleciona uma sequência de frames da mesh por meio do gerenciador comum de animação. Expor seleção e modos já existentes, usando os controles como `setAnim`, `getAnim`, `getIndexFrame` e `setTypeAnim`, conforme seus contratos reais. Não confundir o modo de reprodução da origem com o modo de reprodução do sprite exportado.

Em `MESH::render`, o engine atualiza a animação estática e os players articulado/esquelético; o `frameIndex` selecionado pela estática entra no caminho de renderização escolhido. Portanto, a interface deve permitir manter essa seleção ao habilitar a animação articulada ou esquelética. Não criar um novo formato de animação aninhada, mixer ou regras de composição.

As configurações obrigatórias são: estática isolada, estática + articulada e estática + esquelética. Para uso somente articulado/esquelético, manter um frame base adequado. O renderizador prioriza esquelética ativa, depois articulada e, por fim, a renderização simples; a interface não deve prometer aplicação simultânea de esquelética e articulada nem combinações incompatíveis com os dados do asset.

Na captura combinada, cada amostra precisa reproduzir o estado da animação estática e o tempo da animação complementar no mesmo instante da reprodução original. Respeitar intervalo de frames, tempo por frame, sentido, repetição e demais opções suportadas; não normalizar separadamente os clips nem sincronizar seus finais por uma regra nova. Verificar no protótipo o controle determinístico disponível para a estática e o comportamento nas trocas de frame, inclusive materiais/FX quando presentes. Se faltar acesso Lua, expor somente a operação necessária sobre o comportamento existente.

## Amostragem e estabilidade

Usar tempo explícito por amostra, nunca capturar “a cada tantos frames” do loop do editor. Para intervalo `[a,b]` e `N` quadros:

- Ciclo: `t(i) = a + i * (b-a)/N`, com `i` de `0` a `N-1`; não duplicar a pose final do ciclo.
- Ação única, `N > 1`: `t(i) = a + i * (b-a)/(N-1)`; incluir as duas extremidades.
- `N = 1`: capturar `a`. Rejeitar quantidade não inteira/positiva, tempos não finitos e intervalos fora do clip.

O modo padrão deve seguir o clip quando seus metadados permitirem. Para `bite`, validar explicitamente o comportamento da pose final: um clip que faz wrap no endpoint não pode converter o último quadro na pose inicial silenciosamente. Resolver clamp/amostragem no adaptador ou no runtime, conforme o protótipo demonstrar.

Separar o tempo amostrado do tempo de exibição no `.spt`. Proposta inicial: duração de saída igual ao intervalo, com duração uniforme por quadro de `(b-a)/N`, editável pelo usuário. Para intervalo sem duração ou captura de pose única, exigir duração de saída positiva. Mostrar claramente FPS e duração resultantes.

Antes de amostrar, preservar todas as animações selecionadas na configuração, inclusive a estática combinada, e desativar apenas animações/camadas não selecionadas, transições não solicitadas e avanço automático que contaminem a pose. Registrar método de skinning e política de root motion; a primeira versão preserva o deslocamento presente na pose e desabilita aplicação automática acumulativa ao objeto. Compensação para animação “no lugar” fica como extensão explícita, não centralização automática por quadro.

Manter câmera, tamanho do canvas e pivô fixos para toda a sequência. Não recentralizar nem redimensionar cada pose: isso provoca tremulação e altera o movimento. Um botão de enquadramento automático pode analisar todas as amostras solicitadas uma única vez e calcular a união dos limites, com margem; também deve haver ajuste manual.

Na primeira versão, exportar quads com canvas uniforme, incluindo a área transparente. Recorte automático individual será uma evolução e deverá preservar offsets em relação ao mesmo pivô. Esse desenho já produz quadros de imagem 2D sem transportar a geometria 3D.

## Pipeline de captura e exportação

1. Validar asset, dependências, clip, intervalo, resolução, limites do backend e orçamento de memória.
2. Congelar uma cópia da configuração e criar um alvo RGBA dedicado com a câmera de captura.
3. Carregar/configurar a mesh de captura, materiais e iluminação; incluir apenas os objetos desejados no alvo.
4. Para cada tempo, aplicar o frame/estado estático e a pose articulada ou esquelética selecionada, garantir sua avaliação, renderizar o alvo, aguardar a conclusão necessária e só então ler/gravar o PNG.
5. Registrar imagem, índice, tempo, canvas e pivô; avançar a fila com progresso e possibilidade de cancelamento.
6. Gerar o `.spt` pelo contrato existente do Sprite Maker: um quad por quadro, UVs e índices corretos, animação nomeada e duração por quadro.
7. Reabrir o `.spt` com `sprite` e reproduzi-lo na prévia 2D para validar o produto final.

A fila será uma máquina de estados, por exemplo `preparar -> aplicar_pose -> renderizar -> ler_pixels -> próximo -> exportar`. O protótipo deve determinar o ponto correto de atualização/renderização/leitura em cada backend. Esperar um número arbitrário de loops não constitui garantia de que a imagem corresponde à pose pedida.

Saída mínima: `.spt` acompanhado dos PNGs referenciados, com caminhos relativos e nomes sem colisões. Não assumir que `.spt` embute pixels. Atlas é otimização posterior; não bloquear a primeira versão por empacotamento. Os PNGs finais são parte do produto exportado, mesmo que não façam parte do projeto salvo.

Gravar inicialmente em área temporária e publicar o conjunto apenas quando estiver completo. Cancelamento/falha limpa temporários e preserva a última exportação válida. Limitar miniaturas em memória e processar quadros incrementalmente; estimar pelo menos `largura * altura * 4 * N` bytes para imagens RGBA não comprimidas, além das cópias e recursos do renderizador.

Validar alpha, blending, orientação vertical, culling, profundidade e cores em OpenGL ES, DirectX 9/11 e Metal. Examinar halos em bordas semitransparentes sobre fundos claro e escuro. No DX9, preservar a desassociação dos samplers antes de vincular a textura como alvo. Incluir/remover objetos pela lista do alvo específico, não pelo indicador global `isRender2Texture`.

## Projeto salvo: receita versionada

Proposta: arquivo de dados Lua com extensão `.mesh2sprite`, utilizando a serialização compartilhada onde adequada. Não serializar userdata, objetos de engine ou cache de imagens. Carregar somente a estrutura esperada em ambiente restrito e validar esquema, tipos, valores finitos e limites antes de alterar o editor.

| Grupo | Dados persistidos |
| --- | --- |
| Identificação | Versão do esquema, versão do engine utilizada e nome do projeto |
| Fonte | Caminho relativo da mesh quando possível, referências/overrides de texturas e assinatura do asset para detectar alteração |
| Animações | Seleção estática (nome/índice, intervalo de frames, tempo por frame e modo existente), seleção complementar opcional articulada ou esquelética (nome/índice, duração e opções suportadas), estado inicial para reprodução combinada, intervalo de captura, `N` e modo de amostragem |
| Mesh | Posição, rotação, escala, partes visíveis e política de root motion/skinning |
| Câmera | Posição, alvo/up ou representação canônica equivalente, projeção suportada, parâmetros, near/far e enquadramento |
| Aparência | Iluminação completa usada, materiais/overrides, transparência, cor de limpeza e opções efetivamente suportadas de filtro/qualidade |
| Captura | Resolução, margem, pivô, política de canvas e parâmetros de qualidade |
| Exportação | Nome do clip de saída, duração por quadro, modo de reprodução, destino e política de nomes |

Persistir também a configuração independente dos dois painéis de órbita, reconstruindo o gizmo da luz a partir de sua direção. Parâmetros de câmera derivados não devem competir entre si: escolher uma representação canônica e reconstruir controles orbitais ao abrir. Resolver referências em relação ao projeto. Se a origem mudou, avisar e invalidar a captura; se o clip desapareceu, solicitar seleção, sem substituí-lo pelo primeiro silenciosamente.

Salvar/reabrir deve funcionar antes de qualquer captura. Ao abrir, restaurar a receita e gerar apenas a prévia necessária; capturar a sequência completa mediante comando. Distinguir projeto modificado de captura desatualizada: trocar destino de exportação não exige renderizar novamente, mudar câmera ou iluminação exige.

Reprodutibilidade significa repetir poses e parâmetros com as mesmas dependências. Não prometer PNGs idênticos entre GPUs/backends ou versões diferentes do engine.

## Arquitetura proposta e desempenho

Separar o novo editor em módulos coesos:

- `mesh_to_sprite_editor.lua`: lifecycle, interface e comandos.
- `mesh_to_sprite_project.lua`: esquema, validação, caminhos, migração e persistência.
- `mesh_to_sprite_animation.lua`: adaptação estática/esquelética/articulada, combinação existente e metadados.
- `mesh_to_sprite_capture.lua`: recursos, fila de captura, progresso e cancelamento.
- `mesh_to_sprite_export.lua`: imagens finais e construção do `.spt`.

Concentrar estado numa tabela e evitar o limite de 200 locais do chunk Lua. Usar `dpCall` para chamadas protegidas e tratar também funções que retornam `nil, erro` sem lançar exceção.

Auditar tudo que é alcançável por `onLoop(delta)`. No editor parado, não repetir `seek`, carregar assets, reconstruir geometria, varrer todas as poses, ler pixels ou serializar. Mudanças marcam prévia/captura como sujas; playback atualiza somente o necessário; geração ocorre apenas por comando. Reutilizar alvo, mesh e miniaturas enquanto suas entradas forem válidas e liberar recursos ao trocar projeto/finalizar.

Alterações necessárias no C++ devem preservar PIMPL e headers públicos. Se houver novas APIs, atualizar documentação correspondente; se houver alteração da fronteira de estado, atualizar `docs/core-pimpl-status.md`. Incrementar `MBM_VERSION` ao entregar a funcionalidade, não nesta etapa de planejamento.

## Etapas de implementação

### 1. Prova técnica das três categorias e das combinações

Localizar o asset Tango Way e carregar os arquivos de referência; listar animações/frames e durações, confirmar `bite`, pausar e amostrar estados conhecidos. Verificar estática isolada e combinada com articulada ou esquelética usando assets compatíveis; se necessário, preparar uma fixture com os mecanismos de autoria existentes, sem adicionar semântica de composição. Resolver consulta de duração articulada, avaliação após `seek`, endpoint e sincronização com o render target. Capturar PNGs transparentes e produzir um `.spt` mínimo reproduzível.

Critério de conclusão: frames/poses corretos nas três categorias e nas duas combinações, sem depender do FPS; sprite exportado carrega sem a mesh original.

### 2. Editor e persistência

Construir menu Projeto, painel esquerdo de opções, seleção de animações combináveis, dois painéis independentes de órbita para câmera e luz, transformação e prévia. Implementar esquema versionado e novo/abrir/salvar/salvar como, inclusive recuperação de caminhos ausentes.

Critério de conclusão: fechar e reabrir projeto sem cache de imagens restaura todas as escolhas que influenciam a captura.

### 3. Captura completa e exportação

Implementar amostragem, pivô/canvas fixos, fila incremental, cancelamento, limites, PNGs e `.spt`, seguida da prévia do arquivo recarregado.

Critério de conclusão: quantidade, ordem, duração e aparência dos quadros correspondem à receita; exportação incompleta não substitui saída válida.

### 4. Integração e validação

Adicionar entrada nos launchers desktop aplicáveis, localização em `editor/lang/language.lua`, documentação de uso e versão. Seguir a skill `engine-testing` antes de executar scripts ou o engine; iniciar com Linux debug e registrar quais outros backends foram efetivamente testados.

## Critérios de aceitação e testes

- **Estática:** capturar Tango Way preservando sequência, tempo por frame, sentido e repetição existentes.
- **Combinação:** capturar estática + articulada e estática + esquelética em assets compatíveis, incluindo trocas de frame estático; comparar com a reprodução do engine no mesmo instante, sem desativar a combinação selecionada.
- **Interface:** Projeto aparece como menu, sem seção recolhível equivalente; os dois painéis de órbita seguem o Mesh Debug, com edição numérica e gizmos sincronizados e independentes.
- **Esquelética:** capturar Lorekeeper em início/meio/fim e sequência completa; comparar silhuetas/poses com a prévia original.
- **Articulada:** confirmar a segunda animação `bite` do ChompBot, verificar abertura/fechamento e extremidade final; nenhuma animação não selecionada pode interferir, preservando a estática quando fizer parte da configuração.
- **Determinismo temporal:** repetir com velocidades diferentes do loop e verificar a mesma sequência de poses; testar `N=1`, ciclo e ação única.
- **Persistência:** salvar antes de capturar, reabrir sem imagens temporárias, restaurar câmera/mesh/luzes e recapturar. Testar dependência ausente, clip removido e esquema inválido.
- **Enquadramento:** preservar pivô, escala e posição relativa; nenhuma parte deve ser cortada com margem suficiente. Redimensionar a janela não deve alterar resolução nem composição exportadas.
- **Aparência:** alpha sem fundo da UI, luz ligada/desligada, materiais e texturas; testar transparência sobre fundos contrastantes.
- **Produto final:** abrir `.spt` e PNGs em local separado sem acesso ao `.msh`, reproduzir contagem/duração/modo corretos e abrir também no Sprite Maker.
- **Robustez:** cancelar, trocar projeto, capturar novamente, exceder limites e simular falha de gravação sem vazamento ou destruição da saída anterior.
- **Editor ocioso:** verificar por contadores ou perfil que não há captura, leitura GPU, `seek` ou reconstrução contínua quando parado.
- **Backends:** registrar orientação de imagem, alpha e correspondência da pose em Linux/OpenGL ES, Windows/DX9/DX11 e macOS/Metal conforme ambientes disponíveis; testes pendentes devem constar da entrega.

Testes puros devem cobrir a matemática de amostragem, validação do projeto e offsets/pivô. Testes de integração devem cobrir o caminho completo até o `.spt` recarregado; compilar Lua ou inspecionar PNGs isolados não substitui essa verificação.

## Decisões técnicas a fechar no protótipo

1. Localização do asset Tango Way, acesso determinístico ao estado estático e validação das combinações já suportadas.
2. Origem confiável da duração e do modo de repetição das animações articuladas em `.msh`.
3. Projeção disponível na câmera 3D do alvo. Não anunciar opção ortográfica/FOV editável antes de confirmar suporte ou implementar a extensão necessária.
4. Momento seguro para avaliação da pose, renderização e leitura em cada backend.
5. Tratamento exato do endpoint dos clips e dos materiais iluminados/translúcidos.

Esses pontos são investigações de implementação. O requisito funcional está suficientemente definido para iniciar pela prova técnica, mantendo as escolhas propostas neste documento explícitas e revisáveis.
