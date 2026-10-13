<!--
┌──────────────────────────────────────────────────────────────────────────────┐
│ MIT License                                                                 │
│ Copyright (c) 2026 Michel Braz de Morais                                      │
│                                                                            │
│ Permission is hereby granted, free of charge, to any person obtaining a copy │
│ of this software and associated documentation files (the "Software"), to    │
│ deal in the Software without restriction, including without limitation the │
│ rights to use, copy, modify, merge, publish, distribute, sublicense, and/or │
│ sell copies of the Software, and to permit persons to whom the Software is │
│ furnished to do so, subject to the following conditions:                   │
│ The above copyright notice and this permission notice shall be included in│
│ all copies or substantial portions of the Software.                        │
│ THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR │
│ IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,  │
│ FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL   │
│ THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER│
│ LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING   │
│ FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER       │
│ DEALINGS IN THE SOFTWARE.                                                  │
└──────────────────────────────────────────────────────────────────────────────┘
-->

# Box2D com partículas: integração Lua e CMake

Marco 2, engine **7.365**, 2026-10-13. Implementação integrada e validada no Linux
x86_64/OpenGL ES. Ratworks completo, equivalência visual dos exemplos, performance
integrada e demais plataformas continuam nos marcos seguintes do
[plano](box2d-liquidfun-unification-plan.md). A [baseline](box2d-liquidfun-baseline.md)
registra as comparações anteriores; seus resultados não foram reescritos como se
fossem medições do plugin novo.

## Build

```sh
# Perfil com fluidos
cmake -S . -B build/physics-lua-on -DPLAT=Linux \
  -DMBM_OUTPUT_ROOT="$PWD/build/physics-lua-on/output" \
  -DUSE_LUA=1 -DUSE_BOX2D=1 -DUSE_BOX2D_PARTICLES=ON \
  -DAUDIO=none -DCMAKE_BUILD_TYPE=Debug -DUSE_TEXTURE_MISSING_DIALOG=0
cmake --build build/physics-lua-on --target box2dLiquidFun mini-mbm -j6

# Perfil apenas rígido
cmake -S . -B build/physics-lua-off -DPLAT=Linux \
  -DMBM_OUTPUT_ROOT="$PWD/build/physics-lua-off/output" \
  -DUSE_LUA=1 -DUSE_BOX2D=1 -DUSE_BOX2D_PARTICLES=OFF \
  -DAUDIO=none -DCMAKE_BUILD_TYPE=Debug -DUSE_TEXTURE_MISSING_DIALOG=0
cmake --build build/physics-lua-off --target box2dLiquidFun mini-mbm -j6
```

`MBM_OUTPUT_ROOT` separa `bin/` e `libs/` de cada perfil. Nos comandos acima,
os executáveis ficam em `build/physics-lua-{on,off}/output/bin/debug/linux_x86`.
Sem essa opção, as saídas continuam em `bin/debug/linux_x86` e
`libs/debug/linux_x86` da engine, compartilhadas entre diretórios de build.

Uma execução intermediária falhou antes da cena enquanto builds com opções diferentes
compartilhavam essas saídas. O build habitual consistente voltou a executar normalmente;
a rodada final usa saídas isoladas para eliminar a mistura de artefatos. Separar apenas
os diretórios de CMake não separa automaticamente os binários.

Para distribuir, incluir `box2d` e o pequeno módulo `box2dLiquidFun` caso os scripts
usem o nome antigo. O target `box2dLiquidFun` depende de `box2d`; compilar apenas
`box2d` não atualiza a fachada.

- `USE_BOX2D_PARTICLES` é OFF por padrão, ON por padrão com `USE_ALL=1`.
  Um OFF explícito ou já salvo no cache é respeitado. Para mudar um cache existente,
  passar ON explicitamente.
- Partículas ON exigem `USE_BOX2D=1` e `USE_LUA=1`; a configuração falha caso contrário.
- Com `USE_BOX2D=1`, a árvore antiga nunca é adicionada ao build, mesmo se
  `USE_BOX2D_LIQUID_FUN=1` também foi fornecido. A fachada usa o backend único.
- A comparação legada permanece possível: `USE_LUA=1 USE_BOX2D=0
  USE_BOX2D_PARTICLES=OFF USE_BOX2D_LIQUID_FUN=1`, sem `USE_ALL`.
- OFF exclui `src/particle/` e os dois arquivos de bindings de fluido. A macro
  `MBM_BOX2D_PARTICLES=0/1` é uma definição PUBLIC do target, mantendo os layouts
  consistentes em seus consumidores. Os arquivos continuam no checkout.
- Android/iOS/MAS incluem os entry points de compatibilidade no mesmo target e
  usam o carregador embutido. A configuração foi preparada, mas esses alvos não
  foram compilados/executados neste marco. Os projetos diretos do Visual Studio
  ainda requerem adaptação no marco de plataformas.

## API e compatibilidade

```lua
require "box2d"
assert(box2d:supportsParticles())
local world = box2d:new(0, -10)
local fluid = world:createFluid({type="circle", ray=5}, {
    radius=.1, riseTime=1, fixtureMaskBits=65533, texture="#FFFFFFFF"
})
```

`require "box2dLiquidFun"` registra a factory de compatibilidade sobre o mesmo
backend. Ambas as ordens de carregamento funcionam; não há duas cópias do solver.
A factory moderna mantém seus defaults rígidos. A antiga adapta defaults de joints,
frequência em Hz, razão de amortecimento e rope; `getVersion()` agora retorna 2.4.1.
A fachada OFF falha com diagnóstico explícito. A
[referência Lua](lua-api.md#11-plugin-box2dliquidfun) detalha as unidades.

Foram portados os métodos de partículas e as melhorias recentes: filtros por
categoria, emissão gradual, lifetime, reemissão após esvaziamento, contorno convexo,
renderização 2D/3D, shader e posição Z. A criação continua sob demanda; não existe
sistema de partículas implícito em cada mundo. O caminho rígido não faz verificações
de fluido por fixture/corpo.

A integração também corrige no wrapper novo:

- Handles de fluidos, seus shaders e joints são invalidados ao destruir seus donos.
  Acesso posterior produz erro Lua; a destruição de fluidos verifica o mundo dono.
  O handle não prolonga a vida do mundo. Referências internas prendem apenas os
  userdata e são liberadas na destruição nativa.
- Destruir qualquer um dos dois corpos elimina seus joints registrados.
- Os métodos antigos de frequência/razão convertem unidades; gear mantém sua razão
  adimensional. Rope da factory antiga controla o limite máximo de uma distance joint.
- UVs segmentados usam os offsets corretos dos grupos; `fluid:add` marca atualização
  de UV para o próximo sync, sem recalcular UVs continuamente em estado estável.
- `queryShapeAABB` lê a tabela de shape correta, posições omitidas são inicializadas
  e callbacks de queries/raycast removem os resultados temporários da pilha Lua.

Essas adaptações não prometem trajetórias idênticas ao solver 2.3; essa aceitação
exige os jogos e os exemplos com critérios físicos/visuais.

## Evidência e reprodução

[Resultados e hashes dos artefatos](box2d-liquidfun-integration-results.json).

| Verificação | Resultado |
| --- | --- |
| CMake: padrão, USE_ALL, USE_ALL+OFF, falta de Box2D, falta de Lua, legado isolado | Seis configurações verificadas; dependências ausentes rejeitadas |
| Build real ON/OFF, plugin e fachada | Ambos compilam no Linux/GCC 12.2 |
| Inspeção de símbolos | OFF sem definições `b2ParticleSystem`; fachada sem definições `b2World`/`b2ParticleSystem` |
| Lua: ON, ordem moderna e ordem antiga; OFF | Marcador `PHYSICS_LUA_OK`, saída 0 |
| Lua: frequência/razão, rope, handles, coleta do mundo, lifetime, reemissão, queries | Cobertos por `lua-unified.lua` |
| Quatro cenas existentes de fluido | `fluid_rise`, `fluid_filter`, `fluid_convex`, `fluid_depth`: sucesso com o novo backend |
| Harness nativo Release | 4/4 passaram, incluindo equivalência de trajetória rígida OFF/ON |
| ASan/UBSan no plugin ON | Integração e quatro cenas sem acesso inválido/comportamento indefinido; queries corrigidas também verificadas |
| LeakSanitizer no processo gráfico | **Não limpo**: ON e OFF reportam os mesmos 72.920 bytes em quatro alocações (duas em módulos sem símbolos e duas de 4 bytes no controle de índices de shader). Nenhuma alocação atribuída ao port nesses relatórios |

A rodada de ASan/UBSan das cinco cenas usou `detect_leaks=0`, depois da comparação
LSan ON/OFF com detecção habilitada. Isso não equivale a aprovar ausência de vazamentos
na engine. O relatório LSan completo é uma limitação separada da validação de acesso
à memória. Não houve aprovação visual, replay de input, benchmark integrado ou teste
prolongado do Ratworks nesta etapa.

```sh
python3 src/test-lib/physics-compat/check-cmake.py
cmake -S src/test-lib/physics-compat -B build/physics-native \
  -DPLAT=Linux -DCMAKE_BUILD_TYPE=Release
cmake --build build/physics-native -j6
ctest --test-dir build/physics-native --output-on-failure

# Após compilar ON (repetir com MBM_PHYSICS_ORDER=legacy):
MBM_PHYSICS_BIN="$PWD/build/physics-lua-on/output/bin/debug/linux_x86"
MBM_PHYSICS_PROFILE=on MBM_PHYSICS_ORDER=modern \
  MBM_PHYSICS_MODULE_DIR="$MBM_PHYSICS_BIN" \
  LD_LIBRARY_PATH="$MBM_PHYSICS_BIN" LUA_CPATH="$MBM_PHYSICS_BIN/?.so" \
  timeout -s KILL 15 "$MBM_PHYSICS_BIN/mini-mbm" \
  --scene src/test-lib/physics-compat/lua-unified.lua \
  --disable_select_monitor --nosplash -w 320 -h 240
# Para OFF, usar MBM_PHYSICS_PROFILE=off e o executável do diretório physics-lua-off.
```

As cenas encerram sozinhas e emitem um marcador de sucesso; código 0 sozinho não
aprova execução Lua. No Linux o teste requer um display acessível. O runner aceita
`MBM_PHYSICS_MODULE_DIR` para escolher módulos copiados; configurar também
`LD_LIBRARY_PATH` e `LUA_CPATH` como acima. O ambiente local usado nesta validação
priorizava o diretório habitual da engine; sem substituir essas variáveis, até um
executável isolado carregava o core e os plugins de outro perfil.

Para instrumentar somente o target físico, criar um arquivo CMake com
`add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)` e
`add_link_options(-fsanitize=address,undefined)`, passando seu caminho absoluto em
`-DCMAKE_PROJECT_box2d_INCLUDE=...`. Recompilar o plugin e sua fachada; executar a
engine com `LD_PRELOAD` apontando para o `libasan` do compilador e
`UBSAN_OPTIONS=halt_on_error=1`. A engine gráfica permanece sem instrumentação;
as alocações interceptadas do processo inteiro são visíveis ao LSan. Remover essa
opção e recompilar antes de voltar ao uso normal.

## Próximo marco

Criar o cenário reduzido do Ratworks, executar o jogo com o backend novo, registrar
emissão/filtros/pausa/lifetime/apresentação e medir o custo por etapa. Depois validar
os exemplos de mbm-documentation e as plataformas restantes. Ratworks e a documentação
externa não foram alterados neste marco; a árvore antiga permanece para comparação.

Continuação executada: [cliente Ratworks e teste prolongado](box2d-liquidfun-ratworks.md).
