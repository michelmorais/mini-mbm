# Editor de Sprite Articulado 2D

Primeira implementação: 7.203.0. Este guia reúne o comportamento entregue,
as limitações, a validação e as pendências do editor.

O editor usa subsets rígidos, pivôs e hierarquia do sistema articulado da engine.
Não utiliza ossos, pesos ou skinning. O `.spt` exportado mantém clipes e chaves
articuladas, reproduzidas por `sprite:playArticulatedAnimation()`.

## Abrir

No launcher de desenvolvimento, selecione **Articulated Sprite / Sprite Articulado**.
A entrada foi adicionada para Linux, macOS e Windows. Diretamente, a partir da
raiz do repositório:

```sh
bin/debug/linux_x86/mini-mbm --scene editor/articulated_sprite_editor.lua --disable_select_monitor --nosplash -w 1280 -h 800
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

Use **Adicionar clipe**, defina nome, duração, velocidade, prioridade, repetição
e modo aditivo quando desejado. Para alterar o nome, edite **Nome do clipe** e
clique em **Renomear** ao lado. O texto pode ficar vazio enquanto você digita;
a alteração só é aplicada pelo botão, com um nome não vazio e sem duplicar outro
clipe. Alterne de **Pose inicial** para **Animação**.

Selecione a peça e o instante. Ajuste posição, rotação e escala da chave; a prévia
temporária permite visualizar a alteração antes de gravá-la. **Adicionar / atualizar
chave** grava os valores. **Auto Key** começa desligado; ligado, grava alterações
de transformação no instante selecionado.

A lista de chaves permite selecionar uma pose existente, excluí-la ou movê-la
para outro instante. O combobox **Interpolação** oferece os modos existentes da
engine: linear, entrada, saída, entrada/saída, suave (smoothstep) e Bezier com
controles. Os campos de propriedades e chaves têm largura compacta; a barra de
tempo acompanha a largura disponível da janela.

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
  pasta `<projeto>.asprite.assets`, com nomes próprios e referências relativas.
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
timeout -s KILL 15 bin/debug/linux_x86/mini-mbm --scene src/test-lib/articulated_sprite_startup_smoke.lua --disable_select_monitor --nosplash -w 1280 -h 800
timeout -s KILL 20 bin/debug/linux_x86/mini-mbm --scene src/test-lib/articulated_sprite_editor_smoke.lua --disable_select_monitor --nosplash -w 1280 -h 800
```

O teste de startup usa o launcher real e verifica a inicialização antes de executar
os painéis. O teste de integração exercita separadamente projeto e animações.

A leitura de alfa desta versão é específica para PNG. Outros formatos aceitos
como textura podem ser usados com formas sem extração de alfa. A importação de
geometria suporta triângulos, triangle strips e triangle fans; primitives de linhas
ou pontos não fazem parte desse fluxo de recorte.

A interface foi executada e inspecionada no Linux/OpenGL ES. Houve testes manuais
de interação durante o desenvolvimento, e a importação do `FW_Hero_1.scml` foi
confirmada pelo usuário. Isso não equivale à validação completa de todos os fluxos
e plataformas; os itens ainda abertos estão registrados abaixo.

As prévias possuem malhas privadas, sem acumular versões no cache global de
malhas. Recorte, triangulação e exportação ocorrem quando solicitados ou quando
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
- [ ] Executar e verificar ordenação, transparência, prévia e interação em
  **Windows/DirectX e macOS/Metal**. A validação na engine realizada até aqui foi
  em **Linux/OpenGL ES**.
