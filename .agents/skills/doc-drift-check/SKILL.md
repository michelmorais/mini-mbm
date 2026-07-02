---
name: doc-drift-check
description: 'Systematic workflow for verifying mini-mbm docs against the actual implementation and fixing whichever side is stale. Use when: asked to check/audit/verify docs against code, before/after touching mesh-v11-io.cpp or header-mesh.h (docs/mesh-v11-format.md), before/after touching src/lua-wrap/** (docs/lua-api.md), before/after touching shader*.cpp/light.h (docs/light.md), or generally whenever a docs/*.md file describes binary layouts, invariants, or an API surface that could have silently drifted from the code.'
---

# Doc Drift Check Skill — mini-mbm

## When to Use

This repo has docs that assert precise, falsifiable claims about the implementation — binary
struct layouts, "always/never" invariants, API signatures — not just prose descriptions. Those
claims rot silently: someone changes the code and forgets the doc, or writes the doc from intent
before the code fully matched it. `4a06285` is the proof this is a real, recurring failure mode in
this repo: a doc-vs-code pass on `docs/mesh-v11-format.md` didn't just find stale prose, it found a
genuine bug (`mesh-v11-io.cpp` skipped the `crc32Value` check for zero-length uncompressed
sections, contradicting the doc's own stated invariant that crc32 is validated uniformly).

Invoke this skill when asked to audit a doc against code, or proactively before/after a change that
touches one of the doc/implementation pairs below.

## Doc ↔ Implementation Map

| Doc | Implementation | Nature of claims |
|---|---|---|
| `docs/mesh-v11-format.md` | `src/core_mbm/mesh-v11-io.{h,cpp}`, `src/core_mbm/mesh-manager.cpp`, `include/core_mbm/header-mesh.h` | Binary struct layouts (field name/type/order), section-presence invariants, enum values |
| `docs/lua-api.md` | `src/lua-wrap/**`, `src/lua-wrap/render-table/**` | Function names, arg/return signatures, `:` vs `.` method call convention, constant values |
| `docs/light.md` | `include/core_mbm/light.h`, `src/core_mbm/shader*.cpp`, `src/core_mbm/shader-fx.cpp`, `src/core_mbm/core-manager-common.cpp` | Light-count limits, material model behavior, shader input names, classification-at-creation-time contract |

If the doc in question isn't in this table, find its implementation by grepping for the doc's own
distinctive identifiers (struct names, function names) — the doc almost always names the exact
symbols to search for.

## Procedure

1. **Extract every falsifiable claim from the doc**, not just the ones that look like code. A claim
   is falsifiable if code could contradict it. In order of how often they hide real bugs (highest
   first):
   - **"Always / never / exactly one" invariants** (e.g. "`SECTION_DETAIL_PHYSICS` is the one
     section every mesh type gets", "`crc32Value` is always written... regardless of compression").
     These are the ones worth checking most carefully — verifying them means finding the actual
     branch that would violate the claim and confirming it can't be taken, not just reading the
     happy path.
   - **Struct/payload field order and type** — on-disk layouts in `mesh-v11-format.md` must match
     the read/write pair in `mesh-v11-io.cpp` field-by-field, in order. A field written in a
     different order than documented is a real corruption risk, not a nitpick.
   - **Enum / constant values** — `SECTION_TYPE`, blend states, etc. Compare doc values against the
     `enum` definition, not against usage sites (usage can be wrong in the same stale way as docs).
   - **Function signatures and call conventions** — for `lua-api.md`, check the doc's method
     against the actual `luaL_Reg` registration and C++ function it binds to, including `:` vs `.`
     dispatch (§1b already documents this distinction — a method registered without `self` handling
     but documented with `:` is a real mismatch). Also check the **return value**, not just the
     signature: a Lua-bound wrapper's real return is whatever it pushes and `return`s from the
     `lua_State` — `return 0` (nothing), `return 1` (one value), or `lua_error_debug(...)` (thrown
     Lua error) on failure — which is *not* the same as the C++ function's `bool`/etc. return type
     it wraps. A wrapper can call a `bool`-returning C++ function and still push nothing to Lua on
     success, signaling failure only via a thrown error — documenting it as `Returns: bool` is a
     real and easy-to-make mistake. Where feasible, verify by actually running the call (see the
     `engine-testing` skill) instead of inferring the Lua return from the C++ signature.
   - **Cross-references to other files/line numbers** the doc cites as evidence (e.g.
     `mesh-manager.cpp:905-998`) — these go stale first as the referenced file is edited. A stale
     reference is low severity but flags the doc probably wasn't touched in that commit.
   - **Missing entries — code exposes something the doc never mentions at all.** This is the one
     check that runs *code → doc* instead of doc → code: grep the actual registration surface
     (e.g. every `luaL_Reg` table in `src/lua-wrap/**` for `docs/lua-api.md`) for functions with no
     matching doc section, rather than only checking existing doc claims against code. Found twice
     in one pass in this repo already: a fully-implemented Lighting API (`mbm.setLightEnabled`,
     `setAmbientLight`, `setDirectionalLight*`, `setPointLight*`, `addPointLight`,
     `clearPointLights`, `setRequestedMaxLights`, `getSupportedMaxLights`, `getValidatedMaxLights`,
     `setLightSelectionMode`, `getSelectedPointLights`, `getLightState`, `resetLight` — all
     registered in `src/lua-wrap/framework-lua.cpp`) and `mesh:loadAsync`
     (`src/lua-wrap/render-table/mesh-lua.cpp`) were both live and callable but entirely absent
     from `docs/lua-api.md`.

2. **For each claim, find the concrete code that could violate it** and check it directly — don't
   infer from a nearby comment, since comments drift exactly as fast as docs do (the `4a06285` fix
   included a stale comment in `mesh-manager.cpp` alongside the doc fix). Grep for the struct name,
   section type, or function name, and read the actual read/write/dispatch logic.

3. **Classify every mismatch found** as one of:
   - **Doc is stale** — code is correct/intentional, doc describes old behavior. Fix the doc.
   - **Code is stale/buggy** — doc describes the intended invariant correctly, code doesn't uphold
     it. Fix the code. Flag these clearly since they're functional bugs, not just documentation debt.
   - **Ambiguous/underspecified** — can't tell intent from either side. Ask before changing anything.

4. **Report findings before fixing**, as a table: claim → doc location → code location → verdict.
   Then apply fixes for the ones with a clear verdict; surface ambiguous ones to the user instead of
   guessing.

## Notes

- Don't try to re-verify every prose sentence in a 400-900 line doc — focus on claims that are
  concrete enough to be wrong (numbers, field names, "always/never", struct order). Narrative
  rationale ("this removes a whole category of failure mode") isn't falsifiable and isn't the point
  of this check.
- If the doc has a "Milestone N Decisions" or "Future Work" section, those are explicitly
  scoped-out or historical — don't flag them as drift just because the code doesn't implement them
  yet; check instead that the doc still correctly says they're *not* implemented.
- For `docs/lua-api.md` claims specifically, prefer verifying by actually calling the Lua function
  through a running engine over reading the binding source alone — see the `engine-testing` skill
  for how to launch `mini-mbm`/`testLib` headlessly with a timeout. Reading the C++ wrapper can
  still get the return-value drift wrong (see the return-value bullet above); running it doesn't.
