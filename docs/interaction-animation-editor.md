# Interaction Animation Editor

Status: **primeiro protótipo standalone implementado e registrado nos shortcuts Linux/macOS**.

O protótipo pode ser aberto diretamente com:

```sh
bin/debug/linux_x86/mini-mbm --scene editor/interaction_animation_editor.lua \\
    --disable_select_monitor --nosplash -w 1400 -h 900
```

Em uma build Linux/macOS com Lua, o shortcut gerado pelo CMake é
`interaction_animation_editor`. O editor também aparece no seletor de
aplicacoes padrao de Linux, macOS e Windows (`mini-mbm-dev`).

O editor inicia sem atores ou meshes carregados. Use `Add actor` para criar um
dos dois slots disponíveis; cada ator pode receber um nome opcional, uma mesh e
uma animação. Apenas um ator pode ser marcado como âncora por vez. Para testar a
composição atual do Ratworks,
abra o projeto salvo `test.iae.lua` pelo menu `File > Load project` ou pelo atalho
`Ctrl+O`. Os caminhos das meshes são mantidos no próprio projeto. Projetos
criados antes do modelo de atores genéricos devem ser recriados neste estágio.
O idioma selecionado no menu `Options` também é salvo no projeto e restaurado
ao carregá-lo.

Esta primeira entrega cobre carregamento das duas meshes, seleção de clips,
relógio principal, preview, eventos básicos de transformação/troca de animação e
salvamento/exportação Lua. O player definitivo para uso em runtime ainda não foi
implementado.

Os eventos existentes nesta etapa são eventos de preview do editor. Eles ainda
não constituem um player real-time completo para o jogo: faltam o módulo Lua
executável, o ciclo de vida da interação e a execução robusta de gatilhos no
runtime.

O viewport inicial desta entrega também possui a base de navegação esperada para
um editor 3D: painel de órbita de câmera inspirado no `scene_editor3d.lua`,
linhas de origem X/Y/Z, seleção das meshes por clique, pan/orbit na view e um
gizmo de eixos ou anéis XYZ para mover ou rotacionar o ator selecionado. O ator
pode ser deselecionado clicando no espaço vazio ou pressionando `ESC`; nesse
caso o gizmo é ocultado. A timeline fica ancorada no rodapé com altura reduzida
para preservar a área de preview.
Essas transformações alteram a transformação base relativa da mesh e continuam
sendo exportadas como dados do projeto.

Este documento registra o plano para um editor de sequências de interação entre
duas ou mais meshes. O caso inicial é a interação entre o rato e a máquina do
projeto Ratworks:

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

## Contexto atual

No protótipo do Ratworks, a interação é disparada manualmente pelo código:

1. o rato chega à máquina;
2. a máquina inicia `bite`;
3. o rato inicia `Eaten`;
4. a máquina e o rato são removidos ou avançam para a próxima ação quando os
   clips terminam.

O editor deverá permitir ajustar essa composição visualmente sem transformar
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

Cada mesh carregada no projeto é um ator identificado por um nome lógico. Os dois
slots atuais são genéricos (`actor1` e `actor2`); o editor não cria atores com os
nomes `rat` ou `machine` por padrão:

```lua
actors = {
    actor1 = { name = "rat", file = "Cyclopsv2.msh" },
    actor2 = { name = "machine", file = "ChompBot.msh" }
}
```

O identificador do slot é usado pelos eventos exportados; o nome definido pelo
usuário serve para identificar o ator na interface. O arquivo `.msh` continua
sendo um recurso independente e pode ser instanciado em qualquer posição da cena.
Na interface, o ator mostra apenas o basename da mesh, enquanto o caminho
completo permanece salvo no projeto. A escolha da mesh é feita pelo botão
`Browse`, que já carrega o arquivo.

### Relógio da interação

Uma interação possui um relógio principal configurável. O relógio pode ser
baseado no tempo de um clip específico, por exemplo:

```lua
clock = {
    actor = "machine",
    animation = "bite"
}
```

Nesse caso, um evento em `0.2` ocorre quando `machine:bite` atingir 0,2 s,
independentemente da posição absoluta da máquina na cena.

O editor também poderá oferecer um relógio livre da sequência para ações que
continuem depois do fim do clip principal. Esse período posterior deverá ser
explícito; não se deve silenciosamente aceitar tempos fora da duração do relógio.

### Base da interação

A base de cada ator é definida no tempo `0` da sequência:

- a posição e a rotação configuradas nos campos `POS` e `ROT` ou pelo gizmo;
- a animação selecionada no painel do ator, iniciada no tempo `0`;
- a escala configurada para o ator.

O `clipKey` selecionado é a animação base. Um evento `play` posterior pode
substituí-la a partir do seu próprio tempo. A âncora define os eixos usados para
interpretar os deslocamentos e rotações dos eventos.

No editor, essa base é a pose de referência da prévia. No runtime, a interação
deverá capturar a pose real dos objetos no momento em que começar e aplicar os
eventos relativos a essa pose; as coordenadas absolutas usadas na prévia não
devem limitar a posição da interação no mundo.

O fluxo atual para estabelecer a base é ajustar os atores no tempo `0`, escolher
as animações base e clicar em `Set base reference` na janela de atores. Essa
referência contém as posições, rotações e animações iniciais absolutas de cada
ator e é salva no projeto. A captura de transformações usa essa referência,
mesmo que a pose visual temporária tenha sido alterada durante a edição.

### Gatilhos

Os gatilhos podem observar qualquer ator ou o relógio principal:

- tempo do relógio principal;
- tempo de um clip de um ator;
- início de um clip;
- término de um clip não cíclico;
- evento manual ou callback Lua.

Exemplos conceituais:

```lua
{ when = { clock = 0.2 }, action = ... }

{ when = {
    actor = "machine",
    animation = "bite",
    time = 1.0
}, action = ... }

{ when = {
    actor = "rat",
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
        actor = "rat",
        transform = {
            position = { x = 0, y = -2, z = 0 },
            rotation = { x = 0, y = 20, z = 0 }
        }
    }
}
```

O runtime captura a transformação inicial do rato e aplica esse deslocamento
relativo. A sequência pode, portanto, ser usada quando o rato estiver em outra
posição ou orientação no mundo.

Na primeira versão, a transformação relativa será da mesh inteira. Transformar
ossos ou partes articuladas individualmente fica para uma etapa posterior.

### Captura de uma transformação

Para criar um evento visualmente, mova a timeline para o instante desejado,
selecione o ator e use o gizmo para alterar sua pose. O editor mantém a pose
inicial como base. O botão `Capture transform` cria um evento no tempo atual e
preenche automaticamente os deslocamentos e rotações relativos à base.

## Ações previstas

O primeiro conjunto de ações deverá incluir:

- iniciar, pausar, retomar ou trocar uma animação;
- aplicar transformação relativa;
- interpolar uma transformação durante um intervalo;
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
    clock = {
        actor = "machine",
        animation = "bite"
    },

    actors = {
        rat = { file = "Cyclopsv2.msh" },
        machine = { file = "ChompBot.msh" }
    },

    events = {
        {
            when = { clock = 0.2 },
            action = {
                actor = "rat",
                playSkeletalAnimation = "Eaten"
            }
        },
        {
            when = { clock = 1.0 },
            action = {
                actor = "rat",
                transform = {
                    position = { x = 0, y = -2, z = 0 },
                    rotation = { x = 0, y = 20, z = 0 }
                }
            }
        },
        {
            when = {
                actor = "rat",
                animation = "Eaten",
                ended = true
            },
            action = {
                actor = "rat",
                destroy = true
            }
        }
    }
}
```

O formato acima é ilustrativo. O editor poderá gerar uma forma mais compacta,
mas deve preservar a separação entre atores, relógio, gatilhos e ações.

## Exportação como módulo Lua

O resultado será um arquivo Lua carregável com `require`, e não um trecho que
precise ser copiado manualmente para `onLoop`.

A interface de runtime prevista é:

```lua
local reaction = require "interactions.chomp_eat"

local instance = reaction.start {
    rat = ratObject,
    machine = machineObject
}

-- No onLoop(delta):
instance:update(delta)
```

O módulo exportado deverá:

- receber as instâncias reais dos atores;
- capturar as poses iniciais;
- iniciar os clips configurados;
- observar tempos e callbacks de finalização;
- aplicar cada evento uma única vez;
- aplicar transformações relativas;
- permitir reinício e encerramento da interação.

Callbacks Lua continuarão disponíveis para efeitos que não sejam cobertos pelas
ações padrão do editor.

## Validação e erros do editor

O editor deve apresentar erro ou aviso claro nos seguintes casos:

- ator referenciado inexistente;
- arquivo ou clip não encontrado;
- tipo de animação incompatível com a ação solicitada;
- relógio baseado em clip inexistente;
- duração inválida, negativa ou não finita;
- gatilho além da duração do relógio sem período posterior explícito;
- gatilho de término aplicado a clip cíclico;
- transformação com valores não finitos;
- múltiplas ações conflitantes no mesmo ator e instante;
- evento que depende de uma animação que nunca foi iniciada.

No caso de `ChompBot/bite`, um gatilho em 1,0 s é válido quando o asset carregado
possui duração de 1,5 s. O editor não deve rejeitá-lo com base em uma duração
fixa ou em documentação antiga.

## Interface planejada

A primeira versão do editor deverá conter:

1. carregamento de duas meshes;
2. seleção do papel de cada ator;
3. seleção dos clips disponíveis;
4. escolha do relógio principal;
5. viewport com reprodução conjunta;
6. play, pause, avanço, reinício e scrub;
7. timeline de gatilhos;
8. edição de transformações relativas;
9. ações de troca de animação e callbacks;
10. validação visual e textual;
11. salvar e carregar o projeto do editor;
12. exportar o módulo Lua para `require`.

Todas as operações caras, como carregamento, reconstrução de prévia e
serialização, deverão acontecer somente quando os dados mudarem ou quando o
usuário solicitar uma ação explícita. Uma timeline parada não deve executar
`seek`, reconstruir meshes ou serializar o projeto continuamente.

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

1. Confirmar, no editor, os clips e suas durações nos dois assets do Ratworks.
2. Definir o formato final do projeto salvo pelo editor.
3. Implementar a prévia com relógio baseado em `ChompBot/bite`.
4. Implementar eventos de tempo e término de animação.
5. Implementar transformações relativas.
6. Exportar o primeiro módulo Lua executável no Ratworks.
7. Adicionar testes para tempos inválidos, clips cíclicos, reinício e eventos
   executados uma única vez.
