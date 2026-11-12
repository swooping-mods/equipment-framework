# Equipment Framework: behavior and implementation

Describes the retained payload **0.1.0**. The behavior prose describes the retained payload; current candidate source and metadata are identified below. Gameplay qualification is a separate requirement.


<!-- source-only-sync -->
## Current source candidate

Source version **0.1.0** is saved from workshop revision `ca32bbf79ce0840d5166a232bdb3a9fdcd761254`. The retained installable payload remains **0.1.0** with its original bytes. This source update does not rebuild or requalify that payload. The current source and metadata define the candidate; gameplay, rendered behavior and co-op remain unqualified for rebuilt bytes.

The authored behavior and lifecycle descriptions below are retained from the published payload. They are not new qualification evidence for the source candidate. The integration table below follows the current candidate metadata.

The exported test sources have deferred generated harness includes; their fixture closure is incomplete and they are not qualified executable tests. See `provenance.json` for the exact missing include identities.
<!-- /source-only-sync -->

## Behavior

Registers stable tool definitions into a world-private clone of the native upgrade catalog. Normal inventory assignment, six quickslots, selected-tool presentation and native saved string IDs remain game-owned. Definitions provide ownership callbacks and optional visual templates; registration alone never grants an item. Native purchase routes for registered IDs are denied so provider Shop callbacks remain the payment authority. Physical rack callers retain the vanilla catalog.

## State and lifecycle

The startup registry rejects duplicates, invalid/late registrations and ID collisions. Managed references and Unity asset retention flags protect cloned catalogs through resource unloading; world teardown destroys owned clones. Actual-selected-tool queries gate providers. Selected mod tools suppress borrowed native sweeping/rotation behavior while retaining equip/pose updates. Pickup Hands can temporarily clear selection without rewriting quickslots.

## Integration and configuration

No additional feature-mod runtime dependency is declared. Included shared headers are source reuse, not extra DLL dependencies.

The [module metadata](../source/mods/equipment-framework/mod.json) owns the version and declared integration contract. Native modules start at `MoonLab_ModStart`. Game/Unity integrations target the pinned Steam Windows x64 IL2CPP build.

| Service | ABI | Table size |
|---|---:|---|
| Provides `equipment-framework.menu-cleanup` | 1 | 16 bytes |
| Provides `equipment-framework.equipment` | 1 | 32 bytes |

## Implementation sources

- [mod.c](../source/mods/equipment-framework/mod.c)
- [registry.h](../source/mods/equipment-framework/registry.h)
- [catalog_context.h](../source/mods/equipment-framework/catalog_context.h)

## Verification boundary

No universal ownership ledger, extra quickslots or global interaction-key interception. Providers own purchases, persistence and effects. Catalog/lifetime hooks are game-build-specific; arbitrary tool art and co-op need provider checks.

`python3 build.py --check` verifies the exported source hashes. It does not execute the mod or qualify the behaviors above. Installation and independent build prerequisites are in the [README](../README.md).
