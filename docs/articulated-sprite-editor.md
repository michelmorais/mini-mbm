# Editor de Sprite Articulado 2D

Primeira implementação: 7.203.0. Este guia reúne o comportamento entregue,
as limitações, a validação e as pendências do editor.

O editor usa subsets rígidos, pivôs e hierarquia do sistema articulado da engine.
Não utiliza ossos, pesos ou skinning. O `.spt` exportado mantém clipes e chaves
articuladas, reproduzidas por `sprite:playArticulatedAnimation()`.

Para meshes 3D `.msh`, use o [Mesh Maker (Articulated)](articulated-mesh-editor.md).

## Abrir

No launcher de desenvolvimento, selecione **Sprite Maker (Articulated)**
(**Editor de Sprite Articulado** em português). A entrada está disponível no Linux,
macOS e Windows. O script principal é `editor/sprite_maker_articulated.lua`; os
aliases CMake e o atalho Windows usam o nome `sprite_maker_articulated`.
Depois de atualizar o repositório, recompile o launcher para que o menu use o
novo caminho e recarregue os aliases gerados. Diretamente, a partir da raiz do
repositório:

```sh
bin/debug/linux_x86/mini-mbm --scene editor/sprite_maker_articulated.lua --disable_select_monitor --nosplash -w 1280 -h 800
```

## Recortar e montar

1. Use **Adicionar imagem** para selecionar um ou vários arquivos na mesma janela.
   As imagens entram juntas no projeto; a última fica selecionada para recorte.
   É possível alternar entre elas na lista de origens e desfazer a inclusão do lote
   em uma única ação.
2. Arraste na imagem original para selecionar uma região retangular. Os campos X,
   Y, largura e altura também permitem definir a seleção numericamente. Use
   **Selecionar imagem inteira** para ajustar o retângulo amarelo a toda a imagem
   atual; esse botão apenas ajusta a seleção, sem gerar ou substituir peças.
3. No combobox **Forma do recorte**, escolha retângulo, círculo, cápsula, rosca ou contorno por alfa. Sem alfa, ajuste
   o orçamento de triângulos. Com alfa, use a tolerância de simplificação do contorno.
4. Para combinar uma forma com os pixels visíveis, marque **Combinar com alfa (PNG)**.
   **Preservar furos** e **Um subset por região** começam ativados. Desmarcar a
   segunda opção agrupa as regiões no mesmo subset. O limiar inicial de alfa é 16,
   ajustável, para evitar peças minúsculas formadas por pixels quase transparentes.
5. Clique em **Criar peças**. Cada peça começa centralizada na montagem.
6. Selecione a peça na lista ou no viewport e arraste para posicioná-la. A pose
   inicial também oferece posição, profundidade, rotação, escala e pivô numéricos.
   **Arrastar somente o pivô** altera o pivô sem mover a geometria. O marcador
   magenta mostra o pivô selecionado.
7. Escolha o pai na lista e use os controles de ordem para organizar os subsets.
   Trocar o pai preserva a pose inicial, armazenada em coordenadas do asset.

O botão central do mouse move a câmera; a roda controla o zoom. Em **Opções**,
é possível ocultar a imagem original, permitir mover as janelas e trocar o idioma.

A cápsula tem extremidades vinculadas ou independentes e opção para manter a
circularidade. Sua altura e os raios definem o segmento central. A rosca tem
proporção interna e deslocamento do furo ajustáveis.

O orçamento de triângulos é aproximado e aparece apenas sem recorte por alfa.
Com alfa, a quantidade final depende do contorno, dos furos e da tolerância de
simplificação; não há ajuste automático ou refinamento por um orçamento oculto.
Com alfa, todas as formas ocultam o orçamento. Círculo, cápsula e rosca usam
detalhamento fixo para a forma de interseção, independente do orçamento sem alfa.

## Corrigir peças

Ative **Editar pontos do contorno**, escolha o contorno e arraste seus pontos na
imagem original. Os botões permitem inserir um ponto após o selecionado ou removê-lo.
Contornos inválidos não substituem a última geometria válida. Furos existentes
também são editáveis. Para geometria importada de `.spt`, há campos de vértice e UV.

**Gerar novamente a peça** aplica os parâmetros atuais, substituindo os ajustes
manuais do recorte. Pivô, montagem, identidade e animações permanecem associados à
peça. Desfazer recupera a geometria anterior.

As ações de duplicação são **Duplicar peça** e **Duplicar com animação**.
**Excluir peça** mantém seus filhos ligados ao pai anterior; **Excluir hierarquia**
remove também os descendentes. Essas operações participam do histórico de
desfazer/refazer, acessível pelo menu ou por Ctrl+Z / Ctrl+Y.

## Animar

Use **Adicionar clipe** e abra **Propriedades do clipe** para definir nome,
duração, velocidade, prioridade, repetição e modo aditivo quando desejado. Para alterar o nome, edite **Nome do clipe** e
clique em **Renomear** ao lado. O texto pode ficar vazio enquanto você digita;
a alteração só é aplicada pelo botão, com um nome não vazio e sem duplicar outro
clipe. Alterne de **Pose inicial** para **Animação**.

A Timeline identifica a **Peça selecionada**. Ao trocar a peça ou o instante,
os campos carregam a transformação da trilha correspondente, interpolada no tempo
atual; peças sem trilha começam com posição/rotação zero e escala um.

Ajuste posição, rotação, escala e interpolação da chave. Com **Auto Key** desligado,
a prévia temporária pertence à peça e ao instante em edição. Trocar de peça no
mesmo instante preserva as prévias das outras peças, sem transferir seus valores.
**Adicionar / atualizar chave** grava somente a peça selecionada, em uma ação
reversível. A Timeline avisa quando há prévias não gravadas. Mudar o tempo, clipe
ou modo, ou iniciar a reprodução, descarta essas prévias e recupera as chaves
gravadas. Carregar uma chave existente descarta a prévia daquela peça; as de
outras peças no mesmo instante permanecem.

**Auto Key** começa desligado; ligado, grava alterações da peça selecionada no
instante atual. Editar uma transformação durante a reprodução pausa a animação.

Desde 7.210.0, a Timeline oferece régua e trilhas por peça, seguindo os controles
do editor de animação esquelética 3D. As propriedades da peça/chave ficam à direita
em janelas largas e abaixo das trilhas em janelas estreitas. A lista de clipes é
um combobox; **Propriedades do clipe** recolhe os ajustes menos frequentes.

- Clique nas marcações magentas para selecionar a peça e carregar a chave. Chaves
  selecionadas ficam amarelas; a linha da peça ativa fica destacada.
- Ctrl+clique alterna a seleção de uma chave. Arraste uma região vazia para
  selecionar várias chaves; Ctrl acrescenta à seleção. Arrastar uma marcação
  selecionada move o grupo, preservando os intervalos. A alteração só é gravada
  ao soltar o mouse.
- Clique/arraste na régua para posicionar o instante atual (linha vermelha).
  Ctrl+roda aplica zoom no cursor; arraste com o botão do meio para navegar
  horizontalmente. A roda sozinha percorre as trilhas. **Enquadrar clipe** restaura
  a visualização de toda a duração.
- **Copiar chaves** / **Colar aqui** preservam peças, canais, transformações,
  interpolação e intervalos relativos; a primeira chave copiada é alinhada ao
  instante atual. **Duplicar aqui** faz o mesmo com a seleção atual.
  Ctrl+C / Ctrl+V funcionam com a Timeline focada, fora de campos em edição.
  **Excluir chaves** (ou Delete) apaga a seleção.
- Em **Operações de tempo e snap**, **Inserir trecho copiado** copia o trecho
  selecionado, desloca as chaves posteriores de todas as trilhas e aumenta a
  duração do clipe. Inclui uma pequena margem no limite para não sobrepor a
  última chave copiada à primeira deslocada; exige seleção em instantes distintos.
- **Inserir tempo vazio** desloca as chaves a partir do playhead sem criar novas
  poses. A prévia de remoção mostra a região e a quantidade de chaves afetadas;
  **Remover intervalo** apaga as chaves nessa região, desloca as posteriores e
  reduz a duração. O início pertence ao intervalo removido e o fim é preservado.
- **Snap temporal** começa desligado e oferece intervalo editável e presets
  24/25/30/50/60 FPS para posicionar o tempo e arrastar chaves.

Colar/duplicar exige peças existentes no frame atual e canais compatíveis. Colar
ou mover para fora da duração, ou sobre uma chave existente na mesma trilha, é
rejeitado sem substituir dados. As operações de alteração participam de
Ctrl+Z / Ctrl+Y. A prancheta permanece disponível ao trocar de clipe, mas é
limpa ao carregar outro projeto. Ao alterar keys com essas operações, prévias
não gravadas são descartadas.

A lista numérica de chaves também permite selecionar uma pose existente,
excluí-la ou movê-la para outro instante. O combobox **Interpolação** oferece os
modos existentes da engine: linear, entrada, saída, entrada/saída, suave
(smoothstep) e Bezier com controles. Os campos de propriedades e chaves têm
largura compacta. A geometria da régua e das marcações é reconstruída somente
quando dados, seleção ou visualização mudam; mover apenas o playhead não
reconstrói essa geometria.

**Onion skin** começa desligado. Quando ativado, mostra silhuetas transparentes
a 1/12 de segundo antes e depois da posição atual. Durante a reprodução, as
silhuetas são atualizadas a 12 Hz; a animação principal é executada pela engine.

## Importar animações do Spriter (SCML)

Desde 7.206.0, **Arquivo > Importar SCML (Spriter)** abre o `.scml` e resolve as
imagens referenciadas nas suas subpastas. A importação substitui o projeto atual;
salve alterações antes de importar. Não é necessário usar uma imagem composta
ou atlas como entrada. O Sprite Maker tradicional mantém sua importação estática.

Cada animação recebe um frame base com subsets rígidos e um clipe articulado.
Selecionar o clipe na Timeline seleciona também seu frame. O editor abre em modo
**Animação**; use **Reproduzir** ou a barra de tempo e edite as chaves das peças.
A imagem de origem começa oculta nessa importação e pode ser mostrada em Opções.

O importador resolve os pivôs, transformações herdadas, escalas espelhadas,
sentido de rotação, duração e repetição. Curvas da mainline e das timelines
(linear, instantânea, quadrática, cúbica, quártica, quíntica e Bezier) são avaliadas
na importação. As transformações resultantes são amostradas a 60 Hz em chaves
TRS com interpolação linear no runtime, incluindo os limites das chaves originais.
É uma aproximação temporal das curvas do Spriter; não há avaliação SCML a cada
frame da engine. A hierarquia de ossos é convertida em trilhas independentes das
peças, sem importar ossos ou skinning. A base conserva os recortes sem rotação
nas posições dos pivôs; a montagem rotacionada é aplicada pelas chaves do clipe.

Trocas de imagem e de ordem criam variantes de subsets. A variante inativa usa
escala zero; chaves próximas da transição evitam que ela apareça gradualmente
entre amostras. Essa transição tem uma pequena janela de interpolação (normalmente
0,05 ms), respeitando a tolerância de tempo da engine. No modo Pose inicial,
as variantes ficam visíveis para edição. O SPT continua usando índices de 16 bits.

Opacidade parcial animada não é representável pelas trilhas TRS e impede a
importação com uma mensagem; opacidade zero é tratada como peça oculta. Tipos de
objeto diferentes de sprite/bone não são suportados. Character maps, áudio,
eventos e metadados do Spriter não são convertidos. O `.asprite` conserva os
recortes e as referências de origem para continuar a autoria; não há exportação
SCML.

No exemplo `FW_Hero_1.scml`, o resultado é **8 clipes, 8 frames base e 27 imagens**:
Walking, Running, Jumping, Crouching, Attack, Attack_No_Weapon, Hit e Die.
Die não repete. O importador tradicional gera os mesmos 62 frames estáticos.

Para reproduzir o SPT exportado no jogo, selecione o frame base e o clipe:

```lua
hero:setAnim('Walking')
hero:playArticulatedAnimation('Walking')
```

As animações de frame com o nome dos clipes selecionam apenas seu frame base.
Essa associação é recuperada ao reimportar o SPT no editor.

Referência de interpolação: [Spriter SCML](https://www.brashmonkey.com/ScmlDocs/ScmlReference.html).

## Projeto e binário

- **Salvar projeto** grava um `.asprite` com dados de autoria, formas, contornos,
  imagens, montagem e animações. Por padrão, as imagens são copiadas para uma
  pasta `<projeto>.asprite.assets`, preservando seus nomes e usando referências
  relativas. Nomes repetidos recebem sufixos (`Hammer_2.png`, por exemplo), sem
  sobrescrever outra imagem. Cópias existentes de conteúdo idêntico são
  reaproveitadas ao salvar novamente. Projetos antigos que já referenciam
  `image_1.png` continuam compatíveis, mas não contêm o nome original perdido.
- **Importar SPT** recupera os frames, geometria, texturas e dados articulados
  disponíveis. O editor trabalha em um frame por vez. Parâmetros de formas que
  não existem no binário não são reconstruídos como se fossem os originais.
- **Exportar SPT** usa o formato v11 existente. Os frames precisam conter peças.
  A exportação mantém as referências às imagens utilizadas; distribua também
  essas imagens com o jogo. Salvar o projeto com cópia de imagens facilita
  organizar e transportar os recursos, mas não embute imagens no `.spt`.

## Validação e limites atuais

Foram executados testes de geometria/modelo, incluindo cem máscaras de alfa
sintéticas; testes no engine para exportação/reimportação, dois frames, hierarquia,
pivôs, imagens de nomes iguais, projeto e reprodução independente; reconstruções
repetidas das prévias; e inspeção visual de recortes em `boy-parts.png`.

Os testes automatizados ficam em:

```sh
bin/debug/linux_x86/lua-5.4.1.exe src/test-lib/articulated_sprite_editor_test.lua
bin/debug/linux_x86/lua-5.4.1.exe src/test-lib/articulated_sprite_scml_test.lua
bin/debug/linux_x86/lua-5.4.1.exe src/test-lib/articulated_sprite_pose_test.lua
timeout -s KILL 15 bin/debug/linux_x86/mini-mbm --scene src/test-lib/articulated_sprite_startup_smoke.lua --disable_select_monitor --nosplash -w 1280 -h 800
timeout -s KILL 20 bin/debug/linux_x86/mini-mbm --scene src/test-lib/articulated_sprite_editor_smoke.lua --disable_select_monitor --nosplash -w 1280 -h 800
```

O teste de startup usa o launcher real e verifica a inicialização antes de executar
os painéis. O teste de integração exercita separadamente projeto e animações.

A leitura de alfa desta versão é específica para PNG. Outros formatos aceitos
como textura podem ser usados com formas sem extração de alfa. A importação de
geometria suporta triângulos, triangle strips e triangle fans; primitives de linhas
ou pontos não fazem parte desse fluxo de recorte.

A interface foi executada e inspecionada no Linux/OpenGL ES. O usuário também
confirmou o funcionamento em testes manuais no Windows e macOS, após ajustes
necessários no Windows. A validação nessas duas plataformas está concluída para
os cenários testados; isso não representa cobertura de todas as combinações de
backends e fluxos. A importação do `FW_Hero_1.scml` também foi confirmada pelo
usuário. Os itens ainda abertos estão registrados abaixo.

As prévias usam `meshDebug:loadSpritePreview(sprite, caminho)` e possuem malhas
privadas, sem acumular versões no cache global de malhas. O carregamento pertence
à API de ferramentas Mesh Debug; sprites de jogos continuam usando `load`. Recorte, triangulação e exportação ocorrem quando solicitados ou quando
seus dados mudam. Reconstruções durante arrastes são limitadas a uma a cada 80 ms;
o editor parado não reconstrói assets nem executa seek continuamente.

O teste SCML puro usa um arquivo sintético e aceita um caminho opcional para o
`FW_Hero_1.scml` como primeiro argumento. `articulated_sprite_scml_smoke.lua` usa o
exemplo local em `/home/michel/Downloads/SpriterFile`, exporta/reimporta o SPT e
percorre os oito clipes na engine, verificando que a prévia não é reconstruída
continuamente durante a reprodução. As imagens do exemplo não são distribuídas
com o repositório.

## Pendências

### Implementação

- [ ] Ajustar o orçamento inicial do círculo para **5 triângulos**, conforme
  combinado. Atualmente, selecionar círculo inicia o orçamento em **12**; o
  controle já permite reduzi-lo até 5. O orçamento continua aproximado e é
  mostrado apenas quando o recorte por alfa está desligado.
- [ ] Oferecer edição direta dos vértices da geometria também para peças criadas
  no editor, incluindo os vértices internos da triangulação. Atualmente, essas
  peças oferecem edição dos contornos; os campos de vértices e UV estão
  disponíveis apenas para geometria importada de SPT.

### Validação

- [ ] Completar uma passagem manual do fluxo de autoria: recorte, montagem,
  hierarquia, duplicação/exclusão/reordenação, Auto Key, onion skin,
  desfazer/refazer e exportação. Os testes manuais já realizados não representam
  cobertura integral dessa sequência.
- [ ] Mover uma cópia do projeto com sua pasta de imagens para outro diretório e
  verificar a reabertura pelos caminhos relativos. O teste automatizado atual
  verifica salvamento/reabertura e imagens de nomes iguais, mas não essa mudança
  de diretório.
- [x] Validar o funcionamento em **Windows e macOS**. Testes manuais concluídos
  e confirmados pelo usuário, com ajustes necessários no Windows. A validação
  automatizada local permanece em **Linux/OpenGL ES**.

O teste `articulated_sprite_pose_smoke.lua` exercita a troca entre Hammer e Spin,
prévias separadas, gravação por peça, Auto Key, desfazer e ausência de reconstruções
contínuas com a Timeline parada.

O teste puro `articulated_sprite_timeline_test.lua` cobre cópia, movimentação em
grupo, inserção/remoção de tempo e rejeição atômica de colisões. Na engine,
`articulated_sprite_timeline_smoke.lua` simula os retornos de entrada do ImGui
para selecionar/arrastar marcações e usar Ctrl+clique, verifica desfazer,
clipboard entre peças, exportação/reimportação SPT e estabilidade do cache
em repouso. Essa simulação não substitui uma passagem manual com o mouse.
