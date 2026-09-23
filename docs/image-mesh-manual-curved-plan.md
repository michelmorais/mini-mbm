# Image Mesh Editor — Plano do modo Manual curvo

Status: **Planejado; implementação não iniciada.**
Data: **2026-09-23**

Este documento registra o objetivo discutido e uma proposta de implementação.
As recomendações e decisões pendentes abaixo ainda precisam ser consolidadas antes
de implementar os comportamentos correspondentes. A funcionalidade atual está
documentada em [Image Mesh Editor](image-mesh-editor.md).

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

### 2.2 Interpolação radial candidata

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

### 2.3 Perfis futuros

Manter o progresso espacial separado da função de perfil. Todos os perfis devem
preservar os extremos: `perfil(0) = 0` e `perfil(1) = 1`.

- Linear: primeira entrega; mudança de inclinação no encontro com o platô é esperada.
- Suave: permitir chegada arredondada ao platô.
- Bézier: definir controles e restringir inicialmente a curva para evitar extrapolação
  ou inversões involuntárias de espessura.

A ferramenta pode ajudar na forma geral de um diamante, mas facetas exigem também
controle de triangulação e normais. Facetamento não faz parte da primeira entrega.

## 3. Decisões pendentes antes da implementação

| Tema | Proposta inicial | Decisão necessária |
|---|---|---|
| Unidade dos valores | Espessura total final | Confirmar contrato e distribuição padrão |
| Alvo inicial | Ponto no centro da região, se válido | Definir escolha de centro válido e diagnóstico quando não houver |
| Alvo circular | Mesmo centro do controle radial | Confirmar vínculo entre centro e círculo |
| Formas não radiais | Diagnóstico explícito | Confirmar escopo limitado da primeira entrega |
| Furos | Sem suporte radial inicial | Definir mensagem e política para regiões com furos |
| Espessura zero | Valores estritamente positivos inicialmente | Definir mínimo e tolerância geométrica; lâmina de espessura zero pode degenerar |
| Pintura e áreas manuais existentes | Preservar dados, inicialmente inativos no novo modo | Confirmar composição futura e indicação visual de controles inativos |
| Travamento de borda | Novo modo já fixa a espessura externa | Evitar aplicação adicional do `lockBorder` atual |
| Materiais e verso | Reaproveitar o que for compatível | Definir combinações válidas, incluindo verso aberto e texturas de verso |
| Persistência | Configuração por região | Definir herança, campos, versão do projeto e migração |
| Precisão | Erro geométrico limitado | Fixar tolerâncias e orçamento com protótipo e medidas |

Nenhum nome novo de campo ou de enum neste plano constitui uma API final.

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

- [ ] Resolver as decisões que afetam a primeira entrega na seção 3.
- [ ] Verificar o domínio radial da serra e os limites para deslocamento do centro.
- [ ] Criar fixtures sintéticas reproduzíveis: círculo, polígono dentado e forma inválida.
- [ ] Prototipar o campo linear pontual e circular; definir tolerâncias mensuráveis.
- [ ] Definir esquema persistido e compatibilidade sem alterar projetos do usuário.

### Etapa 2 — Gerar a superfície e a malha

- [ ] Implementar validação e avaliação compartilhada do perfil.
- [ ] Garantir vértice no alvo pontual e representação explícita da borda do platô.
- [ ] Refinar a superfície conforme erro e orçamento; o caso linear também deve ser
  verificado, pois a triangulação pode aproximar mal a função radial.
- [ ] Fechar laterais e calcular normais nas duas distribuições de espessura.
- [ ] Proteger pico, contorno e transição do platô durante simplificação.
- [ ] Integrar cancelamento e progresso aos trabalhos assíncronos existentes.

### Etapa 3 — Integrar a edição

- [ ] Adicionar o modo Manual curvo e controles numéricos de espessura e alvo.
- [ ] Permitir arrastar o centro e redimensionar o círculo sobre a textura.
- [ ] Mostrar contorno, alvo, diagnóstico de validade e estado de prévia desatualizada.
- [ ] Integrar Apply, salvar/carregar, duplicação e desfazer/refazer.
- [ ] Incluir ajuda e traduções em português e inglês com pontuação segura para ImGui.
- [ ] Auditar os caminhos de `onLoop`: nenhum recálculo de campo, triangulação, upload
  ou varredura completa deve ocorrer continuamente com o editor ocioso.
- [ ] Durante arrastes, invalidar por mudança e limitar a cadência de trabalhos;
  impedir que resultados de trabalhos antigos substituam a configuração atual.

### Etapa 4 — Validar e documentar a entrega

- [ ] Executar testes matemáticos, de modelo, integração e exportação apropriados.
- [ ] Validar visualmente a serra no editor real seguindo a skill `engine-testing`.
- [ ] Registrar cobertura por backend; não presumir validação em plataformas não executadas.
- [ ] Atualizar `docs/image-mesh-editor.md` e `docs/lua-api.md` conforme a API entregue,
  aplicando `doc-drift-check`.
- [ ] Atualizar `docs/core-pimpl-status.md` se houver mudança de fronteira interna/pública.
- [ ] Atualizar `include/version/version.h` na entrega da funcionalidade.
- [ ] Atualizar este plano com decisões finais, evidências e limitações restantes.

## 6. Critérios de aceitação

Definir os valores numéricos das tolerâncias na etapa 1 e usá-los nos testes abaixo.

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

Alvos elípticos e poligonais; perfis suave e Bézier; composição com pintura e áreas;
formas que não admitem interpolação radial; furos; múltiplos alvos e facetamento.
Cada expansão deve definir seu comportamento antes de entrar na implementação.
