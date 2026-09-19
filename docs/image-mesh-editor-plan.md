# Plano: Image Mesh Editor

Data: 2026-09-19

Status: proposta de implementação. As funcionalidades descritas neste documento são planejadas, não uma descrição da API disponível.

## Objetivo

Criar um editor totalmente offline para gerar módulos 3D por extrusão de regiões de imagens, com relevo determinístico, sem depender de IA ou serviços pagos. O uso principal é montar cenários com peças posicionadas lado a lado, priorizando a aparência frontal, mas permitindo inspecionar e usar laterais e fundo.

Referência inicial: `/home/michel/Downloads/mesh-tile-experiment/f1-c002ae9c-img01.png`, imagem com 12 painéis organizados em 4 colunas e 3 linhas. Esse caminho é uma referência local de desenvolvimento, não uma dependência do editor ou dos testes distribuídos.

A unidade de trabalho será uma região da imagem com contorno, nome e parâmetros próprios. O projeto comportará peças retangulares, circulares, irregulares e, em etapa posterior, peças com furos.

## Limites da abordagem

- Luminosidade não equivale necessariamente a profundidade: sombras, reflexos, manchas e riscos também influenciam a imagem. O resultado será uma interpretação ajustável de relevo, não uma reconstrução do objeto original.
- O relevo frontal terá uma altura por posição da superfície. Cavidades com saliências sobrepostas e outras formas que exigem múltiplas profundidades na mesma posição ficam fora desse modelo.
- Uma abertura desenhada na textura não vira automaticamente um furo. Furos reais precisam de contornos internos e paredes próprias.
- A imagem original será preservada. Correções de altura serão dados separados do projeto.

## Fluxo de trabalho

1. Abrir uma imagem.
2. Criar regiões manualmente ou por divisão em grade.
3. Ajustar os contornos de cada região.
4. Definir dimensões, espessura, relevo e texturas.
5. Conferir o resultado na prévia 3D.
6. Salvar o projeto editável.
7. Exportar uma peça ou todas as peças como malhas da engine.

Cada região poderá herdar os parâmetros gerais do projeto e sobrescrever valores específicos. Seleção múltipla permitirá aplicar ajustes a várias peças. Informar apenas a quantidade de peças não determina seus recortes; o modo em grade terá linhas, colunas, margens e espaçamento.

## Seleção e contornos

O editor básico incluirá:

| Forma | Controles |
|---|---|
| Retângulo | Mover e redimensionar |
| Círculo e elipse | Centro e raios |
| Polígono | Adicionar, mover e remover pontos; aceitar contornos côncavos |
| Grade | Linhas, colunas, margens e espaçamento; ajustes individuais após criação |

Uma etapa posterior adicionará contornos internos para furos reais, desenho livre convertido em polígono e seleção automática por transparência ou cor de fundo.

A seleção automática produzirá contornos editáveis e deverá distinguir o fundo externo dos detalhes escuros internos. Contornos cruzados, degenerados, furos fora da peça e interseções inválidas receberão diagnósticos claros. Nenhuma geometria inválida deverá ser exportada silenciosamente.

## Espessura e relevo

Forma, espessura e relevo serão independentes:

- Forma: silhueta da peça.
- Espessura: volume básico da extrusão.
- Relevo: deslocamento da superfície frontal dentro do contorno.

Fontes de altura: luminosidade, canal de cor selecionado ou imagem separada. Para um mapa separado, o editor deverá definir explicitamente o alinhamento com a imagem e a região selecionada.

Controles planejados:

- Amplitude e deslocamento da altura.
- Inversão entre claro e escuro.
- Pontos de preto e branco, contraste e curva de resposta.
- Suavização e limites mínimo e máximo.
- Altura fixa no contorno e largura da transição até o interior.

A conversão básica usa uma intensidade normalizada e ajustada para calcular o deslocamento. Valores uniformes e intervalos de intensidade nulos precisam de comportamento definido, sem divisão por zero. O gerador deverá impedir que o relevo atravesse o fundo e produza espessura inválida, inclusive quando houver relevo nas duas faces.

Uma etapa posterior incluirá pintura de altura para elevar, rebaixar, suavizar e achatar áreas. As alterações serão salvas separadamente da textura original e participarão do histórico de desfazer/refazer.

## Laterais, fundo e texturas

| Superfície | Opções planejadas |
|---|---|
| Frente | Textura do recorte original |
| Laterais | Faixa da borda esticada, textura repetida ou cor uniforme |
| Fundo | Plano ou com relevo copiado; textura normal, espelhada ou própria |

O espelhamento da textura e a cópia do relevo serão opções independentes. Para contornos irregulares, a faixa lateral acompanhará o perímetro. Furos terão paredes internas.

A exportação preservará as coordenadas de textura e incluirá os arquivos necessários. Recortes exportados terão margem de proteção para reduzir vazamento de pixels vizinhos durante a filtragem. Normais e separações entre faces deverão preservar a iluminação pretendida nas quinas e na superfície de relevo.

## Geometria, orçamento e encaixe

A triangulação respeitará o contorno e terá subdivisões internas para representar o relevo. Triangular somente os vértices da borda não é suficiente. A escolha do algoritmo deverá considerar contornos côncavos, futura inclusão de furos e preservação do contorno durante refinamento e simplificação.

O editor mostrará resolução, contagem final de vértices e triângulos e limites de exportação. Os limites considerarão frente, fundo, laterais e duplicações necessárias para textura e iluminação. Um orçamento insuficiente para preservar o contorno será reportado, sem degradar silenciosamente a forma.

A simplificação deverá preservar silhueta, furos, fronteiras de textura e bordas de encaixe. A compatibilidade com os limites de índices e buffers da engine deverá ser verificada antes de definir os máximos expostos pela interface.

Recursos para montagem de cenários:

- Dimensões em unidades da engine.
- Origem configurável, incluindo centro e base.
- Orientação padronizada.
- Ajuste a uma grade.
- Presets de dimensões e bordas.
- Prévia com cópias vizinhas para identificar frestas.

A altura fixa na borda ajuda no encaixe geométrico. Ela não garante continuidade visual das texturas, que deverá ser conferida na prévia.

## Interface e projeto editável

A interface terá área da imagem com contornos, prévia 3D, lista de peças e propriedades da seleção. A prévia permitirá orbitar, aproximar, alterar a luz e alternar entre textura, material neutro, mapa de altura e malha de arames. O material neutro ajudará a distinguir volume real de sombras pintadas.

O projeto salvará versão de formato, referências às imagens, regiões, parâmetros, correções de altura e configurações de exportação. Usará caminhos relativos quando possível e permitirá localizar novamente imagens movidas. O estado salvo deverá permitir reproduzir as malhas, sem depender de identificadores transitórios de textura ou objetos de renderização.

Recursos de edição: desfazer/refazer, duplicação de regiões, seleção múltipla e presets. A persistência seguirá os padrões dos editores existentes, com validação de versão e de dados ao reabrir.

## Desempenho

- O editor parado não deverá recalcular malhas, reler imagens, serializar projetos ou reenviar buffers sem alterações.
- Alterações marcarão apenas os dados dependentes como pendentes de atualização.
- Ajustes contínuos usarão prévia reduzida ou atualização após breve pausa; a exportação usará a qualidade final.
- Processamento pesado deverá manter a interface responsiva, com progresso e cancelamento quando necessário.
- Resultados de operações antigas não poderão substituir resultados de parâmetros mais recentes.
- Criação e atualização de recursos gráficos respeitarão as restrições de thread da engine.

## Arquitetura e integração

Separação proposta:

1. Gerador em C++ independente da interface: processamento de imagem, altura, triangulação, fechamento do volume e diagnósticos.
2. API Lua própria: geração, parâmetros e resultados utilizáveis por scripts e pelo editor.
3. Editor Lua/ImGui: seleção, edição, prévia e persistência.
4. Exportação: aproveitar o caminho existente de malhas v11, preservando texturas e normais.

A inspeção inicial identificou pontos a avaliar durante a prova técnica:

- `include/render/shape-mesh.h`: geometria indexada para prototipagem/prévia.
- `src/lua-wrap/render-table/mesh-debug-lua.cpp`: operações de vértices, índices, simplificação e salvamento v11.
- `editor/articulated_sprite_geometry.lua`: referência de triangulação; adequação para esta geração 3D ainda não validada.
- `editor/editor_utils.lua` e `editor/lang/language.lua`: infraestrutura e localização dos editores.

O uso de `meshDebug` para construção, inspeção ou exportação poderá ser interno; a geração terá uma API própria. Assinaturas e localização definitiva dos novos arquivos serão definidas após a prova técnica, evitando ampliar desnecessariamente os cabeçalhos públicos.

A implementação deverá preservar PIMPL e encapsulamento, manter módulos Lua coesos, integrar o editor aos launchers desktop pertinentes e fornecer textos em português e inglês com pontuação compatível com o atlas ImGui. Ao entregar a funcionalidade, atualizar a versão da engine e a documentação correspondente.

## Etapas de entrega

| Etapa | Entrega | Critério de conclusão |
|---|---|---|
| 1 — Prova técnica | Painel do exemplo com relevo, laterais, fundo e exportação | Malha exportada carrega na engine com textura, orientação e iluminação corretas; caminho de geração e exportação validado |
| 2 — Editor básico | Retângulos, elipses, polígonos côncavos, grade, parâmetros básicos, projeto editável, desfazer/refazer e exportação em lote | Gerar os 12 painéis, reabrir o projeto e montar uma parede; validar também uma peça circular e uma côncava |
| 3 — Controle artístico | Pintura de altura, presets, opções completas de textura/fundo e prévia de encaixe | Corrigir relevo localmente, preservar as correções ao reabrir e conferir módulos vizinhos |
| 4 — Contornos avançados | Furos, desenho livre e seleção automática | Exportar peças com aberturas reais e paredes internas, sem triângulos preenchendo os furos |
| 5 — Otimização e acabamento | Simplificação validada, limites finais e refinamento de progresso/cancelamento | Respeitar orçamento ou explicar inviabilidade; manter editor responsivo e sem reconstruções em repouso |

Os limites de recursos e a atualização somente quando houver mudanças são requisitos desde o início; a etapa 5 aprofunda a otimização. O editor básico não será limitado a retângulos.

## Validação

Usar a skill `engine-testing` para execução de scripts e verificação real na engine. Usar `doc-drift-check` ao alterar bindings ou documentação vinculada à implementação.

Cobertura prevista:

- Contornos convexos, côncavos, elípticos e, quando suportados, furos.
- Rejeição de contornos degenerados, cruzamentos e entradas inválidas.
- Imagens uniformes, transparentes, com detalhe fino e mapas de altura separados.
- Relevo invertido, limites de altura e espessura mínima válida.
- Geometria sem triângulos degenerados, normais invertidas ou frestas indesejadas. A verificação de fechamento considerará duplicações de vértices por UV e normal.
- Orçamento final incluindo todas as faces e duplicações.
- Salvamento, reabertura e reprodução determinística do resultado.
- Exportação individual e em lote, com carregamento posterior das malhas e texturas.
- Prévia sob iluminação e com material neutro.
- Editor em repouso sem reconstrução, carga de imagens ou uploads contínuos.
- Cancelamento e descarte de resultados obsoletos quando houver tarefas assíncronas.

O primeiro marco visual será montar uma parede 3D com os 12 painéis da imagem de referência. Os testes automatizados usarão imagens pequenas e sintéticas reproduzíveis, sem depender do arquivo externo em Downloads. Verificações em outros backends serão registradas conforme a disponibilidade real de cada plataforma.
