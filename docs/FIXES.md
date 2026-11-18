# Security fixes
## 1 `CGameUI::Deactivate` — stale player handle crash

**File:** `dlls/game_ui.cpp:130`

**The bug.** `CGameUI::Deactivate()` read the `m_player` entity handle and
immediately dereferenced it:

```cpp
CBasePlayer *pPlayer = m_player;
if ( FBitSet( m_spawnflags, SF_GAMEUI_FREEZE_PLAYER ) )
    m_player->RemoveFlag( FL_ATCONTROLS );   // m_player may be NULL
```

`Deactivate()` is reachable from the `Deactivate` input fired by map logic, from
a `+use`/`+jump` press inside `CGameUI::Think()`, and from the field-of-view
tolerance check. If the player controlling the `game_ui` entity has
disconnected by then, the `CHandle<CBasePlayer>` is stale and resolves to
`NULL`, so the `RemoveFlag` / `m_Local` / `Weapon_Switch` accesses below crash
the server. The same happens when `Deactivate` fires before any player ever
activated the entity while the `FREEZE_PLAYER` or `HIDE_WEAPON` spawnflags are
set. Notably `CGameUI::Think()` already guarded this ("If player is gone, stop
thinking") — `Deactivate()` was the unprotected path.

**Original:** no check; dereference whatever `m_player` resolves to.

**Fixed:**

```cpp
if ( m_player.Get() == NULL )
    return;
```

## 2 `Studio_BoneIndexByName` — null studio header crash

**File:** `public/bone_setup.cpp:4893`

**The bug.** The function dereferenced its `CStudioHdr*` as the first statement:

```cpp
int start = 0, end = pStudioHdr->numbones()-1;
```

Callers pass headers derived from `GetModelPtr()` (ragdoll setup in
`ragdoll_shared.cpp`, physics bone followers, name-based lookups in
`baseanimating.cpp`), so an entity without a valid model reaching a bone lookup
hands this function a `NULL` header and the server dereferences NULL. `pName`
was equally unsafe — passed straight to `Q_stricmp`.

**Original:** no check; crashes on a null header or name.

**Fixed:** returns `-1` (the function's documented "bone not found" result) when
either argument is `NULL`, so callers degrade exactly as they already do for an
unknown bone name.

## 3 `CBaseEntity::TakeDamage` — null gamerules crash

**File:** `dlls/baseentity.cpp:1173`

**The bug.** `TakeDamage()` calls into the gamerules object without checking it
exists:

```cpp
if( !g_pGameRules->AllowDamage(this, inputInfo) )
```

`g_pGameRules` (a global in this server library) is created when a map loads
and destroyed during level change/shutdown. Damage can still be processed while
it is `NULL` — pending physics damage callbacks, explosions finishing their
think, entities taking damage during map change or an error shutdown — and the
dereference crashes the server mid-change.

**Original:** no check; crashes when gamerules are absent.

**Fixed:** the guard sits at the top of the function — no part of damage processing runs without gamerules. On a healthy server the damage would have been decided by
`AllowDamage` anyway, so skipping is behaviorally neutral.

## 4 `CBasePlayer::FindNextObserverTarget` — spectator infinite loop

**File:** `dlls/player.cpp:2615` (guard at `dlls/player.cpp:2630`)

**The bug.** The function picks the next observer target with a `do..while`
loop whose termination condition is `currentIndex != startIndex`:

```cpp
int startIndex = GetNextObserverSearchStartPoint( bReverse );

int currentIndex = startIndex;
do {
    CBaseEntity *nextTarget = UTIL_PlayerByIndex( currentIndex );
    if ( IsValidObserverTarget( nextTarget ) ) return nextTarget;
    currentIndex += iDir;
    // wrap into [1, maxClients]
} while ( currentIndex != startIndex );
```

`currentIndex` is wrapped into `[1, maxClients]` on every step, so the loop can
only ever terminate if `startIndex` itself lies in that range. If the player was
previously spectating something that isn't a player (a non-player entity's
`entindex()` is outside the client range), the start index comes back out of
range, `currentIndex` never equals it, and the server spins in this loop — a
frame-stalling hang of the whole (single-threaded) game server, triggerable by
a spectator switching targets.

**Original:** `startIndex` used verbatim, with no range check.

**Fixed:** clamp before the loop (this is Valve's own later fix, kept with its
`[jason]` comment):

```cpp
if ( startIndex > gpGlobals->maxClients )
    startIndex = 1;
else if ( startIndex < 1 )
    startIndex = gpGlobals->maxClients;
```

## 5 `spec_goto` — unvalidated spectator teleport

**Files:** `dlls/player.cpp:5848` (validator), `dlls/player.cpp:6059` (guard)

**The bug.** `spec_goto` takes five floats straight from a client command and
teleports the roaming/fixed spectator with them:

```cpp
Vector origin;
origin.x = atof( engine->Cmd_Argv(1) );   // no validation
origin.y = atof( engine->Cmd_Argv(2) );
origin.z = atof( engine->Cmd_Argv(3) );
QAngle angle;
angle.x = atof( engine->Cmd_Argv(4) );
angle.y = atof( engine->Cmd_Argv(5) );
angle.z = 0.0f;
JumptoPosition( origin, angle );          // SetAbsOrigin + SnapEyeAngles
```

Any client in `OBS_MODE_FIXED`/`OBS_MODE_ROAMING` can send
`spec_goto inf inf inf nan nan` or values far outside the world. `atof` happily
parses `inf`/`nan` and unbounded magnitudes, so a spectator can be parked at a
NaN origin (NaN poisons PVS/solidity/trace math and spreads through the engine)
or at arbitrary out-of-world coordinates, and NaN eye angles get snapped and
networked.

**Original:** arguments converted with `atof` and applied verbatim.

**Fixed:** every argument must pass `ParseSpecGotoFloat()` before the jump:

```cpp
if ( ParseSpecGotoFloat( engine->Cmd_Argv(1), origin.x ) &&
     ParseSpecGotoFloat( engine->Cmd_Argv(2), origin.y ) &&
     ParseSpecGotoFloat( engine->Cmd_Argv(3), origin.z ) &&
     ParseSpecGotoFloat( engine->Cmd_Argv(4), angle.x )  &&
     ParseSpecGotoFloat( engine->Cmd_Argv(5), angle.y ) )
    JumptoPosition( origin, angle );
```

The validator only accepts plain decimal/scientific input (digits, sign, dot,
exponent), which rejects `nan`, `inf`, hex and trailing-junk spellings at the
string level; then `strtod` must consume the whole string, and the value is
clamped to `±MAX_COORD_FLOAT` (16384). Unusable input skips the teleport
instead of applying it.

## 6 Client usercmd value validation (anti-exploit)

**Files:** `dlls/gameinterface.cpp:169`, `dlls/gameinterface.cpp:2299`,
`game_shared/baseentity_shared.h:251`, `game_shared/baseentity_shared.cpp:55`

New ConVar:

```
sv_max_usercmd_move_magnitude 1000   "Maximum move magnitude that can be requested by client."
```

plus the shared helpers `IsEntityCoordinateReasonable()` and
`IsEntityQAngleReasonable()` (bounded by `MAX_COORD_FLOAT` and ±360000°, whose
only job is to trap NaN/INF).

**Original:** `CServerGameClients::ProcessUsercmds()` used every value decoded
from the client's `usercmd` stream verbatim — view angles and
forward/side/up moves went straight into simulation.

**Fixed:** after each `ReadUsercmd`:

```cpp
if ( !to->viewangles.IsValid() || !IsEntityQAngleReasonable( to->viewangles ) )
    to->viewangles.Init();

if ( !IsFinite( to->forwardmove ) || fabsf(to->forwardmove) > sv_max_usercmd_move_magnitude ...
     !IsFinite( to->sidemove )   || ... !IsFinite( to->upmove ) || ... )
    to->forwardmove = to->sidemove = to->upmove = 0.0f;
```

**Why it matters:** a crafted client could send NaN/INF or astronomically large
move/angle values. Those poison prediction and physics (NaN origin spreads to
every subsequent trace and can crash or wedge the server), and huge magnitudes
are a straight speedhack vector. Invalid input is now reset to neutral instead
of trusted.

## 7 Aim lock across replayed ticks (`sv_maxusrcmdprocessticks_holdaim`)

**Files:** `dlls/player.h:613`, `dlls/player.h:882`, `dlls/player.cpp:527`,
`dlls/cstrike/cs_player.cpp:3642`, `game_shared/cstrike/cs_player_shared.cpp:342`,
`game_shared/cstrike/cs_player_shared.cpp:380`, `game_shared/cstrike/cs_player_shared.cpp:599`

New state: `m_iLockViewanglesTickNumber` + `m_qangLockViewangles`, latched in
`CCSPlayer::NoteWeaponFired()` (when the weapon fires) and in the bullet-trace
code when the shot hits an enemy (not a teammate).

**Original:** whenever the server replayed or held simulation ticks (packet
loss, multi-action commands), the player's aim angles were re-read from the
current command each tick — aim could drift between the shot and its
replay, so hits registered against angles the player never saw.

**Fixed:** `CPlayerMove::RunCommand` restores the latched angles for
`sv_maxusrcmdprocessticks_holdaim` ticks after firing (see 2.5), so the server
simulates the shot against the exact view the client had when it fired.

## 8 Server-generated usercmd random seed

**Files:** `dlls/player.cpp:64`, `dlls/player.cpp:83`, `dlls/player.cpp:3329`

New ConVar:

```
sv_usercmd_custom_random_seed 1   "When enabled server will populate an additional random seed independent of the client"
```

**Original:** `ProcessUsercmds()` copied client commands with
`ctx->cmds[i] = cmds[i]` — the client's `random_seed` field was used verbatim
by the server.

**Fixed:** when the cvar is enabled (default), the server overwrites each
command's `random_seed` with a SHA-1 hash built from `Plat_FloatTime()` and a
server-side `RandomInt()`:

```cpp
custom_random_seed.time    = Plat_FloatTime() + 4294967296.0;
custom_random_seed.randval = RandomInt( 0, 0x7FFFFFFF );
GenerateHash( hash, &custom_random_seed, sizeof(...) );
pCmd->random_seed = *(int*)&hash[0];
```

**Why it matters:** the usercmd seed feeds server-side randomness; letting the
client choose it lets a modified client influence/predict server RNG outcomes.

## 9 Usercmd queue rework — packet loss, flood and idle protection

**Files:** `dlls/player.h:19`, `dlls/player.h:27`, `dlls/player.h:500`,
`dlls/player.h:644`, `dlls/player.h:665`, `dlls/player.h:1007`,
`dlls/player.cpp:566`, `dlls/player.cpp:2924`, `dlls/player.cpp:3040`,
`dlls/player.cpp:3069`, `dlls/player.cpp:3234`, `dlls/player.cpp:3355`,
`dlls/player_command.cpp:25`, `dlls/player_command.cpp:315`,
`dlls/gameinterface.cpp:2250`

New ConVars:

```
sv_maxusrcmdprocessticks 16          "Maximum number of client-issued usrcmd ticks that can be replayed in packet loss conditions, 0 to allow no restrictions"
sv_maxusrcmdprocessticks_warning -1  "Print a warning when user commands get dropped due to insufficient usrcmd ticks allocated, number of seconds to throttle, negative disabled"
sv_player_usercommand_timeout 3      "After this many seconds without a usercommand from a player, the server will RunNullCommand as if client sends an empty command."
```

**Original:**

```cpp
// player.h
#define CMD_MAXBACKUP 30
class CCommandContext {
    CUserCmd cmds[ CMD_MAXBACKUP ];   // fixed 30-slot array
```

`gameinterface.cpp` rejected backlogs at `totalcmds >= CMD_MAXBACKUP - 1`
(≥ 29). `CBasePlayer::PostThink`-path simulation replayed *every* queued
command unconditionally each tick, with no accounting of how many simulation
ticks the player actually had, and an idle client simply stopped feeding
simulation without any timeout handling. Pause-time zeroing of commands
happened at execution time, inside the replay loop.

**Fixed:**

* `CCommandContext::cmds` becomes a growable `CUtlVector<CUserCmd>` and
  `CMD_MAXBACKUP` rises 30 → 64 (`dlls/gameinterface.cpp:2251`), so deep
  packet-loss backlogs queue and validate instead of overflowing a fixed array.
* A per-player **movement-tick budget**
  (`m_nMovementTicksForUserCmdProcessingRemaining`, +1 per simulation tick in
  `player.cpp:3040`) is consumed by `ConsumeMovementTicksForUserCmdProcessing()`.
* `CPlayerMove::RunCommand` refuses to process a command when the budget is
  exhausted (`player_command.cpp:315`) — optionally printing the throttled
  `sv_maxusrcmdprocessticks_warning` — so a client cannot get unlimited
  simulation out of a burst of queued commands (speedhack/rubber-band abuse
  during packet loss).
* When more than `sv_maxusrcmdprocessticks` (16) ticks are pending, the backlog
  collapses to a single `m_LastCmd` tick and the budget clamps
  (`player.cpp:3234`) — deterministic catch-up instead of a replay storm.
* If no command has arrived for `sv_player_usercommand_timeout` seconds, the
  server runs a `RunNullCommand()` so the player keeps simulating predictably
  instead of acting on stale input (`player.cpp:3234`).
* Pause-time command zeroing moved from the replay loop into queue time
  (`player.cpp:3355`).

Supporting plumbing: `GetTimeSinceLastUserCommand()` (`player.h:500`),
budget accessors (`player.h:644`), members `m_flLastUserCommandTime`
(`player.h:1007`, init at `player.cpp:566`) and
`m_nMovementTicksForUserCmdProcessingRemaining` (`player.h:665`),
simulation-tick clamp (`player.cpp:2924`), and the reworked command-assembly
loop (`player.cpp:3069`, original kept in the `#else` branch).

## 10 `se_shootpositionfix` — shoot position correction (pre-move eye latch)

**Files:** `dlls/player_command.cpp` (`FinishCommand`, `FinishMove`, `RunCommand`),
`game_shared/baseplayer_shared.cpp` (convar, latch, `Weapon_ShootPosition`),
`dlls/player.h` / `dlls/player.cpp` (members and constructor init)

**Original:** stock v34 `CBasePlayer::Weapon_ShootPosition()` returns `EyePosition()`, i.e.
`GetAbsOrigin() + GetViewOffset()` after this command's movement has already been committed. The
client aimed the command on a frame rendered from the **pre-move** state, so the bullet starts one
tick of movement ahead of the crosshair — about 3.8 units sideways at 250 u/s and 66 tick, and it is
vertical too while ducking or standing up, where `Duck()` changes the view offset inside
`ProcessMovement()`.

**Fixed:** right after `SetupMove()` and before `ProcessMovement()`,
`CPlayerMove::RunCommand()` latches the eye position the command starts from:
`CMoveData::m_vecAbsOrigin + GetViewOffset()`. At that point the view offset is still the one the
rendered frame used, because only `Duck()` touches it later, inside `ProcessMovement()`.
`CBasePlayer::Weapon_ShootPosition()` returns the latch while the convar is on, the player is not in
a vehicle, and `GetAbsOrigin()` still equals the origin `FinishMove()` committed; the 0.1 unit
tolerance only absorbs float noise, so a teleport (including one from `ProcessImpacts()`) or
`m_bForceOrigin` falls back to the live `EyePosition()`. The latch is cleared in `FinishCommand()`,
so everything outside command processing — HUD, bots, console commands, NPCs — sees stock
`EyePosition()`.

The player's origin is never swapped, saved or restored, and `RunCommand()` keeps the stock v34 order
(`FinishMove -> aim-hold restore -> ProcessImpacts -> RunPostThink -> FinishCommand`) whether the fix
is on or off. Grenades and the C4 plant read `GetAbsOrigin()` directly and stay stock (post-move
origin with post-move velocity), and weapon drops keep their stock placement.

**Off by default.** The convar is `se_shootpositionfix` (`FCVAR_REPLICATED`, default `0`) and the fix
is enabled with `se_shootpositionfix 1` in `server.cfg` (the repository ships no `server.cfg`). The
default cannot be `1`: the engine only sends a replicated convar to a connecting client when its
value differs from the default, so a server default of `1` would leave ClientMod clients at their own
`0` and predicting hits without the fix. The convar name is the one the ClientMod client reads, so a
separate `sv_shootpositionfix` toggle must not be reintroduced, and ClientMod-Api's own
shoot-position fix must not run at the same time.

## 11 Physics follows the server tick interval (100-tick support)

**File:** `dlls/physics.cpp:195`

**Original:**

```cpp
physenv->SetSimulationTimestep( DEFAULT_TICK_INTERVAL ); // 15 ms per tick
```

**Fixed:**

```cpp
physenv->SetSimulationTimestep( TICK_INTERVAL );
```

**Why it matters:** 15 ms is hardwired to the stock 66.6 tickrate. On a 100-tick
server (10 ms) physics desynchronized from game simulation — objects jittered,
lagged behind entities and behaved inconsistently. The physics environment now
steps at the actual server tick interval.

## 12 CS: player physics box toggle

**Files:** `dlls/cstrike/cs_player.cpp:70`, `dlls/cstrike/cs_player.cpp:1001`,
`dlls/cstrike/cs_player.cpp:1012`

New ConVar: `cs_enable_player_physics_box 1`

**Original:** `CCSPlayer::InitVCollision()` always created the player's physics
controller box and installed its event handler.

**Fixed:** the same behavior by default, but setting the cvar to `0` destroys
the object (`VPhysicsDestroyObject()`) instead — letting operators disable
player physics boxes entirely (used to avoid unwanted player-vs-physics-object
interactions).

## 13 Chat flood fix (`say`)

**File:** `dlls/client.cpp:675`

**Original:**

```cpp
if (pPlayer) {
    if ( LastTimePlayerTalked() + TALK_INTERVAL < curtime ) { Host_Say(...); }
}
else
    Host_Say( NULL, 0 );     // any player who spoke within TALK_INTERVAL landed here
```

A player spamming `say` faster than `TALK_INTERVAL` (0.66 s) fell into the
`else` and had their message executed as a **console** message — the flood went
through anyway, misattributed to the server console.

**Fixed:**

```cpp
else if ( UTIL_GetCommandClientIndex() == 0 )   // only the real console
    Host_Say( NULL, 0 );
```

Player spam inside the interval is now dropped; only the dedicated server
console keeps the null-issuer path.

## 14 Server-console-only debug/report commands

28 commands gained this gate:

```cpp
  if ( !UTIL_IsCommandIssuedByServerAdmin() )
    return;
```

`UTIL_IsCommandIssuedByServerAdmin()` (`dlls/util.cpp:625`) allows the server
console (and the host of a listen server) and refuses any player-issued
invocation on a dedicated server.

**Original:** any connected client could execute these commands by sending them
as string commands — leaking server/entity/physics/AI state to every player and
offering a console-spam nuisance vector.

**Fixed:** they run only from the server console.

| File:line | Command |
|---|---|
| `dlls/ai_concommands.cpp:536` | `npc_thinknow` |
| `dlls/ai_concommands.cpp:743` | `npc_heal` |
| `dlls/ai_concommands.cpp:761` | `npc_ammo_deplete` |
| `dlls/ai_concommands.cpp:779` | `ai_clear_bad_links` |
| `dlls/ai_concommands.cpp:800` | `ainet_generate_report` |
| `dlls/ai_concommands.cpp:810` | `ainet_generate_report_only` |
| `dlls/ai_hint.cpp:1465` | `ai_dump_hints` |
| `dlls/ai_moveprobe.cpp:51` | `ai_set_move_height_epsilon` |
| `dlls/ai_speech.cpp:898` | `npc_speakall` |
| `dlls/client.cpp:1223` | `groundlist` |
| `dlls/entitylist.cpp:1580` | `report_entities` |
| `dlls/entitylist.cpp:1598` | `report_touchlinks` |
| `dlls/entitylist.cpp:1624` | `report_simthinklist` |
| `dlls/gameinterface.cpp:1256` | `ListServerUserMessages` |
| `dlls/ndebugoverlay.cpp:250` | `clear_debug_overlays` |
| `dlls/physics.cpp:989` | `physics_highlight_active` |
| `dlls/physics.cpp:999` | `physics_report_active` |
| `dlls/physics.cpp:1093` | `physics_debug_entity` |
| `dlls/physics.cpp:1103` | `physics_select` |
| `dlls/physics.cpp:1113` | `physics_budget` |
| `dlls/sceneentity.cpp:4212` | `scene_flush` |
| `dlls/sceneentity.cpp:4255` | `scene_mem` |
| `dlls/sceneentity.cpp:5130` | `listRecentNPCSpeech` (gate inside its handler) |
| `dlls/wcedit.cpp:690` | `wc_update_entity` |
| `dlls/wcedit.cpp:724` | `wc_update_safe_entities` |
| `game_shared/cstrike/cs_gamerules.cpp:2957` | `mp_dump_timers` |
| `game_shared/physics_shared.cpp:588` | `dump_terrain` |
| `game_shared/soundenvelope.cpp:311` | `report_soundpatch` (also compiled only when not `CLIENT_DLL`) |
