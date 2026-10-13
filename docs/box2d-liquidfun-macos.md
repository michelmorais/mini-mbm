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

# Box2D/LiquidFun: validação macOS e Xcode

Data: 2026-10-13. Apple M4/arm64, macOS 26.6.2, Xcode 27.0
(27A266a), AppleClang 21.0.0.21000334, SDK macOS 27.0, CMake 4.2.0.
Deployment target explícito: macOS 12.0. Backend gráfico: Metal.

O Xcode compilou Box2D rígido e a extensão de partículas no mesmo backend,
inclusive no link estático Mac App Store (MAS). O nome `box2dLiquidFun` continua
compatível, sem compilar uma segunda cópia da física. Isso remove a necessidade
anterior de escolher entre os dois plugins nos builds estáticos: habilite
`USE_BOX2D=1 USE_BOX2D_PARTICLES=ON`.

Os builds ON também receberam `USE_BOX2D_LIQUID_FUN=1` para exercitar a
configuração antiga com ambas as opções. O CMake desabilitou o backend legado
e manteve a fachada unificada. Não significa que os dois solvers antigos sejam
linkados juntos. O perfil OFF mantém a rejeição explícita de fluidos.

## Correções encontradas durante a validação

- Os runners Python e seus contratos Lua presumiam `.so` em todo sistema Unix.
  Agora selecionam `.dylib` no macOS, passam a extensão aos contratos e isolam
  `DYLD_LIBRARY_PATH`. Linux e Windows mantêm suas extensões anteriores.
- `fluid_convex_smoke.lua` encontrou **23 partículas no quadrado triangulado,
  contra 25 no retângulo equivalente**. A falha deixava o loop parado até o timeout;
  agora o teste imprime `FLUID_CONVEX_SMOKE_FAILED` e encerra imediatamente.
- Em Apple Silicon, a contração FMA do produto escalar em `b2PolygonShape::TestPoint`
  pode produzir resíduo positivo nos dois lados de uma diagonal compartilhada.
  Um reprodutor nativo com a mesma geometria produziu 25/22; desabilitar FMA nesse
  reprodutor restaurou 25/25. As contagens diferentes decorrem da construção da
  malha de amostragem do reprodutor, não de equivalência bit a bit com a cena Lua.
- A correção em `plugins/box2d/box-2d-wrap.cpp` usa uma subclasse privada de shape
  somente para preencher os triângulos dos fluidos. Calcula os dois produtos em
  `double`, preservando o cancelamento sem adicionar tolerância ao contorno.
  O preenchimento instantâneo e gradual usa o mesmo predicado. Não modifica o
  solver rígido, o `TestPoint` upstream, as flags globais de compilação ou a API Lua.
  Após a correção, o contrato completo de polígonos passou com **25 partículas**,
  incluindo orientação inversa, união, adição, emissão gradual e entrada inválida.

- O RatWorks instrumentado revelou também uma divisão por intervalo de frame zero
  em `ANIMATION::updateAnimation`, seguida da conversão de infinito para `uint32_t`
  (`animation.cpp`). O UBSan interrompeu corretamente essa primeira execução.
  A atualização de frames agora ignora intervalos não positivos, mantendo a
  atualização anterior dos efeitos de shader. Não houve mudança de API ou de
  ownership/PIMPL. O jogo passou novamente com UBSan ativo após essa proteção.
  A regressão
  `testLib --animation-zero-interval-test` reproduziu o diagnóstico com a biblioteca
  anterior e passou com a nova, incluindo a retomada sem acúmulo indevido de tempo.
- A conferência local encontrou também a assinatura antiga de `obj:addAnim` em
  `docs/lua-api.md`; a linha agora descreve os argumentos e retornos efetivos, sem
  alteração no binding.

## Evidências

Resultados, hashes e comandos: [JSON da validação](box2d-liquidfun-macos-results.json).
Logs completos ficam em `build/physics-macos-results/`; logs dos builds em
`/tmp/physics-macos-*-build.log` nesta máquina.

| Verificação | Resultado | Limite |
| --- | --- | --- |
| Xcode Release dinâmico ON/OFF | Engine e módulos compilados | arm64; outputs separados |
| Xcode Release MAS estático ON/OFF | Aplicação linkada, sem assinatura | Não equivale a Archive/distribuição ou execução sandboxed MAS |
| Símbolos e dependências | Fachada dinâmica depende de `box2d.dylib`; arquivo estático contém quatro entry points; partículas excluídas em OFF | Inspeção dos artefatos |
| Nativo ASan/UBSan | 4/4 passaram, incluindo trace rígido OFF/ON | LSan indisponível neste runtime Apple |
| Contratos Lua Release | ON: 6/6 após correção; OFF: 1/1 | Duas ordens de require em ON; aliases `luaopen_lib*` verificados |
| Contratos Lua engine ASan/UBSan | ON: 6/6 | Validação Metal ativa |
| Profundidade Metal | Captura e assert de pixels passaram | Cena sintética; não é equivalência visual de todos os exemplos |
| RatWorks real Release corrigido, 60 s | 3 ratos emitidos/consumidos, 3 fluidos expirados, pico de 634 partículas | Cenário automatizado existente, não playtest completo |
| RatWorks real ASan/UBSan corrigido, 60 s | 3 ratos/3 fluidos, pico de 634 partículas; passou | Sem diagnósticos ASan/UBSan ou Metal |
| Cenário reduzido ASan/UBSan, 600 s | 75 ciclos, pico de 385 partículas; passou | Build com correção geométrica; ver escopo abaixo |
| Cenário reduzido ASan/UBSan final, 60 s | 8 ciclos, pico de 385 partículas; passou | Ambas as correções incluídas |
| Exemplos da documentação ON/OFF | ON: 40/40 cenas; OFF: 31/31 cenas passaram | Somente cenas autossuficientes; sem reproduzir callbacks interativos |

A execução anterior à correção é preservada em `contracts-on` (falha de polígonos)
e `ratworks`; a correção geométrica em `contracts-fixed` e `ratworks-fixed`.
O teste prolongado instrumentado (`reduced-asan`) preservou o binário com a correção
geométrica, anterior à proteção de animação. A rodada adicional de 60 s
(`reduced-asan-final`) e o RatWorks instrumentado (`ratworks-asan-fixed`) usam ambas
as correções. Os hashes no JSON distinguem essas revisões; não se atribuem os dez
minutos ao binário posterior. Os contratos instrumentados finais estão em
`contracts-asan-final`, o contrato OFF final em `contracts-off-final`, e os
exemplos finais em `examples-final`/`examples-off-final`.

O RatWorks em `/Users/michel/Ratworks` e a documentação externa foram usados
somente para leitura. O runner confere os hashes Lua do jogo antes/depois.

Captura sintética do teste de profundidade (o assert verifica pixels expostos e
ocultos; não é uma captura do RatWorks):

![Fluido e oclusão no Metal](box2d-liquidfun-macos-metal.png)

## Reprodução

Build dinâmico (repetir com `profile=off` e `particles=OFF`):

```sh
profile=on
particles=ON
cmake -S . -B build/physics-macos-$profile -G Xcode \
  -DPLAT=MacOs -DUSE_ALL=1 -DUSE_BOX2D_PARTICLES=$particles \
  -DUSE_TEXTURE_MISSING_DIALOG=0 -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
  -DMBM_OUTPUT_ROOT="$PWD/build/physics-macos-$profile/output"
cmake --build build/physics-macos-$profile --config Release \
  --target mini-mbm box2dLiquidFun -j6
engine="$PWD/build/physics-macos-$profile/output/bin/release/arm64/Release/mini-mbm"
NSUnbufferedIO=YES MTL_DEBUG_LAYER=1 python3 \
  src/test-lib/physics-compat/run-lua-contracts.py \
  --engine "$engine" --profile "$profile" \
  --output build/physics-macos-results/contracts-$profile
```

Nesta rodada, a saída ON corrigida foi `output-fixed` para preservar os binários
usados pelo primeiro teste prolongado. O Xcode acrescenta `Release`/`Debug` aos
diretórios de saída. Os testes gráficos precisam de acesso à sessão gráfica/GPU:
no sandbox do agente `MTLCreateSystemDefaultDevice()` retornou `nil`; fora dele,
identificou Apple M4 e executou normalmente.

Para MAS, configurar outro diretório com `USE_LUA=1 USE_BOX2D=1`, partículas ON
ou OFF, `MAS_DELIVERY=1 MAS_BUNDLE_ID=org.minimbm.physicsvalidation` e o mesmo
deployment target/output isolado. Compilar somente o target `mini-mbm`:

```sh
cmake --build build/physics-macos-mas-on --config Release \
  --target mini-mbm -j6 -- CODE_SIGNING_ALLOWED=NO
```

O perfil MAS gera um `libbox2d.a` único, com os nomes modernos e de compatibilidade;
não possui target dinâmico `box2dLiquidFun`. O CMake escreve `platform-macos/Info.plist`
durante a configuração MAS; o arquivo gerado nesta validação foi restaurado ao
conteúdo anterior. Nenhuma identidade de assinatura foi alterada.

ASan/UBSan da engine: diretório isolado, `USE_ASAN=1`, `CMAKE_BUILD_TYPE=Debug`,
`--config Debug`; usar `UBSAN_OPTIONS=halt_on_error=1` nos runners. Testes nativos:

```sh
cmake -S src/test-lib/physics-compat -B build/physics-macos-native \
  -DPLAT=MacOs -DPHYSICS_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build/physics-macos-native -j6
ctest --test-dir build/physics-macos-native --output-on-failure
```

RatWorks: `run-ratworks.py --engine <binário> --game /Users/michel/Ratworks
--seconds 60 --output <diretório>`. Para o reduzido, omitir `--game` e usar
`--seconds 600`. Exemplos: `inventory.py --examples-only --documentation
/Users/michel/mbm-documentation --output <inventário>`, seguido de `run-examples.py`
com `--inventory <inventário>/inventory.json`; usar `--plugin box2d` em OFF.

## Limites e pendências

- `ASAN_OPTIONS=detect_leaks=1` abortou com `detect_leaks is not supported on this
  platform`. ASan/UBSan aprovado não equivale a aprovação de vazamentos por LSan.
  O runner atual só coleta RSS no Linux; os resultados macOS não alegam RSS estável.
- Não foram medidos overhead rígido, desempenho relativo ao solver legado, GPU
  isolada ou orçamento de partículas. Tempos do harness são observações, com builds
  e testes concorrentes, não benchmark comparativo.
- Ainda pendem execução da aplicação MAS assinada/sandboxed, Intel/x86_64, iOS,
  playtest completo dos jogos e equivalência interativa/visual dos exemplos.
  Não houve publicação, assinatura, alterações no RatWorks ou remoção da árvore antiga.
