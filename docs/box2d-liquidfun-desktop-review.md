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

# Revisão integrada após Windows e macOS

Data: 2026-10-13. Commits revisados: `b6229260` (Windows) e `6c63b7fa`
(macOS), contra `f0c6ed8b`. Working tree limpo no início da revisão.
Não foram identificados achados bloqueantes no escopo revisado. Isso não encerra
a aceitação geral dos marcos 4/5 nem autoriza remover a árvore antiga.

Atualização posterior: o usuário aceitou os exemplos como cobertura dos jogos antigos
e aprovou consolidar Linux, Windows Win32 e macOS arm64. A decisão e a remoção
estão no [relatório final desktop](box2d-liquidfun-retirement.md); o roteiro abaixo
registra a situação anterior a essa decisão.

## Correções e evidências conferidas

- Proteções contra a macro Windows `max` preservam as validações numéricas.
- DX9/DX11 alteram o culling no caminho de `FLUID_GROUP`; o desenho genérico de
  meshes mantém sua seleção. As capturas e contratos registrados verificam fluido
  visível e oclusão na cena sintética.
- `FLUID_POLYGON_SHAPE` mantém o predicado especial restrito ao preenchimento de
  triângulos. Produtos em double preservam os produtos dos operandos float; o
  solver rígido permanece intacto. A união instantânea chama virtualmente
  `TestPoint`; a emissão gradual também usa esse predicado sem guardar os shapes
  locais depois do preenchimento da fila.
- O guard de intervalo de animação acontece depois da atualização dos efeitos de
  shader e antes de acumular tempo/dividir. O teste cobre intervalo zero,
  negativo e retomada com intervalo válido.
- Runners selecionam DLL/dylib/SO e isolam bibliotecas. O cenário reduzido mudou
  para passo fixo: seus números não são diretamente comparáveis à baseline variável.
- Os dez hashes de fontes registrados no JSON macOS correspondem ao checkout
  revisado. Os registros de contratos aceitos Windows/macOS têm saída zero,
  marcador positivo e binários inalterados. Falhas iniciais macOS permanecem
  registradas, distinguindo-as dos resultados corrigidos.

## Estado consolidado

| Plataforma | Evidência aceita no escopo registrado | Limites |
| --- | --- | --- |
| Linux | Histórico ON/OFF, sanitizadores, benchmarks e nova regressão integrada ON | Nova rodada abaixo sem sanitizadores |
| Windows | MSVC Win32, GLES/DX11/DX9 ON/OFF, Ratworks, nativos e exemplos autossuficientes | Não é x64 real, MinGW ou validação de sanitizadores Windows |
| macOS | Apple Silicon/Metal, contratos ON/OFF, ASan/UBSan, Ratworks, exemplos e link MAS | Sem LSan, Intel, execução MAS assinada ou iOS |
| Android | Build/link ARM64 ON/OFF anteriormente registrados | Execução em aparelho ainda pendente |

Os relatórios [Windows](box2d-liquidfun-windows.md) e
[macOS](box2d-liquidfun-macos.md) são as fontes das execuções nessas plataformas;
não foram reexecutadas neste host Linux.

Nova rodada Linux, código `6c63b7fa`: build Debug de `mini-mbm`, fachada e `testLib`
concluído; contratos ON **6/6**; `testLib --animation-zero-interval-test` passou;
cenário reduzido com duração solicitada de 16 s passou em **17,113 s**, com **5
ciclos**, pico de 385 partículas e entradas inalteradas. O passo simulado fixo
não equivale ao tempo de parede. Manifesto MSVS passou: 59 fontes ON, 50 OFF,
duas na fachada. Resultados e hashes: [registro](box2d-liquidfun-desktop-review-results.json).

## Próxima entrega: aceitação dos clientes e exemplos (continuação do marco 4)

1. Regressão de Tower Defense Monster e Spaceship Combat com OFF e ON: conferir
   inicialização, colisões, sensores, projéteis, joints realmente usados,
   destruição, transições de cena e encerramento. Registrar versão, roteiro e
   resultado por jogo; smoke de inicialização não substitui gameplay.
2. Para os exemplos de documentação, reproduzir as entradas e comparar os efeitos
   pretendidos com suas referências. Priorizar joints e fluidos; dar contexto aos
   fragmentos relevantes sem contabilizar os 140 fragmentos como cenas executadas.
3. Registrar diferenças e corrigir regressões antes da remoção do backend antigo.
   A aprovação da flag CMake pelo autor permanece: OFF/ON será mantido.

Em paralelo a essa aceitação funcional, continuar a matriz de plataformas:
Android em aparelho, iOS, Windows x64 real/MinGW e limites macOS acima. Qualquer
redução formal desse escopo deve ser uma decisão explícita do projeto. A remoção
do legado segue como entrega final do marco 5 após fechar os critérios aplicáveis.
