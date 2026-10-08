# Mesh to Sprite

Editor disponível a partir de `7.355`, no menu dos launchers desktop, como **Mesh to Sprite / Mesh para Sprite**.

```sh
bin/debug/linux_x86/mini-mbm --scene editor/mesh_to_sprite_editor.lua \
  --disable_select_monitor --nosplash -w 1400 -h 1000
```

Requer build com Lua e ImGui (`-DUSE_ALL=1`). O [plano de implementação](mesh-to-sprite-editor-plan.md) descreve o desenho e as evoluções previstas.

## Uso

1. Em **Carregar / substituir mesh**, escolher um `.msh` ou `.mbm`.
2. Selecionar a animação estática, seu modo e intervalo entre frames. No modo pausado, escolher o frame base. Em **Combinar com**, selecionar Nenhuma, Articulada ou Esquelética e o clip correspondente. Para ChompBot, escolher **bite**, a segunda animação articulada.
3. Ajustar o intervalo de captura e a quantidade de quadros. **Ciclo** exclui a extremidade final; desmarcado inclui as duas extremidades quando há mais de um quadro. A duração de exibição de cada quadro no sprite é editável separadamente.
4. Ajustar posição, rotação em graus e escala da mesh. Os dois painéis à direita controlam separadamente a órbita da câmera e a luz. A câmera oferece posição, foco, distância, rolagem e planos de corte; a luz oferece direção, ambiente, cor direcional e posição/raio/cor de uma luz pontual.
5. Em **Opções de imagem**, ajustar largura, altura, pivô e fundo RGBA. Alpha zero produz fundo transparente. O xadrez pertence apenas à interface.
6. Usar a linha do tempo para conferir a pose e **Capturar quadros** para gerar a sequência. A captura pode ser cancelada. Mudanças que afetam a imagem invalidam a captura anterior.
7. Conferir os quadros pelo seletor e usar **Exportar sprite**. O `.spt` validado é acompanhado de PNGs com nomes exclusivos, no mesmo diretório. Manter esse conjunto junto ao mover o sprite para o jogo. A segunda prévia permite reproduzir o sprite efetivamente recarregado.

**Projeto** é um menu superior com Novo, Abrir, Salvar e Salvar como, incluindo Ctrl+N, Ctrl+O e Ctrl+S. Há confirmação para descartar alterações nesses comandos. O menu Idioma alterna português/inglês.

## Projeto e dependências

O arquivo `.mesh2sprite` contém uma receita Lua versionada, com seleção das animações, amostragem, transformação, câmera, iluminação, skinning, resolução, fundo, pivô e opções de saída. Não contém imagens capturadas nem a mesh original. Pode ser salvo antes da captura.

Ao salvar, referências dentro do diretório do projeto são relativas; outras permanecem absolutas. Ao reabrir, os caminhos relativos são resolvidos a partir do projeto. A mesh e suas texturas continuam necessárias. A assinatura da mesh permite avisar quando o conteúdo de origem mudou; texturas externas não têm assinatura própria. Se a origem não puder ser carregada, a receita permanece aberta e o botão de substituir mesh permite localizar outra fonte. Revise as seleções após substituir a origem.

Os PNGs intermediários são temporários e removidos ao recapturar, trocar projeto ou sair normalmente. Os PNGs finais são dependências permanentes do `.spt`. Reexportações usam novos nomes para preservar a exportação anterior até a publicação do novo arquivo; imagens de exportações antigas não são apagadas automaticamente.

## Animação e renderização

A animação estática escolhe o frame da mesh. O editor congela esse frame usando `setIndexFrame` e posiciona o player articulado/esquelético no tempo correspondente. Não há um novo mixer: a combinação segue o renderizador existente, inclusive as restrições dos assets e dos backends. Os modos estáticos são Pausada, Crescente, Crescente com loop, Decrescente, Decrescente com loop, Recursiva e Recursiva com loop.

O intervalo selecionado usa segundos desde o início da reprodução. Na articulada, a velocidade do clip é aplicada ao tempo; na esquelética, a velocidade é a padrão. O modo recursivo percorre a sequência nos dois sentidos sem repetir as extremidades. A captura de uma ação única preserva a pose final do clip, em vez de substituí-la pela primeira pose por causa do loop. Camadas adicionais e aplicação automática de root motion ficam desabilitadas. O deslocamento presente na pose é preservado.

O render target usa perspectiva e a mesma câmera para toda a sequência. Pivô e canvas não mudam por quadro. **Enquadrar limites da fonte** usa os limites fornecidos pelo asset, com folga; esses limites podem ser de física e não abranger todas as poses animadas. Confira o intervalo completo na prévia e ajuste foco/distância quando necessário.

As meshes são carregadas com shaders padrão capazes de iluminação, permitindo ligar/desligar luz sem perder a classificação do shader. Alterar o método de skinning recria a instância, pois essa escolha ocorre no carregamento. Materiais e texturas são os do asset original. Shaders personalizados continuam sujeitos às próprias regras de iluminação; efeitos de shader que avançam no tempo não têm amostragem determinística implementada neste editor.

A captura congela a pose antes do render e lê os pixels na próxima chamada da lógica, após a passagem de render correspondente. Para fundo com alpha menor que 1, renderiza sobre preto e branco e reconstrói cor/opacidade em PNG com alpha não pré-multiplicado; isso evita escurecer bordas semitransparentes ao usar o sprite. Essa reconstrução pressupõe a composição alpha padrão; materiais com blend aditivo ou outros modos especiais precisam de conferência própria. Não depende do FPS do editor. No editor parado, o alvo fica desabilitado: não há `seek`, leitura GPU ou reconstrução de assets contínua. Durante reprodução e captura, apenas as atualizações necessárias são executadas.

## Limites e validação

A versão inicial trabalha com uma mesh e uma direção por projeto, até 4096 quadros e resolução de 8 a 4096 pixels por eixo, limitados adicionalmente a 1 GiB estimado de RGBA não comprimido. A criação do alvo ainda depende da capacidade real da GPU. Exporta um quad de canvas uniforme por quadro, sem atlas ou recorte individual. Enquadramento automático pela união das poses, sombras, pós-processamento, edição de materiais e múltiplas direções permanecem como evoluções.

Validação realizada em Linux/OpenGL ES: ChompBot/bite, Lorekeeper, fixtures estáticas de três frames e alpha 128, mudança efetiva de iluminação, `.spt` recarregado/renderizado, persistência sem cache, cancelamento e editor ocioso. O alpha do primeiro quadro do sprite recarregado é comparado ao PNG de origem. As fontes animadas também exercitam o frame estático base combinado com o respectivo player.

O asset **Tango Way** não foi localizado no repositório; a fixture verifica o caminho de animação estática, mas não substitui a validação desse asset. Combinações com troca entre múltiplos frames de uma mesh articulada/esquelética ainda dependem de um asset compatível para validação visual específica. DirectX 9/11 e Metal não foram executados neste ambiente. Gizmos foram renderizados e inspecionados; interação manual de arrastar/clicar ainda requer conferência no editor.

Testes:

```sh
bin/debug/linux_x86/lua-5.4.1.exe src/test-lib/mesh_to_sprite_project_test.lua

timeout -s KILL 40 bin/debug/linux_x86/mini-mbm \
  --scene src/test-lib/mesh_to_sprite_smoke.lua \
  --disable_select_monitor --nosplash -w 1280 -h 1000
```

Para execução automatizada, configurar `-DUSE_TEXTURE_MISSING_DIALOG=0`. O teste gráfico imprime `M2S SMOKE OK` ou `M2S SMOKE FAIL`; conferir o marcador e os logs, além do exit code. Os artefatos finais do teste ficam no diretório temporário para inspeção.
