# Real-Time Facial Animation - Phase 6 deterministic capture replay

Phase 6 adds a reproducible producer for the Phase 5 naming contract. It intentionally uses a
line-oriented JSON file instead of a camera, network socket, or vendor SDK. This makes capture
behavior testable from the bundled Lua interpreter and gives later transports a precise packet
boundary.

## Packet format

Each non-empty line is one JSON object:

~~~json
{"timestamp":0.0333,"sequence":12,"weights":{"jawOpen":0.35,"EyeBlink_L":0.8}}
~~~

timestamp is required and is measured in seconds from the beginning of the replay. sequence is
optional but, when present, must be a strictly increasing non-negative integer. weights is
required and contains finite numeric values keyed by source target name. Values remain unclamped in
the packet; the Phase 5 mapping contract clamps them when applying them to a mesh. Unknown fields,
duplicate keys, malformed numbers, non-finite values, out-of-order timestamps, and decreasing
sequences are rejected.

## Replay API

~~~lua
local Stream = require 'facial_capture_stream'
local Mapping = require 'facial_capture_mapping'

local mapping = Mapping.build(face, {requiredTargets = {'jawOpen'}})
local replay = Stream.newFileReplay('capture.ndjson', {
    loop = true,
    resetOnDisconnect = true,
})

local report = replay:step(delta, face, mapping)
if report.ended then
    -- The source reached EOF and optionally reset the mesh weights.
end
~~~

Stream.newReplay accepts decoded packet tables or JSON lines. newFileReplay loads an NDJSON file
and skips blank lines. step(delta, mesh, mapping) advances the replay clock, applies every packet
whose timestamp has been reached, and returns the per-packet mapping reports. A non-looping replay
marks itself disconnected after its last packet. With resetOnDisconnect, it calls Mapping.reset at
that transition. Looping requires a positive duration and reports how many loops were crossed by
the current step. reset, stop, and status support editor lifecycle controls.

The replay source does not own a mesh or mapping index, so the same source can drive isolated
facial preview or a mesh that also has skeletal animation. Sparse packets keep previous weights
unless the mapping call opts into clearMissing.

## Validation

Run the deterministic packet and clock test with the bundled interpreter:

~~~sh
bin/debug/linux_x86/lua-5.4.1.exe src/test-lib/facial_capture_stream_test.lua
~~~

The test covers strict packet decoding, duplicate and malformed fields, timestamp/sequence
ordering, sparse mapping application, end-of-stream reset, looping, and file replay. The next
transport can produce the same validated table without changing the mapping or mesh runtime.
