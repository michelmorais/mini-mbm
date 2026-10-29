# Real-Time Facial Animation - Future Work

Status: **Optional follow-up work; the current facial runtime is implemented.**

The implemented facial runtime contract is documented with the engine state in:

- [Real-Time Skeletal Animation and Editor](realtime-skeletal-animation.md#facial-composition-on-the-same-mesh)
- [Mesh V11 format](mesh-v11-format.md#facial-sections-45-46-version-1)
- [Lua API - Facial targets and clips](lua-api.md#facial-targets-and-clips-mesh)

This document lists only capabilities that are not part of the current engine contract. They are
independent follow-up projects and do not reopen the delivered Mesh V11 sections, Lua API, or
morph-then-skin composition.

## Live capture integration

Add optional transport adapters for OSC first, followed by webcam or vendor SDK integrations such
as ARKit. Reuse the existing named-weight mapping and deterministic replay contracts without
changing Mesh V11.

The work should define connection state, packet timeout, neutral reset, calibration, target
diagnostics, and optional recording to a facial clip. Transport code should remain outside the core
asset format and should not change the existing manual-weight or clip-playback API.

## Native target authoring

Add an optional Blender-free workflow for creating, naming, duplicating, mirroring, deleting, and
sculpting dense facial target deltas. It should provide undo/redo, topology validation, normal-delta
handling, target preview, and transactional Mesh V11 export.

The workflow should reuse the existing Expressions and Clips worktrees rather than introduce a
second animation model.

## Additional platform acceptance

Extend the existing indexed-resource contract, capability checks, and CPU fallback rules to iOS
Metal and any other supported platform that does not yet have facial runtime acceptance. Platform
work must preserve the current facial sections and Lua control contract.

## Compatibility extensions

Custom vertex/pixel shaders and normal-mapped facial assets remain outside the current facial
contract. Any future support for those combinations must define their shader inputs, material
interactions, validation rules, and CPU/GPU fallback behavior before changing the runtime contract.

When future work changes a binary section or Lua API, update the corresponding format/API
documentation and `include/version/version.h` in the same change.
