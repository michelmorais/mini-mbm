# Future Investigation

Open engineering questions that were surfaced during other work but deliberately not resolved —
either because reproducing them requires conditions this sandbox/dev environment doesn't have
(a real mouse, a specific desktop's display stack), or because fixing them wasn't in scope for the
task that found them. Each entry should have enough context that someone picking it up cold
doesn't have to re-derive the investigation from scratch.

---

## RESOLVED (MBM_VERSION 6.31.9): `mbm.getPickRay` / `obj:collide()` skipped the `camera.scaleScreen2d` correction `mbm.to3d` applies

**Originally found:** 2026-07-15, while debugging a 3D mouse-picking bug in `editor/mesh_debug.lua`'s
Bones node (bone gizmo click-drag — since removed).

**Original status (now superseded):** the investigation below concluded `scaleScreen2d` "evaluates
to 1.0 unless the window is resized after launch, or `-ew`/`-eh` diverges" and therefore couldn't
be the root cause of the reported bug, since no resize/`-ew`/`-eh` was involved. **That assumption
was wrong** — see below.

**Root cause, found 2026-07-21 while revisiting this with a real headless reproduction:** built
`src/test-lib/to3d_investigation.lua`, a scene that places `Crate.msh`/`base.msh`/`building_A.msh`
on-axis at known camera distances (300/600/900 world units) and asserts `mbm.to3d(screenCenter,
depth)` lands exactly on each one — no real mouse needed, since an on-axis marker under an on-axis
camera must hit dead center by symmetry. Running this through the exact launch pattern the
`engine-testing` skill recommends (`--disable_select_monitor -w 800 -h 600`, no `-ew`/`-eh`)
**failed immediately**, off by a fixed proportional amount at every depth. Diagnosis:

1. **`platform-linux/main-lua.cpp` / `platform-macos/main-lua.cpp` hardcoded
   `mbm::set_expected_window_size(1920, 1080)`** whenever `-ew`/`-eh` weren't explicitly passed —
   completely unrelated to the actual `-w`/`-h` requested window size. This is the fast path any
   headless/agent/programmatic launch uses (`--disable_select_monitor` skips the interactive
   monitor-picker dialog, which — in `mini-mbm-lib-Linux.cpp`/`-Windows.cpp` — correctly sets the
   expected size to whatever resolution the human actually picked). The result:
   `camera.expectedScreen` got pinned to `(1920, 1080)` instead of the real `(800, 600)` backbuffer,
   so `camera.scaleScreen2d` came out to `0.556` (`= 600/1080`) instead of `1.0` — **permanently,
   for the whole session, on any launch at a resolution other than exactly 1920×1080.** This is
   exactly the divergence the original investigation assumed couldn't happen without an explicit
   resize or `-ew`/`-eh` — it turns out it happens on effectively every non-1920×1080 headless
   launch. Fixed by defaulting the "expected" size to the actual requested `-w`/`-h` (falling back
   to 1920×1080 only when `-w`/`-h` were *also* omitted, matching `set_window_size`'s own existing
   default), so `scaleScreen2d` comes out to `1.0` unless a caller explicitly opts into
   design-resolution scaling via `-ew`/`-eh`.
2. **`DEVICE::rayCast` (`device-common.cpp`)** — the shared primitive behind `mbm.to3d`,
   `mbm.getPickRay`, and `obj:collide`'s 3D ray/AABB path — only had the `scaleScreen2d` correction
   applied by its one caller inside `transformeScreen2dToWorld3d_scaled` (`mbm.to3d`); the other two
   call sites (`onGetPickRay`, `onCheckCollisionBoundingBoxRenderizable`) passed raw, uncorrected
   screen pixels straight through. This is the original consistency gap this doc entry was about.
   Fixed by moving the `scaleScreen2d` multiplication into `rayCast` itself (and removing the
   now-redundant copy in `transformeScreen2dToWorld3d_scaled`), so all three call sites agree by
   construction. Verified: before the fix, `mbm.getPickRay`'s direction vs. the direction derived by
   sampling `mbm.to3d` at two depths through the same pixel had a dot product of `0.968` (a real,
   measurable ~14° divergence) once `scaleScreen2d` was forced away from 1.0 via `-ew 1920 -eh
   1080`; after the fix, dot product is exactly `1.000` in both the normal (`scaleScreen2d == 1`)
   and design-resolution-scaled (`-ew`/`-eh` set, `scaleScreen2d != 1`) cases.

**On the original bug report** (bone gizmo click-drag failing at camera distance ≥ 5, offset
growing as resolution was lowered): still not directly re-confirmed against the real editor tool
(that feature was already removed, see `mesh_debug_bone_drag_removed` history), but bug #1 above is
a very plausible match in hindsight — any launch not pinned to exactly 1920×1080 got a wrong,
resolution-dependent `scaleScreen2d`, and "offset grows as resolution is lowered" is exactly the
shape of error `scaleScreen2d` diverging further from 1.0 as actual resolution diverges further
from the hardcoded 1920×1080 fallback would produce.

**A residual, non-bug nuance found while building the repro:** `mbm.to3d(centerX, centerY, D)`
only lands exactly on the forward axis when `scaleScreen2d == 1`. When a game *intentionally* opts
into design-resolution scaling (`-ew`/`-eh` set on purpose, `scaleScreen2d != 1` by design),
`transformeScreen2dToWorld3d_scaled` scales the raw screen pixel by `scaleScreen2d` **before**
dividing by the actual backbuffer size to build the NDC coordinate — so the *true* screen-center
pixel no longer produces `vx=vy=0` once that scaling is non-unity. This is a property of `to3d`'s
existing formula, not something touched by the fixes above, and not necessarily wrong — just worth
knowing if a future investigation assumes "screen center always maps to the forward axis"
unconditionally.

**Verification method, for reuse:** `src/test-lib/to3d_investigation.lua` is a live scene, not a
throwaway — it stays in the repo as both an interactive tool (real desktop, real mouse: a marker
cube follows `mbm.to3d` at a selectable depth, 4 more markers trace the frustum cross-section at an
adjustable depth, click-testing runs `obj:collide` against the three reference meshes) and a
headless regression check (numeric self-test asserts on-axis accuracy and `getPickRay`/`to3d`
direction agreement, prints `TO3D_SELFTEST PASS`/`FAIL`, safe to run under `timeout -s KILL` per the
`engine-testing` skill's Path B). See the comment header in that file for full run instructions and
controls.


Ultimas documentações "erradas" e algumas corrigidas:


| Afirmação documentada | Documento | Implementação | Veredito |
|---|---|---|---|
| `Columns`/`NextColumn` estão disponíveis | `lua-api.md:1169` | API removida e não registrada em `imgui-lua.cpp:5708,6968` | documentação desatualizada |
| Posição/tamanho recebem números separados | `lua-api.md:1161` | recebem tabelas `ImVec2`; posição também aceita pivô | documentação desatualizada |
| `TextColored` recebe RGBA separado | `lua-api.md:1179` | recebe tabela de cor + texto | documentação desatualizada |
| `InputText` recebe `maxLen`; multiline recebe `maxLen,w,h` | `lua-api.md:1199` | buffers são automáticos; recebem flags e, no multiline, uma tabela de tamanho | documentação desatualizada |
| `ListBox` é integralmente 0-based ou 1-based | `lua-api.md:1130,1220` | entrada é 1-based, mas retorno atual é 0-based | documentação contraditória; o comportamento do código será explicitado |
| `Button` e imagens recebem largura/altura separados | `lua-api.md:1182,1280` | tamanhos e UVs usam tabelas; `ImageButton` também exige ID | documentação desatualizada |
