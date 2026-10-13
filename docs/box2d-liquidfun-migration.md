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

# Migração e consolidação de Box2D/LiquidFun

Data: 2026-10-13. Engine **7.366.2**. Marco 5 **parcial**: guia, regressão
reproduzível e correções de memória entregues; remoção da árvore antiga pendente.
A aprovação manual do Ratworks está registrada. Windows ainda precisa ser validado.
Continuação desta etapa: [destruição de corpos e validação prolongada](box2d-liquidfun-lifecycle.md).
Veja o [plano](box2d-liquidfun-unification-plan.md) e os
[resultados desta rodada](box2d-liquidfun-consolidation-results.json).

## Perfis de produção

| Uso | Flags CMake, além de `PLAT` |
| --- | --- |
| Somente corpos rígidos | `USE_LUA=1 USE_BOX2D=1 USE_BOX2D_PARTICLES=OFF` |
| Corpos rígidos e fluidos | `USE_LUA=1 USE_BOX2D=1 USE_BOX2D_PARTICLES=ON` |

Passe cada opção com `-D`. `USE_ALL=1` habilita partículas por padrão, mas respeita
`USE_BOX2D_PARTICLES=OFF` explícito. Use diretórios de build e `MBM_OUTPUT_ROOT`
distintos para evitar misturar bibliotecas dos perfis.

`require "box2d"` usa a base 2.4.1. Com partículas ON, o nome
`box2dLiquidFun` é uma fachada para essa mesma implementação; seus defaults de
compatibilidade são preservados. Duas instâncias continuam sendo dois mundos:
compartilhar o backend não faz corpos de mundos separados colidirem.
OFF mantém a fachada para emitir erro explícito de partículas desabilitadas.

Os arquivos exclusivos de partículas permanecem em `third-party/box2d-2.4.1`,
compilados apenas em ON. Não se trata de LiquidFun oficial para Box2D 2.4.1:
é um port local, com origem e adaptações registradas no repositório.
A árvore `third-party/box2d-liquid-fun-2.3.1` e o wrapper antigo ainda são mantidos
para comparação/fallback. O loader embedded unificado chama diretamente `mbm_open_box2d_compat`; somente
o perfil de fallback inclui o header legado. Isso foi compilado/linkado em
Android ARM64 ON/OFF; não substitui execução em dispositivo.

## Validação Windows

Executar em Developer Command Prompt com MSVC e Python 3, a partir da raiz.
Reconstruir e testar cada perfil antes de trocar, pois o output é compartilhado:

```cmd
msbuild platform-msvs\mini-mbm.sln /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:MbmBackend=DirectX11 /p:MbmBox2dParticles=0
py -3 src/test-lib/physics-compat/run-lua-contracts.py --engine bin/release/windows_x86/mini-mbm.exe --profile off --output build/physics-windows/dx11-off
msbuild platform-msvs\mini-mbm.sln /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:MbmBackend=DirectX11 /p:MbmBox2dParticles=1
py -3 src/test-lib/physics-compat/run-lua-contracts.py --engine bin/release/windows_x86/mini-mbm.exe --profile on --output build/physics-windows/dx11-on
```

Confirmar o caminho do executável no output do MSBuild. Repetir para DirectX9 e
OpenGLES, com diretórios de resultados distintos. A correção de destruição dos
buffers DirectX9 nesta rodada também precisa dessa validação.
O runner exige `box2d.dll` e `box2dLiquidFun.dll` ao lado do executável, isola o
carregamento dos plugins, grava logs e hashes e exige saída zero mais marcador de
sucesso. Esperado: **1/1 OFF e 6/6 ON**. Inclui ambas as ordens de require,
emissão gradual, filtros, contorno convexo e profundidade/captura dos fluidos.
Esse roteiro foi executado no Linux; sua execução Windows permanece pendente.

## Reprodução Linux e memória

O mesmo runner aceita `.so` e o executável Linux. Para um build completo Debug
com `USE_ASAN=1`, Lua/Box2D habilitados e áudio `none`:

```sh
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
__EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
python3 src/test-lib/physics-compat/run-lua-contracts.py \
  --engine build/physics-consolidation-asan/output/bin/debug/linux_x86/mini-mbm \
  --profile on --output /tmp/physics-consolidation-asan
```

Repetir com o build OFF e `--profile off`. A seleção Mesa acima é específica
deste host Intel/Mesa; não é supressão de sanitizador nem requisito geral do jogo.
Não use esse caminho em máquinas sem esse driver.

Nesta rodada, ASan/UBSan/LSan e a checagem ODR padrão passaram: **6/6 ON, 1/1 OFF**.
Os mesmos contratos passaram em Release, e `testLib 2 Crate.msh 3d` terminou
com saída zero sem relatos dos sanitizadores no ambiente Mesa.

| Problema identificado | Tratamento e limite |
| --- | --- |
| Controles CPU de índices/subsets não liberados | Destrutores OpenGL ES e DirectX9 passam por `release()`; DirectX11 já fazia isso |
| ODR de tinyfiledialogs em Linux | Removida compilação duplicada no executável; biblioteca continua em core_mbm |
| 72.704 bytes no descarregamento da fachada | Pilha em `_GLOBAL__sub_I_eh_alloc.cc`; link C da fachada Linux evita outra cópia estática do runtime C++ |
| Erros Lua de argumentos inválidos | Mensagem transferida à pilha Lua antes do longjmp; validações riseTime/mask precedem alocação de shapes |
| 208 bytes no ambiente EGL padrão | Pilha simbolizada em `libnvidia-egl-gbm.so.1.1.0`, `loadEGLExternalPlatform`; permanece questão externa acompanhada |

Com enumeração EGL padrão, o contrato moderno termina funcionalmente mas LSan
retorna erro pelos **208 bytes** externos. Selecionando Mesa, não há relatos nos
casos acima. Isso não certifica todos os drivers, todos os caminhos de erro Lua,
nem certifica gameplay prolongado de todos os jogos. A continuação em 7.366.2
validou o Ratworks real por 60 s e 75 ciclos do cenário reduzido em dez minutos,
com sanitizadores completos; veja o relatório de ciclo de vida.
Não foram usadas supressões nem `detect_leaks=0`/`detect_odr_violation=0`.

## Fallback e critérios de remoção

O fallback de comparação continua disponível em build CMake separado:
`USE_LUA=1 USE_BOX2D=0 USE_BOX2D_PARTICLES=OFF USE_BOX2D_LIQUID_FUN=1`, sem
`USE_ALL`. Carregar somente esse conjunto de bibliotecas no processo.

Antes da remoção definitiva, concluir a matriz de plataformas (incluindo Windows,
Android em dispositivo e Apple), a regressão dos jogos rígidos publicados,
a equivalência interativa/visual dos exemplos externos e a aceitação de performance.
Os 140 fragmentos de documentação ainda precisam de contexto; as 40 cenas já
executadas não cobrem todas as interações. Preservar uma revisão identificada do
fallback e atualizar referências de build/embedded/documentação antes de apagar
as pastas antigas. Os relatórios anteriores permanecem como evidência histórica.
