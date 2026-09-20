# Processamento de normais no Mesh Debug

A tabela de normais oferece a escolha **Processamento de normais**. A seleção
vale para os botões **Aplicar** de cada vértice e **Aplicar método em todas** do
subset. O escopo da tabela continua sendo o frame 1. O padrão é reparação.

- **Reparar normais inválidas**: preserva exatamente vetores finitos, não nulos,
  com comprimento dentro de 0,001 de 1 e orientação compatível com a média
  geométrica (produto escalar positivo). Vetores com direção compatível mas
  comprimento incorreto são normalizados, sem substituir sua direção. Normais
  nulas, não finitas ou invertidas são substituídas pela média geométrica quando
  existe uma direção calculável. Sem essa referência, vetores finitos não nulos
  ainda podem ter seu comprimento reparado; normais irrecuperáveis ficam intactas.
- **Recalcular - suavização uniforme**: substitui os vetores pela média normalizada
  das normais unitárias das faces adjacentes. Esse método sobrescreve normais
  personalizadas e pode alterar o sombreamento de uma mesh do Image Mesh Editor.

A validação de orientação é uma convenção baseada no winding e não prova que uma
normal aponta para fora de qualquer sólido. O teste não inverte o resultado em
função da declaração CW/CCW. Triângulos degenerados não fornecem direção útil.
Listas de triângulos indexadas e não indexadas são suportadas; vértices não são
fundidos por posição. Reparação não reconstrói a classificação de patamares que
foi usada pelo Image Mesh Editor. Um arquivo já recalculado com normais válidas
não recupera automaticamente o sombreamento anterior.

## Operações em lote

A janela **Aplicar a todos** tem sua própria seleção de método. O botão de
processamento percorre todos os frames e subsets dos arquivos do tipo escolhido.
Normais preservadas/inalteradas não marcam a entrada como modificada. Entradas
sem normais são ignoradas pelo processamento; **Adicionar normais** serve para
criá-las e ignora arquivos que já possuem normais, evitando sobrescrita acidental.

**Salvar todos (método de normais selecionado)** aplica a política selecionada
antes de salvar com o recálculo implícito da engine desabilitado. Se não houver
normais, cria-as e calcula suavização uniforme, pois não há vetores personalizados
a preservar. Arquivos que não usam TRIANGLES são ignorados nessa operação.
O salvamento comum continua preservando os vetores atuais. Falhas são registradas
por item e não interrompem os demais arquivos do lote.

A seleção do método não altera a geometria nem aplica operações por si só. Ela é
estado temporário da sessão. A computação geométrica da tabela permanece em cache;
os loops completos de processamento executam apenas ao acionar um botão. Não há
novo trabalho de varredura de geometria contínuo no editor ocioso.

## Verificação

- `src/test-lib/mesh_debug_normals_test.lua`: política pura de reparação,
  normalização, NaN/infinito, orientação e substituição uniforme.
- `src/test-lib/mesh_debug_normals_smoke.lua`: dois frames com dois subsets,
  preservação de normais personalizadas, operação idempotente, salvamento/reabertura,
  processamento uniforme, falha isolada no lote, triângulos não indexados e UI.

O modo de reconstrução geométrica que preserva regiões de superfície ainda não
está implementado. Não se presume que ponderação uniforme reproduza normais
artisticamente editadas ou geradas com conhecimento do mapa de alturas.
