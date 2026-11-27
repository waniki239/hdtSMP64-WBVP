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

## Runtime broadphase pair audit

When the option is enabled, the diagnostic build also samples the player hair Body named
`_HP2colWigFull` once every 60 physics collision-detection passes:

- `[BroadphasePairAuditAabb]`: the world-space AABB submitted to Bullet for the hair Body.
- `[BroadphasePairAuditRawPair]`: a raw overlapping pair returned by Bullet's broadphase, with the other Body name.
- `[BroadphasePairAuditSummary]`: total broadphase pair count and the number involving the target hair Body. A zero target-pair count means the broadphase did not return a pair for that sampled pass.
- `[BroadphasePairAuditFilter]`: whether FSMP's `needsCollision` filter accepted the raw pair.
- `[BroadphasePairAuditMidphase]`: whether the BVH midphase found an overlap for an accepted pair.
- `[BroadphasePairAuditNarrowphase]`: confirms that `SkinnedMeshAlgorithm::processCollision` was invoked after a midphase overlap.

The world and dispatcher samples are independent counters, so their frame numbers are local to each diagnostic stage. These records are sampled diagnostics, not a complete per-frame trace. The narrowphase line records invocation, not whether a contact point or solver response was ultimately produced.


## Contact and manifold audit

For the target `_HP2colWigFull` pair, the diagnostic build also emits:

- `[CollisionContactAuditCandidates]`: per shape-direction call, the number of collider-pair checks that returned a contact result and were retained up to `MaxCollisionCount`. A triangle/vertex pair can produce a separate line for each direction.
- `[CollisionContactAuditMerge]`: number of active MergeBuffer cells after aggregation and the number of calls that inserted a point into a Bullet manifold.

Interpretation:

- Candidate results greater than zero but no active merge cells indicate that collision results did not produce accumulated bone-pair contributions.
- Active merge cells greater than zero but zero manifold additions indicate that the accumulated entries were rejected during the final application stage.
- A positive manifold-add count confirms that FSMP called `addManifoldPoint`; it does not by itself prove that Bullet retained every point, generated a solver constraint, or produced a visible physical response.
- These are per-pair diagnostic records. They do not change collision calculations, thresholds, or manifold contents. The additional logging is enabled only with `HDT_BROADPHASE_SPHERE_AUDIT`.
