# Editor de Mesh Articulada 3D

Disponível desde **7.211.0**, em `editor/mesh_maker_articulated.lua`.
Este editor trabalha com animações rígidas por subset, pivôs e hierarquia do
formato `.msh`. Não cria ossos nem pesos de skinning. Para animação esquelética,
use o Skeletal Animation Editor; para sprites 2D, use o
[Sprite Maker (Articulated)](articulated-sprite-editor.md).

## Abrir e salvar

No menu de desenvolvimento, selecione **Mesh Maker (Articulated)** ou
**Editor de Mesh Articulada**. Linux/macOS têm o alias `mesh_maker_articulated`;
o script de atalhos Windows oferece o mesmo nome. Recompile o launcher e
recarregue os atalhos após atualizar o repositório.

```sh
bin/debug/linux_x86/mini-mbm --scene editor/mesh_maker_articulated.lua --disable_select_monitor --nosplash -w 1920 -h 1020
```

**Arquivo > Abrir mesh** carrega um `.msh` existente. A troca de arquivo pede
confirmação quando há alterações não salvas. **Salvar mesh** grava no arquivo
aberto; **Salvar mesh como...** permite outro destino. Ctrl+S salva no destino
atual. Prévias de pose ainda não gravadas como chaves não entram no arquivo.

Uma cópia base temporária mantém geometria, índices, UVs, normais, materiais,
animações de frames e outros dados nativos. As edições substituem as seções
articuladas dessa cópia. Não há conversão para sprites nem reconstrução dos
subsets a partir de triângulos Lua. Texturas encontradas ao lado da mesh pelo
mesmo nome de arquivo têm sua referência resolvida para esse caminho, inclusive
quando o arquivo veio de outra máquina. As imagens não são copiadas ao salvar.

## Partes e pivôs

Selecione o frame e uma parte na lista. O número inicial identifica seu subset.
Também é possível clicar com o botão direito na cena; a seleção usa os limites da parte transformada,
e a caixa ciano opcional identifica o subset selecionado.
**Mostrar subset selecionado** começa desligado; **Mostrar pivô** permanece ligado. Em peças sobrepostas, use a lista
para escolher sem ambiguidade.

**Inicializar partes ausentes** cria vínculos articulados para subsets sem uma
parte e posiciona seus pivôs no centro dos limites. Preserva vínculos e clipes
existentes. Não cria geometria nova.

Edite nome, posição XYZ do pivô, orientação XYZ e pai, depois pressione
**Aplicar parte**. Uma pequena esfera laranja mostra o pivô gravado. A orientação define
os eixos locais usados pela rotação articulada. Pais inválidos e ciclos são
rejeitados. **Remover parte articulada** remove sua identidade e respectivas
trilhas, preserva a geometria e liga os filhos ao pai anterior. As ações podem
ser desfeitas.

No painel de visualização, **Resetar visão** restaura orientação, posição e zoom iniciais para a mesh.
O gizmo de órbita também orienta a câmera. Na cena, o botão esquerdo arrastado
orbita, o botão central arrastado desloca a câmera e a roda aplica zoom. Controles
ImGui capturam o mouse e não movimentam a câmera. A rotação usa os eventos de
movimento e a mesma sensibilidade do Mesh Debug, mantendo fixos o foco e a
distância durante a órbita. No deslocamento central, o arrasto vertical altera
somente Y; o horizontal move X/Z conforme a orientação da vista. **Luz 3D** começa habilitada e
fica em uma janela própria, inicialmente abaixo de Visualização. Ela oferece
ativação, cor ambiente, cor direcional, gizmo de direção com valores XYZ e reset
para os padrões da engine. As alterações afetam somente a prévia, sem alterar
o arquivo da mesh. Todos os painéis podem ser fechados e
reabertos pelo menu Opções, que também oferece inglês e português.

## Clipes e chaves

1. Selecione ou adicione um clipe na Timeline.
2. Em **Propriedades do clipe**, edite nome, duração, velocidade, prioridade,
   repetição e modo aditivo. **Aplicar propriedades do clipe** confirma o conjunto;
   nomes vazios/duplicados e durações menores que as chaves existentes são rejeitados.
3. Selecione a parte e habilite os canais desejados: posição, rotação e escala.
   Cada canal tem valores XYZ. Pelo menos um canal deve permanecer habilitado.
4. Escolha o tempo e ajuste os valores. **Adicionar / atualizar chave** grava a
   pose da parte selecionada. **Auto Key** começa desligado e, quando ativado,
   grava as alterações automaticamente.
5. Escolha interpolação linear, entrada, saída, entrada/saída, suave ou Bezier.
   A rotação usa graus XYZ na convenção articulada da engine e preserva voltas
   completas, inclusive valores maiores que 360 graus.

Trocar de parte no mesmo instante preserva os rascunhos das outras partes.
Trocar tempo, clipe ou modo, ou iniciar reprodução, descarta rascunhos não gravados.
**Pose inicial** desativa a animação para inspecionar a montagem e os pivôs.
A prévia executa um clipe por vez; a engine continua oferecendo composição de
clipes por suas APIs de runtime.

## Timeline gráfica

Os controles e operações são compartilhados com a Timeline 2D:

- Clique nas marcações magentas; a seleção fica amarela e a linha da parte fica destacada.
- Ctrl+clique alterna uma chave na seleção. Arraste uma região vazia para seleção
  por retângulo; Ctrl acrescenta à seleção existente.
- Arraste uma chave selecionada para mover o grupo, preservando os intervalos.
- Copie/cole com os botões ou Ctrl+C/Ctrl+V, ou duplique a seleção no tempo atual.
  A colagem mantém as identidades das partes e todos os componentes XYZ das chaves.
- Exclua a seleção pelo botão ou Delete. Ctrl+Z/Ctrl+Y desfazem/refazem operações.
- **Operações de tempo e snap** oferece inserção de tempo vazio, inserção do
  trecho selecionado com deslocamento das chaves seguintes e remoção de intervalo
  com prévia. Essas operações afetam todas as trilhas do clipe, inclusive outros frames.
- Snap temporal começa desligado, com intervalo editável e presets de FPS.
- Ctrl+roda amplia a régua no cursor; arraste central navega horizontalmente;
  a roda sozinha percorre as trilhas. **Enquadrar clipe** mostra toda sua duração.

Colisões de horário na mesma trilha, destinos fora do clipe e colagens em partes
ou canais incompatíveis são rejeitados. Ao mudar de frame, a Timeline mostra as
partes daquele frame; as chaves de outras partes continuam no arquivo.

## Mesh Debug

A seção **Articulated Animation** do Mesh Debug agora permite somente reproduzir
clipes existentes. Selecionar um clipe no combobox inicia sua reprodução e desativa
o anterior. Play reinicia, Pause pausa, Stop desativa a animação articulada.
Os controles de autoria e a janela de pivô foram removidos desse editor.

## Prévia, desempenho e validação

`meshDebug:loadMeshPreview(mesh, caminho)` carrega uma malha privada, fora do cache
compartilhado. A ferramenta mantém o arquivo temporário enquanto a prévia vive e
libera seus buffers ao substituí-la. Os objetos `mesh` de jogos não recebem um
método especial de editor.

Carregamento, serialização e reconstrução da prévia ocorrem somente quando os
inputs mudam, com reconstruções limitadas a uma a cada 80 ms. A régua e as
marcações usam geometria em cache; uma Timeline parada não faz seek continuamente.
Limites de subsets são lidos ao abrir o arquivo. Na reprodução, os marcadores
avaliam somente as partes necessárias e seus pais.

O teste de iluminação abaixo exercita ativação, cores, direção, reset e ausência
de atualizações de iluminação em repouso, sem depender de assets externos.

Testes disponíveis:

```sh
bin/debug/linux_x86/lua-5.4.1.exe src/test-lib/articulated_mesh_pose_test.lua
bin/debug/linux_x86/lua-5.4.1.exe src/test-lib/articulated_mesh_view_test.lua
timeout -s KILL 30 bin/debug/linux_x86/mini-mbm --scene src/test-lib/articulated_mesh_light_smoke.lua --disable_select_monitor --nosplash -w 1280 -h 800
timeout -s KILL 40 bin/debug/linux_x86/mini-mbm --scene src/test-lib/articulated_mesh_editor_smoke.lua --disable_select_monitor --nosplash -w 1920 -h 1020
timeout -s KILL 30 bin/debug/linux_x86/mini-mbm --scene src/test-lib/articulated_mesh_playback_smoke.lua --disable_select_monitor --nosplash -w 640 -h 480
```

O teste de integração usa o exemplo local `/home/michel/Downloads/Bocao.msh`
(12 subsets/partes, clipes `gears` e `bite`) e sua textura `bocao.png`. Esses assets
não são distribuídos no repositório. O teste verifica edição XYZ, voltas completas,
inicialização de partes, hierarquia, copiar/colar, desfazer/refazer, exportação/recarga e preservação da
geometria. O teste puro cobre a convenção de quaternion, máscaras, hierarquia,
rascunhos por parte e estabilidade em repouso. O teste de playback verifica o
combobox/controles do Mesh Debug, o cache da lista e os controles desabilitados.

### Validação concluída

| Plataforma | Evidência |
|---|---|
| Linux/OpenGL ES | Build, testes automatizados de pose, câmera, iluminação, playback e integração com `Bocao.msh`, além de conferência visual. |
| Windows | Testes manuais do Mesh Maker (Articulated) concluídos pelo usuário, com funcionamento confirmado. |
| macOS | Testes manuais do Mesh Maker (Articulated) concluídos pelo usuário, com funcionamento confirmado. |

A confirmação em Windows e macOS se refere ao editor 3D com os ajustes finais de
câmera, seleção, pivô e janela de iluminação. A pendência anterior de validação
manual nessas plataformas está encerrada. Os backends gráficos usados nesses
testes manuais não foram especificados; a confirmação não representa uma matriz
de testes de todos os backends disponíveis.

Não há pendências conhecidas de implementação ou validação registradas para o
merge desta entrega. As limitações descritas neste guia, como seleção pelos
limites dos subsets e prévia de um clipe por vez, permanecem parte do escopo atual.
