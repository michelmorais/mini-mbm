# Interaction Animation Editor

Este documento descreve o editor de interacoes entre meshes e o modulo Lua
exportado por ele.

O editor pode ser iniciado diretamente com:

```sh
bin/debug/linux_x86/mini-mbm --scene editor/interaction_animation_editor.lua \
    --disable_select_monitor --nosplash -w 1400 -h 900
```

Em builds Linux, macOS e Windows, o editor tambem esta disponivel no seletor
de aplicacoes padrao e pelo shortcut `interaction_animation_editor` quando ele
for gerado pelo CMake.

## Funcionalidade do editor

O editor inicia sem atores carregados. O botao `Add actor` cria slots estaveis
como `actor1`, `actor2`, `actor3` e assim por diante. Cada slot pode receber
uma mesh e uma animacao. O editor nao atribui nomes personalizados aos atores;
o slot e a identidade usada pelos eventos e pelo runtime.

A janela `Interaction actors` permite:

- adicionar atores;
- procurar e carregar uma mesh;
- selecionar a animacao base;
- marcar um unico ator como ancora;
- escolher o ator e a animacao do relogio mestre;
- editar posicao, rotacao e escala da pose base;
- alternar entre os modos `Move` e `Rotate`.

A mesh e mostrada pelo nome do arquivo, enquanto o caminho completo e salvo no
projeto. A escolha da mesh pelo botao `Browse` ja carrega o recurso.

O viewport 3D possui:

- orbita, pan e zoom da camera;
- linhas de origem X, Y e Z;
- plano visual no plano XZ, alternavel no painel da camera;
- selecao de meshes por clique;
- gizmo de translacao e rotacao com `alwaysOnTop`;
- deselecao por clique no espaco vazio ou pela tecla `ESC`.

Ao selecionar um evento, o ator associado recebe por aproximadamente tres
segundos um cubo amarelo translucido com contorno. O cubo usa a mesma
referencia visual do volume de captura do `mesh_debug.lua` e acompanha o
`AABB` atual da mesh.

Na janela `Timeline`, a duracao pertence ao projeto e e independente da duracao
dos clips. A janela oferece edicao da duracao, reproducao, pausa, reinicio,
retrocesso, avanco e deslocamento livre pelo slider. A opcao
`Continue clips after timeline` controla se os clips continuam depois que a
timeline termina.

O menu `File` oferece:

- `Load project`: carrega um arquivo `.iae.lua`;
- `Save project`: sobrescreve o caminho ativo;
- `Save project as...`: escolhe outro caminho e o torna ativo somente depois
  de uma gravacao bem-sucedida;
- `Export require specification`: gera o modulo Lua executavel.

O idioma selecionado no menu `Options` e salvo no projeto.

## Projeto `.iae.lua`

O projeto e um arquivo Lua que atribui uma tabela a `interactionProject`.
Os campos principais sao:

```lua
interactionProject = {
    version = 7,
    language = "pt_br",
    duration = 2.0,
    continueClipsAfterTimeline = false,
    anchorActor = "actor1",
    clockActor = "actor1",
    clockKey = "articulated:bite",
    actorOrder = {"actor1", "actor2"},

    actors = {
        actor1 = {
            path = "meshes/machine.msh",
            clipKey = "articulated:bite",
            base = {
                x = 0, y = 0, z = 0,
                rx = 0, ry = 0, rz = 0,
                sx = 1, sy = 1, sz = 1,
                visible = true
            }
        },
        actor2 = {
            path = "meshes/character.msh",
            clipKey = "skeletal:idle",
            base = {
                x = 100, y = 0, z = 0,
                rx = 0, ry = 0, rz = 0,
                sx = 1, sy = 1, sz = 1,
                visible = true
            }
        }
    },

    events = {
        {
            id = "event_1",
            time = 0.2,
            action = "play",
            target = "actor2",
            clipKey = "skeletal:reaction"
        }
    }
}
```

`baseReference` tambem pode ser gravado pelo editor para manter a referencia
das poses no tempo zero. O projeto nao salva objetos de runtime; ele salva
caminhos, clips, transformacoes e eventos.

## Atores, ancora e relogio

Os slots dos atores sao dinamicos e devem aparecer em `actorOrder`. O campo
`anchorActor` identifica o ator que fornece os eixos da interacao. Todos os
deslocamentos e rotacoes relativos sao interpretados nesse espaco.

`clockActor` e `clockKey` identificam o clip usado como relogio principal no
editor. A timeline do projeto e o limite global da interacao; o fim de um clip
individual nao reduz a duracao da timeline.

No tempo `0`, a posicao, rotacao, escala, visibilidade e animacao selecionadas
formam a pose base. No runtime, as transformacoes reais dos objetos sao
capturadas quando `start` e chamado. A pose da pre-visualizacao nao limita a
posicao absoluta em que a interacao sera executada.

Alteracoes manuais feitas depois do tempo `0` sao uma pre-visualizacao
temporaria. Se elas nao forem capturadas em um evento, mover a timeline,
reiniciar ou reproduzir restaura a base e os eventos ja definidos.

## Eventos

Todos os eventos sao agendados pelo tempo mestre da timeline. O formato atual
possui estas acoes:

### `transform`

Aplica deslocamento relativo nos campos `x`, `y` e `z`, rotacao em graus nos
campos `rx`, `ry` e `rz`, e fatores multiplicativos de escala nos campos `sx`,
`sy` e `sz`.

`transition = "instant"` aplica a transformacao imediatamente. Para uma
transicao suave, use `transition = "smooth"`, `transitionStart` e
`transitionEnd`. O campo `time` representa o fim da transicao. A interpolacao
usa uma curva suave e os valores de rotacao podem ultrapassar 360 graus,
preservando multiplas voltas.

```lua
{
    id = "move_actor2",
    time = 1.2,
    action = "transform",
    target = "actor2",
    transition = "smooth",
    transitionStart = 0.6,
    transitionEnd = 1.2,
    x = 0,
    y = -20,
    z = 0,
    rx = 0,
    ry = 0,
    rz = 720,
    sx = 1,
    sy = 1,
    sz = 1
}
```

O botao `Capture transform` calcula esses valores a partir da pose base e da
pose visual atual do ator.

### `play`

Inicia ou troca a animacao de um ator no tempo indicado. `clipKey` usa o
formato `frame:nome`, `skeletal:nome` ou `articulated:nome`, conforme o tipo de
animacao descoberto no asset. O formato `frame:nome` seleciona uma animacao
estatica tradicional, formada por uma faixa de frames da mesh.

Animacoes estaticas preservam o modo de reproducao configurado na mesh, como
crescimento, decrescimento, loop ou recursao. Durante a edicao, o frame e
calculado diretamente a partir do tempo mestre; no runtime, o modo original
volta a ser aplicado quando `continueClipsAfterTimeline` estiver habilitado.

### `notify`

Nao possui ator-alvo e nao altera uma mesh. Apenas gera o callback `onEvent`
no tempo indicado, permitindo que o jogo execute logica externa.

### `visibility`

Altera a propriedade `visible` do ator no tempo indicado. O estado de
visibilidade da pose base e restaurado por `restart()` e `stop()`.

Eventos preenchidos recebem IDs sugeridos como `event_1`. IDs nao vazios devem
ser unicos; o editor bloqueia salvar e exportar quando encontra duplicidade.
Cada evento e disparado uma vez por execucao. `restart()` rearma os eventos.

## Continuidade dos clips

`continueClipsAfterTimeline` pertence a `Timeline` e tem valor padrao `false`.

- `false`: os clips ativos sao pausados quando a timeline termina;
- `true`: a timeline e seus callbacks terminam, mas os clips continuam sendo
  atualizados pelo engine.

Essa opcao nao determina o destino dos objetos. Destruir, ocultar, liberar
fisica ou reutilizar um ator pertence ao jogo e pode ser implementado pelos
callbacks.

## Modulo Lua exportado

O arquivo exportado contem somente a definicao da interacao. O runtime oficial
e implementado em C++ pelo modulo `interaction_animation_runtime`:

```lua
local definition = require "my_interaction"
local runtime = require "interaction_animation_runtime"

local instance = runtime.start {
    definition = definition,
    actors = {
        actor1 = machineObject,
        actor2 = characterObject,
    },
    callbacks = {
        onStart = function(instance)
            -- Preparacao opcional da interacao.
        end,
        onEvent = function(instance, event, index)
            -- Reacao opcional a um evento, usando event.id.
        end,
        onFinish = function(instance)
            -- Finalizacao opcional controlada pelo jogo.
        end
    }
}

-- A engine atualiza a interacao automaticamente pelo ciclo de plugins.
```

As chaves de `actors` podem usar o slot interno (`actor1`) ou seu alias com
underscore (`actor_1`). O mesmo vale para os demais slots.

Ao iniciar, o runtime:

1. valida os callbacks;
2. exige um binding para cada ator definido;
3. captura posicao, rotacao, escala e visibilidade atuais;
4. inicia os clips base, usando `clockKey` para o ator do relogio;
5. aplica os eventos do tempo zero;
6. chama `onStart`.

Depois de `runtime.start(config)`, a instancia e registrada no ciclo de plugins
da engine. A cada frame o runtime avanca a timeline, dispara eventos, aplica
transformacoes e atualiza visibilidade automaticamente; o jogo nao precisa
chamar um metodo de atualizacao manual.

`instance:isFinished()` informa se a timeline terminou ou se `stop()` foi
chamado. `instance:restart()` restaura as poses capturadas, rearma eventos,
reinicia os clips e chama `onStart` novamente. `instance:stop()` interrompe os
clips ativos, restaura as poses capturadas e chama `onFinish`.

Os callbacks nao sao serializados no arquivo `.iae.lua`; eles pertencem ao
codigo do jogo. `onEvent` recebe a instancia, a tabela do evento e o indice do
evento. Para uma transformacao suave, o callback ocorre no inicio do intervalo
de transicao. `onFinish` e chamado uma vez por execucao, inclusive quando a
interacao e encerrada explicitamente por `stop()`.

## Validacao

Ao salvar, o editor valida:

- a duracao positiva e finita da timeline;
- a ordem e as referencias dos atores;
- os valores finitos das transformacoes base;
- a unicidade dos IDs preenchidos;
- acoes suportadas;
- tempos dentro da timeline;
- alvos validos;
- valores finitos das transformacoes dos eventos;
- tipos de transicao e intervalos validos.

Ao exportar, tambem valida:

- a existencia de pelo menos um ator;
- o caminho de mesh de cada ator;
- clips base validos quando selecionados;
- um ator de relogio valido;
- uma animacao valida para o relogio;
- clips validos nos eventos `play`.

Erros impedem a operacao e sao exibidos pelo aviso padrao do editor.
