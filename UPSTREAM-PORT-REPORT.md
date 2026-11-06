# Upstream Port Report — v34 vs newer Source trees

**Goal:** find functions where newer Source-engine trees diverge from this v34
server, understand *why* they changed (optimization / bug / exploit / feature
/ API churn), and backport the safe ones as drop-in `server_i486.so` code —
without changing the network protocol (a stock v34 client must still connect),
without breaking ABI against `engine.so`/`tier0`, and without importing
game-specific features (TF2/CSGO mechanics are out of scope).

**Status (2026-11-04):** current through commit `af84694`. 79 backport
commits have landed since this report was added (`22084c4`) — see
[Landed backports](#landed-backports) below. `FIXES.md` is an archive of the
pre-port fix set and is no longer updated; this report is the living
document.

## References compared

| Ref | Tree | Vintage | Role |
|---|---|---|---|
| `cssce` | DeadZoneLuna/css-community (`mp/src`) | 2007 CSS leak + community fixes | closest lineage — primary |
| `sdk2013` | ValveSoftware/source-sdk-2013 (`src`) | official 2013 SDK | official Valve fixes |
| `nillerusr` | nillerusr/source-engine | 2017 engine, modernized fork | perf & robustness |
| `csgo` | hy1ex/csgo-src (replaces sr2echa/CSGO-Source-Code) | 2019 CS:GO (builds 730/740/745, Mar 2019) | trend reference only (heavy refactor) |

**CSGO ref swap (2026-10-30):** the `csgo` reference is now
`hy1ex/csgo-src` (2019), replacing the 2017 sr2echa tree; the
inventory's csgo column (line ~61 below) still reflects the old tree.
All four csgo-evidenced fixes were re-verified against the new repo
and are correct: `b282b73` (GetVelocity `&& m_pPhysicsObject` and
`DevMsg( 2, "%s", szBuffer )` — byte-identical), `c1fc932`
(TakeHealth corrected form — byte-identical), `3fc99ae`
(GetNetworkIDString still NULL-guards with the `"UNKNOWN"` fallback,
unchanged from old csgo; we keep the 3-tree `""` form), `1c8b4de`
(lag-comp pose params — same save/apply/restore structure, file still
`player_lagcompensation.cpp`, code unchanged old→new; our
`MAXSTUDIOPOSEPARAM` loop form matches sdk2013 exactly and
`Studio_Set/GetPoseParameter` count-guard the out-of-model indices).

## Methodology

An automated inventory (`function-level`, reproducible) was built:

1. Our sources are read in **both `BUGFIXED` variants** (flag-on and the
   original flag-off code produced by evaluating only the `BUGFIXED`
   conditionals), so our own fixes are never mistaken for upstream divergence
   — a function whose *original* variant matches upstream is classified
   `bf_only`, not `changed`.

   *(This inventory was generated before commit `7fdd22f` removed that
   switch. Today the fixed code is the only code, so a regenerated run
   compares our current sources directly and no longer produces the
   `bf_only` class.)*
2. Every file in `dlls/`, `game_shared/`, `public/`, `common/` is paired with
   **each** reference independently (exact relative path first, basename
   fallback, header-similarity gate ≥ 0.30).
3. Functions are extracted by brace-matching after comment/string stripping
   and compared as whitespace-normalized bodies; per-function unified diffs
   were written for all changes.

**Data quality caveat (validated by hand on `util.cpp`):** raw "changed"
counts are dominated by *API churn* that is **not portable** — `engine->Cmd_Argv`
→ `args[]`, `partition` → `::partition`, `VEC_HULL_MIN` →
`VEC_HULL_MIN_SCALED()`, `#ifdef VPROF_ENABLED` wrapping, memory-debug guards.
The real signal is the smaller subset of **semantic changes** (guards,
bounds checks, control-flow fixes); those are what gets analyzed below and
implemented in Phase 2.

## Inventory (global)

| Ref | Files matched | Functions | Changed | Only-ours | Only-ref |
|---|---|---|---|---|---|
| cssce | 1,356 | 14,635 | 2,956 | 310 | 1,428 |
| sdk2013 | 1,525 | 18,542 | 3,973 | 367 | 1,924 |
| nillerusr | 1,687 | 20,206 | 4,194 | 378 | 1,918 |
| csgo | 1,255 | 14,896 | 3,705 | 582 | 2,593 |

Cross-reference consensus (same function changed vs ≥2 refs — strongest
evidence of real upstream evolution): **4,200 functions**, of which **2,861
changed in ≥3 of 4 refs**.

Files with the most cross-ref changed functions (candidate hot spots):

```
85 ai_basenpc.cpp        77 baseentity.cpp         69 player.cpp
60 tier1/utlrbtree.h     58 baseanimating.cpp      58 cstrike/cs_player.cpp
46 props.cpp             40 nav_mesh.cpp           39 sceneentity.cpp
38 baseentity_shared.cpp 36 physics.cpp            35 basecombatweapon_shared.cpp
32 choreoevent.cpp       31 triggers.cpp           30 baseflex.cpp
30 gameinterface.cpp     28 basecombatcharacter.cpp 27 XZip.cpp
27 vector.h              24 collisionproperty.cpp  24 util.cpp
24 nav_generate.cpp      23 physics_main_shared.cpp 23 baseentity.cpp(hl2mp)…
```

## Findings so far (deep-dive batch 1)

Each item: what changed, **why we believe upstream did it**, and a
compatibility rating.

**A — safe, pure server logic, no protocol/ABI impact**
**B — needs an interface/header availability check against the v34 SDK first**
**C — protocol-adjacent (send proxies / transmit / string tables): avoid**
**D — API churn, not portable**
**E — game-feature: out of scope per project rules**

### 1. `CBaseEntity::SetParentAttachment` — invalid attachment accepted →
broken parent state — **A (bug fix, top of batch 1)**

Ours checks `if ( !iAttachment )` after `LookupAttachment()`, which returns
**-1 on failure**; -1 passes the check, gets stored in
`m_iParentAttachment` (cast to `char`, wrapping to 255), and `SetParent()`
re-reads it — leaving the entity parented with garbage attachment state.
nillerusr changed it to `if ( iAttachment <= 0 )`. Pure logic, no
network/ABI surface.

**Status: landed — `6ded251`.**

### 2. `CBaseEntity::TakeDamage` — upstream validates our fix — **A**

nillerusr independently carries `if ( !g_pGameRules ) return;` at function
top — **the exact guard we ported from ProtectCmds** (`eba67ff`). Good
external confirmation. Their other delta replaces the
`DMG_NO_PHYSICS_FORCE` bit test with `g_pGameRules->Damage_NoPhysicsForce()`
— a refactor (evaluate: v34 gamerules may already expose it → then neutral).

**Status: the `g_pGameRules` guard has been in the tree since the earlier
ProtectCmds port; the `Damage_NoPhysicsForce` refactor was skipped — the
method does not exist on v34's `CGameRules`.**

### 3. `CBaseEntity::SetParent` — destroying a static physics object that is
attached to a constraint — **B**

nillerusr guards `VPhysicsDestroyObject()` with
`IsAttachedToConstraint(false)` + a warning. Destroying an object that a
constraint still references leaves the constraint dangling (crash later in
vphysics). Question to resolve in Phase 2: does v34's `IPhysicsObject`
expose `IsAttachedToConstraint`? If yes, port the guard; if not, equivalent
check must be derived from v34 interfaces.

**Status: landed — `6ded251`.** The Phase-2 question is answered: v34's
`IPhysicsObject` *does* expose `IsAttachedToConstraint(bool)`
(`public/vphysics_interface.h`), so nillerusr's warning was ported as-is; it
logs and then proceeds exactly as before (behavior-neutral).

### 4. `CBaseEntity::~CBaseEntity` — `SetGroundEntity(NULL)` during teardown —
**B (needs study)**

nillerusr clears the ground entity before `DestroyAllDataObjects()`. Ours
relies on `PhysicsRemoveGroundList()` alone. Needs a careful read of v34
teardown ordering before adopting.

**Status: still open — deferred as originally planned.**

### 5. `util.cpp` hardening (sdk2013) — **A (small)**

- `DBG_AssertFunction`: ours calls `Warning( szOut )` — **format-string
  pass-through**; sdk2013 passes it as `Warning( "%s", szOut )`.
- `CC_VoxelTreeBox` / `CC_VoxelTreeSphere`: sdk2013 guards every
  `debugoverlay->` call with `if ( debugoverlay )` — on a dedicated server
  the interface can be absent → admin-run console command crash.

**Status: landed — `9674cee` (format-string and `debugoverlay` guards, plus
a NULL local-player guard not in the original list).**

### 6. `CServerGameClients::ProcessUsercmds` — we are ahead — **no action**

Our NaN/INF-angle and move-magnitude validation (the code formerly behind
the `BUGFIXED` switch) does **not** exist in nillerusr 2017 or the others.
Keep ours; nothing to backport.

### 7. `CServerGameDLL::GameFrame` — alternate-tick skip rework — **D/E**

nillerusr replaces our `static bool skipframe` with a `tickcount & 1` check
plus `UpdateAllClientData()` on skipped frames. Behaviorally equivalent for
us; their version also gates nav/benchmark/query-cache systems we don't have.
Not worth touching (the static never survives level state in practice).

### 8. `CBaseEntity::Teleport` — player_teleport game event — **E (feature)**

skipped (feature, event plumbing a v34 client wouldn't use).

## Standing conclusions

* **Do not bulk-port.** Raw divergence is dominated by API churn; each
  candidate must be read and classified A–E.
* **Consensus signal works**: functions changed in 3–4 refs are much richer
  in real fixes (e.g. `SetParentAttachment` appears across refs).
* **Send proxies / `SetTransmit` / string-table code is category C** — even
  tempting fixes there can shift what a v34 client sees; excluded by rule.
* **tier1 container headers** (`utlrbtree.h`, `utlvector.h`, `vector.h`)
  are compiled into *our* TUs (headers, not the prebuilt `.a`), so genuine
  fixes there are portable — but many diffs are C++ modernization from the
  ports (category D); needs the same A–E pass.

## Phase 2 — implementation (current project rules)

Rules in force for the porting work (set once the switch was retired):

1. **No `BUGFIXED` flag** — the switch was removed repo-wide (commit
   `7fdd22f`, "the fixed code is the only code"); all conditionals were
   flattened to keep the fixed code. Backports are applied unconditionally.
2. **No FIXES.md / README entries** for these changes — the work is too
   large for per-change documentation there.
3. **Every major change gets its own commit with a detailed explanation**
   (what changed, which upstream tree it came from, why upstream changed it,
   risk assessment).
4. **Autonomous prioritization** — no confirmation round-trips. Candidates
   are classified A–E; A is applied, B is applied only after a quick
   interface/API availability check, C (protocol-adjacent) and E (game
   features) are skipped, D is skipped as non-portable churn. Anything that
   looks risky gets the safe alternative or is skipped entirely.
5. **Build-verify both tree states** in the Etch chroot (README build guide)
   before each commit batch.

**Batch 1 — complete:** the `SetParentAttachment` -1-attachment guard and
the nillerusr `SetParent` constraint warning landed in `6ded251`; the
`util.cpp` format-string, `debugoverlay` and NULL local-player hardening
landed in `9674cee`. `Damage_NoPhysicsForce` refactor **skipped** (API
absent from v34).

**Batch 2 — complete:** the `baseanimating.cpp` family
(`d97f4cd`, `ea02fbc`, `9eb1faf`, `f3975e0`), the `physics.cpp`
penetration solver (`dd29423`), the `nav_mesh` ground probe
(`e2b035b`), the `~CBaseEntity` ground-entity teardown (`0f86f08`,
finding 4), the `nav_generate` covered-seed fix (`aa75b38`), the XZip
`PUTBYTE`/`PUTSHORT` flush-bounds fix (`615515d` — verified: Windows-only
dead source not in the build), the `baseentity_shared` physics-reasonableness
guards (`0438a3a`) and the `FireBullets` startsolid tracer clamp
(`6870376`). `XUnzip` had no portable fix (Valve deleted the file; the
cssce delta is their POSIX port).

**Batch 2.5 — hardening landed while triaging (see Landed backports):**
input validation, NULL-guards, string/buffer bounds checks and the
lag-comp/lerp alignment, applied as individual commits with full
explanations rather than waiting for a batch.

**Batch 2.6 — game_shared sweep (2026-10-28):** `saverestore.cpp`
(3 fixes, 454ca83/33ab533/9ea3ba1), `physics_shared.cpp` (2 fixes,
78b6a2e/1ff8704). `collisionproperty.cpp` reviewed end-to-end with
NOTHING portable: its whole delta is model-scale bounds (`PreScaled`
networkvars — a TF2-era feature), the threaded spatial-partition rework
(mutex/CTSList — engine-interface churn), uniform trigger bloat (feature),
the CollideGetAABB reference-vs-pointer signature (vphysics interface
version), and `MarkSurroundingBoundsDirty`'s switch to
`MarkPVSInformationDirty()` — a method this tree does not have, where our
`EFL_DIRTY_PVS_INFORMATION` eflag mechanism is already the correct
implementation for our ServerNetworkProperty. All skipped as C/D/E.

**Batch 2.7 — small-file sweep (2026-10-28):** `animation.cpp` (4 fixes
in `a5a21b6`: the `AE_TYPE_SERVER` event filter in `GetAnimationEvent`,
negative bodygroup guards at all four sites, `IsValid()` in
`FindBodygroupByName`, `ExecuteNTimes` log throttling) and
`public/filesystem_helpers.cpp` (`19681c4`: `ParseFile`'s output token is
now bounded via upstream's `ParseFileInternal` + array-reference
templates — our copy had no length knowledge at all). Three files
reviewed with nothing portable: `interval.cpp` is pure churn (copyright
header plus the `vstdlib`→`tier1` strtools include path);
`decals.cpp` offers only a new `GetDecalNameForIndex` API — its sole
upstream caller is client-side `c_te_worlddecal.cpp`, which this server
does not build, and adding it to `IDecalEmitterSystem` would insert a
virtual into the shared interface (vtable risk against `engine.so`) —
plus an sdk2013-only shadow-variable rename with no semantic effect; and
`animation.cpp`'s `SelectWeightedSequence` fast path was skipped because
it needs the `CStudioHdr::m_ActivityToSequence` subsystem this tree does
not have, making it a feature port rather than a drop-in.

**Batch 3 — tier1 container headers (2026-10-29):** `public/tier1/utlvector.h`
(`fd72abe` — `FastRemove` now skips the memcpy when the removed element is the
last one; 3/3 refs, memcpy onto itself is UB) and `public/tier1/utlrbtree.h`
(`23d4f04` — `CopyFrom` copies `sizeof(Node_t)` per element instead of
`sizeof(T)`, which truncated the node array and left every link past the cut
point as garbage; sdk2013 fix, 1/3 refs, taken as objectively correct and
latent — nothing instantiates `CopyFrom` today). Reviewed with nothing
portable: `tier1/mempool.cpp` (all three refs converged on the
alignment/enum rework — 64-bit modernization with a constructor-signature
change, no i386 bug), `tier1/byteswap.cpp` (not compiled — no Makefile entry
and no includers anywhere in the tree), and `public/vector.h` (deltas are
new APIs — `Normalized()`, `WithinAABox`, Vector4D additions — plus
thread-safety cosmetics; `NormalizeInPlace` is identical to sdk2013). An
apparent `InsertMultipleBefore(elem, -num)` divergence turned out to be an
analysis-script extraction artifact — the shipped file matches cssce
exactly. The refs' `Sort()` non-sequential-allocator fallback is
unreachable here (ours already guards `Count() > 1`, and every allocator in
this tree is sequential).

**Batch 3 (deferred / probably skip):** unchanged — `ai_*` (HL2 NPC
content — mostly irrelevant to a CSS server), `cstrike/*` game-logic
changes (CSS-community bugfixes — in scope but reviewed as game logic, not
engine).

**Batch 4 — baseentity.cpp function-level triage (2026-10-30):** the
inventory's top engine-layer hot spot (77 cross-ref changed functions)
was swept by extracting all 197 out-of-line functions and grouping
normalized bodies per ref: 32 candidates had >=2 refs agreeing against
ours, 165 clean or split. Five applied:

- `b16c437` — `VPhysicsUpdatePusher` rewind math (3/3): our version
  rewound a full extra tick of blocked-pusher motion every physics tick
  and its 1% bounce-noise gate was arithmetically always-true; refs use
  `moveback = movetime * fraction` / `start + movetime*0.99f`, plus the
  blocker-only-if-non-NULL assignment.
- `2d068fd` — local-setter reasonableness guards (2-3/4): completes
  0438a3a with `IsEntityPositionReasonable` (SetLocalOrigin),
  `CheckEntityVelocity` (SetLocalVelocity) and the new
  `IsEntityQAngleVelReasonable` helper (SetLocalAngularVelocity).
- `b282b73` — `GetVelocity` NULL physics-object guard (4/4) and the
  `AcceptInput` `DevMsg( 2, "%s", szBuffer )` format-string fix (3/3;
  same defect class as 6d23314, this site had been missed).
- `8234594` — `SetMoveType` FLY/FLYWATER `UpdateWaterState()` case (3/3).
- `c1fc932` — `TakeHealth` returns the actual healed delta and goes
  through `GetMaxHealth()` (3/4); all call sites audited, none test for
  == 1.

Reviewed with nothing portable: `VPhysicsTakeDamage`/`TakeDamage`
(need `Damage_NoPhysicsForce`, known absent), `PhysicsRelinkChildren`
(`float dt` signature churn), `Restore` (refs deleted a no-op
`EF_NOINTERP` re-add), `OnRestore` (`MarkPVSInformationDirty()` absent
+ ifdef churn), `FVisible` (refs clear `CONTENTS_BLOCKLOS` for players -
an AI/visibility gameplay change with no clear fix rationale, skipped
per the risk rule), `Teleport` (client-side game event feature),
`UpdateOnRemove` (dynamic-model loading feature), `PhysicsTouchTriggers`
(refs pass a 4th `sm_bAccurateTriggerBboxChecks` arg to
`engine->SolidMoved/TriggerMoved`; v34's engine interface is 3-arg - ABI,
class C), `DispatchResponse` (needs `AI_Response::GetResponsePtr`,
absent), `OnEntityEvent` (64-bit `size_cast` churn), `SetParent`
(needs `TransformStepData_*` helpers, absent), `SetTransmit`
(transmit/PVS path rework - class C), `PrecacheModel` (signature/refactor
churn), `GetParentToWorldTransform` (`MDLCACHE_CRITICAL_SECTION`
absent), `RemoveRecipientsIfNotCloseCaptioning`, `SetRefEHandle`,
`StopFollowingEntity`, `SUB_PerformFadeOut`, `DrawDebug*`,
`FunctionCheck` (const/macro/assert/debug churn).

**Batch 5 — player.cpp function-level triage (2026-10-30):** 211
out-of-line functions swept the same way as Batch 4; 59 candidates had
>=2 refs agreeing against ours. Three applied:

- `f20460c` — `VPhysicsShadowUpdate` ROTPUSH/stuck cluster (3/3): when
  game code rotates a player around a `SOLID_VPHYSICS` pusher
  (`ComputeRotationalPushDirection`) the physics shadow controller
  doesn't know about the move and can leave the player embedded in the
  rotating brushwork. Refs set a new `PFLAG_GAMEPHYSICS_ROTPUSH` there;
  `VPhysicsShadowUpdate` consumes it once and forces the stuck check
  for that tick, unifies the two ad-hoc recovery paths into one final
  trace, and skips dead players (`pl.deadflag`) alongside NOCLIP.
  The `WasFrozen()` half of the ref change is **engine-level / deferred**:
  it needs a `virtual bool WasFrozen()` slot on
  `IPhysicsPlayerController` (`public/vphysics/player_controller.h`)
  and the v34 `vphysics` binary has no such vtable entry — ABI, can't
  be fixed game-side.
- `7b0c54b` — `TraceAttack` NULL-attacker guard (3/3): the NPC
  friendly-fire and team-damage checks dereferenced
  `info.GetAttacker()` unconditionally; attacker-less damage events
  (world triggers, removed projectiles) crashed the server.
- `3fc99ae` — `GetNetworkIDString` NULL guard on
  `engine->GetPlayerNetworkIDString()` (3/4 identical, csgo falls back
  to `"UNKNOWN"` instead of `""`): unguarded `Q_strncpy(NULL)` was a
  memcpy-from-NULL crash whenever an ID was unresolvable.

Reviewed with nothing portable: the command-processing cluster
(`PhysicsSimulate`/`DetermineSimulationTicks`/`AdjustPlayerTimeBase`/
`PlayerRunCommand` — class C: our anti-exploit budget
`m_nMovementTicksForUserCmdProcessingRemaining` and clock-correction
logic occupy exactly these functions and upstream's rework would have
to be reconciled with it, not dropped in); signature-change clusters
(`SetVCollisionState`/`InitVCollision`/`SetupVPhysicsShadow`/
`Event_Dying`/`CommitSuicide`/`GetViewModel`/`ChangeTeam`/`CCommand
ClientCommand` — signature or API drift, class C/E); the
freezecam/observer, autoaim, HL2-player, bonus/speedmod and autosave
clusters (class E — game features out of scope); and ~25 pure-churn
items (class D). The deferred `WasFrozen()` work above is engine-level:
it waits for the engine source, per the standing rule.

**Batch 6 — baseanimating.cpp function-level triage (2026-11-01):** 58
cross-ref changed functions swept the same way (top remaining engine-layer
hot spot after Batches 4/5); 50 candidates dumped, three applied:

- `411b183` — `SetLightingOriginRelative` DevWarnings pass `string_t`
  raw to `%s` (both the dead-branch and wrong-entity warnings): `string_t`
  is not a `char*` here, so varargs reads a garbage pointer — the same
defect class as `454ca83`, and the sibling warning in the same function
  already used `STRING()`. Refs dereference through their accessors (3/3).
- `071ebd7` — `CBaseAnimating` constructor initializes the networked
  members `m_vecForce` (SendPropVector, SPROP_NOSCALE) and `m_nForceBone`
  (SendPropInt, 8 bits) unconditionally; they were only initialized under
  `#ifdef _DEBUG`, so release shipped indeterminate values (3/3).
- `0856942` — `GetBoneCache` adds the upstream `m_timeValid <= curtime`
  guard (`CBoneCache::IsValid` is `time - m_timeValid <= dt`, true for any
  *future* stamp, so a cache stamped ahead of curtime after a curtime
  reset read as valid) and switches the mask test to containment
  (`(mask & want) == want`) in both the reuse and destroy paths, so a
  superset cache is reused instead of torn down (3/3; TF_DLL extension
  n/a).

Reviewed with nothing portable: the `StudioFrameAdvanceInternal` cycle
terminal (ours `0.999f`, all 4 refs `1.0f` — our value is the original
2006/v34 import; the only `GetCycle() >= 1.0f` consumers are HL2 NPC
content and the separate per-layer path in `BaseAnimatingOverlay`, and
`bone_setup.cpp` clamps pose evaluation to 0.9999 anyway, so no CSS-side
benefit and an authentic-behavior change → **documented skip**);
`CalculateIKLocks`'s `trace.p1` vs `trace.hip` — the refs rewrote the
whole `x5 trace` struct (ours `{p1,p2,p3}` vs cssce's
`{hipToFoot,hipToKnee,kneeToFoot,hip,closest,knee,farthest,lowest}`) as
part of a 2006→2007 IK refactor, not portable piecemeal (class D);
`TestCollision` — cssce's version drops our `FSOLID_CUSTOMBOXTEST` branch
and derefs a possibly-NULL physics object in its model-scale path (class
C — trace-path churn, hit-reg adjacent); `CopyAnimationDataFrom` — refs
replaced our `EF_NOINTERP` with `IncrementInterpolationFrame()` /
`LockStudioHdr()`, APIs absent from this tree, and our form is the
v34-era equivalent (class D); the dynamic-model / model-scale cluster
(`SetModel`, `ResetSequenceInfo`, `SetupBones`, `GetSkeleton`,
`ComputeHitboxSurroundingBox`, `DrawServerHitboxes`, `OnRestore`) — class
D/E feature drift; and the remaining ~40 candidates were pure churn
(`Assert`→`AssertMsg`, `LocalFlexController_t`, `GetModelScale`,
float-literal, `AUTO_LOCK`/IBoneSetup refactors).

**Batch 7 — BaseAnimatingOverlay.cpp / baseflex.cpp triage (2026-11-01):**
35 cross-ref changed functions swept across the two remaining baseentity-adjacent
animation files (8 in `BaseAnimatingOverlay`, 27 in `baseflex`); three applied:

- `1329ddd` — `AllocateLayer` grew `m_AnimOverlay` with an unguarded
  `AddToTail()`. The vector is networked through `SendPropUtlVector` declared
  with `MAX_OVERLAYS` as its max element count, so once every slot was active
  each further call appended a layer past the networked maximum (such a layer
  can never hold a valid `m_nOrder`, and `VerifyOrder` asserts on the resulting
  duplicate orders). Both `AddToTail` branches now respect the cap and return
  `-1` when full (3/3 refs); the paired `Assert( IsValidLayer( i ) )` in
  `AddLayeredSequence` is dropped with it, since `-1` is now a legal return and
  that function already guards with `IsValidLayer` ("No room?").
- `5f3fc10` — `CAnimationLayer::Init` never initialized `m_flBlendIn`,
  `m_flBlendOut` or `m_flLastEventCheck`: the first two feed `IsAutoramp`/the
  frame-advance weight ramp (whose result *is* networked via the weight prop),
  and the third seeds the animation-event dispatch window (`flStart`) and is a
  save/restore datadesc field, so garbage could be persisted and replayed.
  `RestartGesture` now also resets `m_flLastEventCheck` with the existing
  cycle/prev-cycle reset instead of inheriting the previous run's check
  position (2/3 refs, one-line and behaviourally free).
- `26e2f2a` — `HandleStartGestureSceneEvent` `strcpy`'d the faceposer
  "loop"/"end" tag values from an unbounded KeyValues string into a fixed
  `char[MAX_EVENTTAG_LENGTH]` (128): replaced with `Q_strncpy( ..., sizeof )`
  (the refs' `V_strcpy_safe` does not exist in this tree), and the
  `STUDIO_LOOPING` vcd `DevMsg` now wraps `GetModelName()` — a `string_t` from
  `CBaseEntity`, not a `char*` — in `STRING()`, the same defect class as
  `454ca83`/`411b183` (3/3 refs).

Reviewed with nothing portable: the `IBoneSetup`/`GetSkeleton` cache refactor,
the `SetLayerCycle` 4-argument signature and `GetIntensity( event, time )`
signature changes (class C — interface drift, callers are game-side); the
scene-lock rework cluster (`EnterSceneSequence`/`ProcessMoveTo`/`Facing`/
`RemoveChoreoScene` — intertwined with the scene-lock state machine, class D/C);
the `ProcessFlexSettingSceneEvent` override drop (class E); the `EXPRESSION`
choreo dispatch change in `baseflex` (`git log -S` shows the current form dates
from the initial v34 import, so "fixing" it would restore a newer feature, class
E); the `LocalFlexController_t` loop rewrites, `max`→`MAX` normalization and
float-literal cleanup (class D, ~25 items).

**Batch 8 — props.cpp triage (2026-11-01):** 46 cross-ref changed functions
(dumper found 30 candidates); four applied. The file is the engine entity layer
for physics props, so the wins here are interaction and physics-callback
lifetime bugs:

- `97b4fdd` — `CPhysicsProp::ClearFlagsThink` cleared `FVPHYSICS_WAS_THROWN`
  through `VPhysicsGetObject()` with no NULL check; a prop whose physics object
  was released between scheduling and firing (removal, level teardown) hands
  NULL to `PhysClearGameFlags` and dereferences it. Guarded (4/4 refs; the same
  function in `physics_prop_ragdoll.cpp` already ran guarded).
- `652c93e` — `VPhysicsCollision` dispatched
  `g_interactionHitByPlayerThrownPhysObj` with a NULL pObject, so the NPC's
  handler knew it was hit by a thrown prop but not which one. Passes `this`
  (4/4 refs). Argument-only change: interaction id and handler signature are
  untouched.
- `0b80389` — `HandleAnyCollisionInteractions` impaled *any* NPC the prop hit,
  including allies of the player who threw it, leaving the NPC pinned. Refs
  bail before the stick when `HasPhysicsAttacker( 25.0f )` returns a player the
  hit NPC rates `D_LI` (4/4). Uses the `CBreakableProp::HasPhysicsAttacker`
  override already in `props.h`; props with no recorded attacker behave as
  before.
- `646b31f` — `GetRootPhysicsObjectForBreak` checked only the manager pointer,
  then read follower slot 0; with a manager and zero followers (`CUtlVector`
  emptied while the manager survives) that indexes an empty vector. Gates on
  `GetNumBoneFollowers()` as the refs do (4/4), with the pointer check kept
  because this tree still uses `m_pBoneFollowerManager`.

Reviewed with nothing portable: `Break` (adds `gamestats` crate-smashed
tracking, model-scale explosion and the `m_iszBreakModelMessage` user message —
class E plus protocol-adjacent user messages); `CBreakableProp::OnTakeDamage`
and `PropSetSequence`/`AnimThink`/`FinishSetSequence` (the `m_OnTakeDamage`
output and the `m_iGoalSequence`/`m_iTransitionDirection` transition rework are
absent from this tree, class E/C); `OnUse` (`m_OnLockedUse` output absent);
`OnPhysGunPickup`/`OnPhysGunDrop`/`HandleFirstCollisionInteractions` (the
`PROPINTER_PHYSGUN_NOTIFY_CHILDREN` flag and `IParentPropInteraction` interface
are absent); `VPhysicsUpdate` (`m_OnOutOfWorld` absent); `Spawn` (`IsXbox()` →
`AUTOAIM_ON_CONSOLE` is game/platform behaviour, class E); `TestCollision`
(ours is `CBasePropDoor`'s, refs rewritten on `CDynamicProp` with the bone
follower manager — different class, class C); `OnRestore`, `UpdateOnRemove`,
`GetRootPhysicsObjectForBreak` aside (pointer→object manager refactor, class C);
`InputDisableCollision`/`InputEnableCollision` (ours sets `m_bDisableCollision`,
which `ShouldCollide` still reads, so the input is not dead — refs moved to
`FSOLID_NOT_SOLID`, class D); `DoorTeleportToSpawnPosition`
(`SF_DOOR_START_OPEN_OBSOLETE`, absent here, class C); `PropSetAnim` `Msg`→
`Warning` (log level only, class D); and the `max`/`min`→`MAX`/`MIN` plus
float-literal cleanup (class D, ~8 items).

Highest-value item **not** taken: `HandleInteractionStick` defers its
`Teleport`/`SetEnableMotionPosition`/`EnableMotion`/`SetCollisionGroup` work
through `g_PostSimulationQueue.QueueCall( this, &CBreakableProp::StickAtPosition,
... )` instead of mutating a physics object from inside its own collision
callback — that is the correct shape of the fix, but this tree has neither the
post-simulation queue nor `StickAtPosition` (grep: zero hits), so porting it
would mean inventing deferred-call infrastructure rather than backporting, and
it stays a documented skip.

**Batch 9 — physics.cpp / physobj.cpp triage (2026-11-04):** the physics
seam this project had not swept yet: the collision-event callbacks the engine
calls back into, and the entity side of the vphysics glue. Cross-ref candidate
counts were 15 (`physics.cpp`), 8 (`physobj.cpp`), 7 (`physconstraint.cpp`) and
6 (`physics_prop_ragdoll.cpp`); three applied. Most of the rest is the 2007+
vphysics API (`SetPerformanceSettings`, `EnableConstraintNotify`,
`IPhysicsConstraintGroup::SolvePenetration`, `Vector*` velocity/ABB out-params)
or game features this tree does not have.

- `819168a` — `CCollisionEvent::ShouldSolvePenetration` marked a pair
  `COLLSTATE_TRYDISABLE` after three seconds of interpenetration and *also*
  returned 1, so the default solver pushed the pair apart in the same frame
  that `UpdatePenetrateEvents` forced it to sleep; the two undo each other and
  the pair keeps re-arming the event. Returns 0 from the disable branch, with
  the refs' block order (report → reset `startTime` → try disable) so
  reporting still happens (4/4 refs).
- `eafc450` — `CPhysMagnet::ConstraintBroken` dereferenced the magnetized
  entry's `hEntity` and then `pPhysObject->GetMass()` unguarded; a prop removed
  (or object released) before constraint teardown crashes there. Both guarded,
  entry still removed and `m_OnMagnetDetach` still fired (4/4 refs).
- `eff5879` — `CPhysImpact::InputImpact` traced once from its own origin, so an
  impact entity embedded in geometry got a `startsolid` trace with no real
  surface normal and applied its force along the impact axis (or zero). Re-traces
  from `start + dir * 0.1` when the first trace starts solid and uses that
  result throughout (3/3 refs).

Reviewed with nothing portable: `ShouldSolvePenetration`'s ragdoll
self-penetration block (`pEntity0 == pEntity1` → `Ragdoll_GetRagdoll` →
`pGroup->SolvePenetration`) needs `IPhysicsConstraintGroup::SolvePenetration`,
which this tree's `vphysics_interface.h` does not declare — engine-side API,
class C, so ragdoll limbs keep the default solver; `LevelInitPreEntity`
(`SetPerformanceSettings`, `EnableConstraintNotify`, `GetCurrentGravity`) and
`Friction`/`InitRagdoll` (`GetVelocityAtPoint( Vector* )`,
`CollideGetAABB( Vector* )`, `RagdollActivate` wake flag) are vphysics ABI
churn, class C; `FrameUpdate`/`PostSimulationFrame` differ only by
`g_PostSimulationQueue` + `FlushQueuedOperations` (absent here, and
`UpdateImpulseEvents`/`UpdateSetVelocityEvents` are idempotent — each clears
its list — so the apparent double call is not a double apply); `LevelShutdownPostEntity`
and `PreClientUpdate` need `m_breakSounds`/`FlushVehicleScripts`, class E;
`AddDamageEvent`'s `g_pGameRules->Damage_GetTimeBased()` is `DMG_TIMEBASED` in
CS:S gamerules, so behaviour-identical; `InputConvertTarget` (`SF_CONVERT_AS_DEBRIS`,
`m_flMassOverride`, `UTIL_TransferPoseParameters`), `PushThink`
(`SF_PUSH_PLAYER`/`SF_PUSH_PHYSICS`), `OnPhysGunPickup` (`m_OnPhysGunPunt`) and
`CreateVPhysics` (`ShouldDampRotation`) reference spawnflags/outputs/APIs this
tree does not have, class E; `DeltaTimeSinceLastFluid`'s raw `pEntity` →
`EHANDLE` is a real lifetime tidy-up but would put a handle type into
`game_shared/physics_shared.h`, which is deliberately forward-declaration-only
(only `physics.h` includes it) for a non-crashing symptom (a recycled address
can suppress a water splash for `FLUID_TIME_MAX`), so it is deferred; the
remaining items are `const`-ness, `max`→`MAX`, `EnsureCapacity(4)`,
`swap`→`V_swap`, `DevMsg` level and `#ifdef PORTAL` churn (class D).

**Batch 10 — func_break.cpp / physics_main.cpp triage (2026-11-04):** two
files at a time from here on. `func_break.cpp` is the breakable-prop state
machine (8 candidates of 27 functions) and `physics_main.cpp` is the entity push
/ step seam (14 of 42); three applied.

- `0fbbeef` — `CBreakable::Die` spawned its chunk loop unconditionally.
  `GetRandomChunkModel("")` compares with `strlen("") == 0`, so it matches the
  *first* section in propdata.txt: an unconfigured breakable shattered into
  whichever chunk set parsed first, and a section missing from propdata.txt
  returns NULL into `modelinfo->GetModelIndex()`. Loop now requires
  `m_iszModelName` and a non-NULL chunk model (3/3 refs for the string guard).
- `00e96ae` — `CBreakable::OnTakeDamage` computed an unused
  `GetInflictor()->GetAbsOrigin() - WorldSpaceCenter()` into `vecTemp`, the only
  dereference of an inflictor that physics-impact damage can leave NULL. Dropped
  statement and local (2/2 refs).
- `1c783c2` — `CBaseEntity::PhysicsStep` returned "not going to think" *above*
  the npc_vphysics shadow-follow block, so a MOVETYPE_STEP entity under vphysics
  only took its simulated origin on think frames, lagging up to a whole think
  interval and snapping when the tick arrived. The gate now sits after the feed,
  where it still guards the think/time-step work (3/3 refs).

Reviewed with nothing portable: `VPhysicsCollision`'s
`if ( m_Material == matGlass ) SetMass( 2.0f )` is a gameplay tuning hack around
glass damage, not engine logic, class E; `TraceAttack`'s `CDmgAccumulator`
parameter is a damage-refactor API this tree does not have, class C;
`ResetOnGroundFlags` (portal fizzle), `Precache` (`HL2_EPISODIC` case) and
`PhysicsStepRunTimestep` (dead `hitsound` block) are `#ifdef`/dead-code churn,
class D; `DamageSound` `char *`→`const char *`, `UpdateHealth` `0,1`→`0.f,1.f`
and `V_swap` are class D; `EntityToWorldTransform`-based `Teleport` in
`physics_prop_ragdoll.cpp` is the one remaining ragdoll candidate but it needs
`VPhysicsSwapObject` reordering proof on a live sim, deferred rather than
guessed at. In `physics_main.cpp`: `g_pPushedEntities` (a per-thread pushed-entity
object instead of the file-static `s_PushedEntities`), `m_flMoveTime`,
`partition->HideElement`/`UnhideElement`, `GetCurrentGravity()` and the
`sv_gravity`→per-entity-gravity switch are all newer-tree API or new members
(class C), and `SpeculativelyCheckPush`/`PhysicsDispatchThink`/`PhysicsPushEntity`
differ only in `TF_DLL` message formatting, `PORTS`-style `#elif` and warning
text (class D).

**Batch 11 — gameinterface.cpp / physics_bone_follower.cpp (2026-11-04):**
the DLL entry points and level lifecycle (19 candidates of 54 functions) plus
the bone-follower shadow proxies (10 of 22); three applied.

- `203a9aa` — `CBoneFollower::Init` marked every follower's shadow physically
  controlled. Its own comment states the precondition: the entity does not run
  `PhysicsPusher()`, so vphysics has to move it -- true only for NPC owners. A
  prop owner *does* push its followers, so marking those physically controlled
  too let vphysics simulate them a second time and they stopped tracking the
  bone transform. Guarded with `pOwner->IsNPC()` (4/4 refs).
- `f3c73b5` — `CServerGameDLL::LevelInit` assigned `gpGlobals->eLoadType` only
  *after* `BeginRestoreEntities()` and `engine->LoadGameState()`; every restore
  path that branches on it (`CAmbientGeneric::Activate` choosing SND_SPAWNING vs
  SND_NOFLAGS, `CLogicAuto::Think`, the commentary system) read the previous
  level's value. Assignment moved above the restore (2/2 refs that keep the
  single-player path).
- `f55c2a1` — `CServerGameDLL::LevelShutdown` tore down entities and released
  their model references without ever taking `MDLCACHE_CRITICAL_SECTION()`
  (3/3 refs take it before `LevelShutdownPreEntityAllSystems()`).

Reviewed with nothing portable: `CheckTransmit` needs
`CServerNetworkProperty::GetNetworkParent`, `CBasePlayer::IsReplay` and
`sv_force_transmit_ents`, none of which exist here (class C);
`CreateNetworkStringTables` would add particle/mapcycle string tables, i.e.
protocol-adjacent (class C); `ClientSettingsChanged` needs
`g_pGameRules::IsConnectedUserInfoChangeAllowed` (class C); `LevelShutdown`'s
steam-context/pre-clear-system calls, `LevelInit`'s ECON item schema, X360 rich
presence and `g_pServerBenchmark`, `GameFrame`'s `UpdateQueryCache` /
`TheNextBots` and `PostInit`'s `IGameSystem::PostInitAllSystems` are all
newer-tree API (class C); `GameFrame`'s static `skipframe` versus tickcount
parity and the `oldframetime` capture point are equivalent in practice and the
frametime restore in our order is the safer of the two (class D);
`GetTickInterval` ignoring `-tickrate` for CS/TF/DOD would change how the
server picks its tick rate, deliberately not touched; `PreClientUpdate`'s
`#ifdef _DEBUG` around `sv_showhitboxes` removes a working feature (class E);
`ServerActivate` / `GetBugReportInfo` / `LoadMessageOfTheDay` /
`ClientActive` differ only in nav-mesh hooks, format strings,
`LoadSpecificMOTDMsg` and TF GC client calls (class D/E).

`sceneentity.cpp` was triaged and deliberately left for later: its 28
candidates are dominated by a newer async-VCD-loading rework our tree predates
in the opposite direction (`m_bAsyncVCDLoadPending` / `PollForAsyncLoading` /
`BlockingLoadScene` versus `m_bSceneMissing` / `LoadScene` / `OnLoaded`), by
`!IsMultiplayer()` guards around expression/flex/lookat/moveto/face dispatch,
by the `AI_Response` value-type refactor, and by `m_OnTrigger9..16` outputs this
tree does not declare -- adding those would be a datatable/ABI change. The
close-caption path also switched from `WRITE_LONG( CRC32 )` to
`WRITE_STRING`, which is a network-message change and therefore out of scope.
`FindNamedEntity`'s `GetFilterResult()`, `PrefetchAnimBlocks`'s
`pAnim( &iFrame )`, `DoThink`'s `m_fPitch` and `UpdateTransmitState`'s
`m_pRecipientFilter` are absent here (class C).

**Batch 12 — logicentities.cpp / doors.cpp (2026-11-04):** the logic_* entity
family (12 candidates of 51 functions) and the door mover (10 of 35); two
applied.

- `d8abc9f` — `CLogicBranchList::DoTest` dereferenced the resolved handle of
  every entry in its branch list. The list holds entity handles, so a
  `logic_branch` that is killed or cleaned up while the listener lives leaves a
  NULL handle and the listener faults on its next test. Guarded with
  `pBranch &&` (4/4 refs), with a removed branch still counting as "not all
  true" exactly as before.
- `1ab83b7` — `SF_DOOR_NONSOLID_TO_PLAYER` only set
  `COLLISION_GROUP_PASSABLE_DOOR`, which stops the player's traces hitting the
  door but not the door's own push from finding the player in the way: the door
  then reported `Blocked`, reversed and could crush-damage the player it is
  supposed to be transparent to. Added `FL_UNBLOCKABLE_BY_PLAYER`, the flag
  `CPhysicsPushedEntities::PerformLinearPush/PerformRotatePush` reads to move a
  player/NPC blocker to the push destination instead of registering a blockage
  (3/3 refs).

Reviewed with nothing portable: in `logicentities.cpp`, `CMathCounter`'s
`m_bDisabled` guards go with Enable/Disable inputs this tree never declares
(class E), `CEnvGlobal`'s `counter` key and `CLogicAutosave`'s
`m_minHitPoints`/`m_minHitPointsToCommit` keys do not exist here (class E), and
`DrawDebugTextOverlays`/`InputSetValue`/`InputDisable` are triage misalignments
where ours and the refs define different classes with the same function name
(class D). In `doors.cpp`, `m_NoiseMovingClosed`/`m_NoiseArrivedClosed`,
`m_eSpawnPosition` (`spawnpos`), `m_OnLockedUse` and `m_bIgnoreNonPlayerEntsOnBlock`
are all new keyvalues/outputs/members (class E), and the
`SF_DOOR_START_OPEN` -> `SF_DOOR_START_OPEN_OBSOLETE` renames are the same bit
`1` (class D).

`trains.cpp` was triaged alongside them and deferred as a whole: `m_OnNext`,
`m_flNextMPSoundTime`, `m_bAccelToSpeed`/`m_flSpeedForwardModifier`,
`m_lastBlockPos`/`m_lastBlockTick` and `path_track`'s `InTeleport` input are
absent (class C/E), `SetSpeed(float,bool)` is a signature change into that
acceleration feature, and the one behavioural candidate -- `Blocked` returning
early for a player when `SF_TRACKTRAIN_UNBLOCKABLE_BY_PLAYER` has a physics
blocker, so the train pushes through instead of crush-damaging -- removes map
crush damage and needs a live train map to judge. `doors.cpp`'s
`( m_bForceClosed || m_flWait < 0 )` solver condition was left alone for the same
reason.

**Batch 15 — world.cpp / physics_npc_solver.cpp / soundent.cpp (2026-11-04):** a
candidate that had been sitting in the queue as "probably churn" turned out to be
a real bug, so `world.cpp` (4 candidates of 17 functions) went in with the two
physics solvers (2 of 11); `soundent.cpp`'s leftover diagnostics, sized too small
to justify a batch of their own, rode along. Three files, five fixes, three
commits.

- `e819563` — `CWorld::StaticDecal` built its decal entity index as
  `entityIndex = (short)trace.m_pEnt ? trace.m_pEnt->entindex() : 0`. The cast
  binds to the *pointer*, so the NULL test ran on the low 16 bits of the
  address: a valid entity whose low bits happened to be zero read as "no
  entity" and its decal was baked into world space instead of following the
  brush, which made the result depend on heap layout. The cast now applies to
  `entindex()` (3/3 refs).
- `3c8e6cf` — `CPhysicsNPCSolver::BecomePenetrationSolver` dereferenced
  `m_hEntity` unguarded; both sides are EHANDLEs and the moving entity can be
  removed in the same frame, leaving a NULL handle and a crash on the next
  physics tick. `CPhysicsEntitySolver::Spawn` dereferenced
  `VPhysicsGetObject()->RecheckContactPoints()` on a blocker that may no longer
  have a physics object. Both guarded (4/4 refs each).
- `af84694` — diagnostics only: the `soundlist` add/remove messages now carry
  `gpGlobals->curtime`, and level init warns when the pool cannot cover
  `maxClients+16` on this tree's 128-entry `MAX_WORLD_SOUNDS_MP`. Allocation and
  expiry behaviour is untouched.

Skipped in `world.cpp`: `Precache` and `KeyValue` are `ConVar *` +
`FindVar` -> `ConVarRef` cleanups plus `g_pGameRules->Init()` and
`PrecacheStandardParticleSystems()`, neither of which exists here (class D/C/E);
`Spawn`'s `GlobalEntity_Add` pair adds global-state keys this mod never reads
(class E). In `soundent.cpp` the remaining delta is `min` -> `MIN` (class D), and
in `util.cpp` the single candidate switches
`Msg( " %s: %d", name, GetEntitySize() )` to `%llu` + `(uint64)` -- that tree's
`GetEntitySize()` is 64-bit, this tree's returns `int`, so the existing `%d` is
correct and the change does not apply (no-op port, not taken).

**Batch 14 — physics_prop_ragdoll.cpp / filters.cpp (2026-11-04):** the ragdoll
prop (6 candidates of 43 functions) plus the smallest queued engine file, the
entity filters (2 of 9); two files, four fixes, two commits.

- `cbc7509` — `CRagdollProp::VPhysicsUpdate` ignored the return value of
  `RagdollGetBoneMatrix`, which already returns `bool` in this tree and returns
  `false` when the group is in the error state, leaving `boneToWorld`
  untouched. The loop then ran uninitialised stack matrices through
  `MatrixAngles` and published the result as `m_ragPos`/`m_ragAngles`; since the
  tail of the function does `SetAbsOrigin( m_ragPos[0] )`, the prop itself was
  moved to a garbage origin. Failed reads now publish zeroed pos/angles
  (3/3 refs). The `RagdollSolveSeparation` call also moved from before the read
  to after it, and out of the sleeping case entirely — it was correcting the
  object before the matrices it corrects had been read (3/3 refs). The
  surrounding-bounds loop also dereferenced `m_ragdoll.list[i].pObject` with no
  NULL check; a released object now contributes an empty AABB (3/3 refs).
- `a8dc7bd` — `CFilterMultiple::Activate` stored any entity found by name into
  `m_hFilter[]`, and all four consumers cast it back to `CBaseFilter *` with a
  C-style cast and call a virtual on it. A `filter_multi` key naming a non-filter
  entity (a typo that resolves to a prop, or a mapper pointing at `logic_auto`)
  produced a bogus `CBaseFilter *` and a virtual call through the wrong vtable —
  crashable from map data alone. `dynamic_cast` now rejects it with a warning and
  the surviving handles are compacted to the front of the array (3/3 refs); the
  consumers only test slots for NULL, so AND/OR behaviour is unchanged.

Nothing portable left in either file. In `physics_prop_ragdoll.cpp`:
`InitRagdoll` takes a new `bWakeRagdoll` parameter and fills in
`allowStretch`/`fixedConstraints` (class E — `SF_RAGDOLLPROP_STARTASLEEP` and
`SF_RAGDOLLPROP_ALLOW_STRETCH` do not exist in this tree, and `RagdollActivate`
already defaults to `bForceWake = true`, so the parameter would be a no-op here);
`Spawn`'s additions need `DisableMotion()` on the prop and `m_bStartDisabled`,
neither of which exists on `CRagdollProp` (class C/E); `SetupBones` calls
`UpdateModelScale()` and `pStudioHdr->boneFlags()` where this tree has
`UpdateModelWidthScale()` and a `mstudiohitboxset`-era `mstudiobone_t::flags`
(class C); `Teleport` is a rewrite that changes where the entity lands relative
to its ragdoll, which in this tree already derives the delta from the
post-teleport entity transform and stays self-consistent (class B — skipped as
risky); and `TraceAttack` changes signature for `CDmgAccumulator` (class C). In
`filters.cpp`, `PassesProximityFilter`'s `swap` -> `::V_swap` is a helper rename
this tree does not have (class D/C).

Also triaged from the queue: `func_movelinear.cpp` (1 candidate,
`CFuncMoveLinear::Blocked`) is the HL2 "remove breakables the door crushes"
hack, gated on a `func_breakable` classname test that means nothing in this mod
(class E); `soundent.cpp` (3, `Think`/`InsertSound`/`Initialize`) and
`util.cpp` (1, `ReportEntitySizes`) are counted but not read in detail, so they
stay queued. `physics_npc_solver.cpp` (2) remains open.

**Batch 13 — triggers.cpp / buttons.cpp (2026-11-04):** the trigger family
(12 candidates of 63 functions) and the button movers (6 of 39); two files, four
fixes, two commits.

- `1136d95` — `CTriggerLook::Trigger` cancelled its timeout think with
  `SetNextThink( 0 )`, which means next frame rather than never; the trigger
  stayed on the think list being dispatched against a NULL think function, and
  every successful look parked another instance of that. Now
  `TICK_NEVER_THINK` (4/4 refs). `CTriggerHurt::HurtEntity` also short-circuits a
  player whose client has already gone: a leaver keeps `m_takedamage` and passes
  the filters for the rest of the frame, so trigger_hurt damaged it and fired
  `OnHurtPlayer` on it (2/2 refs).
- `8ef3c55` — `CMomentaryRotButton` handed `min( flAngleDelta / m_flSpeed, 0.1f )`
  straight to `SetMoveDoneTime`, so a sub-tick interval fired move-done before
  the angle moved at all and the button snapped/overshot its goal. The interval
  is now clamped to 1..6 ticks, with the angular velocity raised to cover the
  remainder inside one tick when the honest interval is below it; the same
  re-derivation replaces the stale `m_flTimeDelta` in `SetPositionMoveDone`
  (4/4 refs). `Use` also re-armed the per-frame `UpdateThink` unconditionally, so
  a held button thought twice in a frame on a stale angular velocity; it now
  only arms the think when none is pending (4/4 refs).

Reviewed with nothing portable in `triggers.cpp`: `CTriggerCamera::Enable`
and `Disable` need the `m_nOldTakeDamage` member plus the HL2_EPISODIC
`m_bInterpolatePosition` path (class E), `CTriggerVPhysicsMotion::Simulate`
calls `GetCurrentGravity()` which this tree does not have (class C),
`InitTrigger`'s `FSOLID_TRIGGER_TOUCH_DEBRIS` does not exist here and adding a
solid-flags bit the collision code never reads would be a no-op (class C),
`CBaseVPhysicsTrigger::PassesTriggerFilters` is a different class from the
`CBaseTrigger::` body the refs show (class D), `CBaseTrigger::Touch`'s
`SF_TRIG_TOUCH_DEBRIS` guard needs `lagcompensation->IsCurrentlyDoingLagCompensation()`
(class C) and its `m_flPushSpeed` half is a new keyfield (class E), and
`FindQualifiedNPC`/`InputChangeLevel` change signatures or call
`GetBonusChallenge()` (class C). In `buttons.cpp`, `InputPress` was folded into a
`Press()` helper this tree does not have (class C) and the rest is `0` -> `0.f`
churn (class D).

Also scanned and rejected in the same pass, all engine-layer files still
unswept: `physconstraint.cpp` (7 candidates, all part of the `OnConstraintSetup`
/ `GetRagdollConstraintGroup` / `m_minTeleportDistance` / noise-precache rework,
class C/E), `rope.cpp` (6, the `m_iRopeMaterialModel` -> `m_iRopeMaterialModelIndex`
networkvar rename plus a `ROPE_NO_WIND` key, class C/E/D), `world.cpp` (4),
`soundent.cpp` (3), `filters.cpp` (2), `physics_npc_solver.cpp` (2),
`func_movelinear.cpp` (1) and `util.cpp` (1) — counted but not triaged in
detail, so they stay in the queue rather than being called clean;
`physics_impact_damage.cpp`, `entityblocker.cpp`, `mapentities.cpp` and
`ndebugoverlay.cpp` have no candidates at all. `physics_prop_ragdoll.cpp` (6),
`world.cpp`, `soundent.cpp`, `filters.cpp`, `physics_npc_solver.cpp`,
`func_movelinear.cpp` and `util.cpp` remain open for future batches.

## Landed backports

Commits oldest → newest; each body carries the full explanation (why
upstream changed it, compatibility rating, build verification). `7fdd22f`
removed the `BUGFIXED` switch and `22084c4` added this report.

| Commit | Change |
|---|---|
| `6ded251` | SetParentAttachment -1-attachment guard; warn on destroying constraint-bound physics objects (nillerusr) |
| `9674cee` | util.cpp console-command hardening: format string, missing debugoverlay, NULL local player |
| `d97f4cd` | CBaseAnimating hardened against stale/invalid model state |
| `e2b035b` | CNavMesh::GetGroundHeight → upstream iterative ground probe |
| `dd29423` | Debris stuck inside non-static physics objects (nillerusr penetration solver) |
| `ea02fbc` | IK_GROUND foot trace re-anchored to estimated floor height (CalculateIKLocks) |
| `9eb1faf` | IK step-height estimator reset on teleport (InitStepHeightAdjust) |
| `f3975e0` | Entity flame removed when it vanishes into a client-side ragdoll (BecomeRagdollOnClient) |
| `a448fed` | NULL-guard UTIL_PlayerByIndex( 1 ) in the dangerous-autosave timer paths |
| `2b122f2` | Validate numcmds against totalcmds before processing client usercmds (Asserts vanish under -DNDEBUG) |
| `b36c793` | NULL-guard g_pGameRules in the CServerGameClients::ClientConnect path |
| `2c563cc` | Clear the pending dangerous-autosave timer on level change (LevelInit) |
| `c5bd690` | Honor cl_interp when the server computes a player's interpolation time (lag-comp rewound less than the client interpolated) |
| `2ab23a1` | NULL-guard pPlayer in CServerGameClients::ClientSetupVisibility |
| `6d23314` | Entity I/O debug buffers passed to DevMsg as data, not format strings |
| `6c30f85` | Detect truncated output in V_snprintf/V_vsnprintf; fix size_t underflow in V_strncat |
| `99af5f1` | Guard the null-termination write in CUtlBuffer::AddNullTermination against a negative index |
| `5843414` | Clamp the hitbox cross-axis separation test with a projection epsilon (ClipRayToHitbox) |
| `e0f5723` | Tolerate a NULL value string in ConVar::InternalSetValue and ConVar::ChangeStringValue |
| `b888440` | README + this report brought back in sync with the tree (docs-only; FIXES.md frozen as archive) |
| `0f86f08` | `~CBaseEntity` clears the ground entity during teardown (3/3-ref consensus, finding 4) |
| `aa75b38` | nav_generate skips already-covered seeds instead of creating duplicate nodes (safe deviation from upstream's abort) |
| `615515d` | XZip PUTBYTE/PUTSHORT flush-bounds guard (3/3 refs; file is Windows-only dead source) |
| `0438a3a` | Physics-reasonableness guards in baseentity_shared: VPhysicsUpdate bounds checks (fixes the `IsFinite(angles.x)`-twice/never-z bug), impulse clamps, throttled spew |
| `6870376` | FireBullets clamps a startsolid trace back to its start position (Tracker 70354/63250 — out-of-range networked tracer endpos) |
| `9870ae4` | IntersectRayWithOBB transforms its result back to world space and un-doubles the fraction (3/3 refs; endpos was OBB-local, fraction was half the true value) |
| `cb7a5e0` | IntersectRayWithBox snaps a startsolid trace's startpos to the solid exit point (3/3 refs; pFractionLeftSolid param skipped — no callers) |
| `550029b` | ConVar::Create parses its default int with atoi, not a float cast (3/3 refs — >2^24 defaults lost precision) |
| `16970b2` | CUtlCharConversion table lookups cast the index to unsigned char (3/3 refs — signed-char negative subscripts) |
| `af15460` | CUtlBuffer::Get rejects non-positive sizes and asserts the read index (3/3 refs) |
| `ab8d754` | CUtlBuffer::SeekGet validates the landing position via OnGetOverflow (3/3 refs) |
| `78b6a2e` | NULL-guard the five shared physics object factories incl. PhysCreateWorld_Shared's vcollide checks (upstream consensus) |
| `1ff8704` | Bound the worldspawn fluid index before indexing the solids array (sdk2013; second site is dead USE_PHX_FILES code) |
| `454ca83` | STRING(classname) in the global-entity mismatch Warning (3/3 refs — strong string_t passed raw to %s) |
| `33ab533` | NULL-guard SaveInit's engine allocations (3/3 refs; upstream's error-path leak corrected) |
| `9ea3ba1` | CSave::WriteFunction writes a BADFUNCTIONPOINTER placeholder instead of dropping the field (3/3 refs — a dropped field desynced the save stream) |
| `19681c4` | Bound ParseFile's output token with a caller-supplied max length (3/3 refs; OUT_Z_* annotations dropped — absent from this tree) |
| `a5a21b6` | animation.cpp: AE_TYPE_SERVER event filter (3/3), negative bodygroup guards, FindBodygroupByName IsValid(), ExecuteNTimes log throttling |
| `1d60224` | sv_shootpositionfix reworked onto the ClientMod-Api/firebulletsfix algorithm: origin swapped back around PostThink, stock RunCommand order restored (hit-registration bug hunt) |
| `fd72abe` | CUtlVector::FastRemove skips the self-memcpy when removing the last element (3/3 refs) |
| `23d4f04` | CUtlRBTree::CopyFrom copies whole nodes instead of sizeof(T): truncated node array (sdk2013 fix) |
| `1af5d81` | Lag compensation teleport guard measures horizontal distance only (Length2DSqr + sv_lagcompensation_teleport_dist cvar, 3/3 refs + Valve 2011 fix): vertical game-code motion no longer aborts a backtrack or skips a restore |
| `1c8b4de` | Lag compensation records and restores the victim's pose parameters (`m_flPoseParameters[MAXSTUDIOPOSEPARAM]`) around a backtrack, mirroring sdk2013/CS:GO: rewound bones are rebuilt in the pose the shooter actually saw (CS:S players are LEGANIM_9WAY and `FinishMove` rewrites `body_pitch` every command), not in the current pose — a real contributor to model/hitbox misalignment |
| `b16c437` | `VPhysicsUpdatePusher` blocked-pusher rewind math corrected (moveback = movetime*fraction, gate start+0.99f, blocker NULL-check) — 3/3 refs |
| `2d068fd` | Local-setter physics-reasonableness guards: SetLocalOrigin/SetLocalVelocity/SetLocalAngularVelocity + new `IsEntityQAngleVelReasonable` helper |
| `b282b73` | `GetVelocity` NULL physics-object guard (4/4) + `AcceptInput` `DevMsg("%s", szBuffer)` format-string fix (3/3) |
| `8234594` | `SetMoveType` calls `UpdateWaterState()` for MOVETYPE_FLY/FLYWATER (3/3) |
| `c1fc932` | `TakeHealth` returns the healed delta and uses `GetMaxHealth()` (3/4) |
| `f20460c` | `VPhysicsShadowUpdate` ROTPUSH stuck-check + unified recovery + dead-player skip (3/3, `WasFrozen()` half ABI-deferred) |
| `7b0c54b` | `TraceAttack` NULL-attacker guard (3/3) |
| `3fc99ae` | `GetNetworkIDString` NULL-guards the engine network-ID string (3/4, csgo uses `"UNKNOWN"`) |
| `2e4e3a5` | Lag compensation refuses a second `Start` while a session is active (sdk2013 runtime guard + Finish/FrameUpdate self-clears; Finish made idempotent so a stray second Finish cannot replay the restore) |
| `411b183` | SetLightingOriginRelative DevWarnings wrap their `string_t` args in `STRING()` (3/3 refs; same defect class as `454ca83`) |
| `071ebd7` | CBaseAnimating constructor initializes networked `m_vecForce`/`m_nForceBone` unconditionally (was `_DEBUG`-only; 3/3 refs) |
| `0856942` | GetBoneCache rejects future-stamped cache entries and uses mask containment instead of exact equality (3/3 refs) |
| `26e2f2a` | HandleStartGestureSceneEvent bounds its faceposer tag copies (Q_strncpy) and STRING()s GetModelName() in the looping-gesture warning (3/3 refs) |
| `1329ddd` | AllocateLayer caps m_AnimOverlay at MAX_OVERLAYS (the SendPropUtlVector max) and returns -1 when full; paired Debug Assert dropped (3/3 refs) |
| `5f3fc10` | CAnimationLayer::Init zero-fills m_flBlendIn/m_flBlendOut/m_flLastEventCheck; RestartGesture resets the event check (2/3 refs) |
| `97b4fdd` | CPhysicsProp::ClearFlagsThink NULL-guards the physics object before clearing FVPHYSICS_WAS_THROWN (4/4 refs) |
| `652c93e` | VPhysicsCollision passes the thrown prop as pObject to g_interactionHitByPlayerThrownPhysObj (4/4 refs) |
| `0b80389` | HandleAnyCollisionInteractions does not impale NPCs friendly (D_LI) to the prop's physics attacker (4/4 refs) |
| `646b31f` | GetRootPhysicsObjectForBreak requires a bone follower before indexing follower 0 (4/4 refs) |
| `819168a` | ShouldSolvePenetration returns 0 for the COLLSTATE_TRYDISABLE branch so the solver stops fighting the sleep path (4/4 refs) |
| `eafc450` | CPhysMagnet::ConstraintBroken NULL-guards the magnetized entity handle and its physics object (4/4 refs) |
| `eff5879` | CPhysImpact::InputImpact re-traces from start + dir*0.1 when the first trace starts solid (3/3 refs) |
| `0fbbeef` | CBreakable::Die only emits chunk models when a gib set is configured and the chunk lookup succeeded (3/3 refs) |
| `00e96ae` | CBreakable::OnTakeDamage drops the unused inflictor-origin computation that could dereference a NULL inflictor (2/2 refs) |
| `1c783c2` | PhysicsStep feeds the vphysics shadow position before the think-tick gate instead of only on think frames (3/3 refs) |
| `203a9aa` | CBoneFollower::Init only marks the follower physically controlled for NPC owners, whose followers vphysics has to move (4/4 refs) |
| `f3c73b5` | LevelInit assigns eLoadType before BeginRestoreEntities/LoadGameState so restore paths see the right load type (2/2 refs) |
| `f55c2a1` | LevelShutdown holds MDLCACHE_CRITICAL_SECTION across entity teardown and model release (3/3 refs) |
| `d8abc9f` | CLogicBranchList::DoTest guards the branch handle so a removed logic_branch no longer faults (4/4 refs) |
| `1ab83b7` | Non-solid-to-player doors also get FL_UNBLOCKABLE_BY_PLAYER so players in the doorway stop blocking the push (3/3 refs) |
| `1136d95` | trigger_look parks its timeout think with TICK_NEVER_THINK; trigger_hurt skips disconnected players (4/4, 2/2 refs) |
| `8ef3c55` | momentary_rot_button clamps its move interval to whole ticks and arms the move think only when idle (4/4 refs) |
| `cbc7509` | Ragdoll prop respects RagdollGetBoneMatrix failure, solves separation after the read and guards a null object (3/3 refs) |
| `a8dc7bd` | filter_multi rejects non-filter entities instead of calling a virtual through the wrong vtable (3/3 refs) |
| `e819563` | world StaticDecal tests the trace entity for NULL before truncating it to the decal entity index (3/3 refs) |
| `3c8e6cf` | Physics NPC/entity solvers guard a cleared EHANDLE and a missing physics object instead of dereferencing them (4/4 refs) |
| `af84694` | soundent timestamps its pool diagnostics and warns when the pool is undersized for the client count (3/3 refs) |

## Lag compensation deep dive (hit-registration evidence follow-up)

Full read of `dlls/player_lagcompensation.cpp` against sdk2013 and the 2019
CS:GO reference, plus the whole bullet-hit path: `FX_FireBullets` (the
Start/Finish window) → `CCSPlayer::FireBullet` →
`UTIL_TraceLineIgnoreTwoEntities` with `CS_MASK_SHOOT|CONTENTS_HITBOX` +
`UTIL_ClipTraceToPlayers` → engine `ClipRayToEntity` →
`CBaseAnimating::TestHitboxes` → `GetBoneCache()` → `SetupBones()`.

**Verified equivalent to the newer trees (no action):**

- record capture/pruning (`sv_maxunlag` deadtime, duplicate-simulation-time skip)
- rewind walk: dead-record stop, teleport-distance abort, target-tick selection
  with the 200 ms latency-correction fallback, frac interpolation between records
- restore ordering (size → angles → origin → animation → simulation time) and the
  `sv_unlag_fixstuck` recursion path (off by default)
- bone-cache flush on rewind (`sv_lagflushbonecache` default 1): traces inside
  the window cannot reuse pre-rewind bones; `GetBoneCache()` rebuilds them from
  the rewound sequence/cycle/layers/pose parameters
- hitbox-level clipping on both the main trace and the 60-unit player clip;
  `WantsLagCompensationOnEntity` matches sdk2013 (team / PVS / distance / cone)
- the `sv_shootpositionfix` origin swap (`1d60224`) is atomic inside the
  shooter's own RunCommand, so end-of-frame record capture never observes a
  swapped origin, and Finish restores a settled-phase origin
- every Start/Finish call site pairs tightly (`fx_cs_shared`, `weapon_knife`,
  hl2mp paths) — no nested session exists in this tree today

**Divergences found:**

1. Landed as `2e4e3a5`: the double-Start guard both newer trees have (full
   rationale in that commit body).
2. sdk2013 restores pose parameters it never captures — only CS:GO captures
   `restore->m_flPoseParameters`; `1c8b4de` already matches CS:GO, so this tree
   is ahead of sdk2013 here.
3. Kept deliberately: 2D teleport check (`1af5d81`), integer target-tick math
   (sdk2013-identical; CS:GO's float version moves the rewind by well under one
   tick), `sv_maxunlag 1.0` (CS:GO's 0.2 default is a gameplay change), retained
   animation interpolation (CS:GO dropped it; interpolating matches what the
   client rendered), `MAXSTUDIOPOSEPARAM` loops (bounded by the count guards in
   `Studio_Set/GetPoseParameter`).

**Evidence mapping:** a headshot registered against a <1-second-old head
location with the rendered model offset from the tested hitbox is exactly the
signature of a rewound origin whose animation pose was not rewound with it.
Four landed fixes each cover part of that class — `1c8b4de` (pose parameters),
`c5bd690` (cl_interp), `1af5d81` (teleport abort), `1d60224` (shoot position) —
and it is unknown whether the recorded evidence predates them; re-test the
recorded scenario on the current build before treating the symptom as open.

## Artifacts

* `inventory.json` — per-file, per-ref function lists (`changed`,
  `bf_only`, `added`, `removed`) with reference paths.
* `diffs/<ref>/<file>.txt` — per-function unified diffs for every changed
  function (27 MB corpus).
* Tooling: the inventory script (re-runnable after any source change).
  The `inventory.json`/`diffs` corpus was generated **before** `7fdd22f` and
  is not checked in — regenerate it after any source change before trusting
  its line/function data again.

*(This report is committed as the standing analysis document for the porting
project. `FIXES.md` is an archive of the pre-port fix set and is no longer
updated; `README.md` links here for current status. The machine artifacts
(inventory + diff corpus) live outside the repo.)*
