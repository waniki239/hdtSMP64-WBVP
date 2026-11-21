# Upstream tracking and WBVP integration policy

- Upstream repository: `https://github.com/DaymareOn/hdtSMP64`
- Tracking branch: `dev`
- Initial fork base: `e52ad960048c14e637d14fcbd65314ca017fc3b8`
- WBVP integration branch: `wbvp-integration`
- WBVP integration API version: `2.1.0`

## Maintenance rules

1. Keep the upstream `dev` branch free of WBVP-specific changes.
2. Keep all WBVP integration changes on `wbvp-integration`.
3. Keep the WBVP-specific source changes limited to:
   - `src/PluginAPI.h`
   - `src/PluginInterfaceImpl.h` and `.cpp`
   - `src/hdtSkinnedMesh/hdtSkinnedMeshBody.h` and `.cpp`
   - skeleton-owner FormID capture in `src/hdtSkyrimSystem.cpp`
4. Do not modify bone-skinning math, Bullet collision behavior, vertex compaction,
   or physics update scheduling for WBVP.
5. When upstream advances, fetch the new `dev`, then merge or rebase it into
   `wbvp-integration`. Resolve only the isolated integration points above.
6. After every upstream update, verify API versioning, event dispatch location,
   body ID/owner metadata, callback threading rules, all target builds, and an
   API smoke test.
7. If upstream changes `SkinnedMeshBody::internalUpdate()` or its threading/
   scheduling model, re-evaluate the design instead of blindly resolving
   conflicts.

## Compatibility

WBVP must check for Plugin API major version 2 and minor version >= 1 before
registering its listener. If the extension is unavailable, it must disable
deformation safely. No private function hooks, signature scanning, or hard-coded
function addresses are permitted as a fallback.

## Threading

`PreSkinningEvent` runs as part of FSMP's physics/update work. The
`BSTEventSource` dispatch lock is held while listeners run. Keep callbacks
short, non-blocking, allocation-free, and limited to precomputed snapshots and
bounded vertex access. Do not retain event pointers or call game-thread APIs.
