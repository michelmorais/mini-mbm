# Interaction Animation Editor

Status: **editor implementado e runtime validado no protótipo Ratworks**.

O protótipo pode ser aberto diretamente com:

```sh
bin/debug/linux_x86/mini-mbm --scene editor/interaction_animation_editor.lua \\
    --disable_select_monitor --nosplash -w 1400 -h 900
```

Em uma build Linux/macOS com Lua, o shortcut gerado pelo CMake é
`interaction_animation_editor`. O editor também aparece no seletor de
aplicacoes padrao de Linux, macOS e Windows (`mini-mbm-dev`).

O editor inicia sem atores ou meshes carregados. Use `Add actor` para criar
slots dinâmicos (`actor1`, `actor2`, `actor3`, ...); cada ator pode receber uma
mesh e uma animação. Apenas um ator pode ser marcado como âncora por vez. Para testar a
composição atual do Ratworks,
abra o projeto salvo `test.iae.lua` pelo menu `File > Load project` ou pelo atalho
`Ctrl+O`. Os caminhos das meshes são mantidos no próprio projeto. Projetos
criados antes do modelo de atores genéricos devem ser recriados neste estágio.
O idioma selecionado no menu `Options` também é salvo no projeto e restaurado
ao carregá-lo.

Esta entrega cobre carregamento de múltiplas meshes, seleção de clips, relógio
principal, preview, eventos básicos de transformação/troca de animação,
salvamento do projeto, salvamento com novo caminho e exportação de um módulo
Lua executável para runtime.
O módulo contém o ciclo de vida básico da interação e a execução dos gatilhos
de transformação e troca de animação.

O viewport inicial desta entrega também possui a base de navegação esperada para
um editor 3D: painel de órbita de câmera inspirado no `scene_editor3d.lua`,
linhas de origem X/Y/Z, seleção das meshes por clique, pan/orbit na view e um
gizmo de eixos ou anéis XYZ para mover ou rotacionar o ator selecionado. O ator
pode ser deselecionado clicando no espaço vazio ou pressionando `ESC`; nesse
caso o gizmo é ocultado. A timeline fica ancorada no rodapé com altura reduzida
para preservar a área de preview.
No tempo `0`, essas transformações definem a pose base relativa da mesh e são
exportadas como dados do projeto. Em qualquer tempo maior que `0`, alterações
manuais são uma prévia temporária até serem capturadas em um evento.
Os anéis de rotação mantêm o ângulo acumulado durante o arraste, portanto os
campos `ROT` podem ultrapassar `180` e `360` graus. Isso permite registrar duas
ou mais voltas no mesmo eixo; a orientação visual equivalente não perde a
quantidade de voltas armazenada.

O viewport também possui um plano visual criado dinamicamente no plano XZ,
posicionado em `Y = 0`. Seu tamanho acompanha a distância da câmera e sua
posição acompanha o foco horizontal da órbita. A visibilidade pode ser alternada
em `Camera > Show XZ plane`; o plano é somente uma referência visual e não é
salvo no projeto nem participa da interação exportada.

Este documento registra o desenho e o estado implementado do editor de
sequências de interação entre duas ou mais meshes. O caso validado é a
interação entre o rato e a máquina do projeto Ratworks:

- `Cyclopsv2.msh`, com a animação esquelética `Eaten`;
- `ChompBot.msh`, com a animação articulada `bite`.

O editor não criará necessariamente uma terceira animação dentro de um arquivo
`.msh`. Ele criará uma sequência que coordena instâncias de meshes, animações,
transformações relativas e eventos.

A sequência possui uma âncora de interação selecionável no editor. Inicialmente
ela é a máquina, porque o clip `bite` é o relógio principal. Os eixos de edição
do gizmo são derivados da transformação da âncora, permitindo que o rato seja
editado de forma consistente mesmo quando a interação inteira estiver orientada
para outra direção no mundo.

## Milestone validado no Ratworks

O módulo exportado foi integrado ao protótipo do jogo e validado em uma cena de
teste independente. A cena permite alternar a flag `machine_look_to_left` entre
as orientações esquerda e direita da máquina. Em ambas as orientações, a mesma
interação mantém o comportamento visual esperado:

- a interação captura a pose atual dos atores quando `start` é chamado;
- o clip base de cada ator começa no início da interação;
- os deslocamentos e rotações são relativos à pose capturada, não à posição
  absoluta usada na prévia do editor;
- a máquina continua sendo a âncora dos eixos relativos;
- o rato entra na boca com a mesma translação e rotação quando a máquina está
  orientada para a esquerda ou para a direita;
- o corpo Box2D do rato é destruído ao reservar o ator para a interação, pois a
  física não deve continuar sobrescrevendo a transformação controlada pela
  sequência.

Essa validação confirma que uma sequência exportada pode ser disparada em
qualquer ponto compatível da cena sem recalcular manualmente os eventos para a
orientação absoluta dos atores.

## Contexto atual

No protótipo do Ratworks, a interação é disparada manualmente pelo código:

1. o rato chega à máquina;
2. a máquina inicia `bite`;
3. o rato inicia `Eaten`;
4. a máquina e o rato são removidos ou avançam para a próxima ação quando os
   clips terminam.

O editor permite ajustar essa composição visualmente sem transformar
   cada tentativa de sincronização em código manual.

### Duração dos clips

A duração é lida do asset carregado. Ela não deve ser presumida pelo editor nem
copiada de documentação histórica.

No estado atual usado pelo Ratworks, `ChompBot.msh` informa **1,5 segundos**
para `bite`, conforme verificado no editor. Uma documentação anterior do
Ratworks registrava aproximadamente 0,5 segundo; essa observação está
desatualizada e não deve ser usada como contrato.

O asset atual `Cyclopsv2.msh` informa **2,6 segundos** para `Eaten`. As durações
de ambos os clips devem continuar sendo lidas dos assets carregados; os valores
acima são apenas uma referência do estado atual do Ratworks.

O editor deve sempre consultar a duração real do clip e informar inconsistências
quando um gatilho não puder ser satisfeito.

## Conceitos principais

### Atores

Cada mesh carregada no projeto ocupa um slot interno estável. Os slots são
`actor1`, `actor2`, `actor3` e assim por diante; o editor não solicita nomes
personalizados e não salva um nome de ator no projeto:

```lua
actors = {
    actor1 = { file = "ChompBot.msh" },
    actor2 = { file = "Cyclopsv2.msh" }
}
```

O identificador do slot é usado pelos eventos exportados e também é a única
identidade necessária no runtime. Uma única âncora define os eixos da interação;
os demais atores são percorridos em loop e recebem suas transformações relativas
nesse mesmo espaço. O arquivo `.msh` continua
sendo um recurso independente e pode ser instanciado em qualquer posição da cena.
Na interface, o ator mostra apenas o basename da mesh, enquanto o caminho
completo permanece salvo no projeto. A escolha da mesh é feita pelo botão
`Browse`, que já carrega o arquivo.

### Relógio da interação

Uma interação possui uma timeline própria e um relógio principal configurável.
São conceitos diferentes:

- a duração da timeline pertence ao projeto e define a extensão total da
  interação;
- o relógio identifica o ator e o clip usados como referência para sincronizar
  os eventos;
- a duração de um clip não encerra automaticamente a timeline do projeto.

Ao criar um projeto, a duração inicial da timeline deve ser o maior tempo entre
as animações base dos atores carregados. Depois de criada, essa duração é um
valor do projeto e pode ser ampliada para incluir ações posteriores ao fim dos
clips. Trocar a animação do relógio não deve reduzir silenciosamente a duração
da timeline.

O relógio pode ser baseado no tempo de um clip específico, por exemplo:

```lua
clock = {
    actor = "actor1",
    animation = "bite"
}
```

Nesse caso, um evento em `0.2` ocorre quando `actor1:bite` atingir 0,2 s,
independentemente da posição absoluta da máquina na cena. Se `bite` terminar
antes da timeline, o clip pode permanecer no seu último frame enquanto a
timeline continua avançando e os eventos posteriores continuam válidos.

O módulo exportado deve usar a duração da timeline para determinar quando a
interação termina. O relógio serve para sincronização; não é o limite global da
sequência.

O projeto possui a opção `continueClipsAfterTimeline`, disponível na janela
`Timeline` e desativada por padrão:

- `false`: os clips ativos são pausados quando a timeline termina;
- `true`: a timeline e seus eventos terminam, mas os clips continuam sendo
  reproduzidos pelo engine.

Essa opção controla somente a reprodução dos clips. A decisão de destruir,
manter ou reutilizar os atores pertence ao jogo, inclusive quando um clip está
em loop. `isFinished()` indica o fim da timeline e dos eventos da interação;
não representa uma ordem para destruir os atores.

Essa regra é aplicada pelo editor e pelo módulo exportado. O campo `duration`
do projeto representa a duração da timeline; a duração exibida do relógio é
apenas informativa e não reduz esse valor.

### Base da interação

A base de cada ator é definida no tempo `0` da sequência:

- a posição e a rotação configuradas nos campos `POS` e `ROT` ou pelo gizmo;
- a animação selecionada no painel do ator, iniciada no tempo `0`;
- a escala configurada para o ator.

O `clipKey` selecionado é a animação base. Um evento `play` posterior pode
substituí-la a partir do seu próprio tempo. A âncora define os eixos usados para
interpretar os deslocamentos e rotações dos eventos.

No editor, essa base é a pose de referência da prévia. No runtime, a interação
captura a pose real dos objetos no momento em que começa e aplica os eventos
relativos a essa pose; as coordenadas absolutas usadas na prévia não limitam a
posição da interação no mundo.

O fluxo atual para estabelecer a base é ajustar os atores no tempo `0` e
escolher as animações base. A referência é criada e atualizada automaticamente
nesse instante; não há uma ação separada para defini-la. Essa referência contém
as posições, rotações, escalas e animações iniciais de cada ator e é salva no
projeto. A captura de transformações usa essa referência, mesmo que a pose
visual temporária tenha sido alterada durante a edição.

### Gatilhos

Os gatilhos podem observar qualquer ator ou o relógio principal:

- tempo do relógio principal;
- tempo de um clip de um ator;
- início de um clip;
- término de um clip não cíclico;
- evento manual ou callback Lua.

Os eventos aparecem na janela em ordem cronológica. Um evento novo é inserido
diretamente na posição correspondente ao tempo atual da timeline; se o tempo de
um evento existente for alterado, a lista é reordenada e o evento editado
continua selecionado.

Exemplos conceituais:

```lua
{ when = { clock = 0.2 }, action = ... }

{ when = {
    actor = "actor1",
    animation = "bite",
    time = 1.0
}, action = ... }

{ when = {
    actor = "actor2",
    animation = "Eaten",
    ended = true
}, action = ... }
```

Cada gatilho deve ser disparado uma única vez por execução da interação. Reiniciar
uma interação rearma todos os gatilhos.

### Transformações relativas

Posição, rotação e escala editadas pelo usuário serão relativas à pose inicial
capturada quando a interação começar. O editor não deve exportar coordenadas
absolutas da cena para uma reação que precisa funcionar em diferentes locais.

Exemplo:

```lua
{
    when = { clock = 1.0 },
    action = {
        actor = "actor2",
        transform = {
            position = { x = 0, y = -2, z = 0 },
            rotation = { x = 0, y = 20, z = 0 },
            scale = { x = 0.5, y = 0.5, z = 0.5 }
        }
    }
}
```

O runtime captura a transformação inicial do rato e aplica esse deslocamento
relativo. A escala usa fatores multiplicativos: `1.0` preserva a escala base,
`2.0` dobra o tamanho e `0.5` reduz pela metade. A sequência pode, portanto,
ser usada quando o rato estiver em outra posição, orientação ou escala no mundo.

Na primeira versão, a transformação relativa será da mesh inteira. Transformar
ossos ou partes articuladas individualmente fica para uma etapa posterior.

### Captura de uma transformação

Para criar um evento visualmente, mova a timeline para o instante desejado,
selecione o ator e use o gizmo ou os campos de transformação para alterar sua
pose. O editor mantém a pose inicial como base. O botão `Capture transform`
cria um evento no tempo atual e preenche automaticamente os deslocamentos,
rotações e fatores de escala relativos à base.
Cada evento de transformação pode ser `Instant` ou `Smooth`. No modo suave,
`Transition interval (s)` define explicitamente os tempos de início e fim da
interpolação de posição, rotação e escala. O início nunca pode ser menor que zero nem
maior que o fim. A suavização deve ser associada ao intervalo da animação
atual, e não esticada automaticamente até o fim da timeline do projeto. O
evento é considerado concluído no fim do intervalo; não há mais uma regra
implícita baseada no último evento `play`.

Quando a referência for o clip do relógio mestre, o intervalo da suavização é
calculado dentro do período desse clip. Por exemplo, se `bite` começar em
`0.2 s`, durar `1.5 s`, e a suavização for desejada entre 30% e 100% do clip,
os tempos absolutos da timeline serão `0.65 s` e `1.70 s`:

```text
inicio = 0.2 + (1.5 * 0.30) = 0.65 s
fim    = 0.2 + (1.5 * 1.00) = 1.70 s
```

Depois de `1.70 s`, a transformação já está concluída, mas a timeline pode
continuar até a sua duração de projeto. Em uma etapa posterior, outros atores
ou clips poderão fornecer a referência da suavização; nesse caso, a mesma regra
será aplicada ao intervalo do clip ativo associado ao evento.

Alterações manuais feitas depois do tempo `0` são apenas uma prévia da pose
atual. Se a timeline for movida, reiniciada ou reproduzida sem usar `Capture
transform`, essa prévia é descartada e a pose volta para a base mais os eventos
já definidos. Para tornar a alteração permanente na interação, capture-a em um
evento.

## Ações previstas

O primeiro conjunto de ações deverá incluir:

- iniciar, pausar, retomar ou trocar uma animação;
- aplicar transformação relativa;
- interpolar uma transformação durante um intervalo;
- emitir uma notificação nomeada em um instante da timeline;
- alterar visibilidade;
- destruir ou liberar um ator;
- chamar uma função Lua definida pelo usuário.

O tipo de animação deverá ser descoberto pelo editor. A exportação deve chamar a
API adequada para clips esqueléticos ou articulados, sem exigir que o usuário
escreva essa distinção manualmente.

## Exemplo da interação rato-máquina

Uma sequência inicial poderia ser representada assim:

```lua
return {
    duration = 3.0,
    continueClipsAfterTimeline = false,

    clock = {
        actor = "actor1",
        animation = "bite"
    },

    actors = {
        actor1 = { file = "ChompBot.msh" },
        actor2 = { file = "Cyclopsv2.msh" }
    },

    events = {
        {
            id = "rat_entered_mouth",
            when = { clock = 0.2 },
            action = {
                actor = "actor2",
                playSkeletalAnimation = "Eaten"
            }
        },
        {
            id = "fluid_start",
            when = { clock = 0.6 },
            action = {
                notify = true
            }
        },
        {
            when = { clock = 1.0 },
            action = {
                actor = "actor2",
                transform = {
                    position = { x = 0, y = -2, z = 0 },
                    rotation = { x = 0, y = 20, z = 0 }
                }
            }
        },
        {
            when = {
                actor = "actor2",
                animation = "Eaten",
                ended = true
            },
            action = {
                actor = "actor2",
                destroy = true
            }
        }
    }
}
```

O formato acima é ilustrativo. O editor preserva a separação entre atores,
relógio, gatilhos e ações na especificação embutida no módulo exportado.

## Exportação como módulo Lua

O resultado será um arquivo Lua carregável com `require`, e não um trecho que
precise ser copiado manualmente para `onLoop`.

A interface de runtime exportada é:

```lua
local reaction = require "chomp_eat-interaction"

local instance = reaction.start {
    actor1 = machineObject,
    actor2 = ratObject,

    callbacks = {
        onStart = function(instance)
            -- opcional: preparar a interacao, remover fisica etc.
        end,
        onEvent = function(instance, event, index)
            -- opcional: reagir a um evento discreto da timeline
        end,
        onFinish = function(instance)
            -- opcional: liberar recursos ou iniciar a proxima acao
        end
    }
}

-- No onLoop(delta):
instance:update(delta)

-- Opcionalmente:
instance:restart()
instance:stop()
if instance:isFinished() then
    -- a interacao terminou
end
```

O módulo exportado:

- receber as instâncias reais dos atores;
- capturar as poses iniciais;
- iniciar os clips configurados nos eventos `play`;
- observar os tempos da timeline e finalizar ao atingir a duração do projeto;
- aplicar cada evento uma única vez;
- aplicar transformações relativas;
- permitir reinício e encerramento da interação.
- chamar callbacks opcionais de início, evento e finalização.

Os callbacks são fornecidos pelo jogo na chamada `start`; eles não são
serializados no projeto `.iae.lua`. `onStart` é chamado depois que a pose base
e os clips iniciais foram preparados. `onEvent` é chamado uma vez para cada
evento atingido, recebendo a instância, a tabela do evento e seu índice. A
tabela pode conter o campo opcional `id`, que permite ao jogo identificar o
evento sem depender de sua posição na lista. Para uma transformação suave, o
callback ocorre no início do intervalo de transição. `onFinish` é chamado uma
vez quando a timeline termina ou quando
`stop()` encerra explicitamente a interação. Em `restart()`, `onStart` e os
callbacks dos eventos são habilitados novamente para a nova execução.

O editor sugere IDs como `event_1` para novos eventos. Projetos antigos podem
manter eventos sem ID. IDs preenchidos devem ser únicos; o editor bloqueia o
salvamento e a exportação enquanto houver duplicidade.

A ação `Notify event` não possui ator-alvo e não altera meshes. Ela apenas
dispara `onEvent` no tempo mestre informado, permitindo que o jogo inicie
fluido, áudio, partículas ou outra lógica externa. O tempo do evento não é
limitado à duração de um clip individual, mas à duração da timeline do projeto.

A ação `Visibility` exige um ator-alvo e altera `visible` no tempo mestre. O
estado inicial de visibilidade faz parte da pose base e é restaurado por
`restart()` ou `stop()`. Uma ação de visibilidade também pode ser observada
por `onEvent` quando for atingida.

As chaves da tabela de `start` devem usar os slots internos (`actor1`, `actor2`,
`actor3`, ...). O runtime também aceita os aliases fixos correspondentes
(`actor_1`, `actor_2`, `actor_3`, ...), usados pela integração atual do Ratworks.
Não há nomes definidos pelo usuário nessa camada.
Callbacks inválidos causam erro no início da interação.

## Validação e erros do editor

O editor deve apresentar erro ou aviso claro nos seguintes casos:

- ator referenciado inexistente;
- arquivo ou clip não encontrado;
- tipo de animação incompatível com a ação solicitada;
- relógio baseado em clip inexistente;
- duração inválida, negativa ou não finita;
- evento além da duração da timeline do projeto;
- gatilho de término aplicado a clip cíclico;
- transformação com valores não finitos;
- múltiplas ações conflitantes no mesmo ator e instante;
- evento que depende de uma animação que nunca foi iniciada.

Ao salvar, o editor valida a estrutura, referências, tempos, transformações e
transições do projeto. Ao exportar, aplica também as verificações necessárias
para o runtime: pelo menos um ator, meshes carregadas, clips válidos e uma
animação válida para o relógio mestre. A operação é bloqueada quando encontra
um erro impeditivo e a mensagem aparece no aviso padrão do editor.

No caso de `ChompBot/bite`, um gatilho em 1,0 s é válido quando o asset carregado
possui duração de 1,5 s. O editor não deve rejeitá-lo com base em uma duração
fixa ou em documentação antiga.

## Teste automatizado do runtime

O comportamento do runtime pode ser verificado sem abrir uma janela usando
meshes falsas que registram as chamadas de animação, transformação e pausa:

```sh
bin/debug/linux_x86/lua-5.4.1.exe \
    tests/interaction_animation_runtime_test.lua
```

Também é possível validar um módulo exportado específico, incluindo o arquivo
gerado para o protótipo Ratworks:

```sh
bin/debug/linux_x86/lua-5.4.1.exe \
    tests/interaction_animation_runtime_test.lua \
    /home/michel/Ratworks/logic/chomp_eat-interaction.lua
```

Esse teste cobre eventos únicos, transformações suaves, callbacks, reinício,
encerramento e a opção `continueClipsAfterTimeline`.

O teste de projeto cobre a validação automatizada do formato multi-ator e da
exportação. Ele verifica também a preservação de rotações maiores que 360 graus
no arquivo `.iae.lua` e no módulo Lua resultante.

O projeto e o módulo exportado também podem ser verificados como artefatos,
sem abrir uma janela. O teste confirma a ordem dos atores, os caminhos das
meshes, a âncora, o relógio, a duração da timeline e a preservação de uma
rotação maior que 360 graus:

```sh
bin/debug/linux_x86/lua-5.4.1.exe \
    tests/interaction_animation_project_test.lua \
    /home/michel/mini-mbm/new-chomp_eat.iae.lua \
    /home/michel/Ratworks/new-chomp_eat-interaction.lua
```

Para uma validação completa, execute também o teste de runtime passando o
módulo exportado como argumento.

## Interface implementada

A primeira versão do editor contém:

1. carregamento de múltiplas meshes;
2. seleção do papel de cada ator;
3. seleção dos clips disponíveis;
4. escolha do relógio principal e edição da duração da timeline do projeto;
5. viewport com reprodução conjunta;
6. play, pause, avanço, retrocesso, reinício e scrub;
7. timeline de gatilhos;
8. edição de transformações relativas;
9. ações de troca de animação, transformação, notificação e visibilidade;
10. validação visual e textual;
11. salvar, salvar como e carregar o projeto do editor;
12. exportar o módulo Lua para `require`.

No menu `File`, `Save project` grava diretamente no caminho ativo do projeto.
`Save project as...` sempre abre o diálogo de gravação para escolher outro
arquivo; o caminho ativo só muda depois que o novo projeto é gravado com
sucesso. Cancelar o diálogo ou falhar na gravação preserva o projeto e o
caminho atuais.

Todas as operações caras, como carregamento, reconstrução de prévia e
serialização, deverão acontecer somente quando os dados mudarem ou quando o
usuário solicitar uma ação explícita. Uma timeline parada não deve executar
`seek`, reconstruir meshes ou serializar o projeto continuamente.

Ao pressionar `Play` quando a timeline já estiver no fim, a reprodução retorna
automaticamente ao início. Os botões de avanço e retrocesso movem a timeline
um frame de 1/30 s por vez e mantêm a reprodução pausada.

## Fora do escopo inicial

- edição de ossos individuais dentro da sequência;
- edição de pivôs e chaves internas das animações originais;
- mistura automática complexa entre vários clips;
- áudio, partículas e efeitos externos como tipos nativos de evento;
- mais de uma sequência aninhada dentro de outra;
- geração de uma nova animação persistida dentro de `.msh`.

Esses recursos podem ser adicionados depois que o modelo de atores, relógios,
gatilhos e exportação Lua estiver validado com o caso do rato e da máquina.

## Próximas etapas

1. Adicionar testes automatizados para tempos inválidos, clips cíclicos,
   reinício e eventos executados uma única vez.
2. Expandir os tipos de ação, incluindo destruição como ação configurável da
   sequência, mantendo a decisão final de liberar recursos no jogo.
3. Avaliar edição de grupos e organização visual para projetos com muitos atores.
