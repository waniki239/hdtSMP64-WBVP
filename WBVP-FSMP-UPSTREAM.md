# Upstream tracking and WBVP integration policy

- Upstream repository: `https://github.com/DaymareOn/hdtSMP64`
- Tracking branch: `dev`
- Initial fork base: `e52ad960048c14e637d14fcbd65314ca017fc3b8`
- Stable WBVP integration branch: `wbvp-integration`
- API 2.2 development branch: `wbvp-pluginapi-2.2.0-final-collision-diagnostics`
- FSMP product/CMake version: `3.0.0`
- WBVP integration Plugin API version: `2.2.0`
- WBVP consumer version: `0.2.17-beta2`

## Maintenance rules

1. Keep the upstream `dev` branch free of WBVP-specific changes.
2. Keep stable WBVP integration changes on `wbvp-integration`; develop API
   additions on a separate branch and merge them only after review.
3. Keep WBVP-specific source changes limited to:
   - `src/PluginAPI.h`
   - `src/PluginInterfaceImpl.h` and `.cpp`
   - `src/hdtSkinnedMesh/hdtSkinnedMeshBody.h` and `.cpp`
   - `src/hdtSkinnedMesh/hdtDispatcher.cpp` for the post-skinning callback
   - skeleton-owner FormID capture in `src/hdtSkyrimSystem.cpp`
4. Do not modify bone-skinning math, Bullet collision behavior, vertex
   compaction, or physics update scheduling for WBVP.
5. When upstream advances, fetch the new `dev`, then merge or rebase it into
   `wbvp-integration`. Resolve only the isolated integration points above.
6. After every upstream update, verify API versioning, event dispatch location,
   body ID/owner metadata, callback threading rules, all target builds, and an
   API smoke test.
7. If upstream changes `SkinnedMeshBody::internalUpdate()` or its threading/
   scheduling model, re-evaluate the design instead of blindly resolving
   conflicts.

## Compatibility

- API 2.1 provides the pre-skinning deformation callback.
- API 2.2 adds the final collision snapshot callback and primitive/AABB access.
- WBVP Beta 2 requires API major version 2 and minor version at least 2 for
  final collision diagnostics. It must not assume API 2.2 exists on older
  FSMP builds.
- No private function hooks, signature scanning, or hard-coded function
  addresses are permitted as a fallback.

## Threading

Both callbacks execute as part of FSMP physics/update work. The
`BSTEventSource` dispatch lock is held while listeners run. Keep callbacks
short, non-blocking, allocation-free, and limited to precomputed snapshots and
bounded accessors. Do not retain event pointers or call game-thread APIs.
