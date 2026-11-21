# Plano: sistema de attachments e constraints

## Status

Proposta de roadmap.

A extensao `follow_actor` foi retirada do escopo do editor e do runtime de
interacao de animacao. O comportamento de seguir outro ator deve pertencer a
um sistema separado de attachments e constraints.

## Decisao

O plugin `interaction_animation_runtime` continuara responsavel por:

- timeline de uma interacao;
- eventos de transformacao, animacao, visibilidade e notificacao;
- sincronizacao temporal entre atores da interacao;
- callbacks de inicio, evento, atualizacao e finalizacao.

O novo sistema sera responsavel por:

- manter um objeto anexado a outro objeto ou a um ponto de uma mesh;
- seguir transformacoes de origem em tempo real;
- aplicar apenas os componentes autorizados da transformacao;
- ativar, desativar e reconfigurar attachments;
- resolver constraints em uma ordem deterministica.

Essa separacao evita transformar o runtime de interacao em um sistema geral de
hierarquia e permite reutilizar attachments fora de uma timeline.

## Problema

Um jogo pode precisar manter uma relacao espacial entre objetos que possuem
ciclos de vida e controladores diferentes. Exemplos:

- um chapeu seguindo um personagem que e movido pelo usuario;
- uma espada posicionada na mao de um personagem;
- um escudo seguindo um socket do braco durante um clipe;
- um efeito visual seguindo um osso ou um ponto de referencia;
- um objeto mantendo apenas a rotacao ou apenas a escala da origem;
- um objeto anexado durante parte de uma acao e liberado depois.

Esses casos nao devem exigir que a mesh de origem seja controlada pelo runtime
de interacao.

## Objetivos

1. Permitir attachments entre objetos do runtime, sem depender de uma
   interacao de animacao.
2. Permitir que a origem continue sendo controlada pelo usuario, por fisica,
   por outra animacao ou por outro sistema.
3. Permitir selecao independente de posicao, rotacao e escala.
4. Permitir offsets salvos no espaco local da origem.
5. Suportar origem no transform raiz de um objeto e, posteriormente, em ossos
   ou sockets de uma mesh.
6. Permitir ativacao e desativacao deterministicas durante o jogo.
7. Manter o comportamento atual do core e dos plugins existentes quando o novo
   sistema nao estiver sendo usado.

## Fora de escopo inicial

Na primeira versao, o sistema nao deve tentar resolver:

- constraints fisicas ou integracao direta com Box2D/Bullet;
- IK, retargeting ou alteracao da animacao skeletal;
- sockets criados ou editados dentro do formato MSH;
- attachments entre objetos de cenas diferentes;
- uma hierarquia geral de scene graph para todos os renderizaveis;
- simulacao de mola, atraso, amortecimento ou compensacao de colisao.

Esses recursos podem ser adicionados depois, desde que nao mudem o contrato
basico de attachment.

## Conceitos

### Origem

Objeto, mesh, osso ou socket que fornece a transformacao de referencia.

### Alvo

Objeto que recebe a transformacao calculada pelo attachment.

### Offset

Transformacao relativa salva no espaco local da origem no momento da
autoria. O runtime nao deve recalcular esse offset automaticamente a cada
inicio, exceto quando isso for solicitado explicitamente.

### Componentes herdados

Cada attachment escolhe independentemente:

- posicao;
- rotacao;
- escala.

Quando a escala for herdada, ela tambem afeta a distancia do offset de
posicao. A composicao esperada e a de uma hierarquia transformacional comum.

## Contrato funcional inicial

Um attachment deve possuir, no minimo:

- identificador estavel;
- origem;
- alvo;
- tipo de origem: transform raiz, osso ou socket;
- offset local;
- mascara de componentes herdados;
- estado ativo ou inativo;
- politica de desanexacao.

Uma API Lua futura pode seguir uma forma semelhante a esta, sem representar
um contrato final:

```lua
local attachment = attachments.attach {
    source = character,
    target = hat,
    inherit = {
        position = true,
        rotation = true,
        scale = false,
    },
    offset = {
        position = {x = 0, y = 1.8, z = 0},
        rotation = {x = 0, y = 0, z = 0},
        scale = {x = 1, y = 1, z = 1},
    },
}

attachment:detach({preserveWorldPose = true})
```

O nome do modulo e os nomes finais dos campos devem ser definidos durante a
fase de contrato da API.

## Propriedade das transformacoes

O sistema deve ser por componente. Um objeto pode, por exemplo:

- ter a posicao controlada pelo usuario;
- receber a rotacao de um attachment;
- manter a escala definida pelo jogo.

O sistema nao deve sobrescrever componentes que nao pertencem ao attachment.
Quando dois sistemas tentarem escrever o mesmo componente do alvo no mesmo
ciclo, a situacao deve ser explicita. A primeira versao deve rejeitar ou
reportar a duplicidade, em vez de depender apenas da ordem dos plugins.

A origem deve ser somente lida. O sistema de attachments nao deve reposicionar
ou rotacionar a origem para resolver a relacao.

## Ciclo de atualizacao

O attachment precisa ser resolvido depois que a origem tiver recebido sua pose
do frame, incluindo:

- movimento feito pelo gameplay;
- atualizacao de fisica, quando aplicavel;
- amostragem da animacao skeletal;
- atualizacao de objetos externos.

O alvo deve receber a pose do attachment antes da renderizacao. A ordem exata
deve respeitar o contrato de fases do engine: `scene:onLoop()`, simulacao,
callbacks de plugins e renderizacao.

Quando a origem for um osso, a pose deve ser obtida da animacao ja amostrada
naquele frame. O sistema nao deve depender de um callback Lua `onUpdate` para
fazer a parte essencial do attachment, pois isso pode introduzir atraso e
ordem variavel.

## Espacos e composicao

O offset deve ser armazenado no espaco local da origem. A composicao deve
preservar:

- rotacao da origem aplicada ao deslocamento;
- escala da origem aplicada ao deslocamento quando a escala for herdada;
- rotacao relativa do alvo;
- escala relativa do alvo.

O estado interno deve preferir matrizes ou quaternions para composicao. A
conversao para Euler deve ocorrer apenas na fronteira da API que exige angulos,
reduzindo problemas com gimbal lock e multiplas voltas.

Escala nao uniforme combinada com rotacao deve ser tratada explicitamente. A
primeira implementacao deve validar os casos suportados e nao produzir
silenciosamente uma transformacao incorreta.

## Ciclo de vida

O sistema deve definir estes comportamentos:

- `attach`: cria ou ativa uma relacao;
- `detach`: desativa a relacao;
- `update`: reavalia a relacao no ciclo normal do engine;
- `destroy`: libera a relacao quando origem ou alvo deixam de existir.

Por padrao, `detach` deve preservar a pose mundial atual do alvo. Uma politica
alternativa pode restaurar a pose anterior, mas deve ser solicitada
explicitamente.

Attachments devem ser seguros quando a origem ou o alvo forem destruidos. O
sistema deve remover a relacao sem deixar ponteiros pendentes nem continuar
escrevendo em um objeto invalido.

## Grafo de dependencias

Attachments podem formar cadeias:

```text
personagem -> mao -> espada -> efeito
```

O sistema deve calcular uma ordem deterministica para resolver essas cadeias.
Ciclos devem ser detectados e rejeitados:

```text
A -> B -> A
```

Na primeira versao, recomenda-se permitir cadeias aciclicas e impedir que dois
attachments escrevam o mesmo componente do mesmo alvo simultaneamente.

## Autoria e editor

O offset deve poder ser capturado visualmente no editor. A autoria precisa
mostrar:

- origem e alvo;
- espaco de referencia;
- componentes herdados;
- offset atual;
- preview da relacao;
- indicacao de conflitos;
- acao de recapturar o offset.

O editor de interacao de animacao nao deve receber essa responsabilidade.
Ele deve ser usado apenas como referencia para o desenho do novo editor e do
novo plugin, especialmente nos seguintes aspectos:

- fluxo de autoria visual;
- selecao de atores e recursos;
- captura e edicao de offsets;
- preview no viewport;
- salvar, carregar e exportar definicoes;
- separacao entre dados de editor e dados de runtime;
- integracao entre a ferramenta Lua e o plugin C++.

O novo editor e o novo plugin devem possuir modelos, ciclo de vida e contrato
proprios. Nao devem depender do runtime de interacao nem adicionar
responsabilidades de attachments ao editor existente.

A autoria pode futuramente ser:

- uma ferramenta independente de attachments;
- uma extensao do editor skeletal para attachments em ossos e sockets;
- uma integracao com ferramentas de cena para attachments entre objetos.

O formato de autoria deve ser separado do formato de runtime quando isso
permitir guardar metadados de editor sem poluir a API executavel.

## Fases do roadmap

### Fase 0 - Contrato e casos de teste

- definir nomes finais da API e do formato de dados;
- definir unidades e convencoes de angulo;
- definir politicas de conflito, detach e destruicao;
- documentar a ordem de atualizacao;
- criar casos de teste para posicao, rotacao, escala, offset e cadeias.

### Fase 1 - Attachments de transform raiz

- origem e alvo como objetos/meshes existentes;
- offset local salvo;
- heranca independente de posicao, rotacao e escala;
- attach/detach em runtime;
- suporte a origem controlada externamente;
- deteccao de ciclos e conflitos;
- preservacao da pose no detach.

### Fase 2 - Integracao com o ciclo do engine

- garantir que a origem seja lida depois das atualizacoes do frame;
- garantir que o alvo seja escrito antes da renderizacao;
- tratar corretamente restart, troca de cena e destruicao;
- expor a pose efetiva para Lua e ferramentas de debug;
- validar multiplos attachments e cadeias.

### Fase 3 - Ossos e sockets

- resolver a origem em um osso skeletal;
- definir a origem em espaco local, global ou bind pose;
- suportar sockets nomeados quando o formato de mesh os fornecer;
- integrar com a amostragem do clipe ativo;
- criar ferramentas de selecao e preview do osso/socket.

Este e o caso que habilita posicionar uma espada na mao de um personagem com
base no tempo e na pose do clipe.

### Fase 4 - Ferramenta de autoria

- criar editor independente ou integracao com o editor skeletal;
- capturar offsets visualmente;
- exibir gizmos e linhas de relacao;
- salvar e recarregar projetos;
- exportar uma definicao de runtime;
- fornecer preview com objetos reais.

### Fase 5 - Constraints adicionais

Somente depois do contrato basico estar estavel:

- limites de rotacao;
- look-at ou aim constraints;
- mola e amortecimento;
- blending entre varias origens;
- integracao opcional com fisica;
- constraints condicionais por estado de gameplay.

## Riscos principais

### Ordem entre sistemas

Gameplay, fisica, animacao, attachments e renderizacao precisam ter uma ordem
observavel. A solucao nao deve depender de qual plugin foi carregado primeiro.

### Composicao de escala

Escala nao uniforme e rotacao podem exigir uma representacao matricial completa.
O comportamento deve ser testado antes de ser exposto como recurso geral.

### Conflitos de escrita

Um alvo pode ser controlado simultaneamente por gameplay, fisica, interacao e
attachment. A API precisa informar quem possui cada componente ou rejeitar a
configuracao ambigua.

### Conversao para Euler

Attachments em ossos podem produzir orientacoes que nao tenham uma
representacao Euler estavel. A composicao interna nao deve depender de somar
angulos diretamente.

### Destruicao e troca de cena

O sistema precisa observar o ciclo de vida dos objetos e das cenas para nunca
reter referencias invalidas.

## Fora do runtime de interacao

O plugin de interacao nao deve conhecer attachments, constraints ou flags de
propriedade externa. Uma interacao pode coexistir com attachments porque ambos
atuam no mesmo frame, mas a relacao deve ser criada e gerenciada pelo sistema
proprio.

Se uma interacao precisar ativar ou desativar um attachment, a integracao deve
ser feita por uma API ou evento de alto nivel, sem incorporar a implementacao
de attachment ao plugin de interacao.

## Criterio de pronto para a primeira entrega

A primeira entrega sera considerada pronta quando for possivel:

1. anexar um objeto a outro objeto raiz;
2. mover a origem externamente a cada frame;
3. manter offset relativo de posicao e rotacao;
4. escolher independentemente posicao, rotacao e escala;
5. aplicar escala da origem ao offset quando habilitado;
6. desanexar preservando a pose mundial;
7. detectar ciclos e conflitos;
8. sobreviver a restart, destruicao e troca de cena;
9. executar sem alterar o comportamento de projetos que nao usam o sistema.

