# Broadphase Bone-Sphere Audit

This is an opt-in diagnostic for investigating whether FSMP's per-bone spheres contain the source vertices assigned to each bone.

## Enable

Configure the normal build with:

```powershell
cmake --preset vs2022-windows -DHDT_BROADPHASE_SPHERE_AUDIT=ON
cmake --build out/build/vs2022-windows --config Release
```

Use the corresponding preset for the desired AVX variant. The option defaults to OFF, so normal builds do not emit the audit logs or pay the audit cost.

## Output

At Body construction, after unused vertices are removed and vertex indices are remapped, the plugin emits:

- `[BroadphaseSphereAudit]`: one aggregate line per Body.
- `[BroadphaseSphereAuditBone]`: one detail line for each bone with at least one out-of-sphere influence.

Fields:

- `influences`: positive-weight vertex-to-bone influences examined.
- `outside`: influences whose transformed source vertex lies outside that bone's local bounding sphere.
- `maxExcess`: maximum distance beyond the sphere radius, in FSMP's local units.
- `maxDistanceRadiusRatio`: maximum center distance divided by sphere radius.

The check uses `SkinnedBone::vertexToBone` and the `localBoundingSphere` that FSMP stores at Body construction. It does not modify the sphere, AABB, vertex positions, broadphase, or collision behavior.

## Interpretation and limits

- `outside=0` means all checked source vertex influences are enclosed by their corresponding local spheres at construction time.
- `outside>0` indicates that at least one source vertex influence extends beyond its stored sphere. Inspect the per-bone lines and `maxExcess`.
- This is a geometric containment audit, not proof of a missed Bullet pair or a failed contact. It does not capture the runtime broadphase candidate list, transformed body AABB, or narrowphase result.
- The audit examines the source geometry at Body construction. It does not independently model later morph/weight changes to the mesh.

This diagnostic is intended to decide whether further instrumentation of the runtime AABB and broadphase pair selection is warranted.
