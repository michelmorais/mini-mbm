# Editor de Sprite Articulado 2D

Primeira implementação: 7.203.0. As decisões de produto estão no
[plano do editor](articulated-sprite-editor-plan.md).

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

1. Use **Adicionar imagem**. É possível carregar várias imagens e alternar entre elas.
2. Arraste na imagem original para selecionar uma região retangular. Os campos X,
   Y, largura e altura também permitem definir a seleção numericamente.
3. Escolha retângulo, círculo, cápsula, rosca ou contorno por alfa. Ajuste o orçamento
   de triângulos e, quando necessário, a tolerância de simplificação do contorno.
4. Para combinar uma forma com os pixels visíveis, marque **Combinar com alfa (PNG)**.
   **Preservar furos** e **Um subset por região** começam ativados. Desmarcar a
   segunda opção agrupa as regiões no mesmo subset. O limiar inicial de alfa é 16,
   ajustável, para evitar peças minúsculas formadas por pixels quase transparentes.
5. Clique em **Criar peças**. Cada peça começa centralizada na montagem.
6. Selecione a peça na lista ou no viewport e arraste para posicioná-la. A pose
   inicial também oferece posição, profundidade, rotação, escala e pivô numéricos.
   **Arrastar somente o pivô** altera o pivô sem mover a geometria. O marcador
   amarelo mostra o pivô selecionado.
7. Escolha o pai na lista e use os controles de ordem para organizar os subsets.
   Trocar o pai preserva a pose inicial, armazenada em coordenadas do asset.

O botão central do mouse move a câmera; a roda controla o zoom. Em **Opções**,
é possível ocultar a imagem original, permitir mover as janelas e trocar o idioma.

A cápsula tem extremidades vinculadas ou independentes e opção para manter a
circularidade. Sua altura e os raios definem o segmento central. A rosca tem
proporção interna e deslocamento do furo ajustáveis.

O orçamento de triângulos é aproximado. Para alfa, o ajuste automático preserva
componentes e furos e limita a variação da área de cada contorno a 5%. Caso o alvo
exija descaracterizar a peça, o total gerado pode ficar acima do solicitado.

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
e modo aditivo quando desejado. Alterne de **Pose inicial** para **Animação**.

Selecione a peça e o instante. Ajuste posição, rotação e escala da chave; a prévia
temporária permite visualizar a alteração antes de gravá-la. **Adicionar / atualizar
chave** grava os valores. **Auto Key** começa desligado; ligado, grava alterações
de transformação no instante selecionado.

A lista de chaves permite selecionar uma pose existente, excluí-la ou movê-la
para outro instante. A interpolação oferece os modos existentes da engine:
linear, entrada, saída, entrada/saída, smoothstep e Bezier com controles.

**Onion skin** começa desligado. Quando ativado, mostra silhuetas transparentes
a 1/12 de segundo antes e depois da posição atual. Durante a reprodução, as
silhuetas são atualizadas a 12 Hz; a animação principal é executada pela engine.

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
timeout -s KILL 15 bin/debug/linux_x86/mini-mbm --scene src/test-lib/articulated_sprite_startup_smoke.lua --disable_select_monitor --nosplash -w 1280 -h 800
timeout -s KILL 20 bin/debug/linux_x86/mini-mbm --scene src/test-lib/articulated_sprite_editor_smoke.lua --disable_select_monitor --nosplash -w 1280 -h 800
```

O teste de startup usa o launcher real e verifica a inicialização antes de executar
os painéis. O teste de integração exercita separadamente projeto e animações.

A leitura de alfa desta versão é específica para PNG. Outros formatos aceitos
como textura podem ser usados com formas sem extração de alfa. A importação de
geometria suporta triângulos, triangle strips e triangle fans; primitives de linhas
ou pontos não fazem parte desse fluxo de recorte.

A interface foi executada e inspecionada no Linux/OpenGL ES. Cliques e arrastes
reais ainda precisam de uma passagem manual, e a execução em Windows/DirectX e
macOS/Metal não foi validada neste ambiente.

As prévias possuem malhas privadas, sem acumular versões no cache global de
malhas. Recorte, triangulação e exportação ocorrem quando solicitados ou quando
seus dados mudam. Reconstruções durante arrastes são limitadas a uma a cada 80 ms;
o editor parado não reconstrói assets nem executa seek continuamente.
