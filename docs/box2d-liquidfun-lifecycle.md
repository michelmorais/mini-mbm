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

# Consolidação: destruição de corpos e validação prolongada

Data: 2026-10-13. Engine **7.366.2**. Continuação do marco 5; não há marco 6
no plano original. Windows e os demais critérios de remoção continuam pendentes.

## Falha encontrada no Ratworks real

A engine inteira instrumentada parou no consumo do primeiro rato com
`AddressSanitizer: heap-use-after-free` em `PHYSICS_BOX2D::safeDestroyBody`.
O método apagava a entrada de `lsShape` e continuava o loop comparando
`infoBox2d->ptr`; esse registro já havia sido liberado. A leitura só acontecia
quando havia outro corpo depois dele na lista.

A correção preserva o ponteiro do renderizável antes da liberação e usa essa
identidade nas comparações seguintes. Não altera o solver, as forças, os defaults
nem acrescenta trabalho ao loop normal por frame. A destruição continua adiada
até depois de `world->Step`, preservando o comportamento dos callbacks existentes.

O contrato `lua-unified.lua` agora coloca um terceiro corpo depois do corpo
removido, verifica a invalidação do joint e confirma que o corpo sobrevivente
continua respondendo a `testPoint`. A versão anterior reproduziu o mesmo erro
ASan com esse teste; a corrigida passou em ON e OFF. O backend legado foi
preservado como referência, sem aplicar esta correção à sua árvore.

## Loader embedded

Com `USE_BOX2D`, `require_embedded.cpp` inclui apenas o header do backend
unificado e chama `mbm_open_box2d_compat` para os dois nomes de LiquidFun.
O fallback `USE_BOX2D_LIQUID_FUN` sem `USE_BOX2D` mantém o header e a chamada
originais. Nenhuma assinatura pública ou nome de módulo mudou.

Android ARM64/API 24, NDK r29: engine completa compilada e linkada em ON/OFF.
Os arquivos de dependência do objeto do loader não contêm o header antigo;
a biblioteca final importa `mbm_open_box2d_compat`, exportado por `box2d.so`.
O fallback passou em compilação de sintaxe. Não houve execução em aparelho.

Um adaptador temporário Linux também expôs a função real do loader à cena
`lua-unified.lua`: os contratos ON (LiquidFun primeiro) e OFF passaram com
sanitizadores. Isso verifica o despacho do loader com o backend real, mas não
substitui a execução nas plataformas embedded. O teste foi preparado em `/tmp`;
os hashes e comandos de reprodução ficam no registro desta rodada.

## Validação e limites

Resultados e hashes: [registro da rodada](box2d-liquidfun-lifecycle-results.json).
Ambiente: Linux x86_64, seleção explícita Mesa neste host Intel,
`ASAN_OPTIONS=detect_leaks=1`, `UBSAN_OPTIONS=halt_on_error=1`, ODR padrão ativo,
sem supressões. A pendência externa EGL/NVIDIA de 208 bytes continua descrita no
[guia](box2d-liquidfun-migration.md).

- Contratos atualizados: **6/6 ON, 1/1 OFF**, incluindo o corpo sobrevivente.
- Ratworks real: **61,348 s**, três ratos consumidos, três fluidos emitidos e
  expirados, todos sobrevivendo à remoção do rato; saída zero e nenhum relato de
  ASan/UBSan/LSan. Nenhum arquivo do jogo foi modificado.
- Teste prolongado: **75 ciclos em 602,583 s**, pico de 385 partículas, saída
  zero, hashes de entrada inalterados e nenhum relato de ASan/UBSan/LSan.
- Build normal local atualizado; manifestos Visual Studio continuam consistentes.

RSS mediano foi de 284.296 KiB (60–120 s) para 536.114 KiB (540–600 s).
Esse aumento no processo instrumentado não deve ser apresentado como RSS estável
nem comparado diretamente ao Release; LSan aprovou a liberação no encerramento.

As medições de CPU desta rodada instrumentada e concorrente não são orçamento de
performance de produção. A aprovação funcional manual anterior do Ratworks não
substitui validação Windows, regressão completa dos jogos rígidos publicados ou
equivalência interativa/visual dos exemplos externos. A árvore antiga permanece.

## Reprodução

Com o build instrumentado ON descrito no guia, executar separadamente:

```sh
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
__EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
python3 src/test-lib/physics-compat/run-ratworks.py \
  --engine build/physics-consolidation-asan/output/bin/debug/linux_x86/mini-mbm \
  --seconds 600 --output /tmp/physics-lifecycle-soak

ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
__EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
python3 src/test-lib/physics-compat/run-ratworks.py \
  --engine build/physics-consolidation-asan/output/bin/debug/linux_x86/mini-mbm \
  --game /home/michel/Ratworks --seconds 60 --output /tmp/physics-lifecycle-game
```

A seleção Mesa é específica deste ambiente. Exigir código zero, marcador de
sucesso, hashes inalterados e nenhum relato de sanitizador; RSS sozinho não
aprova ausência de vazamento.
