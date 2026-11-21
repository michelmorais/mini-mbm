# Prompt de implementacao: sistema de attachments e constraints

Voce esta trabalhando no repositorio `/home/michel/mini-mbm`.

Implemente a primeira fase do sistema separado de attachments e constraints,
seguindo o plano em:

```text
docs/attachments-constraints-plan.md
```

Antes de alterar arquivos, leia `AGENTS.md`, o plano acima e os documentos
relacionados ao ciclo de vida de plugins, meshes, animacao skeletal e editores.

## Objetivo

Criar um sistema reutilizavel que mantenha um objeto alvo anexado a uma origem
em tempo real, sem depender do `interaction_animation_runtime`.

O primeiro caso suportado deve ser um attachment entre transforms raiz de
objetos/meshes existentes. A origem pode continuar sendo movimentada pelo
gameplay, fisica, animacao ou outro sistema. O attachment deve apenas ler a
origem e escrever o alvo.

## Separacao obrigatoria

Nao transforme o editor ou o runtime de interacao de animacao em um sistema de
attachments.

Nao adicione `follow_actor`, `stop_follow_actor` ou `externalTransform` ao
plugin de interacao. O editor de interacao pode ser consultado somente como
referencia de UX, fluxo de autoria, preview e integracao Lua/C++.

O novo plugin deve possuir ciclo de vida, API, estado e validacao proprios.

## Escopo da primeira entrega

Implemente somente:

- attachment entre origem e alvo como transforms raiz;
- offset local de posicao, rotacao e escala;
- heranca independente de posicao, rotacao e escala;
- escala da origem afetando a distancia do offset quando habilitada;
- `attach`, `detach` e atualizacao automatica por frame;
- detach preservando a pose mundial por padrao;
- origem controlada externamente;
- deteccao de referencias invalidas, ciclos e conflitos de escrita;
- limpeza segura quando origem, alvo ou cena forem destruidos;
- API Lua e documentacao inicial;
- testes ou smoke tests proporcionais ao novo comportamento.

Deixe ossos, sockets, IK, constraints de aim, mola, amortecimento e fisica
para fases posteriores, salvo se a investigacao mostrar que algum suporte
minimo e indispensavel para o transform raiz.

## Regras de comportamento

- A origem e somente leitura.
- Componentes nao herdados nao podem ser sobrescritos pelo attachment.
- A composicao deve ocorrer no espaco local da origem.
- A rotacao da origem deve orientar o offset de posicao.
- A escala da origem deve escalar o offset de posicao quando `scale` estiver
  habilitado.
- O estado interno deve usar uma representacao adequada para composicao de
  transforms, preferindo matrizes ou quaternions quando necessario.
- A conversao para Euler deve ocorrer apenas na fronteira que exige angulos.
- Dois attachments nao podem escrever o mesmo componente do mesmo alvo sem
  uma politica explicita e validada.
- Ciclos no grafo de attachments devem ser rejeitados.
- A atualizacao deve ocorrer depois que a origem recebeu a pose do frame e
  antes da renderizacao.
- O sistema nao deve depender de um callback Lua por frame para aplicar a
  transformacao essencial.

## Decisoes que devem ser documentadas antes da implementacao

Se alguma decisao ainda nao estiver definida no repositorio, registre-a em um
documento ou na documentacao da API antes de codificar:

- nome final do plugin e do modulo Lua;
- formato dos offsets e unidades de angulo;
- politica para origem ou alvo destruidos;
- politica para conflitos de escrita;
- semantica de `detach`;
- forma de registrar e resolver a ordem das dependencias;
- fronteira entre API de runtime e dados de autoria do editor.

Nao escolha silenciosamente uma semantica que altere o escopo do plano.

## Editor

O novo editor deve ser separado do editor de interacao. Nesta primeira fase,
priorize o contrato de dados e o runtime; se o editor completo aumentar o
escopo, crie primeiro um esqueleto pequeno que permita:

- selecionar origem e alvo;
- editar posicao, rotacao e escala do offset;
- escolher os componentes herdados;
- visualizar o attachment;
- capturar novamente o offset;
- salvar e carregar uma definicao.

Reutilize padroes de UX do editor de interacao quando forem adequados, mas nao
reutilize o estado interno nem crie dependencia entre os dois editores.

## Compatibilidade e qualidade

- Preserve o comportamento de todos os plugins e projetos existentes.
- Nao introduza estado mutavel novo no core publico sem justificativa.
- Prefira manter o estado no plugin ou em estruturas privadas.
- Siga as convencoes de C++, Lua, CMake, licenca e logging do repositorio.
- Atualize `include/version/version.h` se a entrega representar uma nova
  funcionalidade publicada.
- Atualize a documentacao afetada, incluindo a API Lua quando aplicavel.
- Use uma fase de validacao clara para rejeitar configuracoes ambiguas.

## Verificacao obrigatoria

Antes de concluir, verifique pelo menos:

1. origem movida externamente atualiza o alvo no mesmo frame adequado;
2. posicao, rotacao e escala podem ser herdadas separadamente;
3. escala herdada altera a distancia do offset;
4. componentes nao herdados permanecem sob controle externo ou do jogo;
5. detach preserva a pose mundial;
6. cadeias aciclicas funcionam;
7. ciclos e conflitos sao rejeitados;
8. destruicao de origem, alvo e cena nao deixa referencias invalidas;
9. projetos que nao usam o novo plugin continuam funcionando;
10. a verificacao visual do editor, se implementada, corresponde ao runtime.

Ao final, apresente um resumo dos arquivos alterados, da API criada, das
decisoes tomadas, dos testes executados e das limitacoes que permanecem para
as fases de ossos, sockets e constraints avancadas.

