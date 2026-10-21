# Watchers of the Void 0.9.2156

## 0.9.2156

- Development DLL b2179 / protocol 2179. All players must use the same package and game build.
- Preserve native per-player sleep and dream checks, scene selection, effects and wake return. Sleep packets report presentation only; they do not start another player's personal dream.
- Keep dreams, Insomniac checks and story dates outside event-frequency scaling. Frequency controls the five currently verified repeatable generators.
- Include held-resource persistence, shared-computer startup isolation and selected-model ragdoll setup corrections alongside the accumulated 2155 changes.

Dream scene rewards/effects, late join, installed-game Blueprint routes and multiplayer gameplay still need acceptance. Full scope and qualification evidence: RELEASE_2156.md.