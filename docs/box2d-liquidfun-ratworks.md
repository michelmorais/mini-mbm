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

# Box2D unificado: cliente Ratworks (marco 3)

Data: 2026-10-13. **Validação automática implementada e teste manual do Ratworks aprovado pelo
usuário; orçamento de performance continua pendente.** A árvore antiga permanece como
referência, e os exemplos externos seguem na etapa 4.

## Entrega

- `src/test-lib/physics-compat/ratworks-reduced.lua`: reprodução sem assets externos,
  com raio `.03`, escala visual `2.1`, emissão de `1.5 s`, lifetime de `5 s`,
  apresentação 3D e máscara `65533`, conforme o cliente. Cada ciclo remove o
  emissor, pausa fluido/mundo separadamente, verifica passagem pela categoria 2 e
  contato com o chão, espera expiração, reemite e destrói explicitamente o sistema.
- `ratworks-client.lua`: carrega o `main.lua` verdadeiro do Ratworks, preserva ondas,
  assets e lógica, observa emissão/remoção/expiração e encerra automaticamente.
  A instrumentação envolve `Rat:releaseFluid` apenas para reter handles e observar
  o resultado; não antecipa animações nem força ratos a chegarem à máquina.
- `run-ratworks.py`: seleciona bibliotecas do próprio perfil de build, impõe timeout,
  verifica marcador de sucesso e código de saída, registra hashes e amostra RSS a
  cada segundo. `--legacy` permite a comparação do jogo com o plugin antigo.

O cliente usado é `/home/michel/Ratworks`, revisão
`c52d9918bfee242861fef0b7a502ea141f8be68d`. Nenhum arquivo do jogo foi modificado.
A configuração atual tem três ratos e mantém seus sistemas vazios no mundo após a
expiração. Isso é ownership existente, não prova de vazamento: o cenário reduzido
exercita também a destruição explícita e recriação.

## Correção encontrada pela reprodução

`INFO_FLUID::lua` e `INFO_JOINT::lua` guardavam a coroutine que criou o handle.
O registry é compartilhado, mas essa coroutine pode ser coletada antes da liberação
do recurso. A reprodução caiu em `luaL_unref` durante teardown, usando a coroutine
já liberada. Os dois registros agora guardam `LUA_RIDX_MAINTHREAD`.

A regressão `lua-unified.lua` cria handles em `coroutine.wrap`, descarta a coroutine,
força GC e depois destrói fluidos/corpos/joints. O binário anterior falhou com
SIGSEGV; os perfis corrigidos ON e OFF passaram. Versão da correção: **7.365.1**.

## Validação

Resultados numéricos e hashes: [registro da execução](box2d-liquidfun-ratworks-results.json).

- Ratworks completo com backend unificado e antigo: três ratos criados e consumidos,
  três fluidos emitidos, todos observados vivos após remoção do rato e todos expirados.
- Regressão Lua ON/OFF: passou, incluindo handles originados em coroutines coletadas.
- `fluid_rise_smoke`, `fluid_filter_smoke`, `fluid_convex_smoke` e
  `fluid_depth_smoke`: passaram; o último salvou imagem e verificou pixels de
  oclusão por geometria frontal/traseira.
- Teste prolongado: **75 ciclos em 602,32 s**, pico de 385 partículas por ciclo,
  com todas as verificações funcionais aprovadas. RSS mediano de 104.076 KiB na
  janela 60–120 s e 104.404 KiB na janela 540–600 s (+328 KiB); essa pequena
  variação não demonstra ausência de leaks.
- ASan/UBSan: a engine inteira instrumentada inicialmente não inicia por duplicação
  de globals tinyfiledialogs (ODR). Com `detect_odr_violation=0`, a regressão chega
  ao marcador de sucesso, sem erro de acesso/UB, e termina com **LSan exit 1:
  72.920 bytes em quatro alocações**. O ciclo reduzido também chega ao marcador
  funcional, mas termina com LSan exit 1: **72.936 bytes em oito alocações**; os
  16 bytes adicionais vêm de controles de índices de mais shapes. A pendência
  de memória continua aberta no
  [plano](box2d-liquidfun-unification-plan.md#61-pendência-de-memória-lsan-no-processo-gráfico).

## Medições e limites

Máquina: Linux x86_64, Intel i7-9750H, Mesa Intel UHD 630, OpenGL ES 3.2.
Os testes de jogo usam Release, áudio desativado e janela 800x600. São medições
exploratórias; havia outros testes/compilações na mesma máquina. Não há controle
comprovado de VSync nem cinco repetições com aquecimento descartado.

| Jogo completo (uma execução de 60 s por backend) | Unificado | Antigo |
| --- | ---: | ---: |
| Ratos criados/consumidos | 3/3 | 3/3 |
| Fluidos emitidos/expirados | 3/3 | 3/3 |
| Pico simultâneo de partículas observado | 638 | 637 |
| CPU do processo (s) | 16,195 | 11,347 |
| `game:update` p50 / p95 / p99 (ms) | 0,0248 / 0,0591 / 0,0749 | 0,0200 / 0,0691 / 0,0899 |

A CPU total observada é maior no candidato e precisa ser investigada em execuções
isoladas. Estes números não aprovam performance nem permitem atribuir a diferença
ao solver: entradas por frame, fases de carga e concorrência não foram controladas.
A diferença no pico simultâneo também não é comparação da quantidade total emitida;
a emissão e a expiração são amostradas em frames distintos.

`cpu_s` é CPU do processo durante a cena. `update_p*_ms` mede o intervalo de
`os.clock()` em torno de `game:update`; **não é tempo do solver**. O loop nativo de
física, sincronização do fluido e desenho ocorre fora dessa chamada. O harness
mantém amostras e handles para observação, portanto seu RSS inclui instrumentação.
RSS do cenário reduzido permite procurar crescimento grosseiro ao longo dos ciclos,
mas não substitui diagnóstico de leaks nem prova igualdade de desempenho.

O usuário relatou em 2026-10-13 que testou o Ratworks com seu novo build e tudo
pareceu normal, autorizando avançar ao marco 4. Essa aceitação do cliente não
encerra a pendência de performance. Ainda faltam: medições
isoladas por etapa (Step/partículas/emissão/sincronização/desenho), repetições
comparáveis antigo/unificado e definição do orçamento no dispositivo alvo. Não
avançar para remoção do backend antigo com base somente nestes smoke tests.

## Reprodução

```sh
cmake -S . -B build/physics-ratworks -DPLAT=Linux \
  -DCMAKE_BUILD_TYPE=Release -DUSE_LUA=1 -DUSE_BOX2D=1 \
  -DUSE_BOX2D_PARTICLES=ON -DAUDIO=none -DUSE_TEXTURE_MISSING_DIALOG=0 \
  -DMBM_OUTPUT_ROOT="$PWD/build/physics-ratworks/output"
cmake --build build/physics-ratworks --target mini-mbm box2dLiquidFun -j6
python3 src/test-lib/physics-compat/run-ratworks.py \
  --engine build/physics-ratworks/output/bin/release/linux_x86/mini-mbm \
  --seconds 600 --output /tmp/ratworks-soak
python3 src/test-lib/physics-compat/run-ratworks.py \
  --engine build/physics-ratworks/output/bin/release/linux_x86/mini-mbm \
  --game /home/michel/Ratworks --seconds 60 --output /tmp/ratworks-client
```

Para a referência antiga, configurar outro diretório/output com
`USE_BOX2D=0`, `USE_BOX2D_PARTICLES=OFF`, `USE_BOX2D_LIQUID_FUN=1`, sem `USE_ALL`,
e passar `--legacy --game /home/michel/Ratworks` ao runner. Para OFF, compilar
`box2dLiquidFun` também: a fachada retorna erro explícito sobre a flag e é testada
por `lua-unified.lua` com `MBM_PHYSICS_PROFILE=off`.

O prazo do runner usa relógio real; os checkpoints do cenário reduzido acumulam
`delta`, o mesmo tempo simulado usado pelo plugin. O delta suavizado da engine não
é idêntico a `mbm.getTimeRun()`, especialmente durante o aquecimento. O ciclo em
andamento termina antes do marcador final, por isso a duração pode exceder 600 s.
