# Fixes

This file documents every fix this fork adds over the stock Counter-Strike: Source v34
dedicated server source: the security fixes ported from the
[ProtectCmds](https://github.com/PowerTakeDev/ProtectCmds) extension, and all
behavioral changes guarded by the `BUGFIXED` compile switch.

## How the `BUGFIXED` switch works

`linux_sdk/Makefile` defines `DEFINES=-DBUGFIXED` by default, so a normal build
contains all fixes. Every changed block in the code is wrapped like this:

```cpp
#ifdef BUGFIXED
    // fixed behavior
#else
    // original stock behavior (only present when the change replaces code)
#endif
```

* **With `BUGFIXED` defined** (default): the fixed code is compiled in.
* **Without the flag**: the preprocessor emits the original stock code — the
  binary behaves exactly like the upstream v34 server.

Line numbers below refer to the current tree.

---

# Part 1 — Security fixes (ported from ProtectCmds)

ProtectCmds is a SourceMod extension that patches the compiled server/engine
binaries with runtime detours. Three of its ten protections target functions
that live in *this* repository's code (`server_i486.so`), so they are
implemented directly in the source — no detour library, no gamedata signatures,
no hardcoded offsets. Each one is `BUGFIXED`-guarded and was committed
separately (`3c3aa5e`, `caec919`, `eba67ff`).

## 1.1 `CGameUI::Deactivate` — stale player handle crash

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

Behavior matches ProtectCmds' detour exactly: an invalid handle skips the whole
function body.

## 1.2 `Studio_BoneIndexByName` — null studio header crash

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

## 1.3 `CBaseEntity::TakeDamage` — null gamerules crash

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

**Fixed:** the guard sits at the top of the function so the semantics match
ProtectCmds' wrapping detour — no part of damage processing runs without
gamerules. On a healthy server the damage would have been decided by
`AllowDamage` anyway, so skipping is behaviorally neutral.

## 1.4 Protections that remain engine-side (not portable to this source)

The other seven ProtectCmds fixes patch the closed-source `engine.so` and
cannot be implemented here; keep running the extension for them:

| ProtectCmds fix | What it does |
|---|---|
| `FilterPacket` | Hooks `recvfrom` to neutralize malformed UDP packets before they reach the engine net layer |
| `NetChanFix` | Hardens `CNetChan::ProcessMessages`; validates `net_File` paths (blocks `downloads/../` traversal exfiltration), disconnects cleanly on unknown net messages |
| `VoiceFix` | Rate-limits `CLC_VoiceData` (size cap, messages/sec cap, flood disconnect) |
| `BlockCmds` | Caps string-command length and rate; blocks client-issued `sm `/`meta ` probes |
| `SourceTVFix` | Forces `m_bIsHLTV = false` so fake HLTV clients can't displace the real SourceTV slot |
| `HashSteamID` | Stores a CRC32-hashed SteamID in the network-visible client slot |
| `PreConnectHook` | SourceMod `OnClientPreConnect` forward for early connection rejection |

---

# Part 2 — All `BUGFIXED`-conditional changes

83 conditional blocks in this section (plus the 3 security fixes above = 86 in
total across 25 files). Each entry shows the original stock code and the fixed
code.

## 2.1 Client usercmd value validation (anti-exploit)

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

## 2.2 Aim lock across replayed ticks (`sv_maxusrcmdprocessticks_holdaim`)

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

## 2.3 Server-generated usercmd random seed

**Files:** `dlls/player.cpp:64`, `dlls/player.cpp:83`, `dlls/player.cpp:3318`

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

## 2.4 Usercmd queue rework — packet loss, flood and idle protection

**Files:** `dlls/player.h:19`, `dlls/player.h:27`, `dlls/player.h:500`,
`dlls/player.h:644`, `dlls/player.h:665`, `dlls/player.h:1007`,
`dlls/player.cpp:566`, `dlls/player.cpp:2913`, `dlls/player.cpp:3029`,
`dlls/player.cpp:3058`, `dlls/player.cpp:3223`, `dlls/player.cpp:3344`,
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
#ifndef BUGFIXED
#define CMD_MAXBACKUP 30
#endif
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
  `player.cpp:3029`) is consumed by `ConsumeMovementTicksForUserCmdProcessing()`.
* `CPlayerMove::RunCommand` refuses to process a command when the budget is
  exhausted (`player_command.cpp:315`) — optionally printing the throttled
  `sv_maxusrcmdprocessticks_warning` — so a client cannot get unlimited
  simulation out of a burst of queued commands (speedhack/rubber-band abuse
  during packet loss).
* When more than `sv_maxusrcmdprocessticks` (16) ticks are pending, the backlog
  collapses to a single `m_LastCmd` tick and the budget clamps
  (`player.cpp:3223`) — deterministic catch-up instead of a replay storm.
* If no command has arrived for `sv_player_usercommand_timeout` seconds, the
  server runs a `RunNullCommand()` so the player keeps simulating predictably
  instead of acting on stale input (`player.cpp:3223`).
* Pause-time command zeroing moved from the replay loop into queue time
  (`player.cpp:3344`).

Supporting plumbing: `GetTimeSinceLastUserCommand()` (`player.h:500`),
budget accessors (`player.h:644`), members `m_flLastUserCommandTime`
(`player.h:1007`, init at `player.cpp:566`) and
`m_nMovementTicksForUserCmdProcessingRemaining` (`player.h:665`),
simulation-tick clamp (`player.cpp:2913`), and the reworked command-assembly
loop (`player.cpp:3058`, original kept in the `#else` branch).

## 2.5 `sv_shootpositionfix` — shoot position correction (Valve PR #442)

**Files:** `dlls/player_command.cpp:408`, `dlls/player_command.cpp:440`,
`dlls/player_command.cpp:450`, `dlls/player_command.cpp:465` (cvar defined in
the block at `dlls/player_command.cpp:25`)

**Original:** inside `CPlayerMove::RunCommand`, `moveHelper->ProcessImpacts()`
and `RunPostThink()` ran *after* movement processing, right before
`FinishCommand()`.

**Fixed (default on):** both are moved *before* `RunPreThink`/movement, so
collision impacts resolve before the player's shoot position (eye position) is
captured for the shot; with `sv_shootpositionfix 0` the original order is kept.
This is the backport of
[ValveSoftware/source-sdk-2013#442](https://github.com/ValveSoftware/source-sdk-2013/pull/442):
shots no longer originate from a position that misses the tick's collision
resolution (the classic "shot from the wrong spot" desync at high tickrates).
The same block also restores the locked aim angles from 2.2 and runs
`RunPostThink` under the fixed ordering.

## 2.6 Physics follows the server tick interval (100-tick support)

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

## 2.7 CS: round-flow control cvars

**Files:** `game_shared/cstrike/cs_gamerules.cpp` — cvar definitions at `:420`,
win-condition gates at `:1436` and `:2809`, and 12 `TerminateRound` sites
(`:1622`, `:1738`, `:1765`, `:1791`, `:1812`, `:1860`, `:1884`, `:1894`,
`:2832`, `:2846`, `:2857`, `:2869`)

New ConVars:

```
mp_round_restart_delay 5.0        "Number of seconds to delay before restarting a round after a win"
mp_ignore_round_win_conditions 0  "Ignore conditions which would end the current round"
```

**Original:** every win path called `TerminateRound( 5, ... )` with a hardcoded
5-second delay, and `CheckWinConditions()` / `CheckRoundTimeExpired()` always
ran — there was no way to hold a round open or change the inter-round delay.

**Fixed:** all 12 sites use `TerminateRound( mp_round_restart_delay.GetFloat(), ... )`,
and both checkers return early when `mp_ignore_round_win_conditions` is set —
enabling custom rules (retakes, minigames, deathrun-style maps, screwdraw
rounds) that must not end on standard win conditions.

## 2.8 CS: bunnyhop speed cap

**Files:** `game_shared/cstrike/cs_gamemovement.cpp:27`,
`game_shared/cstrike/cs_gamemovement.cpp:647`

New ConVar: `sv_enablebunnyhopping 0` ("Allow player speed to exceed maximum
running speed")

**Original:** `CheckJumpButton()` applied the jump with no cap against the
player's maximum speed — chained jumps accumulated speed without bound
(bunnyhop speedhack).

**Fixed:** unless the cvar is enabled, velocity is clamped to
`MaxSpeed() * 1.1` on every jump:

```cpp
float maxspd = player->MaxSpeed() * 1.1f;
if ( spd > maxspd ) { float ratio = maxspd / spd; mv->m_vecVelocity *= ratio; }
```

## 2.9 CS: player physics box toggle

**Files:** `dlls/cstrike/cs_player.cpp:70`, `dlls/cstrike/cs_player.cpp:1001`,
`dlls/cstrike/cs_player.cpp:1012`

New ConVar: `cs_enable_player_physics_box 1`

**Original:** `CCSPlayer::InitVCollision()` always created the player's physics
controller box and installed its event handler.

**Fixed:** the same behavior by default, but setting the cvar to `0` destroys
the object (`VPhysicsDestroyObject()`) instead — letting operators disable
player physics boxes entirely (used to avoid unwanted player-vs-physics-object
interactions).

## 2.10 Chat flood fix (`say`)

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

## 2.11 Server-console-only debug/report commands

28 commands gained this gate:

```cpp
#ifdef BUGFIXED
    if ( !UTIL_IsCommandIssuedByServerAdmin() )
        return;
#endif
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

---

## Inventory check

| Section | Blocks |
|---|---|
| 1. Security fixes | 3 |
| 2.1 Usercmd validation | 4 |
| 2.2 Aim lock plumbing | 7 |
| 2.3 Custom random seed | 3 |
| 2.4 Queue rework | 15 |
| 2.5 Shoot position fix | 4 |
| 2.6 Physics timestep | 1 |
| 2.7 Round-flow cvars | 15 |
| 2.8 Bunnyhop cap | 2 |
| 2.9 Physics box toggle | 3 |
| 2.10 Chat flood | 1 |
| 2.11 Console-only commands | 28 |
| **Total** | **86** |
