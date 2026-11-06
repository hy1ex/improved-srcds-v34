# Feature ports

This file documents every gameplay feature that has been ported into this server from another
Source game. For each feature it records:

* where the feature comes from,
* exactly what it does and which entity/game-rule code implements it,
* every convar it introduces, with its default and accepted values,
* why it is safe for a stock Counter-Strike: Source v34 client (server-side only).

## Rules these ports follow

1. **Server side only.** No new networked entity class, no new `SendProp` on a networked class, no
   new usermessage, no client command a v34 client would have to understand.
2. **No ABI change.** No vtable/interface change shared with the engine, no reordering of a
   datatable or sendtable.
3. **Stock behaviour by default.** Every convar added by a port defaults to the value that
   reproduces stock Counter-Strike: Source behaviour, unless a feature section explicitly states
   that the default was changed. Server operators opt in by changing a convar.
4. **New convars are not `FCVAR_REPLICATED`.** They stay server-side so nothing is pushed at
   clients that do not know them. `FCVAR_NOTIFY` is used only so operators see changes in the
   server console.

## Convar index

| Convar | Default | Feature | Notes |
| --- | --- | --- | --- |
| `mp_death_drop_gun` | `1` | [1. Death drop control](#1-death-drop-control) | 0=none, 1=best, 2=current or best |
| `mp_death_drop_c4` | `1` | [1. Death drop control](#1-death-drop-control) | 0/1 |
| `mp_death_drop_defuser` | `1` | [1. Death drop control](#1-death-drop-control) | 0/1 |
| `mp_death_drop_grenade` | `1` | [1. Death drop control](#1-death-drop-control) | 0=none, 1=best, 2=current or best, 3=all |
| `mp_defuser_allocation` | `0` | [2. Defuse kit allocation](#2-defuse-kit-allocation) | 0=none, 1=one random CT, 2=every CT |
| `mp_respawn_on_death_ct` | `0` | [3. Respawn on death](#3-respawn-on-death) | 0/1 |
| `mp_respawn_on_death_t` | `0` | [3. Respawn on death](#3-respawn-on-death) | 0/1 |
| `mp_respawnwavetime_ct` | `10` | [3. Respawn on death](#3-respawn-on-death) | seconds |
| `mp_respawnwavetime_t` | `10` | [3. Respawn on death](#3-respawn-on-death) | seconds |
| `mp_respawn_immunitytime` | `4` | [3. Respawn on death](#3-respawn-on-death) | seconds after respawn |
| `mp_respawn_weapons` | `0` | [3. Respawn on death](#3-respawn-on-death) | 0=default pistol, 1=random, 2=keep |
| `mp_respawn_buytime` | `4` | [3. Respawn on death](#3-respawn-on-death) | seconds after respawn, 0 = round end |
| `mp_buy_anywhere` | `0` | [3. Respawn on death](#3-respawn-on-death) | 0=stock, 1=both, 2=T, 3=CT |
| `mp_solid_teammates` | `1` | [5. Player, hostage and grenade solidity](#5-player-hostage-and-grenade-solidity) | 0 = walk through teammates |
| `mp_solid_enemies` | `1` | [5. Player, hostage and grenade solidity](#5-player-hostage-and-grenade-solidity) | 0 = walk through enemies |
| `mp_solid_hostages` | `1` | [5. Player, hostage and grenade solidity](#5-player-hostage-and-grenade-solidity) | 0 = walk through hostages |
| `mp_solid_grenades` | `1` | [5. Player, hostage and grenade solidity](#5-player-hostage-and-grenade-solidity) | 0 = walk through thrown grenades |
| `mp_players_nonsolid_time` | `0` | [5. Player, hostage and grenade solidity](#5-player-hostage-and-grenade-solidity) | seconds of spawn/respawn non-solidity |
| `mp_maxmoney` | `16000` | [6. Economy control](#6-economy-control) | money cap |
| `mp_afterroundmoney` | `0` | [6. Economy control](#6-economy-control) | flat cash per round |
| `mp_playercashawards` | `1` | [6. Economy control](#6-economy-control) | individual cash rewards |
| `mp_teamcashawards` | `1` | [6. Economy control](#6-economy-control) | team win/loss cash |

---

# 1. Death drop control

**Origin:** CS:GO, `game/shared/cstrike15/cs_gamerules.cpp` (convar definitions) and
`game/server/cstrike15/cs_player.cpp` (`CCSPlayer::DropWeapons`, around the "drop the best weapon"
and grenade blocks).

**Why:** stock Counter-Strike: Source hardcodes what a dying player leaves behind. Server operators
running deathmatch, arena, gungame or pistol-only servers have no way to tune it. CS:GO exposes the
whole drop policy as convars, and the drop code in this tree already performs every one of those
operations unconditionally, so the port is a set of gates on existing code instead of new logic.

**What it does.** When a player dies, `CCSPlayer::Event_Killed` calls `CCSPlayer::DropWeapons()`
(`dlls/cstrike/cs_player.cpp`). That function drops, in order:

1. the bomb (`weapon_c4`), if the player was carrying it,
2. the defuse kit (`item_defuser`), if the player owned one,
3. the shield, or the best primary/pistol,
4. any grenade that is currently pin-pulled or being thrown (so it can explode),
5. the best grenade left in the inventory.

The port makes steps 1, 2, 3 and 5 policy driven:

* `mp_death_drop_c4` — whether the bomb is dropped. **Default `1`** (stock behaviour). Setting it
  to `0` leaves the bomb on the corpse, which makes the round unwinnable for the terrorist team on
  bomb maps; that is the operator's decision, not a bug.
* `mp_death_drop_defuser` — whether a defuse kit is dropped. **Default `1`** (stock behaviour). The
  dying player always loses the kit itself; only the ground pickup is gated.
* `mp_death_drop_gun` — which gun is dropped:
  * `0` — nothing,
  * `1` — the best primary, falling back to the best pistol (**default**, stock behaviour),
  * `2` — the weapon the player was holding if it is a primary or a pistol, otherwise the best one.
* `mp_death_drop_grenade` — which grenade is dropped:
  * `0` — nothing,
  * `1` — the best grenade left in the inventory (**default**, stock behaviour),
  * `2` — the grenade the player was holding, otherwise the best one,
  * `3` — every grenade left in the inventory.

Step 4 (live grenades) is deliberately **not** gated by any convar, matching CS:GO: a pulled or
in-flight grenade always leaves the hand, otherwise it would detonate inside the dying player's
inventory and vanish.

**Convars**

| Convar | Default | Values | Meaning |
| --- | --- | --- | --- |
| `mp_death_drop_gun` | `1` | `0`-`2` | Which gun a dying player drops |
| `mp_death_drop_c4` | `1` | `0`/`1` | Drop the bomb on death |
| `mp_death_drop_defuser` | `1` | `0`/`1` | Drop the defuse kit on death |
| `mp_death_drop_grenade` | `1` | `0`-`3` | Which grenade(s) a dying player drops |

Defaults differ from CS:GO in one place on purpose: CS:GO ships `mp_death_drop_grenade 2`
("current or best"), while the stock Counter-Strike: Source behaviour this port preserves is
"best only", hence the default `1`.

**Client compatibility.** The drops use `CSWeaponDrop()` and `CBaseEntity::Create( "item_defuser" )`,
both of which already exist and already produce entities the v34 client renders. No convar added
here is replicated, so the client is never asked to know about them. No network protocol, sendtable
or ABI surface changed.

**Files:** `game_shared/cstrike/cs_gamerules.cpp` (convar definitions),
`dlls/cstrike/cs_player.cpp` (`DropWeapons`).

---

# 2. Defuse kit allocation

**Origin:** CS:GO, `mp_defuser_allocation` (`game/shared/cstrike15/cs_gamerules.cpp`, enum
`DefuserAllocation` in `cs_gamerules.h`), applied from `CCSPlayer::Spawn` and `item_defuser.cpp`.

**Why:** in stock Counter-Strike: Source a defuse kit only ever exists if a CT buys one. Public
servers that want every CT equipped, or that want the classic "one kit per round" role without a
buy menu, have no way to do it. Everything the feature needs already exists in this tree: the
`item_defuser` entity is precached (`dlls/cstrike/cs_player.cpp`), `CCSPlayer::GiveDefuser()` /
`RemoveDefuser()` / `HasDefuser()` are implemented, and `m_bHasDefuser` is already a networked
`SendPropBool`, so the stock HUD and scoreboard already draw the kit.

**What it does.**

* `0` — no free kits; a kit exists only if a CT buys one. **Default**, stock behaviour.
* `1` — one CT is chosen at random at each round restart (on bomb defuse maps only) and receives a
  kit. The choice is made by reservoir sampling over the CTs with a playable class, and the chosen
  entindex is stored in `CCSGameRules::m_iDefuserCarrier`. If that CT leaves, nobody else inherits
  the kit until the next round.
* `2` — every CT receives a kit when it spawns, on bomb defuse maps only.

The allocation happens in `CCSGameRules::PlayerSpawn()` (after `GiveDefaultItems()`), so it also
covers respawns from feature 3: with mode `2` a player who respawns is re-equipped, with mode `1`
only the round's designated carrier keeps getting one.

**Convars**

| Convar | Default | Values | Meaning |
| --- | --- | --- | --- |
| `mp_defuser_allocation` | `0` | `0`-`2` | How free defuse kits are handed to CTs |

**Client compatibility.** `m_bHasDefuser` is already networked and already rendered by the v34
client; the new convar is not replicated. `m_iDefuserCarrier` is a plain, non-networked member: it
is not added to the network table or to `BEGIN_DATADESC`, so no sendtable or datatable layout
changes. Server-side ABI is internal to this DLL only.

**Files:** `game_shared/cstrike/cs_gamerules.cpp` (convar, `AllocateDefuserForRound()`,
`PlayerSpawn`), `game_shared/cstrike/cs_gamerules.h` (`m_iDefuserCarrier`).

---

# 3. Respawn on death

**Origin:** CS:GO — `mp_respawn_on_death_ct` / `mp_respawn_on_death_t`,
`mp_respawnwavetime_ct` / `mp_respawnwavetime_t` and `mp_respawn_immunitytime`
(`game/shared/cstrike15/cs_gamerules.cpp`), plus `mp_buy_anywhere` /
`mp_buy_during_immunity` (`game/shared/gamevars_shared.cpp`) for the buy rules.

**Why:** stock Counter-Strike: Source only ever brings players back at round restart. Deathmatch,
arena, gungame and warmup-style servers therefore have to fake respawns with round restarts, which
resets scores, money and objectives. This port adds real per-player respawns with a per-team wave
delay, an optional loadout policy, optional spawn immunity and a post-respawn buy window.

**What it does.**

* When a player dies, `CCSPlayer::Event_Killed` calls `CCSPlayer::ScheduleRespawn()`. If
  `mp_respawn_on_death_<team>` is `1`, the player gets `mp_respawnwavetime_<team>` seconds of death
  cam and is then put back into the round by `CCSPlayer::DoRespawn()`, which reuses the same
  `StopObserverMode` / `State_Transition( STATE_ACTIVE )` / `respawn()` path a round restart uses.
* The timer is serviced from `CCSPlayer::PreThink()`, so it needs no think function, survives
  save/restore and stops firing as soon as the round goes to intermission.
* The respawned player's loadout is chosen by `mp_respawn_weapons`:
  * `0` — the team's default pistol and knife, exactly what a normal spawn gives. **Default.**
  * `1` — a random primary and a random pistol for the player's team (the lists only contain
    weapons this tree ships: ak47/galil/sg552/awp/scout/g3sg1/mp5navy/mac10/p90/ump45/m3/xm1014/m249
    for Terrorists, m4a1/famas/aug/awp/scout/sg550/mp5navy/tmp/p90/ump45/m3/xm1014/m249 for CTs;
    glock/deagle/p228/elite and usp/deagle/p228/fiveseven for pistols).
  * `2` — the weapons the player owned when they died. The list is captured in
    `RememberLoadoutForRespawn()` *before* the death drops run, ignoring the bomb (the game rules
    re-give it) and the knife. Ammunition is refilled by `GiveNamedItem`; armour and defuse kit are
    not preserved.
* `mp_respawn_immunitytime` grants damage immunity after a respawn. Damage is dropped at the top of
  `CCSPlayer::OnTakeDamage`, so nothing (bullets, grenades, falls) can hurt the player during the
  window. `0` disables it.
* `mp_respawn_buytime` re-opens the buy menu for a short window after a respawn even when the
  round's normal `mp_buytime` has already elapsed. `0` means "until the round ends". This window
  only ever applies to players actually respawned by this feature (`m_flRespawnTime` is cleared by
  a normal spawn), so stock behaviour is untouched.
* `mp_buy_anywhere` lets players buy outside `func_buyzone` volumes:
  * `0` — stock behaviour, buy zones only (**default**),
  * `1` — both teams may buy anywhere,
  * `2` — Terrorists only,
  * `3` — Counter-Terrorists only.
  It is implemented in `CCSPlayer::IsInBuyZone()`, so `buy`, `autobuy` and `rebuy` all follow it.

**Convars**

| Convar | Default | Values | Meaning |
| --- | --- | --- | --- |
| `mp_respawn_on_death_ct` | `0` | `0`/`1` | Respawn dead CTs |
| `mp_respawn_on_death_t` | `0` | `0`/`1` | Respawn dead Terrorists |
| `mp_respawnwavetime_ct` | `10` | seconds | CT respawn delay |
| `mp_respawnwavetime_t` | `10` | seconds | Terrorist respawn delay |
| `mp_respawn_immunitytime` | `4` | seconds, `0` = off | Damage immunity after a respawn (CS:GO default `4.0`) |
| `mp_respawn_weapons` | `0` | `0`-`2` | Loadout policy after a respawn |
| `mp_respawn_buytime` | `4` | seconds, `0` = round end | Buy window after a respawn (default matches `mp_respawn_immunitytime`) |
| `mp_buy_anywhere` | `0` | `0`-`3` | Buy outside buy zones, optionally per team |

The respawn feature itself is **off by default** (`mp_respawn_on_death_ct/t 0`), so a stock server
never respawns anyone: immunity, the buy window and the loadout policy are all inert. This is the
one feature in this file whose convars do not individually default to "off", because the values
only matter once respawning is enabled.

**Client compatibility.** Respawning uses the stock spawn path (`respawn()` →
`CCSPlayer::Spawn()` → `CCSGameRules::PlayerSpawn()`), so the v34 client sees an ordinary spawn:
death cam ends, HUD health/armour/money reset, ordinary weapon pickup events. No new netprop,
usermessage, entity class or client command. `m_flNextRespawnTime`, `m_flRespawnTime`,
`m_flSpawnImmuneUntil`, `m_szRespawnWeapons` and `m_nRespawnWeaponCount` are plain server-side
members on `CCSPlayer`: not in `BEGIN_DATADESC`, not in the sendtable. The convars are not
replicated.

**Risks / limitations.**

* Bots respawn through the same path. Their AI state is reset the way a round restart resets it,
  but a bot that respawns mid-round is not explicitly told to re-path; that is the main thing to
  watch on a bot server.
* Objective maps keep their normal rules: if respawns are enabled on a bomb map, a dead bomb
  carrier still drops the bomb (subject to feature 1) and the round can still be won or lost on
  objectives while players respawn.
* Round win conditions are unchanged: respawning does not disable `mp_roundtime`, `mp_maxrounds`,
  `mp_winlimit` or the objective checks. Servers that want pure deathmatch should set a long round
  time and `mp_ignore_round_win_conditions`-style rules themselves.

**Files:** `game_shared/cstrike/cs_gamerules.cpp` (convars), `dlls/cstrike/cs_player.cpp`
(`ScheduleRespawn`, `DoRespawn`, loadout helpers, `PreThink`, `OnTakeDamage`, `IsInBuyZone`,
`CanPlayerBuy`, `Event_Killed`, `Spawn`), `dlls/cstrike/cs_player.h` (members and declarations).

---

# 5. Player, hostage and grenade solidity

**Origin:** CS:GO `mp_solid_teammates` (`game/shared/cstrike15/cs_gamerules.cpp`) and its
collision plumbing: `CCSPlayer::ShouldCollide`
(`game/server/cstrike15/cs_player.cpp`), the team tags returned by
`CCSPlayer::PhysicsSolidMaskForEntity` (`game/shared/cstrike15/cs_player_shared.cpp`) and the
movement trace filter (`game/shared/gamemovement.cpp`,
`CTraceFilterSkipTwoEntitiesAndCheckTeamMask`). The enemy, hostage and grenade switches are the
same idea applied to the other blockers, and `mp_players_nonsolid_time` is new.

**Why:** stock Counter-Strike: Source makes every player, hostage and thrown grenade solid to
everyone. That is wrong for deathmatch modes (spawn camping, blocked spawn exits), for
teammate-friendly modes (stair-stepping on teammates), and for servers that want players to walk
through hostages or grenades without a physics fight.

**What it does.** A player's movement traces carry a team tag in the trace mask
(`CONTENTS_TEAM1` for CTs, `CONTENTS_TEAM2` for Terrorists), which `CCSGameMovement::PlayerSolidMask`
adds from `CCSPlayer::GetCollisionTeamMask()`. When a player's movement trace reaches another
player, the engine calls that player's `CCSPlayer::ShouldCollide( COLLISION_GROUP_PLAYER_MOVEMENT,
contentsMask )`, which compares the two tags:

* same tag and `mp_solid_teammates 0` → no collision (walk through teammates),
* different tags and `mp_solid_enemies 0` → no collision (walk through enemies),
* everything else → the stock collision rules.

`mp_players_nonsolid_time` uses the same tag: for N seconds after a spawn or respawn a player
reports **both** tags, and both sides of the comparison treat "both tags" as "no player collision
at all". That makes the window symmetric - neither the freshly spawned player nor the player
walking into them is blocked - and it also covers players respawned by feature 3, because the
timer is started in `CCSPlayer::Spawn()`.

The other two blockers are handled where the entity itself is asked about the collision:

* `mp_solid_hostages 0` — `CHostage::ShouldCollide` refuses `COLLISION_GROUP_PLAYER_MOVEMENT`, so
  players walk through hostages. (Hostages are `COLLISION_GROUP_PLAYER` + `SOLID_BBOX`, which is
  why the group-only game-rules hook cannot tell them apart from players.)
* `mp_solid_grenades 0` — `CBaseCSGrenadeProjectile::ShouldCollide` refuses
  `COLLISION_GROUP_PLAYER_MOVEMENT`, so players walk through thrown grenades. The projectile still
  collides with the world and props, and still bounces off players through its own physics - only
  the "this grenade blocks my walking" test is switched off.

**Convars**

| Convar | Default | Values | Meaning |
| --- | --- | --- | --- |
| `mp_solid_teammates` | `1` | `0`/`1` | Teammates block movement (stock: yes) |
| `mp_solid_enemies` | `1` | `0`/`1` | Enemies block movement (stock: yes) |
| `mp_solid_hostages` | `1` | `0`/`1` | Hostages block movement (stock: yes) |
| `mp_solid_grenades` | `1` | `0`/`1` | Thrown grenades block movement (stock: yes) |
| `mp_players_nonsolid_time` | `0` | seconds | Spawn/respawn non-solid window (`0` = off, stock) |

**Client compatibility.** Collision decisions are server-side. Player-vs-player movement collision
is not predicted by the client, so switching it off cannot desynchronise movement. `CONTENTS_TEAM1`
and `CONTENTS_TEAM2` are game-specific bits already defined in `public/bspflags.h` and used by no
other code in this tree, so the extra mask bits cannot change what a trace hits. No netprop, no
sendtable change, no new entity. `m_flLastSpawnTime` is a plain server-side member.

**Limitations.** These switches cover the *movement* collision test (the one that stops you walking
through somebody). They do not disable the vphysics push that separates two players whose bodies
already overlap, and they never affect hitboxes or damage. A player standing exactly inside another
player can therefore still be nudged apart by physics, which is also how CS:GO behaves.

**Files:** `game_shared/cstrike/cs_gamerules.cpp` (convars),
`game_shared/cstrike/cs_gamemovement.cpp` (`CCSGameMovement::PlayerSolidMask`),
`dlls/cstrike/cs_player.cpp` (`GetCollisionTeamMask`, `ShouldCollide`, `Spawn`),
`dlls/cstrike/cs_player.h`, `dlls/cstrike/hostage/cs_simple_hostage.cpp/.h`,
`game_shared/cstrike/basecsgrenade_projectile.cpp/.h`.

---

# 6. Economy control

**Origin:** CS:GO `mp_maxmoney`, `mp_afterroundmoney`, `mp_playercashawards` and
`mp_teamcashawards` (`game/shared/cstrike15/cs_gamerules.cpp`).

**Why:** stock Counter-Strike: Source hardcodes the money cap (`16000`) and always pays both
individual kill awards and team win/loss cash. Servers running pistol-only, no-economy or
"everyone earns the same" modes have no switch for either. The four CS:GO convars are the minimal
set that covers those cases. CS:GO's escalation ladder (`mp_consecutive_loss_max` and friends) is
deliberately **not** ported: it is a balance redesign of the whole economy, not a switch, and it
would change stock behaviour for anyone who leaves it alone.

**What it does.**

* `mp_maxmoney` — the cap applied by `CCSPlayer::AddAccount()` after every credit or debit. Stock
  CS:S clamps at `16000`, which is the default here, so nothing changes until an operator lowers or
  raises it. `mp_startmoney` still bounds its own value at `16000`.
* `mp_afterroundmoney` — flat cash handed to every player on both teams together with the round's
  normal cash, regardless of the round result. `0` (default) means "pay nothing extra", exactly
  stock behaviour.
* `mp_playercashawards` — gates the individual cash rewards in `CCSGameRules::PlayerKilled`:
  the `+300` kill award and the `+2500` VIP kill award. `1` (default) keeps stock behaviour. Team
  kill penalties (`-3300`) and hostage penalties are *not* gated: they are punishments, not awards.
* `mp_teamcashawards` — gates the team win/loss cash (`m_iAccountCT` / `m_iAccountTerrorist`)
  handed out at round restart, including the losing-bonus ladder. `1` (default) keeps stock
  behaviour.

**Convars**

| Convar | Default | Values | Meaning |
| --- | --- | --- | --- |
| `mp_maxmoney` | `16000` | `0`+ | Maximum money a player can carry |
| `mp_afterroundmoney` | `0` | `0`+ | Flat cash per player per round |
| `mp_playercashawards` | `1` | `0`/`1` | Individual cash rewards (kill/VIP awards) |
| `mp_teamcashawards` | `1` | `0`/`1` | Team win/loss cash at round start |

**Client compatibility.** Money is already a networked player field rendered by the v34 buy menu and
HUD, so any value these convars produce is displayed correctly by a stock client. No convars are
replicated, no netprops, no protocol change.

**Files:** `game_shared/cstrike/cs_gamerules.cpp` (convars, kill awards, round-start cash),
`dlls/cstrike/cs_player.cpp` (`AddAccount` cap).
