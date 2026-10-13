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

# Box2D unificado: plataformas e regressão (marco 4)

Data: 2026-10-13. **Marco 4 parcialmente executado.** O usuário testou seu novo
build com Ratworks e relatou comportamento normal, autorizando esta etapa.
Isso registra a aceitação do cliente; não transforma os números exploratórios
anteriores em aprovação de performance.

Continuação no Windows: [validação MSVC Win32](box2d-liquidfun-windows.md),
com builds, contratos ON/OFF, Ratworks e correções visuais em DirectX9/11.
A tabela abaixo registra a rodada Linux original; o relatório Windows acrescenta
as evidências de execução sem encerrar as demais pendências.

Continuação no macOS: [validação Apple Silicon/Xcode/Metal](box2d-liquidfun-macos.md),
incluindo builds dinâmicos e link estático MAS ON/OFF, contratos Lua e correção do
preenchimento de polígonos de fluido. A linha macOS/iOS abaixo é o estado histórico
da rodada Linux; iOS e execução MAS assinada permanecem pendentes.

## Implementação

A solução Visual Studio agora usa o mesmo desenho do CMake: `box2d` compila a base
2.4.1 e, opcionalmente, as partículas; `box2dLiquidFun` contém apenas a fachada e
seu `DllMain`. A referência entre projetos garante a dependência do import library
`box2d.lib`. Não há fontes da árvore LiquidFun antiga nesse projeto.

`MbmBox2dParticles` aceita `0` ou `1` e tem default `1`, preservando a disponibilidade
de fluidos da solução completa. OFF exclui nove fontes: sete de partículas e duas
de bindings de fluido. Ambas as configurações mantêm a fachada para retornar erro
explícito quando partículas estão desabilitadas. A compilação do backend declara
C++17 nas quatro configurações Win32/x64 Debug/Release.

O runner de exemplos agora isola `LD_LIBRARY_PATH`, `LUA_CPATH` e `package.cpath`
para evitar carregar plugins do build normal ao testar outro perfil. Permite
`--plugin box2d` para a matriz OFF. O inventário ganhou `--examples-only`, sem
precisar de um checkout upstream para repetir os testes da documentação.

Versão atualizada para **7.366**; build Linux normal recompilado.

## Evidências

[Resultados e hashes](box2d-liquidfun-platform-results.json).

| Verificação | Resultado | Limite |
| --- | --- | --- |
| Linux Release ON/OFF | Engine e plugins compilados | Ambiente local x86_64 |
| Contratos Lua | ON nas duas ordens de require e OFF passaram | Não equivale a playtest completo |
| Testes nativos | 4/4, incluindo igualdade do trace rígido OFF/ON | Não prova igualdade com todos os jogos |
| Documentação, ON | 40 cenas passaram: 31 rígidas + 9 de fluidos | Smoke de 2 s simulados; callbacks de entrada não reproduzidos |
| Documentação, OFF | 31 cenas rígidas passaram | Exemplos de fluido fora desse perfil |
| Android NDK r29, ARM64, API 24 | Engine completa e plugin linkados ON/OFF | Sem execução em aparelho/emulador |
| Símbolos Android | Dois nomes de entrada no mesmo backend; partículas ausentes em OFF | Inspeção dos artefatos, não runtime |
| Visual Studio | Manifestos consistentes: 59 fontes ON, 50 OFF, 2 na fachada | MSBuild/MSVC não disponíveis neste Linux |
| macOS/iOS | Pendente | Sem SDK Apple/host correspondente |

Os 180 blocos Lua da documentação incluem 140 fragmentos que precisam de contexto.
Não foram classificados como aprovados: o runner executou somente as 40 cenas
com construção autossuficiente. Tampouco é possível concluir equivalência visual
ou efeito interativo só porque os exemplos não geraram erros.

`/home/michel/mbm-documentation/doc/box2d.rst` e `box2LiquidFun.rst` receberam notas
sobre as flags e a fachada. **Nenhum dos 180 blocos Lua foi modificado**; seus hashes
foram comparados antes/depois. Os exemplos testados não exigiram adaptação de código.
As mudanças na documentação externa estão no working tree daquele repositório,
sem commit ou publicação.

## Pendências para encerrar o marco e consolidar

- Completar Windows x64 real/MinGW, os limites macOS registrados na continuação,
  iOS e execução em dispositivo Android; conferir
  renderização dos fluidos nos backends gráficos correspondentes.
- Fazer regressão completa dos jogos rígidos publicados. Foram localizadas cópias
  em `/home/michel/tower-defense` e `/home/michel/space-ship-combat`, mas seu gameplay
  completo não foi executado nesta rodada. Os testes nativos não substituem isso.
- Reproduzir os callbacks e comparar os efeitos visuais dos exemplos. Dar contexto
  aos fragmentos relevantes ainda não cobertos, sem apresentá-los como 140 falhas.
- Concluir medições isoladas de performance e a pendência LSan/ODR já registrada
  no plano. Nenhuma dessas pendências foi suprimida pela aceitação do Ratworks.

A árvore antiga permanece como referência. Não iniciar a remoção do marco 5
com base apenas nos resultados desta rodada.

## Reprodução

Preparar os dois perfis Linux conforme o relatório de integração, usando
`MBM_OUTPUT_ROOT` distinto e `USE_TEXTURE_MISSING_DIALOG=0`. Nesta rodada:
`build/physics-platform-on` e `build/physics-platform-off`, Release, Lua/Box2D ON,
áudio `none`, variando somente `USE_BOX2D_PARTICLES`.

```sh
python3 src/test-lib/physics-compat/inventory.py --examples-only \
  --documentation /home/michel/mbm-documentation --output /tmp/physics-examples
python3 src/test-lib/physics-compat/run-examples.py \
  --inventory /tmp/physics-examples/inventory.json \
  --documentation /home/michel/mbm-documentation \
  --engine build/physics-platform-on/output/bin/release/linux_x86/mini-mbm \
  --output /tmp/physics-examples-on
python3 src/test-lib/physics-compat/run-examples.py \
  --inventory /tmp/physics-examples/inventory.json \
  --documentation /home/michel/mbm-documentation --plugin box2d \
  --engine build/physics-platform-off/output/bin/release/linux_x86/mini-mbm \
  --output /tmp/physics-examples-off
python3 src/test-lib/physics-compat/check-msvs.py
```

Para Android, configurar com `NDK_ROOT=/home/michel/android-ndk-r29`,
`PLAT=Android`, `ANDROID_ABI=arm64-v8a`, `ANDROID_NATIVE_API_LEVEL=24`,
`USE_LUA=1`, `USE_BOX2D=1`, `AUDIO=none` e outputs separados; compilar o target
`mini-mbm` em ON e OFF. A biblioteca final depende de `box2d.so`; o alias legado
está dentro dessa biblioteca, sem `box2dLiquidFun.so` separado.

Comandos MSBuild ON/OFF: [notas do Visual Studio](../platform-msvs/README.md#optional-box2d-particles).
