# Mesh to Sprite

Editor disponível a partir de `7.355`, no menu dos launchers desktop, como **Mesh to Sprite / Mesh para Sprite**.

```sh
bin/debug/linux_x86/mini-mbm --scene editor/mesh_to_sprite_editor.lua \
  --disable_select_monitor --nosplash -w 1400 -h 1000
```

Requer build com Lua e ImGui (`-DUSE_ALL=1`). O [plano de implementação](mesh-to-sprite-editor-plan.md) descreve o desenho e as evoluções previstas.

## Uso

1. Em **Carregar / substituir mesh**, escolher um `.msh` ou `.mbm`.
2. Selecionar a animação estática, seu modo e intervalo entre frames. No modo pausado, escolher o frame base. Em **Combinar com**, selecionar Nenhuma, Articulada ou Esquelética e o clip correspondente. Para ChompBot, escolher **bite**, a segunda animação articulada. Ao selecionar a animação, o nome de saída recebe o nome do clip combinado, ou o da animação estática quando não há combinação; a sugestão é limitada a 31 bytes sem cortar caracteres UTF-8 e pode ser editada antes da captura. Reabrir o projeto preserva o nome personalizado.
3. Ajustar o intervalo de captura e a quantidade de quadros. **Ciclo** é inicializado a partir do modo de repetição da animação selecionada (por exemplo, desabilitado para ChompBot/bite), mas pode ser alterado manualmente e é preservado ao reabrir o projeto. Controla a repetição da prévia e do sprite exportado. Quando habilitado, exclui a extremidade final; desmarcado inclui as duas extremidades quando há mais de um quadro. A duração de exibição de cada quadro no sprite é editável separadamente.
4. Ajustar posição, rotação em graus e escala da mesh. Os dois painéis à direita controlam separadamente a órbita da câmera e a luz. A câmera oferece posição, foco, distância, rolagem e planos de corte; a luz oferece direção, ambiente, cor direcional e posição/raio/cor de uma luz pontual.
5. Em **Opções de imagem**, ajustar a largura e altura do PNG em pixels, o tamanho do frame em unidades da engine, o pivô e o fundo RGBA. **Frame acompanha a imagem** começa habilitado; desmarcar permite editar largura/altura do frame independentemente da resolução. **Manter proporção da imagem** também começa habilitado e ajusta a outra dimensão do frame. Desabilitá-lo permite esticamento, sinalizado por um aviso quando as proporções diferem. Alpha zero produz fundo transparente.
6. **Mostrar pivô** desenha um ponto amarelo nas prévias, sem gravá-lo nas imagens. O padrão `(0.5, 0.5)` é o centro; X cresce da esquerda para a direita e Y de cima para baixo, entre 0 e 1. O xadrez e o ponto pertencem apenas à interface.
7. Usar a linha do tempo para conferir a pose e **Capturar quadros** para gerar a sequência. A captura pode ser cancelada. Mudanças que afetam a imagem invalidam a captura anterior.
8. Conferir os quadros pelo seletor e usar **Exportar sprite**. O diálogo sugere o nome da mesh com extensão `.spt`; após exportar, reutiliza o destino escolhido até substituir a mesh. O `.spt` validado é acompanhado de PNGs com nomes exclusivos, no mesmo diretório. Manter esse conjunto junto ao mover o sprite para o jogo. A segunda prévia permite reproduzir o sprite efetivamente recarregado.

A prévia ocupa o espaço central disponível até o rodapé e se adapta ao tamanho da janela. A imagem ao vivo preserva a proporção do PNG; o quadro capturado é mostrado na proporção do frame, permitindo perceber eventual esticamento. A prévia do sprite exportado representa a última exportação.

Alterar tamanho do frame ou pivô não exige recapturar: os novos valores são usados na próxima exportação. Alterar a resolução da imagem exige nova captura.

**Projeto** é um menu superior com Novo, Abrir, Salvar e Salvar como, incluindo Ctrl+N, Ctrl+O e Ctrl+S. Há confirmação para descartar alterações nesses comandos. O menu Idioma alterna português/inglês.

## Projeto e dependências

Desde `7.356`, o esquema do projeto é versão 2. Projetos versão 1 são migrados ao abrir, mantendo frame com tamanho da imagem, proporção habilitada e marcador oculto. O arquivo `.mesh2sprite` contém uma receita Lua versionada, com seleção das animações, amostragem, transformação, câmera, iluminação, skinning, resolução, fundo, pivô e opções de saída. Não contém imagens capturadas nem a mesh original. Pode ser salvo antes da captura.

Ao salvar, referências dentro do diretório do projeto são relativas; outras permanecem absolutas. Ao reabrir, os caminhos relativos são resolvidos a partir do projeto. A mesh e suas texturas continuam necessárias. A assinatura da mesh permite avisar quando o conteúdo de origem mudou; texturas externas não têm assinatura própria. Se a origem não puder ser carregada, a receita permanece aberta e o botão de substituir mesh permite localizar outra fonte. Revise as seleções após substituir a origem.

Os PNGs intermediários são temporários e removidos ao recapturar, trocar projeto ou sair normalmente. Os PNGs finais são dependências permanentes do `.spt`. Reexportações usam novos nomes para preservar a exportação anterior até a publicação do novo arquivo; imagens de exportações antigas não são apagadas automaticamente.

## Animação e renderização

A animação estática escolhe o frame da mesh. O editor congela esse frame usando `setIndexFrame` e posiciona o player articulado/esquelético no tempo correspondente. Não há um novo mixer: a combinação segue o renderizador existente, inclusive as restrições dos assets e dos backends. Os modos estáticos são Pausada, Crescente, Crescente com loop, Decrescente, Decrescente com loop, Recursiva e Recursiva com loop.

O intervalo selecionado usa segundos desde o início da reprodução. Na articulada, a velocidade do clip é aplicada ao tempo; na esquelética, a velocidade é a padrão. O modo recursivo percorre a sequência nos dois sentidos sem repetir as extremidades. A captura de uma ação única preserva a pose final do clip, em vez de substituí-la pela primeira pose por causa do loop. Camadas adicionais e aplicação automática de root motion ficam desabilitadas. O deslocamento presente na pose é preservado.

No OpenGL ES, o render target prefere profundidade de 24 bits (GLES 3 ou extensão `GL_OES_depth24`) para preservar a oclusão de superfícies próximas, como a haste e a engrenagem do ChompBot. Dispositivos sem suporte ou com framebuffer incompatível mantêm o fallback de 16 bits. Neles, um plano próximo muito pequeno pode causar disputa de profundidade; aproximar os planos de corte da faixa ocupada pela mesh melhora a precisão, sem mudar culling.

O render target usa perspectiva e a mesma câmera para toda a sequência. Pivô e canvas não mudam por quadro. **Enquadrar limites da fonte** usa os limites fornecidos pelo asset, com folga; esses limites podem ser de física e não abranger todas as poses animadas. Confira o intervalo completo na prévia e ajuste foco/distância quando necessário.

As meshes são carregadas com shaders padrão capazes de iluminação, permitindo ligar/desligar luz sem perder a classificação do shader. Alterar o método de skinning recria a instância, pois essa escolha ocorre no carregamento. Materiais e texturas são os do asset original. Shaders personalizados continuam sujeitos às próprias regras de iluminação; efeitos de shader que avançam no tempo não têm amostragem determinística implementada neste editor.

A captura congela a pose antes do render e lê os pixels na próxima chamada da lógica, após a passagem de render correspondente. Para fundo com alpha menor que 1, renderiza sobre preto e branco e reconstrói cor/opacidade em PNG com alpha não pré-multiplicado; isso evita escurecer bordas semitransparentes ao usar o sprite. Essa reconstrução pressupõe a composição alpha padrão; materiais com blend aditivo ou outros modos especiais precisam de conferência própria. Não depende do FPS do editor. No editor parado, o alvo fica desabilitado: não há `seek`, leitura GPU ou reconstrução de assets contínua. Durante reprodução e captura, apenas as atualizações necessárias são executadas.

## Limites e validação

A versão inicial trabalha com uma mesh e uma direção por projeto, até 4096 quadros e resolução de 8 a 4096 pixels por eixo, limitados adicionalmente a 1 GiB estimado de RGBA não comprimido. A criação do alvo ainda depende da capacidade real da GPU. Exporta um quad de canvas uniforme por quadro, sem atlas ou recorte individual. Enquadramento automático pela união das poses, sombras, pós-processamento, edição de materiais e múltiplas direções permanecem como evoluções.

Validação realizada em Linux/OpenGL ES: ChompBot/bite, Lorekeeper, fixtures estáticas de três frames e alpha 128, mudança efetiva de iluminação, `.spt` recarregado/renderizado, persistência sem cache, cancelamento, navegação de quadros nos dois sentidos e editor ocioso. A importação no Sprite Maker também é verificada com um sprite CCW, incluindo salvar/reabrir o projeto. Também são verificados tamanho independente do frame, pivô deslocado, proporção e migração de projetos versão 1. O alpha do primeiro quadro do sprite recarregado é comparado ao PNG de origem. As fontes animadas também exercitam o frame estático base combinado com o respectivo player.

O asset **Tango Way** não foi localizado no repositório; a fixture verifica o caminho de animação estática, mas não substitui a validação desse asset. Combinações com troca entre múltiplos frames de uma mesh articulada/esquelética ainda dependem de um asset compatível para validação visual específica. DirectX 9/11 e Metal não foram executados neste ambiente. Gizmos foram renderizados e inspecionados; interação manual de arrastar/clicar ainda requer conferência no editor.

Testes:

```sh
bin/debug/linux_x86/lua-5.4.1.exe src/test-lib/mesh_to_sprite_project_test.lua

timeout -s KILL 40 bin/debug/linux_x86/mini-mbm \
  --scene src/test-lib/mesh_to_sprite_smoke.lua \
  --disable_select_monitor --nosplash -w 1280 -h 1000
timeout -s KILL 25 bin/debug/linux_x86/mini-mbm \
  --scene src/test-lib/sprite_maker_import_smoke.lua \
  --disable_select_monitor --nosplash -w 1280 -h 1000
```

O teste `src/test-lib/mesh_to_sprite_depth_smoke.lua` usa ChompBot em 512 x 512 e compara a mesma pose com planos próximos 0.1 e 10, verificando oclusão e parada da prévia sem ciclo. Requer render target com profundidade de 24 bits; o fallback de 16 bits não atende esse teste de precisão.

Para execução automatizada, configurar `-DUSE_TEXTURE_MISSING_DIALOG=0`. O teste gráfico imprime `M2S SMOKE OK` ou `M2S SMOKE FAIL`; conferir o marcador e os logs, além do exit code. Os artefatos finais do teste ficam no diretório temporário para inspeção.
