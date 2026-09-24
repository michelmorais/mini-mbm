# Image Mesh Editor — Plano do modo Manual curvo

Status: **Transição pelo interior entregue em 7.274.0 (marco 15).**
Data: **2026-09-23**

Este documento registra o contrato, a implementação inicial e as expansões do modo.
O uso da funcionalidade está documentado em [Image Mesh Editor](image-mesh-editor.md).

## Entrega inicial — 2026-09-23

- Espessuras totais positivas, distribuição simétrica por padrão ou verso plano.
- Alvo pontual ou circular concêntrico ao controle radial, com raio em unidades da malha.
- Interpolação radial linear; diagnóstico para centro fora do núcleo de visibilidade,
  círculo tocando/atravessando a borda e regiões com furos.
- Pico/anel do platô explícitos na triangulação; refinamento por amostras em centros
  e pontos médios dos triângulos. Tolerância padrão de 0,03 da diferença entre espessuras,
  ajustável entre 0,001 e 1. Não é um limite global de erro certificado.
- Campos opcionais `curvedX`, `curvedY`, `curvedRadius`, `curvedEdge`, `curvedTarget`
  e `curvedSymmetric`; `heightSource="curved"`. Projetos antigos recebem os defaults
  ao abrir; o formato permanece na versão 1. Leitores antigos não recebem garantia
  de aceitar os novos campos.
- Arraste, redimensionamento, Apply, cancelamento de arraste, bloqueio, histórico,
  persistência, prévia de alturas, geração assíncrona e exportação integrados.
- Pintura, áreas de altura, controles automáticos e modos antigos de verso ficam
  inativos, com seus dados preservados. Verso fechado com textura de origem e
  espelhamento UV opcional. A simplificação automática também fica inativa para
  preservar o pico e o platô; suporte a simplificação fica adiado.
- Não houve nova exposição de armazenamento interno: opções públicas continuam sendo
  valores de entrada, enquanto campo e triangulação permanecem em helpers privados.

**Evidências:** build Linux Debug/OpenGL ES; testes
`image_mesh_curved_smoke.lua` e `image_mesh_curved_editor_smoke.lua`, incluindo
fechamento/orientação da malha, normais, métricas, inversão, mapa, histórico, bloqueio,
cancelamento de arraste, salvar/reabrir, exportação e ausência de reconstrução ociosa.
O teste nativo inclui o contorno da serra como dados, sem depender da imagem local.
Os testes existentes de modelo, áreas manuais e montagem de módulos também passaram.

A serra real foi aberta em uma sessão de teste sem salvar sobre o original. Com
espessuras 1 e 8, centro (0,5; 0,5), raio 25 e resolução 24, gerou 6.158 vértices e
11.400 triângulos. A prévia foi inspecionada visualmente. Foi corrigida a perda de
precisão em raios que atravessam vértices oblíquos, que antes causava refinamento
excessivo; o contorno foi incluído na regressão.

Dois testes antigos continuam falhando e reproduzem a mesma falha com o código
anterior isolado em `/tmp`: `image_mesh_relief_smoke.lua:81` exige orçamento exato
incompatível com o limite conservador do refinamento; `image_mesh_back_smoke.lua:115`
trata a exportação assíncrona como retorno síncrono. Não foram alterados nesta entrega.
Windows, macOS e backends diferentes de OpenGL ES não foram executados.

## 1. Objetivo e requisitos definidos

Criar um modo de relevo **Manual curvo**, no qual a espessura varia do contorno
externo de uma região até um alvo interno controlado pelo usuário.

- O contorno externo recebe uma espessura determinada pelo usuário.
- Um ponto interno móvel determina onde a superfície termina de crescer ou diminuir.
- Uma área interna pode substituir o ponto: toda a área mantém a espessura final,
  formando um platô.
- A primeira função de transição é linear. O desenho deve permitir outros perfis,
  incluindo Bézier, posteriormente.
- A direção da variação pode ser invertida: o alvo pode ser mais grosso ou mais fino
  que o contorno.
- A textura continua fornecendo a aparência do objeto; sua luminosidade não deve
  determinar a transição deste modo.

Caso de referência local: `/home/michel/Downloads/sharp/sharp.imesh`, com a imagem
`rail_trap.png` na mesma pasta. O projeto possui uma região poligonal para a serra.
O círculo visível no centro pertence à textura, não a uma área geométrica de controle.
Esses arquivos locais não constituem uma fixture disponível em outros checkouts.

Resultados desejados para a serra:

| Configuração | Resultado |
|---|---|
| Contorno 1, ponto 8 | Crescimento linear até o ponto escolhido |
| Contorno 1, círculo 8 | Crescimento até a borda do círculo e interior plano em 8 |
| Contorno 8, alvo 1 | Variação invertida |
| Contorno e alvo iguais | Espessura uniforme |

O círculo deve ser ajustável sobre o desenho. A referência visual de aproximadamente
50% não fixa raio, diâmetro ou área percentual; preferir medidas explícitas na interface.

## 2. Modelo proposto

Separar três elementos:

1. **Domínio:** o contorno da região que receberá a extrusão.
2. **Alvo interno:** ponto ou círculo inicialmente; elipse e polígono ficam para expansão.
3. **Perfil de transição:** função que transforma o progresso espacial em espessura.

Usar os nomes “espessura no contorno” e “espessura no alvo”, evitando chamar o alvo
de máximo, pois ele também pode ser o mínimo. O comando de inversão troca os dois
valores sem deslocar o alvo.

### 2.1 Espessura e distribuição entre as faces

**Recomendação:** os valores apresentados neste modo representam a espessura total
final, em unidades da malha, e não um deslocamento somado à profundidade existente.

Oferecer duas distribuições:

- **Simétrica**, sugerida como padrão: para espessura `e`, faces em `-e/2` e `+e/2`
  em relação ao plano central.
- **Verso plano:** verso em um plano fixo e frente afastada desse plano pela espessura `e`.

Os modos atuais devem preservar sua semântica. Hoje, em
[`image-mesh.cpp`](../src/core_mbm/image-mesh.cpp), a frente usa
`z = -depth/2 - height`; o verso plano usa `z = depth/2`, e `backRelief` espelha a
frente. Portanto, a espessura é `depth + height` ou `depth + 2*height`, respectivamente.
O projeto de referência tem `depth = 20` e `relief = 8`; esses valores não representam
automaticamente a faixa de espessura total 1 a 8 desejada.

Decidir como apresentar os controles antigos de profundidade, relevo e verso quando
o novo modo estiver ativo. Evitar duas configurações concorrentes para a mesma grandeza.

### 2.2 Interpolação radial adotada

Para cada direção a partir do centro `C`, encontrar a distância até o contorno
externo `R`. O alvo pontual tem raio `a = 0`; o alvo circular tem raio `a > 0`.
Para um ponto à distância `r` do centro, fora do alvo:

```text
t = clamp((R - r) / (R - a), 0, 1)
espessura = espessura_contorno
          + (espessura_alvo - espessura_contorno) * perfil(t)
```

Dentro do alvo, usar diretamente a espessura do alvo. Tratar o centro explicitamente,
pois sua direção radial é indefinida. No perfil linear, `perfil(t) = t`.

Medir distâncias no plano da malha com sua proporção final, evitando que coordenadas
normalizadas de uma região retangular distorçam círculos e distâncias.

Essa proposta mantém o valor da borda em pontas e reentrâncias dos dentes. Usar apenas
uma distância ao centro com raio externo constante não garante essa propriedade.

**Condição de validade:** cada segmento do centro até o contorno deve permanecer no
domínio; o alvo deve caber estritamente dentro dele. Definir tolerâncias para tangências,
arestas, interseções em vértices e `R - a` próximo de zero. Não basta validar apenas
se o centro está dentro do polígono.

Uma serra pode ser côncava e ainda satisfazer essa condição para certos centros.
Deslocamentos do centro podem deixar de satisfazê-la. Validar o contorno real da fixture
antes de adotar este algoritmo como primeira entrega.

Se a condição falhar, a proposta inicial é apresentar diagnóstico e impedir a geração
inválida, preservando a última prévia válida e indicando que está desatualizada.
Não trocar silenciosamente para outro algoritmo. Uma solução geral sobre o interior
da malha poderá ser estudada depois, com comportamento próprio documentado.

### 2.3 Perfis previstos (entregues no marco 9)

Manter o progresso espacial separado da função de perfil. Todos os perfis devem
preservar os extremos: `perfil(0) = 0` e `perfil(1) = 1`.

- Linear: primeira entrega; mudança de inclinação no encontro com o platô é esperada.
- Suave: permitir chegada arredondada ao platô.
- Bézier: definir controles e restringir inicialmente a curva para evitar extrapolação
  ou inversões involuntárias de espessura.

A ferramenta pode ajudar na forma geral de um diamante, mas facetas exigem também
controle de triangulação e normais. Facetamento não faz parte da primeira entrega.

## 3. Decisões da primeira versão

| Tema | Decisão |
|---|---|
| Unidade dos valores | Espessura total; simétrica por padrão |
| Centro inicial | (0,5; 0,5); erro explícito se inválido, sem reposicionamento automático |
| Círculo | Compartilha o centro radial; raio em unidades finais da malha |
| Formas não radiais | Rejeitadas com diagnóstico |
| Furos | Recortes passantes desde o marco 12; não alteram o campo de espessura |
| Espessura zero | Não suportada; mínimo 0,001 |
| Pintura e áreas | Dados preservados e inativos neste modo |
| Travamento de borda | Inativo; o perfil já fixa a espessura externa |
| Materiais e verso | Verso fechado com textura de origem; modos antigos inativos |
| Simplificação | Opcional e com restrições preservadas desde o marco 11 |
| Persistência | Parâmetros por região com herança dos defaults, campos opcionais |
| Precisão | Refinamento por amostragem dentro do orçamento; círculo representado por cordas |

## 4. Integração prevista

| Área | Pontos de integração a revisar |
|---|---|
| Modelo e persistência | `editor/image_mesh_model.lua`, `editor/image_mesh_io.lua` |
| Controles e canvas | `editor/image_mesh_editor.lua`, `editor/image_mesh_areas.lua`, `editor/image_mesh_canvas.lua` |
| Prévia e geração | `editor/image_mesh_height_preview.lua`, `editor/image_mesh_generation.lua`, caches e orçamento |
| Opções e bindings | `include/core_mbm/image-mesh.h`, `src/lua-wrap/render-table/mesh-debug-lua.cpp`, `image-mesh-job.h` |
| Superfície e topologia | `src/core_mbm/private/image-mesh-height.h`, `image-mesh-topology.*`, `src/core_mbm/image-mesh.cpp` |
| Ajuda e traduções | `editor/image_mesh_help.lua`, `editor/lang/language.lua` |

Extrair a lógica coesa para helpers privados e módulos Lua pequenos conforme necessário.
Preservar a direção PIMPL; não expor caches, containers ou estado mutável interno em
headers públicos para viabilizar o recurso.

Prévia de alturas, geração e exportação devem compartilhar o cálculo de superfície.
Rever todos os caminhos que hoje derivam Z de `depth`/`relief`, incluindo laterais,
verso, estatísticas e simplificação. A alteração não pode se limitar à face frontal.

Salvar/carregar, duplicação de região e desfazer/refazer devem preservar o alvo e os
parâmetros. Projetos antigos devem abrir com o mesmo resultado anterior.

## 5. Etapas e entregáveis

### Etapa 1 — Fechar contrato e provar a geometria

- [x] Resolver as decisões que afetam a primeira entrega na seção 3.
- [x] Verificar o domínio radial da serra e os limites para deslocamento do centro.
- [x] Criar fixtures sintéticas reproduzíveis: círculo, polígono dentado e forma inválida.
- [x] Prototipar o campo linear pontual e circular; definir tolerâncias mensuráveis.
- [x] Definir esquema persistido e compatibilidade sem alterar projetos do usuário.

### Etapa 2 — Gerar a superfície e a malha

- [x] Implementar validação e avaliação compartilhada do perfil.
- [x] Garantir vértice no alvo pontual e representação explícita da borda do platô.
- [x] Refinar a superfície conforme erro e orçamento; o caso linear também deve ser
  verificado, pois a triangulação pode aproximar mal a função radial.
- [x] Fechar laterais e calcular normais nas duas distribuições de espessura.
- [x] Proteger pico, contorno e transição do platô no simplificador (marco 11, 7.270.0).
- [x] Integrar cancelamento e progresso aos trabalhos assíncronos existentes.

### Etapa 3 — Integrar a edição

- [x] Adicionar o modo Manual curvo e controles numéricos de espessura e alvo.
- [x] Permitir arrastar o centro e redimensionar o círculo sobre a textura.
- [x] Mostrar contorno, alvo, diagnóstico de validade e estado de prévia desatualizada.
- [x] Integrar Apply, salvar/carregar, duplicação e desfazer/refazer.
- [x] Incluir ajuda e traduções em português e inglês com pontuação segura para ImGui.
- [x] Auditar os caminhos de `onLoop`: nenhum recálculo de campo, triangulação, upload
  ou varredura completa deve ocorrer continuamente com o editor ocioso.
- [x] Durante arrastes, invalidar por mudança e limitar a cadência de trabalhos;
  impedir que resultados de trabalhos antigos substituam a configuração atual.

### Etapa 4 — Validar e documentar a entrega

- [x] Executar testes matemáticos, de modelo, integração e exportação apropriados.
- [x] Validar visualmente a serra no editor real seguindo a skill `engine-testing`.
- [x] Registrar cobertura por backend; não presumir validação em plataformas não executadas.
- [x] Atualizar `docs/image-mesh-editor.md` e `docs/lua-api.md` conforme a API entregue,
  aplicando `doc-drift-check`.
- [x] Verificar a fronteira interna/pública: sem mudança de propriedade; ledger PIMPL não requer alteração.
- [x] Atualizar `include/version/version.h` na entrega da funcionalidade.
- [x] Atualizar este plano com decisões finais, evidências e limitações restantes.

## 6. Critérios de aceitação

Critérios originais abaixo. A primeira entrega usa a tolerância amostrada registrada acima;
a preservação pelo simplificador fica adiada com a simplificação desativada.

1. Contorno completo com espessura 1, incluindo pontas e reentrâncias da serra.
2. Alvo pontual com espessura 8 e valores intermediários conformes ao progresso radial.
3. Círculo interno inteiramente plano em 8; transição alcançando sua borda sem degrau.
4. Inversão 8 para 1 e valores iguais funcionando sem cruzamento entre faces.
5. Distribuição simétrica mantendo o plano central; distribuição com verso plano
   mantendo o plano do verso fixo.
6. Centro deslocado válido produzindo a variação esperada; posições inválidas e alvo
   tangente/externo produzindo diagnóstico determinístico.
7. Regiões não quadradas mantendo a métrica definida para o alvo circular.
8. Malha fechada, nas configurações fechadas suportadas, sem triângulos degenerados,
   rachaduras no platô ou erros de orientação; normais coerentes com a superfície.
9. Simplificação respeitando o erro acordado e preservando as restrições geométricas.
10. Prévia e exportação concordando dentro da tolerância, inclusive após reabrir o projeto.
11. Projetos antigos e modos imagem/manual/misto mantendo seus resultados anteriores.
12. Editor ocioso sem reconstruções repetidas; cancelamento e alterações rápidas sem
    instalar resultados obsoletos ou ultrapassar os limites de geometria.

## 7. Expansões posteriores

Composição com pintura e áreas;
formas que não admitem interpolação radial (próxima prioridade acordada);
facetamento com regiões locais independentes, edição manual de arestas e padrões
de lapidação adicionais.
Cada expansão deve definir seu comportamento antes de entrar na implementação.

## 8. Contrato consolidado — cadeias e regiões locais (2026-09-23)

Aprovado na continuação do desenho: a hierarquia ramifica por **regiões locais**;
cada forma continua tendo no máximo **um alvo principal**. Essa etapa substitui a
proposta de ajustes ordenados por prioridade. A ordem dos irmãos não define o resultado.

- A raiz é o contorno do módulo, com espessura própria. Sem alvo, permanece plana.
- Um alvo fechado recebe uma espessura no seu contorno. Sem outro alvo, seu interior
  fica plano; com alvo, passa a transicionar até ele. A cadeia pode subir e descer.
- Ponto e segmento de linha são alvos terminais. Uma linha determina uma crista/vale
  finito; uma faixa poligonal pode ser usada para largura e alvos internos.
- Cada forma fechada admite regiões locais independentes. A borda local herda a
  superfície do proprietário antes de aplicar essa região. Sem alvo, a região é neutra.
- A transição local parte dos valores herdados na borda até a espessura de seu alvo.
  Fora da região, a superfície permanece inalterada. A distribuição simétrica ou de
  verso plano é compartilhada pelo módulo.
- Pais são explícitos. Não existem ciclos, alvos compartilhados nem dois alvos para
  o mesmo proprietário. Regiões independentes não se cruzam, tocam ou contêm umas às
  outras; encaixes exigem declarar a dependência. Um alvo principal descreve a base
  e não disputa prioridade com uma região local aplicada sobre essa base.
- Todas as formas ficam estritamente dentro do proprietário. A implementação linear
  inicial exige que o alvo esteja no núcleo de visibilidade do contorno do proprietário,
  para que a ligação por segmentos não saia da forma. Alvos fechados são convexos;
  regiões locais podem ser côncavas. Falhas produzem diagnóstico.
- Para um alvo estendido, projetar o ponto da superfície no alvo mais próximo e seguir
  a direção para fora até o contorno do proprietário. Interpolar linearmente entre essa
  borda e o alvo; dentro de um alvo fechado, avaliar sua própria cadeia e regiões.
- Pontos, linhas e contornos dos controles devem integrar a triangulação explicitamente;
  uma grade de amostragem sozinha pode perder uma crista ou detalhe estreito.

### Persistência e compatibilidade

Adicionar `curvedNodes` por módulo: lista com proprietário, papel (alvo/região),
contorno normalizado e espessura do alvo. Lista ausente mantém o caminho ponto/círculo
7.266.0; lista vazia significa raiz plana. Conversão explícita preserva os parâmetros
antigos e permite desfazer. Círculos/elipses da hierarquia são contornos poligonais.
A geração assíncrona deve possuir uma cópia de todos os novos dados emprestados.

### Entregáveis desta etapa

- [x] Campo hierárquico com alvo em segmento, ponto e área fechada.
- [x] Cadeia serra 1 -> círculo 8 -> círculo 3; região terminal plana.
- [x] Espada com linha principal e dois sulcos locais independentes.
- [x] Validação de parentesco, contenção, visibilidade e sobreposição.
- [x] Triangulação conforme aos controles, com refinamento e orçamento limitados.
- [x] Interface para criar, selecionar, editar e remover formas/dependências; canvas,
      Apply, histórico, persistência, duplicação e exportação.
- [x] Testes de continuidade na borda herdada, ordem dos irmãos, fechamento, cancelamento,
      cópia assíncrona, compatibilidade e ociosidade; documentação e versão atualizadas.

Na entrega 7.267.0, Bézier, linha com vários segmentos, composição por sobreposição e
simplificação ficaram posteriores. Os perfis foram entregues no marco 9 abaixo.

### Evidência da entrega 7.267.0

- `image_mesh_curved_hierarchy_smoke.lua`: linha, cadeia de platôs, dois sulcos,
  bordas herdadas, ordem dos irmãos, verso plano, malha fechada/normais, validação,
  raiz vazia e cópia assíncrona/exportação.
- `image_mesh_curved_hierarchy_editor_smoke.lua`: mover/redimensionar descendentes,
  editar extremidade de linha, cancelar, histórico, Apply, salvar/reabrir, mapa,
  exportação e painel ImGui ocioso sem reconstruções.
- Conversão é explícita e usa projeção no alvo mais próximo; não promete igualdade
  geométrica com o círculo radial legado. Verso plano da hierarquia fica em Z=0.

Regressões `image_mesh_curved_smoke.lua` e `image_mesh_curved_editor_smoke.lua`
aprovadas. Projeto real `sharp.imesh` gerado em memória no modo legado, após
conversão para ponto e com dois círculos encadeados; arquivo original não alterado.

Builds `mini-mbm` e `testLib` aprovados. A malha exportada pelo teste hierárquico
foi carregada em 3D pelo `testLib` com encerramento normal após 3 segundos.

## 9. Perfis por alvo — entregue em 7.268.0

Cada alvo passa a definir o perfil da ligação recebida do proprietário. Perfil
omitido equivale a `linear`, preservando projetos existentes. Regiões locais mantêm
a borda herdada; o perfil fica no alvo da região. Cada ligação é independente.

- Linear: `f(t)=t`.
- Suave: `f(t)=t*t*(3-2*t)`, com derivada zero nos extremos do perfil normalizado.
- Bézier cúbica: extremos `(0,0)` e `(1,1)`; controles `(1/3,b1)` e `(2/3,b2)`.
  Manter cada controle em `[0,1]` garante progressão monotônica e sem ultrapassagem
  das espessuras dos extremos. X fixo permite edição por dois controles de curvatura.
- Aplicar `f` à progressão espacial já calculada. Não modificar contornos, hierarquia,
  contenção ou regras de sobreposição. Suave não promete suavidade global nos cantos
  da forma nem entre regiões cujo valor herdado varia ao longo da borda.
- UI por alvo, prévia normalizada da curva, controles Bézier e explicação do sentido
  borda -> alvo. Valores ficam no projeto, participam do histórico e cópia assíncrona.
- Refinamento continua usando o campo final e tolerância existente; testar perfis
  em subida/descida, cadeia mista e regiões independentes, mapa, exportação e ociosidade.

Entrega concluída. Furos, sobreposição e simplificação continuam posteriores.

Validação:

- Build `mini-mbm` e `testLib` aprovado.
- `image_mesh_curved_profiles_smoke.lua`: suave e Bézier, subida e rebaixo,
  cadeia mista, regiões independentes, bordas herdadas, controles inválidos,
  fechamento/normais, mapa e snapshot assíncrono dos perfis.
- `image_mesh_curved_profiles_editor_smoke.lua`: histórico, persistência dos controles,
  prévia Bézier em ImGui, mapa/exportação e cache da curva estável durante ociosidade.
- Regressões `image_mesh_curved_smoke.lua` e `image_mesh_curved_hierarchy_smoke.lua`
  aprovadas; omitir perfil continua equivalendo a linear.
- Malha Bézier exportada carregada em 3D pelo `testLib`, com saída normal.
- Projeto real `sharp.imesh` gerado em memória nos três perfis, sem alterar o arquivo.

### Ajuste 7.268.1: controles Bézier independentes

Removida a restrição conservadora `b1 <= b2`. Com os extremos fixos e X dos
controles em 1/3 e 2/3, cada Y em [0,1] já preserva progressão monotônica e limites
de espessura. A UI permite cruzamento vertical; a API e o projeto validam os limites
individualmente. Testes incluem (0.8,0.2), (1,0), limites inválidos, histórico e persistência.

## 10. Bézier com 2, 3 ou 4 controles internos — 7.269.0

Decisão: manter todos os controles em [0,1]; a proposta de ampliar esse limite foi
abandonada antes da entrega. Acrescentar radiobuttons 2/3/4 por alvo, além dos dois
extremos fixos. Projetos existentes usam 2, com a mesma avaliação cúbica anterior.

Com `k` controles internos, o grau é `k+1` e as posições X são `i/(k+1)`.
Os controles continuam independentes. Com 3/4, podem criar ondulações internas,
sem ultrapassar as espessuras dos extremos. Não há garantia de monotonicidade
nesses dois novos modos. O motor avalia por de Casteljau sem alocar por amostra.

Aumentar a quantidade preserva a curva por elevação de grau. Reduzir aproxima o
polígono de controle; a interface explica que a curva pode mudar. Contagem e novos
valores participam de Apply, histórico, persistência, cópia assíncrona e exportação.

Validação: testes nativos de graus 4/5, ondulação sem ultrapassagem, limites dos
controles adicionais e snapshot assíncrono; testes do editor de elevação de grau,
radiobuttons 2/3/4, histórico, persistência, mapa, exportação e cache ocioso.

## 11. Simplificação protegida — 7.270.0

Opção separada do simplificador geral, desligada por padrão. `curvedSimplify`,
`curvedSimplifyRatio` (0.01..1, padrão 0.5) e `curvedSimplifyError`
(0.0001..0.25, padrão 0.01) participam de Apply, histórico, persistência,
snapshot assíncrono e exportação. Nenhum projeto antigo ativa a redução sozinho.

A redução remove vértices internos e retriangula a cavidade antes da extrusão.
Preserva vértices do contorno, dos alvos e regiões locais, pico/círculo legado e
extremos locais amostrados. Vértices mantidos não se deslocam; normais são recalculadas.
Cada substituição propaga um limite conservador do erro relativo à malha densa
original, comparando as superfícies lineares nas interseções dos triângulos.
A tolerância adicional multiplica a variação total de espessura; não substitui
nem certifica a tolerância amostrada da geração inicial.

A fração é uma meta: restrições, erro, precisão float, cavidades com mais de
32 triângulos ou limite de 32 passagens podem interromper a redução antes.
O resultado parcial é válido e a UI informa isso. A malha densa ainda precisa
caber no orçamento antes de simplificar. Mapas continuam usando o campo analítico.
Comparação lado a lado do simplificador geral permanece fora deste marco.

Relatório específico: faces da superfície antes/depois, limite conservador de erro
e indicador de meta alcançada. Cancelamento é verificado durante o processamento;
nenhum trabalho de simplificação ocorre continuamente no editor ocioso.

Validação: builds `mini-mbm` e `testLib`; testes nativo e ImGui específicos de
simplificação (contorno, platôs encaixados, linha Bézier com ondulação, regiões
independentes, verso plano, limite conservador, redução parcial, parâmetros inválidos,
mapa inalterado, snapshot/cancelamento, histórico, persistência, exportação e ociosidade).
Regressões nativas do modo legado, hierarquia e perfis aprovadas. Malha exportada
carregada em 3D no `testLib`. Projeto `sharp.imesh` lido e gerado em memória, sem
alterar o arquivo: 7.381 para 4.471 faces de superfície, limite normalizado de erro
0,009973 para tolerância 0,01.

## 12. Furos no relevo curvo — 7.271.0

Prioridade escolhida pelo usuário após o marco 11. Furos são recortes passantes da
superfície calculada, sem alterar o campo de espessura. Alvos continuam sendo
controles virtuais e podem ficar dentro dos recortes; limites de alvos e regiões
podem ser atravessados pelos furos. A hierarquia continua válida sobre o contorno
externo completo. Furos não viram novos alvos nem novos pontos de espessura.

Reutilizar `holes` e o painel Furos: até 16 contornos simples com 3..128 pontos,
estritamente internos, sem contato, sobreposição ou encaixe entre furos. Criar as
paredes internas com a espessura local e os materiais de lateral existentes.
Recortar frente e verso nas distribuições simétrica e verso plano. Proteger todas
as bordas dos furos na simplificação; mapas ficam transparentes no interior removido.
Projetos sem furos mantêm o caminho anterior. Não editar arquivos do usuário.

- [x] Recorte conforme à triangulação e refinamento da superfície restante.
- [x] Integração com paredes, normais, simplificação e mapa de alturas.
- [x] Painel, diagnóstico, histórico, persistência, prévia e exportação.
- [x] Testes de furos cruzando alvos/linhas, múltiplos furos, validação,
      simetria/verso plano, fechamento, cancelamento, regressão e ociosidade.

Implementação privada: a geometria curva completa recebe os segmentos dos furos,
os triângulos internos são removidos, as bordas são reconstruídas pelas arestas
restantes e a superfície é refinada novamente. A geração inteira, inclusive
intermediários, respeita o orçamento; recortes não garantem caber em um orçamento
no qual a superfície completa não caberia. As bordas recebem paredes e proteção
na simplificação. A proteção do pico legado usa sua posição, pois o recorte pode
remover o centro e reordenar os vértices.

Validação: builds `mini-mbm`/`testLib`; testes nativos de platô, pico removido,
alvos encaixados/atravessados, linha Bézier, verso plano, raiz plana, 16 furos,
contornos côncavos/elípticos, materiais laterais, fechamento, Euler, área e volume,
mapa transparente, snapshot assíncrono e cancelamento sem resultado parcial.
Teste ImGui de movimento/redimensionamento, rejeição de posição inválida,
histórico, salvar/reabrir, exportação, mapa e ociosidade aprovado. Regressões do
modo legado, perfis, simplificação e furos dos outros modos aprovadas. Projeto
`sharp.imesh` testado com furo central em memória, sem alterar o original.
A cópia da serra com furo central e simplificação foi exportada em `/tmp` e
carregada em 3D pelo `testLib`, com encerramento normal após 3 segundos.

## 13. Facetamento automático — 7.272.0

Escolha do usuário: lapidação automática por setores e anéis, antes de edição
manual de arestas. Primeiro marco: perfil legado de ponto/círculo e contorno
convexo, sem hierarquia. Ponto forma um pico; raio forma um platô poligonal central.
O contorno poligonal conserva os cantos, adicionando setores quando necessário;
elipses usam a contagem de setores na aproximação do contorno.

Campos opcionais: `curvedFaceted=false`, `curvedFacetSectors=8` (8..128),
`curvedFacetRings=1` (1..16). Anéis uniformes dividem a transição linear; planos
coplanares não ganham uma aresta visual artificial. Não é uma simulação de corte
brilhante gemológico. Mantém espessuras totais, centro móvel e verso simétrico/plano.

A superfície é linear por triângulo, compartilhada por mapa, geometria e recortes.
Furos subdividem os planos sem arredondar a lapidação. Normais independentes por
triângulo, UVs e materiais preservados. O orçamento inclui vértices duplicados nas
arestas duras. Simplificação fica inativa, com a configuração salva preservada.
Grade e tolerância adaptativa ficam inativas; setores/anéis controlam a geometria.

- [x] Gerar a superfície facetada e compartilhar a avaliação com mapa/furos.
- [x] Normais planas, materiais, verso e orçamento de vértices duplicados.
- [x] UI, ajuda, validação, histórico, persistência, geração assíncrona e exportação.
- [x] Testar planos/normais, contorno, recortes, mapa, compatibilidade e ociosidade.

Validação: builds `mini-mbm` e `testLib`; teste nativo de normais por face,
fechamento/Euler/volume, setores/anéis, pico/platô, centro deslocado, inversão,
espessuras iguais, verso plano, recortes coplanares, materiais, mapa coerente com
a geometria, limites de entradas e orçamento com vértices duplicados, salvar/carregar,
snapshot assíncrono e cancelamento. Teste ImGui de controles, histórico, persistência,
prévia de alturas/exportação e ociosidade aprovado. Regressões do editor de perfis,
geração de malhas por imagem e furos curvos aprovadas.
A malha facetada exportada pelo teste do editor foi carregada em 3D pelo `testLib`,
com encerramento normal após 3 segundos.


## 14. Facetamento com hierarquia de alvos — 7.273.0

Escopo acordado: cadeia de polígonos convexos encaixados, com espessura por alvo,
platô final ou ponto terminal. Regiões locais e linhas ficam fora deste modo.
O contorno externo também deve ser convexo. Hierarquia vazia gera superfície plana.

O centro comum é a média dos vértices do último polígono ou o ponto terminal.
Raios passam pelos cantos de todos os polígonos e pelos setores mínimos. Anéis
subdividem cada ligação. As bordas são compartilhadas entre as transições, sem
fendas; cada alvo conserva sua espessura. Mover um alvo pode alterar a triangulação
da cadeia. Perfis suave/Bézier ficam preservados, mas inativos no facetamento;
a altura varia linearmente entre os alvos. Platôs internos não recebem anéis extras.

Furos recortam os planos existentes. Mapa, verso, materiais, orçamento de normais
independentes, geração assíncrona e persistência mantêm o contrato do marco 13.
A conversão para hierarquia fica disponível com facetamento ativo. Botões para
adicionar regiões locais e alvos em linha ficam desabilitados nesse modo.
Validações anteriores de contenção, quantidade e profundidade continuam aplicáveis.

Próximo marco prioritário, por escolha do usuário: formas que não admitem
interpolação radial, com definição do contrato de transição interna e alvos em
polilinha antes da implementação. Pintura e outras expansões ficam depois.


Validação do marco 14: builds `mini-mbm` e `testLib` aprovados; testes nativos
verificam alturas nas bordas de alvos com formatos/centros diferentes, mesa e pico,
transições ascendentes/descendentes, normais, fechamento/Euler/volume, furos,
mapa coerente, verso plano, exportação, entradas não suportadas e execução assíncrona.
Teste ImGui cobre conversão de círculo (incluindo direções quase coincidentes),
histórico, preservação de Bézier, salvar/reabrir, mapa, exportação e ociosidade.
Regressão dos perfis sem facetamento aprovada. Malha exportada carregada em 3D
pelo `testLib`, com encerramento normal após 3 segundos. Interação manual ainda
requer a validação visual do usuário.


## 15. Transição pelo interior — 7.274.0

Escolha confirmada pelo usuário: superfície suave calculada dentro da peça,
fixando alturas de borda e alvo; não há promessa de crescimento linear radial.
Opção `curvedInterior=false` por padrão. Primeiro escopo: hierarquia vazia (plana)
ou um único alvo raiz em ponto, segmento ou `shape="polyline"` com 2..128 vértices.
Polilinhas são abertas, sem cruzamentos, retornos sobre si mesmas ou contato com
bordas. Alvos fechados, regiões locais, múltiplos alvos e facetamento ficam fora.

A triangulação respeita contorno côncavo, furos e segmentos do alvo. Um Laplaciano
discreto de grafo, com pesos positivos inversos ao comprimento no espaço da peça,
resolve as alturas internas por gradientes conjugados precondicionados. Bordas
externas e de furos têm `curvedEdge`; o alvo tem sua espessura. Nenhuma aresta cruza
vazios. Mapa e malha compartilham o campo linear por triângulo, com normais suaves.
É uma aproximação dependente da resolução, não uma superfície globalmente C1.

Colunas/linhas controlam a resolução. Tolerância adaptativa, perfis radiais e
simplificação ficam inativos, mantendo os valores salvos. Furos participam do
cálculo e alteram as alturas ao redor; alvos não podem atravessá-los. Verso plano
em Z=0 ou simétrico, materiais, orçamento, cancelamento e exportação permanecem.
Falha de convergência deve retornar diagnóstico sem malha parcial.

O editor permite criar a hierarquia vazia e adicionar a polilinha em posição
inicial válida dentro do contorno. Edição de vértices rejeita segmentos inválidos
com mensagem. Preservar o caminho aberto no desenho, edição, histórico e arquivo.
Cálculo e busca da posição inicial só ocorrem em ações; nada é resolvido por frame.


Validação: build Linux Debug (`mini-mbm` e `testLib`) aprovado. Testes nativos
cobrem U/S, dimensões não quadradas, polilinha/ponto/segmento, hierarquia vazia,
alturas fixas, malha fechada, ausência de faces nos vãos, coerência do mapa,
furos como bordas fixas, inversão, verso plano, limites, autocruzamentos e retornos,
snapshot assíncrono e cancelamento sem resultado parcial. Teste ImGui cobre posição
inicial válida, edição/rejeição com mensagem, histórico, salvar/reabrir, mapa,
exportação e ociosidade. Regressões de facetamento e perfis radiais aprovadas.
Malha exportada carregada em 3D no `testLib` por 3 segundos, encerrando normalmente.


## Correção 7.274.1 — vincos artificiais na hierarquia curva

Investigação do `sharp.imesh` confirmado pelo usuário: sem simplificação, alturas
nos vértices concordavam com o campo, mas a inserção de alvos seguida apenas por
bisseção da maior aresta preservava triângulos muito estreitos da triangulação
inicial. A média uniforme das normais de faces amplificava os vincos visuais.

Correção: trocas locais de diagonais durante o refinamento, protegendo segmentos
de alvos/regiões e contornos, sem mover amostras. Normais da hierarquia radial são
ponderadas pelo ângulo do canto, mantendo a preferência dos platôs e simetria do
verso. Facetamento e transição pelo interior mantêm seus caminhos próprios.

No projeto original, com as mesmas curvas, grade 24 x 24, tolerância 0,03 e
simplificação desativada: 15.436 para 6.464 triângulos totais. Faces de superfície
com área projetada / maior aresta ao quadrado menor que 0,001: 447 para 10.
A redução é consequência da melhor triangulação, não de simplificação habilitada.
Comparação lado a lado no renderizador com iluminação idêntica confirma a remoção
dos vincos finos artificiais. Ondulações determinadas pelos dentes, perfis e
mudanças de inclinação continuam fazendo parte do relevo. Projeto do usuário
permaneceu sem alterações; reproduções e imagens de comparação foram salvas em `/tmp`.

Teste de regressão independente: `image_mesh_curved_quality_smoke.lua`, com
contorno dentado, alvo circular Bézier, alvo retangular interno e furo. Verifica
qualidade de triângulos, alturas nas bordas dos alvos, fechamento/Euler/volume,
normais angulares e exportação. Testes de perfis, furos e simplificação também
exercitados. A soma de área no teste de furos usa compensação numérica para o Lua
float32: a soma ingênua de milhares de faces produzia 9999,922 para área real 10000;
a tolerância existente foi preservada.
