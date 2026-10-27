# Upstream Port Report — v34 vs newer Source trees

**Goal:** find functions where newer Source-engine trees diverge from this v34
server, understand *why* they changed (optimization / bug / exploit / feature
/ API churn), and backport the safe ones as drop-in `server_i486.so` code —
without changing the network protocol (a stock v34 client must still connect),
without breaking ABI against `engine.so`/`tier0`, and without importing
game-specific features (TF2/CSGO mechanics are out of scope).

**Status (2026-10-27):** current through commit `e0f5723`. 19 backport
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
| `csgo` | sr2echa/CSGO-Source-Code | 2017 CS:GO | trend reference only (heavy refactor) |

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

**Batch 2 — partially landed:** the `baseanimating.cpp` family
(`d97f4cd`, `ea02fbc`, `9eb1faf`, `f3975e0`), the `physics.cpp`
penetration solver (`dd29423`) and the `nav_mesh` ground probe
(`e2b035b`). **Still open:** the destructor ground-entity study
(finding 4), `nav_generate` sdk2013 fixes, `XZip/XUnzip` file-IO fixes,
and the `baseentity_shared` trace/touch helpers.

**Batch 2.5 — hardening landed while triaging (see Landed backports):**
input validation, NULL-guards, string/buffer bounds checks and the
lag-comp/lerp alignment, applied as individual commits with full
explanations rather than waiting for a batch.

**Batch 3 (deferred / probably skip):** unchanged — `ai_*` (HL2 NPC
content — mostly irrelevant to a CSS server), `cstrike/*` game-logic
changes (CSS-community bugfixes — in scope but reviewed as game logic, not
engine), tier1 container headers after D-filtering.

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
