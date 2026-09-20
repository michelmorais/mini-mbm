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
- **Recalcular - preservar superfícies**: identifica regiões de faces conectadas
  por arestas, agrupando vizinhas cujo ângulo entre normais não excede o limite
  escolhido (25 graus por padrão; ajustável de 1 a 89). Nas fronteiras entre
  regiões, prioriza a região com maior área total, usando a média das normais
  unitárias de suas faces incidentes. Regiões com pelo menos 95% da área da maior
  contribuem juntas, evitando preferência arbitrária em quinas simétricas.
  Não usa normais anteriores, mapa de alturas nem um eixo privilegiado.
- **Recalcular - suavização uniforme**: substitui os vetores pela média normalizada
  das normais unitárias das faces adjacentes. Esse método sobrescreve normais
  personalizadas e pode alterar o sombreamento de uma mesh do Image Mesh Editor.

A validação de orientação é uma convenção baseada no winding e não prova que uma
normal aponta para fora de qualquer sólido. O teste não inverte o resultado em
função da declaração CW/CCW. Triângulos degenerados não fornecem direção útil.
Listas de triângulos indexadas e não indexadas são suportadas; vértices não são
fundidos por posição. Reparação não reconstrói a classificação de patamares que
foi usada pelo Image Mesh Editor. Um arquivo já recalculado com normais válidas
não recupera automaticamente o sombreamento anterior no modo Reparar. O modo
Preservar superfícies faz uma nova inferência geométrica, sem garantia de reproduzir
normais artisticamente editadas ou todos os detalhes de um gerador especializado.

## Operações em lote

A janela **Aplicar a todos** tem sua própria seleção de método. O botão de
processamento percorre todos os frames e subsets dos arquivos do tipo escolhido.
Normais preservadas/inalteradas não marcam a entrada como modificada. Entradas
sem normais são ignoradas pelo processamento; **Adicionar normais** serve para
criá-las e ignora arquivos que já possuem normais, evitando sobrescrita acidental.

**Salvar todos (método de normais selecionado)** aplica a política selecionada
antes de salvar com o recálculo implícito da engine desabilitado. Se não houver
normais, cria-as e aplica o método escolhido (Reparar usa suavização uniforme
nesse caso, pois não há vetores personalizados a preservar). Arquivos que não usam TRIANGLES são ignorados nessa operação.
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

## Preservar superfícies: uso e limites

Escolha o método e o ângulo, aplique ao subset ou aos arquivos carregados e examine
a prévia 3D sob diferentes direções de luz antes de salvar. A operação muda apenas
normais em memória e marca o arquivo como modificado; não salva implicitamente.
O botão de salvamento com método selecionado aplica e salva explicitamente.
A largura do combobox é fixa em 320 px, tanto na tabela quanto na janela de lote.

Regiões crescem por continuidade local: uma superfície curva pode formar uma única
região mesmo quando a diferença de orientação entre suas extremidades é grande.
Ângulos altos podem unir uma parede ao patamar; baixos podem fragmentar superfícies
curvas. A maior área é uma heurística de superfície dominante, não uma classificação
semântica de topo/fundo. Em arestas compartilhadas por mais de duas faces, as regiões
não são unidas. Sem índices compartilhados (incluindo listas não indexadas), não se
infere conectividade por posição; não há soldagem de UVs, índices ou vértices.
Normais de quinas sem uma região dominante são misturadas. Não são criadas costuras
nem novos vértices; posições, UVs, índices e contagens ficam inalterados.

A prévia de meshes usa o carregamento isolado do editor, inclusive com filtro
de frames, para exibir o arquivo temporário editado sem reutilizar uma mesh
do cache de renderização. Ela só é recarregada quando a entrada muda.

O agrupamento executa somente quando o método é acionado, nunca ao desenhar o
combobox ou alterar o ângulo. Usa estruturas lineares no número de faces/arestas
e conjuntos disjuntos para encontrar componentes conectados. O cache geométrico
usado para status/visualização continua independente dessa operação.

Milestone concluído em 7.232.0: modo geométrico individual/lote/salvamento,
controle de ângulo, largura fixa, testes de invariância por rotação, regiões de
áreas equivalentes, triângulos degenerados e preservação de contagens. No exemplo
module_002-recompute-all.msh, a inferência recuperou visualmente os patamares,
comparada com a exportação original. Isso é validação desse exemplo, não garantia
para qualquer malha.
