# Estudo: partículas e fluidos em cenas 3D

Data: 2026-10-14. Estado: **proposta para avaliação futura; sem implementação nova**.

O estudo surgiu durante o desenvolvimento do Ratworks, primeiro jogo 3D do autor
com a mini-mbm. O objetivo é melhorar o comportamento e a aparência das partículas
com mudanças localizadas, preservando os assets e os jogos existentes.

## Decisão atual

- Manter a ordenação especial dos fluidos 3D, restaurada após o experimento descrito abaixo.
- Não iniciar uma reorganização geral do renderizador por material/subset.
- Avaliar primeiro o visual do fluido com os controles existentes.
- Tratar a profundidade das partículas comuns como um trabalho futuro separado.
- Este documento não autoriza nem representa a implementação dessas propostas.

## Comportamento conferido no código

| Caminho | Comportamento atual | Referência |
|---|---|---|
| Ordenação 3D principal | `TYPE_CLASS_STEERED_PARTICLE` depois dos demais objetos da mesma categoria/prioridade; prioridades explícitas de `alwaysOnTop` continuam prevalecendo | [`CORE_MANAGER::prepareRender3d`](../src/core_mbm/core-manager-common.cpp) |
| Ordenação 3D em render-to-texture | Mesma preferência por desenhar fluidos depois dos demais objetos | [`RENDER_2_TEXTURE`](../src/render/render-2-texture.cpp) |
| Fluido 3D | Teste de profundidade habilitado e escrita desabilitada; o renderer passa `is3DObject()` ao shader | [`STEERED_PARTICLE::renderParticle`](../src/render/steered_particle.cpp) e overloads com `FLUID_GROUP` nos backends |
| Partículas comuns | Os overloads com `PARTICLE_CONTROL` selecionam estado sem teste de profundidade em OpenGL ES, DX9, DX11 e Metal | [`OpenGL ES`](../src/core_mbm/shader-opengl_es.cpp), [`DX9`](../src/core_mbm/shader-directx9.cpp), [`DX11`](../src/core_mbm/shader-directx11.cpp), [`Metal`](../src/core_mbm/shader-metal.mm) |
| Simulação do fluido | Box2D atualiza as posições X/Y das partículas; o desenho usa quads planares e o Z do renderizable | [`PHYSICS_BOX2D`](../plugins/box2d/box-2d-wrap.cpp), [`FLUID_GROUP::setVertex`](../src/core_mbm/particle-control.cpp) |
| Blend inicial do fluido | `BLEND_ONE`, aditivo; no OpenGL usa `SRC_ALPHA, ONE` | [`STEERED_PARTICLE::createAnimationAndShader2Particle`](../src/render/steered_particle.cpp), [`blend-opengl_es.cpp`](../src/core_mbm/blend-opengl_es.cpp) |

A ordenação especial não é uma classificação geral de opacidade: os objetos
não-fluidos podem incluir transparências. Ela resolve a relação comum entre
fluido e geometria opaca, sem garantir a composição de toda cena transparente.
Os comportamentos dos quatro backends acima foram conferidos por leitura do código;
os testes executados nesta investigação foram em Linux/OpenGL ES.

## Por que testar profundidade sem escrever

O teste consulta a profundidade já registrada e rejeita partículas atrás de uma
superfície mais próxima. A escrita registraria a profundidade da própria partícula
para bloquear desenhos posteriores. São controles independentes.

Em partículas com transparência suave, escrever profundidade pode impedir a
contribuição de outras partículas visíveis através delas. Mesmo um fragmento com
alpha zero pode escrever profundidade se o shader não o descartar. O blending
mistura cores; ele não transforma automaticamente alpha zero em descarte.

Por isso, a política atual do fluido tem uma finalidade de composição, não apenas
uma possível economia de escrita: a geometria preenche o Z-buffer e o fluido o
consulta depois. Habilitar escrita sem tratar a transparência pode introduzir
oclusões com o formato dos quads. Um modo de gotas opacas com recorte por alpha
poderia escrever profundidade, mas teria aparência e requisitos diferentes.

Referências conceituais: [Khronos: Transparency Sorting](https://wikis.khronos.org/opengl/Transparency_Sorting)
e [Khronos: Write Mask](https://wikis.khronos.org/opengl/Write_Mask).

## Evidências da investigação

### Remoção e restauração da ordenação

Foi removida temporariamente a preferência por `TYPE_CLASS_STEERED_PARTICLE` nos
dois comparadores 3D. O fluido continuou testando profundidade sem escrever nela.

No teste existente [`fluid_depth_smoke.lua`](../src/test-lib/fluid_depth_smoke.lua),
com captura em render-to-texture:

| Configuração | Sem preferência por fluidos | Após restauração |
|---|---|---|
| `MBM_FLUID_SHIFT=0` | Passou | Passou |
| `MBM_FLUID_SHIFT=150` | Falhou: `Fluid behind rear geometry or missing` | Passou |

O autor também observou no Ratworks que as partículas apareciam na emissão, mas
sumiam durante o deslocamento até o chão. Sem a preferência, geometria desenhada
depois pode cobrir o fluido, pois ele não registrou profundidade. A distância do
pivô do renderizable não representa necessariamente a localização das partículas,
que se deslocam em coordenadas XY do mundo.

A condição foi restaurada, a build Debug recompilada e os dois casos passaram.
Os testes existentes devem ser preservados; não relaxar suas verificações para
acomodar uma implementação que reintroduza o desaparecimento.

### Partículas sobre o rato durante `Eaten`

O jogo criava o fluido com `is3d=true` e `position.z = model.z`, então a hipótese de
criação acidental em 2D foi descartada. Compartilhar o Z do pivô não coloca todas
as partículas atrás da superfície de uma mesh volumétrica e animada.

Comparações temporárias congelaram a física e a pose, usaram partículas magenta
para identificação e compararam o rato visível/oculto. Houve oclusão parcial tanto
no render-to-texture quanto na janela OpenGL. Deslocar o plano para `z=-20` mudou
a sobreposição sobre a cabeça. Isso sustenta a explicação de interseção geométrica;
não houve evidência de um estado incondicional de “sempre por cima” nesse caso.

Esses ensaios alteravam câmera e aparência para diagnóstico e não constituem uma
aprovação visual do efeito final. O deslocamento `-20` foi experimental, não um
valor recomendado universalmente. As capturas/scripts ficaram em `/tmp` e não são
fixtures persistentes. A divergência visual dos pisos entre execuções não foi
esclarecida e não deve ser atribuída conclusivamente aos arquivos temporários.

## Proposta A: melhorar o visual sem alterar o renderizador

Fazer comparações com a mesma câmera, emissão, pose e tempo de simulação, mudando
uma variável por vez. Consultar as opções atuais em [Lua API: fluidos](lua-api.md#112-fluid-construction-and-options).

| Experimento | Resultado a observar | Limite |
|---|---|---|
| `blend='inv_src_alpha'` em vez do aditivo padrão | Mais preservação de cor e aparência de corpo líquido; menos saturação por sobreposição | Alpha convencional depende da ordem quando partículas de cores/opacidades diferentes se sobrepõem |
| Textura com borda suave e centro mais opaco | Melhor união visual entre gotas e menos pontos brancos destacados | Não suaviza automaticamente a interseção com meshes |
| Ajustar `radiusScale` | Continuidade visual entre partículas sem alterar o raio físico | Quads maiores podem avançar visualmente sobre colliders e personagens; menores podem deixar lacunas |
| Ajustar origem e plano Z da emissão | Compatibilidade entre o núcleo do rato, a animação e o caminho do líquido | A simulação permanece 2D; um deslocamento fixo não garante ocultação em toda câmera/pose |

Não mudar o blend padrão global para obter o visual de um jogo. Se um experimento
for aprovado, configurar o efeito específico. Brilho aditivo adicional é opcional
e implica desenho extra; não é requisito desta proposta.

Critério de escolha: o líquido deve ser legível durante a emissão e no chão,
preservar sua cor e não depender de esconder interseções por uma ordem incorreta.
Não há textura, blend ou valor de `radiusScale` aprovado por este estudo.

## Proposta B: correção localizada das partículas comuns em 3D

Problema candidato: `PARTICLE_CONTROL` desliga o teste de profundidade nos quatro
backends. Antes de alterar, criar uma reprodução com partículas comuns atrás,
à frente e cruzando uma mesh opaca; não assumir que todos os efeitos atuais usam
a mesma composição.

Escopo proposto para efeitos com transparência suave:

1. Informar ao caminho de desenho se o objeto é 3D, seguindo o precedente do fluido.
2. Em 3D, testar profundidade e desabilitar escrita durante o desenho das partículas.
3. Garantir que esses efeitos sejam desenhados após a geometria normal em ambos
   os destinos: janela e render-to-texture. Alterar apenas o estado de profundidade
   não basta, como mostrou o experimento com fluidos.
4. Restaurar corretamente os estados alterados, incluindo retornos antecipados.
5. Preservar o comportamento 2D e as prioridades explícitas de sobreposição.
6. Verificar os quatro backends, sem introduzir estado mutável público nos headers
   do core nem exigir conversão do formato dos assets.

A classificação poderia continuar restrita a fluidos e partículas comuns. Isso
não requer uma fila geral por material/subset. A decisão sobre o padrão 3D e uma
possível opção de compatibilidade para efeitos antigos que intencionalmente
ignoravam profundidade fica aberta até o inventário desses usos.

Também fica aberta a composição entre partículas comuns, fluidos e outras
transparências. Ordenar sistemas pelo pivô continua sendo uma aproximação. Entre
contribuições puramente aditivas a ordem normalmente não altera a soma; ao misturar
alpha convencional e aditivo, a ordem pode importar. Não prometer solução geral
para transparências intersectantes como resultado desta correção.

## Validação antes de considerar uma implementação pronta

- Reexecutar o teste de fluido com deslocamentos 0 e 150, com captura e validação
  de pixels, e conferir também a janela principal.
- Criar um teste equivalente para partículas comuns: geometria opaca à frente
  deve ocultá-las; geometria atrás não deve apagá-las.
- Testar emissão distante do pivô, câmera deslocada/inclinada, múltiplos sistemas,
  fluido junto de partículas comuns e bordas transparentes dos quads.
- Verificar que um objeto desenhado após o efeito não herda escrita de profundidade
  desligada ou outro estado indevido.
- Cobrir `2dw`, `2ds`, `3d`, prioridades de `alwaysOnTop` na janela e regras próprias
  do render-to-texture. Não presumir que os dois destinos tratem overlays igualmente.
- Rodar OpenGL ES, DX9, DX11 e Metal nas plataformas disponíveis e registrar
  explicitamente os backends ainda não testados.
- Comparar custo com o mesmo número de partículas e resolução. Medir CPU/GPU,
  chamadas de desenho e sobreposição de pixels; não atribuir o custo principal
  à escrita de Z sem medição. Não acrescentar ordenação por partícula sem necessidade
  demonstrada pelo efeito.
- Usar a skill `engine-testing` para os comandos, timeouts e requisitos do ambiente.

## Por que adiar a reorganização por material

Uma solução geral distinguiria objetos/subsets opacos, com recorte e translúcidos.
Hoje, o blend é aplicado a partir da animação em [`MESH::render`](../src/render/mesh.cpp),
e o desenho pode atualizar animação antes de enviar a geometria. Separar uma mesh
em etapas exigiria cuidado para não atualizar animações/callbacks mais de uma vez
por frame, além de integração com todos os backends, overlays e render-to-texture.

Uma futura implementação não precisaria exigir configuração manual de todo asset:
poderia preservar o comportamento legado, usar padrões no importador e permitir
configuração explícita apenas onde necessário, salva uma vez no asset. Entretanto,
alpha na textura e blend não descrevem toda intenção visual, principalmente com
shaders customizados. Não prometer inferência automática perfeita.

Há ainda uma ambiguidade já documentada: `BLEND_DISABLE` usa composição alpha
padrão, não significa blend realmente desligado. Ver
[Explicit blend-state API](future-features.md#explicit-blend-state-api). Não usar
esse enum como classificador automático de material opaco nem renumerar os valores
serializados para implementar esta proposta.

A reorganização completa é trabalho maior que corrigir os dois tipos de partículas.
Ela fica fora do escopo atual, assim como transparência independente de ordem,
soft particles com leitura da profundidade da cena, reconstrução de superfície
líquida, simulação volumétrica e migração em massa de materiais.

## Como retomar

1. Conferir novamente o código e a documentação: este documento é um retrato datado.
2. Escolher entre avaliação visual do fluido (A) e correção das partículas comuns (B).
3. Reproduzir o caso antes de modificar o backend e definir a política de compatibilidade.
4. Implementar somente o escopo escolhido e executar a matriz de validação pertinente.
5. Atualizar este estudo com resultados, limites e decisões efetivamente aprovadas;
   atualizar a documentação da API e a versão apenas quando houver entrega que o exija.
