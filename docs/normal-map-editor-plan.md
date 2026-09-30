# Plano: editor gerador de normal maps

Status: entregas 1 e 2 implementadas (editor independente, módulos Lua e
integração ao Image Mesh). No uso real, o checkbox foi revisado para preservar a
geometria e acrescentar normal map. Separação automática de frequências/relevo
residual permanece pendente.

Este documento registra o escopo e a sequência de implementação. Consulte
[o manual do gerador](normal-map-editor.md) para os contratos implementados,
limitações e validação do gerador. A integração implementada está descrita em
[Image Mesh Editor](image-mesh-editor.md#normal-map-relief); o processamento residual
continua planejado.

## Objetivo

Criar um editor independente para gerar texturas normal map a partir de mapas de
altura, com módulos Lua reutilizáveis por outros editores. Integrar posteriormente
o mesmo fluxo ao Image Mesh, aproveitando suas fontes e ajustes de altura.

Referência funcional: [NormalMap Online](https://cpetry.github.io/NormalMap-Online/).
O foco inicial é conversão de altura, ajustes, preview iluminado e exportação PNG.

Normal maps alteram a resposta à iluminação, sem modificar silhueta ou colisão.
São úteis para rachaduras, poros, madeira, tijolos e outros detalhes que exigiriam
muitos triângulos. Converter luminosidade em altura é uma interpretação artística:
uma mancha escura pode virar uma cavidade indesejada. O usuário deve conseguir
inspecionar e ajustar a altura antes de gerar as normais.

## Entregas

1. Gerador independente, com módulos compartilhados e um fluxo completo de
   abertura, ajuste, preview, persistência e exportação.
2. Integração ao Image Mesh, preservando a geometria ao habilitar normal map.
3. Distribuição automática do relevo entre geometria e normal map residual,
   evitando reforçar detalhes já representados pela malha.

## Etapa 1: verificar a infraestrutura

Antes de escolher a implementação do processamento, verificar no código:

- Leitura de pixels, criação de texturas e exportação PNG disponíveis em Lua.
- Geração por shader e recuperação do resultado para exportação.
- Vinculação do normal map ao material e iluminação do preview.
- Capacidades de compilação e diferenças entre OpenGL ES, DirectX 9, DirectX 11
  e Metal; não presumir suporte equivalente sem validação.
- Partes do processamento de altura do Image Mesh que podem ser compartilhadas.
- Preparação da malha, tangentes e convenção dos canais no fluxo de consumo.

Pontos de partida:

- [Normal map authoring](../editor/normal_map_authoring.lua): preparação da malha
  e configurações de normal mapping; não é um gerador de textura.
- [Preview de altura do Image Mesh](../editor/image_mesh_height_preview.lua).
- [Editor Image Mesh](../editor/image_mesh_editor.lua).
- [Documentação de normal mapping](normal-mapping.md).
- [Documentação do Image Mesh](image-mesh-editor.md).

Resultado esperado: escolher o caminho de processamento e registrar eventuais
complementos C++ necessários. A interface dos módulos será Lua; trabalho pesado
por pixel poderá usar shaders ou processamento nativo da engine. Medir antes de
assumir que laços Lua atendem imagens grandes com fluidez.

## Etapa 2: módulos e contratos

Nomes provisórios, sob `editor/`:

| Módulo | Responsabilidade |
|---|---|
| `height_map_source.lua` | Fonte, canal, níveis, curva, inversão e suavização da altura |
| `normal_map_generator.lua` | Conversão de altura para normais e controle da geração |
| `normal_map_preview.lua` | Visualização da textura e do material iluminado |
| `normal_map_panel.lua` | Controles ImGui reutilizáveis |
| `normal_map_project.lua` | Parâmetros persistidos, caminhos e exportação |
| `normal_map_editor.lua` | Cena do editor independente que reúne os módulos |

O gerador não deverá depender de ImGui nem de variáveis globais do editor.
Cada instância terá estado próprio. O módulo existente `normal_map_authoring.lua`
continuará responsável pela preparação da malha e pelas configurações do material.

Definir no contrato compartilhado:

- Resolução, orientação da imagem e correspondência com UVs.
- Escala de altura e comportamento da intensidade ao mudar a resolução.
- Convenção `+Y/-Y`, evitando inversão duplicada na geração e no material.
- Tratamento de bordas, pixels transparentes e alfa exportado.
- Formato dos resultados e erros, propriedade e descarte dos recursos temporários.
- Invalidação de resultados quando as entradas mudarem.

## Etapa 3: geração inicial

Fluxo: imagem -> altura ajustada -> normal map -> preview -> PNG.

Controles iniciais:

- Fonte de altura por luminância ou canal R, G, B ou alfa.
- Níveis de preto e branco, curva e inversão da altura.
- Suavização e intensidade do relevo.
- Convenção `+Y/-Y`.
- Bordas com repetição para texturas contínuas ou tratamento para imagens isoladas.

Escolher e documentar o filtro de derivadas na implementação. Para sprites, pixels
transparentes não devem criar contornos artificiais. Exportar os vetores sem
iluminação ou correção de cor destinada à imagem visual.

## Etapa 4: editor independente

Exibir imagem original, altura, normais e superfície iluminada. Permitir mover a
luz e comparar o efeito ligado e desligado.

Incluir abertura de imagem, salvamento e reabertura de projeto, exportação PNG,
textos em português e inglês e entrada no launcher das plataformas suportadas.
Persistir a fonte e os parâmetros necessários para reproduzir o resultado.

Durante ajustes, usar preview reduzido; na exportação, gerar na resolução escolhida.
Reprocessar somente quando as entradas mudarem. Mover a luz deverá atualizar a
visualização sem regenerar a textura.

Auditar todos os caminhos por frame: não repetir leitura de arquivos, conversão,
varreduras de pixels, criação de recursos ou uploads quando o editor estiver ocioso.
Se houver processamento assíncrono, impedir que resultados de revisões anteriores
substituam a versão atual e definir cancelamento e descarte.

## Etapa 5: integração ao Image Mesh

Separar a origem da altura de sua aplicação:

| Escolha | Opções |
|---|---|
| Origem da altura | Imagem e demais fontes já disponíveis no Image Mesh |
| Aplicação do relevo | Geometria, normal map ou combinação |

Reutilizar o painel e o gerador, respeitando recortes, UVs, regiões e configurações
persistidas. Alterar apenas o normal map não deverá reconstruir a geometria.
Definir como os arquivos gerados acompanham o projeto e os assets exportados.

A integração atual preserva a geometria e acrescenta normal map ao material da
frente. Uma evolução deverá reservar geometria para volumes maiores e normal map
para detalhes residuais, evitando reforçar na iluminação o relevo já representado
pela malha. Essa separação automática permanece planejada.

## Etapa 6: validação e conclusão

Critérios de aceite:

- Altura constante produz normal neutra.
- Rampas conhecidas produzem direções corretas; inversão Y funciona.
- Transparência, recortes e repetição não introduzem costuras indevidas.
- Intensidade e orientação são coerentes entre preview, PNG e material da engine.
- Reabrir o projeto reproduz o resultado para a mesma fonte.
- Duas instâncias dos módulos não compartilham estado acidentalmente.
- Editor ocioso não continua gerando ou enviando texturas.
- Recursos temporários são liberados ao trocar a fonte e fechar a cena.
- Integração preserva UVs e evita reconstrução de geometria em ajustes só de textura.

Aplicar testes unitários às transformações e contratos que puderem ser isolados,
com imagens sintéticas. Validar o fluxo completo na engine seguindo a skill
`engine-testing`, incluindo a aparência do PNG reimportado. Registrar quais
backends foram efetivamente testados e quais permanecem pendentes.

Ao implementar, atualizar a documentação das funcionalidades e APIs afetadas,
verificar sua correspondência com o código e atualizar `include/version/version.h`
conforme a política do projeto. Este plano, por si só, não exige alteração de versão.

## Fora do escopo inicial

- Ambient occlusion e specular.
- Geração por quatro fotografias com iluminação diferente.
- Processamento em lote.
- Pintura específica de normais.

Essas extensões serão avaliadas depois que geração, preview, exportação e
reutilização dos módulos estiverem validados.
