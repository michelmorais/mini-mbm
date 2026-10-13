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

# Box2D/LiquidFun: validação Windows MSVC

Data: 2026-10-13. **Validação funcional MSVC Win32 aprovada no escopo abaixo**.
Builds padrão Debug/OpenGL ES e Release/DirectX11 disponíveis com partículas ON;
DirectX9 validado em output separado. A migração geral permanece parcial.
[Resultados, hashes e falhas iniciais](box2d-liquidfun-windows-results.json).

## Correções encontradas na execução

| Problema | Correção e evidência |
| --- | --- |
| C2589/C2059 no `plugin-helper.cpp` | Proteger `std::max` e `numeric_limits<float>::max` contra a macro Windows. A solução passou a compilar e os LNK1104 em cascata desapareceram. |
| Fluido invisível em DirectX11 e DirectX9 | Os quadriláteros de fluido eram descartados por orientação da face. Renderizar os dois lados, como no OpenGL ES, preservando teste de profundidade e ausência de escrita de profundidade. As capturas falhavam antes e passaram depois. |
| Verificação de manifesto falhava no Windows | Usar componentes de `Path` para reconhecer o diretório `particle`, sem presumir separador `/`. |
| Runners Ratworks/exemplos presumiam Linux | Selecionar `.dll`, isolar o caminho dos plugins, usar caminhos Lua com `/` e registrar o core Windows. A exceção de proprietário do Git fica limitada ao comando de leitura do checkout explicitamente testado, sem alterar configuração global. |
| Cenário reduzido dependia do FPS do host | Fixar passo de 1/60 s. Aos 3 s, o AABB superior foi 20,89 no passo variável e 18,73 a 60 Hz; aos 4 s foi 1,38. A emissão termina perto de 2,1 s após as pausas. A checagem ocorre agora aos 4 s, mantendo limites (-2,18), geometria, máscara, gravidade, quantidade, lifetime e demais asserts. |
| Falha de captura esperava timeout | O smoke de profundidade registra `FLUID_DEPTH_SMOKE_FAILED` e encerra em caso de assert. Sucesso ainda exige o marcador positivo e pixels de fluido expostos, sem pixels sobre a geometria da frente. |

Nenhuma alteração no solver de física, API pública ou fronteira PIMPL foi
necessária. O erro visual demonstra por que o sucesso da física/Lua sozinho não
aprova a renderização. O teste reduzido foi ajustado explicitamente; seu resultado
não deve ser comparado como se tivesse o mesmo timestep da rodada Linux anterior.

## Matriz executada

MSVC 19.51.36260.0, Visual Studio 18.10.1, SDK 10.0.28000.0, binários **x86**.
A configuração `x64` da solução atual também aponta para projetos Win32.
`MbmCoreFeatureDefines=AUDIO_ENGINE_PORT_AUDIO` desabilitou o seletor de textura
ausente nos testes. Nenhum arquivo do Ratworks foi modificado; os runners conferem
hashes dos scripts e binários antes/depois de cada execução.

| Verificação | Resultado |
| --- | --- |
| Solução Debug/OpenGL ES e Release/DirectX11 | Build completo sem erros; warnings preexistentes permanecem |
| Solução Release/DirectX9 isolada | Build completo sem erros |
| Contratos Lua, OpenGL ES Debug | 6/6 ON, 1/1 OFF |
| Contratos Lua, DirectX11 Release | 6/6 ON, 1/1 OFF |
| Contratos Lua, DirectX9 Release | 6/6 ON, 1/1 OFF |
| Física nativa MSVC Release | 4/4: rígido, legado, candidato e igualdade do trace rígido OFF/ON |
| `libTest 2 Crate.msh 3d`, DX11 e DX9 | Malha carregada, timeout interno atingido, saída zero |
| Ratworks, 60 s em cada um dos três backends | 3 ratos consumidos, 3 fluidos emitidos/expirados, fluido sobrevive à remoção do rato |
| Cenário reduzido, OpenGL ES Debug, 600 s | 63 ciclos em 601,207 s; pico de 385 partículas; pausa, máscara, lifetime, reemissão e handles inválidos aprovados |
| Exemplos da documentação, DX11 OFF | 31 cenas rígidas passaram |
| Exemplos da documentação, DX11 ON | 40 cenas passaram; as 9 de fluido foram repetidas após a correção visual |
| DLLs | Fachada depende de `box2d.dll`; OFF sem exports `b2ParticleSystem`; manifesto 59 fontes ON, 50 OFF, 2 na fachada |

Os exemplos mantêm os hashes dos blocos originais. Os 140 fragmentos que exigem
contexto não foram apresentados como cenas aprovadas. Os testes de exemplo são
smokes de dois segundos simulados, sem reprodução de entrada ou comparação com
os GIFs publicados. As capturas abaixo validam especificamente a cena de
profundidade, não toda a apresentação do jogo ou dos exemplos.

| DirectX11 antes | DirectX11 corrigido | DirectX9 corrigido |
| --- | --- | --- |
| ![Fluido ausente](box2d-liquidfun-windows-dx11-before.png) | ![Fluido com oclusão correta](box2d-liquidfun-windows-dx11.png) | ![Fluido com oclusão correta](box2d-liquidfun-windows-dx9.png) |

O MSBuild FileTracker precisou executar fora do sandbox. DirectX9 também precisou
do desktop fora do sandbox: dentro dele, a criação do dispositivo falhava antes
de carregar a cena. O mesmo binário funcionou fora dele. Esses erros de ambiente
não foram tratados como regressões da física nem como testes aprovados.

## Reprodução

O [guia de migração](box2d-liquidfun-migration.md#validação-windows) contém os
comandos ON/OFF atualizados. Para a configuração usada pelo autor, usar
`Configuration=Debug`, `Platform=x86`, `MbmBackend=OpenGLES`, com executável
`platform-msvs/Debug/mini_mbm.exe`. Para Release, o padrão é DirectX11 e a pasta
é `platform-msvs/Release`. Não executar testes enquanto recompila suas DLLs.

Nesta rodada, as trocas ON/OFF reconstruíram o projeto `box2d.vcxproj` com
`/t:Rebuild /p:Platform=Win32 /p:SolutionDir=<raiz>/platform-msvs/`, preservando
as demais DLLs do mesmo backend. Os builds completos e os links finais usaram
`mini-mbm.sln /t:Build /p:Platform=x86`. O caminho isolado DirectX9 foi
`build/physics-windows/dx9-bin/`, via `OutDir`, e cada projeto usou
`IntDir=build/physics-dx9/`. As DLLs de runtime portaudio/ANGLE/d3dcompiler do
repositório foram copiadas para esse output antes da execução.

```powershell
py -3 src/test-lib/physics-compat/run-ratworks.py --engine platform-msvs/Release/mini_mbm.exe --game C:/Users/miche/Documents/Ratworks --seconds 60 --output build/physics-windows/ratworks
py -3 src/test-lib/physics-compat/run-ratworks.py --engine platform-msvs/Debug/mini_mbm.exe --seconds 600 --output build/physics-windows/soak
cmake -S src/test-lib/physics-compat -B build/physics-windows-native -G "Visual Studio 18 2026" -A Win32 -DPLAT=Windows
cmake --build build/physics-windows-native --config Release --parallel 4
ctest --test-dir build/physics-windows-native -C Release --output-on-failure
py -3 src/test-lib/physics-compat/check-msvs.py
```

Logs completos e relatórios individuais ficam em `build/physics-windows/`;
logs MSBuild em `build/windows-*.log`, testes nativos em
`build/physics-windows-native/`. O JSON versionado guarda resultados, hashes e
limites mesmo quando os outputs de build forem limpos.

## Pendências preservadas

Esta rodada não aprova performance: houve builds/testes concorrentes e o relógio
Lua não fornece métricas comparáveis entre plataformas. Não executou sanitizadores
MSVC e o runner não coleta RSS no Windows; dez minutos funcionais não provam ausência
de vazamentos. Permanecem pendentes x64 real, MinGW, Android em dispositivo,
macOS/iOS, playtest completo dos jogos rígidos publicados e equivalência
interativa/visual dos exemplos. Manter a árvore antiga e os critérios dos marcos
4/5 no [plano](box2d-liquidfun-unification-plan.md).
