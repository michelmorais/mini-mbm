# Mesh Audit: public offline Lua API

Mesh Audit diagnoses geometry without changing the source. The optional external
`mbm-cgal-audit` executable contains CGAL; the engine runtime does not link CGAL.
The independently usable CLI reads OBJ/OFF and emits a versioned JSON report.
See the mbm-cgal repository's `tools/audit/README.md` for its full wire schema.

## Lua module

`editor/mesh_audit.lua` has no ImGui dependency. Add the engine's `editor/?.lua`
to `package.path` when using it from an external application. It requires the
engine's `mbm.executeProcessAsync` and `mbm.getTimeRun` for jobs.

```lua
local Audit = require 'mesh_audit'
assert(Audit.setPath('/path/to/mbm-cgal-audit', false))
local job = assert(Audit.startMesh(meshDebugObject, {
    frame = 1,
    selfIntersections = true,
    timeout = 300,
}))

-- Poll from onLoop; do not spin in a blocking loop.
local status = job:getStatus()
if status.state == 'completed' then
    print(status.report.triangles, status.report.surface_area)
    -- job.json retains the original versioned JSON for export.
elseif status.state == 'failed' then
    print(status.error)
end
```

| Entry point | Contract |
|---|---|
| `getPath()` | Cached executable preference; initial `MBM_CGAL_AUDIT_EXECUTABLE` overrides saved preference |
| `setPath(path, persist)` | Set executable; optionally save. Returns true or nil/error on save failure |
| `startFile(path, options)` | Audit an OBJ/OFF file directly; returns job or nil/error |
| `startMesh(asset, options)` | Export an unmodified geometry snapshot from a meshDebug object, then start the worker; returns job or nil/error |
| `decodeReport(json)` | Decode the flat v1 report to a Lua table; throws on malformed/unsupported protocol; never executes Lua source |
| `shutdown()` | Cancel/clean up all jobs owned by this module instance |
| `job:getStatus()` | `{state='running'}`, `{state='completed', report=...}`, `{state='failed', error=...}` or `{state='cancelled'}` |
| `job:cancel()` | Cancel and remove temporary files; terminal jobs keep their state |
| `job:destroy()` | Cancel if needed and release resources; safe to repeat |

Options: `printJson=true` enables JSON output in the terminal (default false; requires a worker supporting `--quiet`).
`executable` overrides the configured path for one job;
`selfIntersections=false` skips that expensive check; `timeout` is a positive
number of seconds (default 300). `startMesh` additionally accepts `frame`
(default 1) and `subset` (default all subsets in that frame), both 1-based.
Only TRIANGLES draw mode is supported for engine snapshots. The snapshot reads
stored frame coordinates, not the current rendered skin deformation or world
transform. It does not load textures or modify attributes/physics.

The snapshot export is synchronous when requested; geometric analysis is in the
background process. Polling does not repeat the export, rebuild geometry or
reread a completed report. Call cancel/destroy/shutdown when discarding a job.
Timeouts are enforced while polling. No external report files are removed by
`startFile`; only the job's generated report and snapshots are cleaned up.

JSON `null` fields become absent/nil fields in Lua. Always inspect the associated
`*_status`: a skipped self-intersection test is **not** a negative result.
`status='completed'` means the audit ran; inspect `valid_polygon_mesh`,
`degenerate_triangles` and the other diagnostics to evaluate the geometry.

## Editors

Mesh Debug exposes **Analyze mesh / Analisar malha** per loaded asset. Image Mesh
Editor analyzes the selected generated preview. Their **Options > CGAL
executable** panel selects a shared tools folder and shows whether
`mbm-cgal-audit` (`.exe` on Windows) is present in its availability table.
Selecting a folder saves it automatically; Refresh rescans the files. Audit results are
snapshots of stored frame 1: explicitly analyze again after editing. Reports
are not undo/history mutations and do not change the asset's modified state.

The mesh3dgen result panel uses this same Lua API and UI on the selected MSH.
Its Audit settings share the engine preference, with an optional
`MESH3DGEN_CGAL_AUDIT_EXECUTABLE` override. It creates no provider request or new
mesh variant. All three panels support disabling self-intersection checks,
cancellation and JSON export. Export asks for a destination using the normal
save dialog.

The shared `.mini-mbm-cgal-folder` preference (`MBM_CGAL_FOLDER_CONFIG` override)
takes priority. Without a selected folder, the legacy audit preference remains
`MBM_CGAL_AUDIT_CONFIG`, or `.mini-mbm-cgal-audit-path` under APPDATA/HOME. Analysis is only launched on explicit request. Per-frame callbacks
poll pending jobs and draw cached reports; no mesh scans, loads or serialization
occur while idle.

The C++ worker shares topology checks with Remesh. Mesh Audit does not repair
geometry. Approximate triangle-target search is available in Remesh, which
uses surface area and topology diagnostics internally; running Audit first is optional.

## Validation

- `src/test-lib/mesh_audit_test.lua`: standalone Lua protocol and job lifecycle tests.
- `src/test-lib/mesh_audit_smoke.lua`: real engine, source preservation, completion,
  skipped checks, cancellation, worker failure, UI and temporary-file cleanup.
- Existing `mesh_cgal_editor_smoke.lua` and `image_mesh_cgal_editor_smoke.lua`
  additionally audit their assets when `MBM_CGAL_AUDIT_EXECUTABLE` is set.
- mesh3dgen `tests/engine_mesh_audit_smoke.lua`: public API, MSH snapshot and result panel.

Use the existing engine-testing launch flags and a hard external timeout. The
worker must be built first. Image Mesh's integrated test still requires its
existing authored `MBM_CGAL_PROJECT` fixture.
