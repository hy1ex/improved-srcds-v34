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
5. **Feature numbers are sorted by approval.** A feature that is skipped or deferred moves to the
   last number and every feature after it moves up by one, so the ported features stay contiguous
   and the pending ones sit at the end.

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
| `mp_respawn_immunitytime` | `4` | [3. Respawn on death](#3-respawn-on-death) | seconds of damage immunity + model fade |
| `mp_respawn_immunity_fade` | `1` | [3. Respawn on death](#3-respawn-on-death) | 0=none, 1=fade in, 2=half transparent |
| `mp_respawn_weapons` | `0` | [3. Respawn on death](#3-respawn-on-death) | 0=default pistol, 1=random, 2=keep |
| `mp_respawn_buytime` | `-1` | [3. Respawn on death](#3-respawn-on-death) | -1 = stock round window, 0 = no cutoff, else hard cutoff after respawn |
| `mp_buy_anywhere` | `0` | [3. Respawn on death](#3-respawn-on-death) | 0=stock, 1=both, 2=T, 3=CT (also opens the menu) |
| `mp_solid_player` | `1` | [4. Player, hostage and grenade solidity](#4-player-hostage-and-grenade-solidity) | 0 = walk through every player, no push |
| `mp_solid_hostages` | `1` | [4. Player, hostage and grenade solidity](#4-player-hostage-and-grenade-solidity) | 0 = walk through hostages |
| `mp_solid_grenades` | `1` | [4. Player, hostage and grenade solidity](#4-player-hostage-and-grenade-solidity) | 0 = walk through thrown grenades |
| `mp_players_nonsolid_time` | `0` | [4. Player, hostage and grenade solidity](#4-player-hostage-and-grenade-solidity) | seconds of spawn/respawn non-solidity |
| `mp_teammates_are_enemies` | `0` | [4. Player, hostage and grenade solidity](#4-player-hostage-and-grenade-solidity) | 0/1, teammates are valid targets |
| `mp_maxmoney` | `16000` | [5. Economy control](#5-economy-control) | money cap |
| `mp_afterroundmoney` | `0` | [5. Economy control](#5-economy-control) | flat cash per round |
| `mp_playercashawards` | `1` | [5. Economy control](#5-economy-control) | individual cash rewards |
| `mp_teamcashawards` | `1` | [5. Economy control](#5-economy-control) | team win/loss cash |
| `mp_damage_scale_ct_head` | `1` | [6. Damage scaling](#6-damage-scaling) | multiplier for Counter-Terrorist headshots |
| `mp_damage_scale_ct_body` | `1` | [6. Damage scaling](#6-damage-scaling) | multiplier for the rest of a Counter-Terrorist |
| `mp_damage_scale_t_head` | `1` | [6. Damage scaling](#6-damage-scaling) | multiplier for Terrorist headshots |
| `mp_damage_scale_t_body` | `1` | [6. Damage scaling](#6-damage-scaling) | multiplier for the rest of a Terrorist |
| `mp_damage_headshot_only` | `0` | [6. Damage scaling](#6-damage-scaling) | 0/1, only headshot bullets deal damage |
| `mp_free_armor` | `0` | [7. Free armour](#7-free-armour) | 0=none, 1=kevlar, 2=kevlar and helmet |
| `mp_buy_allow_guns` | `255` | [8. Purchase restrictions](#8-purchase-restrictions) | bitfield: 1 pistols, 2 SMGs, 4 rifles, 8 shotguns, 16 snipers, 32 machine guns |
| `mp_buy_allow_grenades` | `1` | [8. Purchase restrictions](#8-purchase-restrictions) | 0/1 |
| `mp_max_armor` | `2` | [8. Purchase restrictions](#8-purchase-restrictions) | 0=none, 1=kevlar, 2=kevlar and helmet |
| `mp_halftime` | `0` | [9. Halftime](#9-halftime) | 0 = never swap, 1 = swap at half of mp_maxrounds |

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
`mp_respawnwavetime_ct` / `mp_respawnwavetime_t`, `mp_respawn_immunitytime` and
`mp_respawn_weapons` (`game/shared/cstrike15/cs_gamerules.cpp`), plus `mp_buy_anywhere`
(`game/shared/gamevars_shared.cpp`) for the buy rules.

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
    re-give it) and the knife. `DoRespawn()` has to copy that list into local storage around
    `respawn()`, because a spawn clears the remembered loadout: without the copy the restore loop
    had nothing left to give and the player respawned with only the knife. Ammunition is refilled
    by `GiveNamedItem`; armour, helmet and defuse kit are not preserved.
* `mp_respawn_immunitytime` grants damage immunity after a respawn. Damage is dropped at the top of
  `CCSPlayer::OnTakeDamage`, so nothing (bullets, grenades, falls) can hurt the player during the
  window. `0` disables it.
  The same window can show on the player's **world model**, selected by
  `mp_respawn_immunity_fade`: `0` draws nothing at all (immunity is then invisible), `1` (default)
  fades the model in, and `2` holds it at half transparency for the whole window. The fade mode is
  `CCSPlayer::UpdateSpawnImmunityRender()`, which runs from `PreThink()` and drives
  `SetRenderMode( kRenderTransTexture )` + `SetRenderColorA()`:
  * `1` — almost transparent right after the respawn, getting more opaque every frame, fully opaque
    exactly when the immunity expires. This is how CS:GO shows spawn protection. Alpha never drops
    below 16, so the player is never completely invisible.
  * `2` — alpha pinned at 128 (50%) for the entire immunity window with no animation, then a
    straight snap back to 255 when the window ends. No transition frame is drawn by the server: the
    model simply becomes opaque.
  * `0` — no render changes at all, so a protected player looks like everybody else.

  Every mode is cleared again by `ResetSpawnImmunityRender()` when the immunity ends, on death, on
  an ordinary spawn and when the mode is switched to `0` mid-window, so a player can never be left
  translucent. `m_nRenderMode` and `m_clrRender` are stock networked entity fields, so this needs no
  new netprop and no client support.
* `mp_respawn_buytime` gives a respawned player his own buy window:
  * `-1` - no separate window, the respawned player follows the round's `mp_buytime` exactly like
    stock (**default**, so the feature is off unless a server asks for it),
  * `0` - no cutoff at all, buying stays available for the rest of the round,
  * `N > 0` - `N` seconds after the respawn.
  When it is set, the window replaces the round's in both directions: inside it the player may buy
  even when the round's `mp_buytime` has already elapsed, and once it runs out he may not buy even
  while the round's `mp_buytime` is still running. It only applies to players actually respawned by
  this feature (`m_flRespawnTime` is cleared by a normal spawn).
  The server refuses every buy command in that state - `buy`, `autobuy`, `rebuy` and the ammo and
  equipment aliases alike - with "Your respawn buy time has expired" rather than the stock
  `#Cant_buy` line, which counts whole minutes. The VGUI buy menu closes with it: the networked
  `m_bInBuyZone` bit is only set while the window is open (`CBuyZone::BuyZoneTouch`, and the
  `mp_buy_anywhere` pin in `CCSPlayerMove::StartCommand`), so a stock client, which reads that bit
  before it opens the menu, refuses to open it. The reverse direction is not possible from the
  server: a respawned player inside his window while `mp_buytime` has already elapsed can buy with
  the commands but still cannot open the menu, because the client's own `IsBuyTimeElapsed()` check
  knows nothing about the respawn window.
* `mp_buy_anywhere` lets players buy outside `func_buyzone` volumes:
  * `0` — stock behaviour, buy zones only (**default**),
  * `1` — both teams may buy anywhere,
  * `2` — Terrorists only,
  * `3` — Counter-Terrorists only.
  It is implemented in `CCSPlayer::IsInBuyZone()`, so the `buy`, `autobuy` and `rebuy` console
  commands follow it. The client's VGUI buy menu is a client-side panel that cannot see a
  server-only convar, so `CCSPlayerMove::StartCommand` additionally pins the **networked**
  `m_bInBuyZone` bit for players who are allowed to buy: that is the same 1-bit sendprop
  `func_buy_zone` sets on touch, and it is what a stock v34 client checks before it opens the
  menu. The bit is cleared every command by the stock code and re-set here, so a player who loses
  the right loses the menu again immediately. Both writers of the bit test
  `CCSPlayer::IsBuyTimeAvailable()`, so the bit means "this player may buy right now" rather than
  "this player is standing in a zone", and the buy window closes the menu as well as the commands.
* Buying for the whole round is a `mp_buytime` setting, not a separate switch: `mp_buytime` is
  `FCVAR_REPLICATED`, so the server and the stock client agree on the window, and raising it (for
  example `mp_buytime 999`) keeps the VGUI buy menu open as well as the commands. A server-only
  "ignore the buy time" convar cannot do that, because the client decides on its own whether to
  open the menu at all; that is why there is no such convar here.

**Convars**

| Convar | Default | Values | Meaning |
| --- | --- | --- | --- |
| `mp_respawn_on_death_ct` | `0` | `0`/`1` | Respawn dead CTs |
| `mp_respawn_on_death_t` | `0` | `0`/`1` | Respawn dead Terrorists |
| `mp_respawnwavetime_ct` | `10` | seconds | CT respawn delay |
| `mp_respawnwavetime_t` | `10` | seconds | Terrorist respawn delay |
| `mp_respawn_immunitytime` | `4` | seconds, `0` = off | Damage immunity and model fade after a respawn (CS:GO default `4.0`) |
| `mp_respawn_immunity_fade` | `1` | `0`-`2` | Immunity visual: `0` none, `1` fade in, `2` fixed half transparency |
| `mp_respawn_weapons` | `0` | `0`-`2` | Loadout policy after a respawn |
| `mp_respawn_buytime` | `-1` | `-1` = stock, `0` = no cutoff, else seconds | Buy window after a respawn; when set it replaces the round's `mp_buytime` for that player |
| `mp_buy_anywhere` | `0` | `0`-`3` | Buy outside buy zones, optionally per team |

The respawn feature itself is **off by default** (`mp_respawn_on_death_ct/t 0`), so a stock server
never respawns anyone: immunity, the fade, the buy window and the loadout policy are all inert.
This is the one feature in this file whose convars do not individually default to "off", because
the values only matter once respawning is enabled.

**Client compatibility.** Respawning uses the stock spawn path (`respawn()` →
`CCSPlayer::Spawn()` → `CCSGameRules::PlayerSpawn()`), so the v34 client sees an ordinary spawn:
death cam ends, HUD health/armour/money reset, ordinary weapon pickup events. The immunity fade
only writes `m_nRenderMode`/`m_clrRender`, which the client already receives for every entity and
already renders (it is the same pair SourceMod render natives use for translucent players). The buy
menu uses the existing `m_bInBuyZone` sendprop. No new netprop, usermessage, entity class or client
command. `m_flNextRespawnTime`, `m_flRespawnTime`, `m_flSpawnImmuneUntil`, `m_szRespawnWeapons`,
`m_nRespawnWeaponCount` and `m_nImmunityRenderAlpha` are plain server-side members on `CCSPlayer`:
not in `BEGIN_DATADESC`, not in the sendtable. The convars are not replicated.

**Risks / limitations.**

* Bots respawn through the same path. Their AI state is reset the way a round restart resets it,
  but a bot that respawns mid-round is not explicitly told to re-path; that is the main thing to
  watch on a bot server.
* Objective maps keep their normal rules: if respawns are enabled on a bomb map, a dead bomb
  carrier still drops the bomb (subject to feature 1) and the round can still be won or lost on
  objectives while players respawn.
* Round win conditions are unchanged: respawning does not disable `mp_roundtime`, `mp_maxrounds`,
  `mp_winlimit` or the objective checks. Servers that want pure deathmatch should set a long round
  time and `ignore_round_win_conditions`-style rules themselves.
* `mp_buy_anywhere` lifts the buy *zone* only. The buy *window* still applies, and the round's
  other blockers (dead, VIP, `m_bCTCantBuy`/`m_bTCantBuy`) are unaffected.
* The buy-menu bit is the only buy-zone signal a stock client exposes. If a particular client build
  gates the menu on something else, `mp_buy_anywhere` keeps working for the console commands but the
  VGUI menu cannot be opened without a client DLL change, which the stock-client rule forbids.

**Files:** `game_shared/cstrike/cs_gamerules.cpp` (convars), `dlls/cstrike/cs_player.cpp`
(`ScheduleRespawn`, `DoRespawn`, `RememberLoadoutForRespawn`, `RestoreLoadoutForRespawn`,
`UpdateSpawnImmunityRender`, `ResetSpawnImmunityRender`, `PreThink`, `OnTakeDamage`, `IsInBuyZone`,
`CanPlayerBuy`, `Event_Killed`, `Spawn`), `dlls/cstrike/cs_playermove.cpp` (`StartCommand`),
`dlls/cstrike/cs_player.h` (members and declarations).

---

# 4. Player, hostage and grenade solidity

**Origin:** CS:GO `mp_solid_teammates` and `mp_teammates_are_enemies`
(`game/shared/cstrike15/cs_gamerules.cpp`) together with its collision plumbing, and the shared
collision-group rules in `game/shared/gamerules.cpp` (`CGameRules::ShouldCollide`). CS:GO's switch
is per relationship; this port has one global switch instead, because the engine and a stock client
cannot express a per-relationship one (see below). The enemy, hostage and grenade switches are the
same idea applied to the other blockers; `mp_players_nonsolid_time` is new.

**Why:** stock Counter-Strike: Source makes every player, hostage and thrown grenade solid to
everyone. That is wrong for deathmatch modes (spawn camping, blocked spawn exits, no ghosting
through a crowd), for teammate-friendly modes (stair-stepping on teammates), and for servers that
want players to walk through hostages or grenades without a physics fight.

**How solidity actually works in this engine.** Two independent filters decide whether you can walk
into something:

1. **The movement trace.** Player movement traces carry `COLLISION_GROUP_PLAYER_MOVEMENT`, and both
   the server's `CTraceFilterSimple` and the client's movement prediction run entities through
   `CBaseEntity::ShouldCollide()` and `CGameRules::ShouldCollide()`.
2. **The entity's collision group.** `m_CollisionGroup` is a **networked 5-bit field on every
   entity** (base-entity sendtable), so the client receives it and its prediction uses it too.
   `CGameRules::ShouldCollide()` refuses any pair whose lower group is `COLLISION_GROUP_DEBRIS` or
   `COLLISION_GROUP_DEBRIS_TRIGGER`, so an entity in `COLLISION_GROUP_DEBRIS_TRIGGER` is ignored by
   player movement, by the client's prediction, by the prop push-away code (`IsPushAwayEntity` only
   accepts `COLLISION_GROUP_PUSHAWAY`) and by physics, while traces that use
   `COLLISION_GROUP_NONE` — bullet and damage traces, most gameplay traces — still hit it. That is
   why this does not make players unshootable.

An earlier revision of this feature filtered the *movement trace* only, with a team tag carried in
the contents mask. Walking through worked, but standing inside somebody still got you pushed out:
that filter is server-side, the client cannot see it, and its prediction keeps resolving the
overlap against a body it still believes is solid. The group route was added next to it, and testing
on this build settled it — `mp_solid_player 0` removes the push, the team-tag switches never did —
so the team switches were removed. A collision group belongs to the entity, not to a relationship,
and the client-side push is unavoidable without a client change, which the stock-client rule
forbids.

**What it does.**

* `mp_solid_player` — `1` leaves every player in `COLLISION_GROUP_PLAYER` (stock), `0` puts every
  living player in `COLLISION_GROUP_DEBRIS_TRIGGER`, so no player blocks or pushes any other
  player. `CCSPlayer::UpdateSolidityCollisionGroup()` runs from `PreThink()` and writes the group
  only when it changes, so a runtime flip takes effect on the next frame with no respawn. Players
  inside a vehicle are skipped: the vehicle's own collision decides there. The group is restored to
  `COLLISION_GROUP_PLAYER` as soon as the convar goes back to `1` or the non-solid window below
  expires, and that transition runs the unstick check described after the list.
* `mp_players_nonsolid_time` — for N seconds after any spawn or respawn the player is put in
  `COLLISION_GROUP_DEBRIS_TRIGGER`, so neither side is blocked *or* pushed. The timer starts in
  `CCSPlayer::Spawn()`, which covers players respawned by feature 3 as well.
* `mp_solid_hostages` — `0` puts the hostage in `COLLISION_GROUP_DEBRIS_TRIGGER` at spawn, refreshed
  in `CHostage::HostageThink()` so a runtime change takes effect; `1` restores
  `COLLISION_GROUP_PLAYER` (stock). Players walk through the hostage, the player's push-away does
  not fire, and the hostage's own `AvoidPhysicsProps()`/`PushawayThink` has nothing to shove.
* `mp_solid_grenades` — `0` puts a thrown projectile in `COLLISION_GROUP_DEBRIS_TRIGGER` in
  `CBaseCSGrenadeProjectile::Spawn()`, which removes it from player movement and from the prop
  push-away; `1` leaves the group the base class assigned (stock). The switch is read when the
  grenade spawns, so it applies to grenades thrown after the change.
* `mp_teammates_are_enemies` — `1` forces friendly fire on (`CCSGameRules::IsFriendlyFireOn`) and
  makes `CCSGameRules::PlayerRelationship()` report `GR_NOTTEAMMATE`, so every rule that asks treats
  a teammate as an enemy. It applies to damage and to the rules built on top of it (team-kill
  punishment, bot targeting); it no longer has any solidity side effect, since solidity is a single
  global switch now.
  Hitting a teammate in this mode is not reported as a team attack: `CCSPlayer::OnTakeDamage()`
  skips the whole warning block, so there is no `#Game_teammate_attack` line in team chat, no
  `attacked a teammate` line in the server log, and no `#Hint_try_not_to_injure_teammates` hint.
  Killing one is not punished either, because `IPointsForKill()` asks `PlayerRelationship()`: no
  `-3300`, no `#Killed_Teammate`, no team-kill counter, no autokick.
  The stock team-attack damage mitigation is skipped as well, so a teammate takes full bullet and
  knife damage instead of 35%: being a valid target means being a *real* target. Blast damage was
  never mitigated, so grenades are unchanged.

* **Unstick when turning solid** — `CCSPlayer::UnstickFromPlayers()`, called from
  `UpdateSolidityCollisionGroup()` on the transition from non-solid back to solid, i.e. exactly when
  `mp_players_nonsolid_time` runs out or `mp_solid_player` is set back to `1`. If the player's box
  then overlaps another **solid** player's box (the case that used to leave both of them trapped
  inside each other until the movement solver separated them), he is moved to the nearest free
  standing spot just outside that box: eight directions around the other player at 36, 56 and 76
  units, each one verified with a player-hull trace using the movement collision group, then
  settled onto the floor below it with a short line trace. Velocity is cleared and the move is a
  teleport of a few tens of units, not a shove. A player who is still non-solid is ignored (he
  cannot trap anybody), vehicles are not involved, and on a stock server nothing ever fires because
  the group never leaves `COLLISION_GROUP_PLAYER`, so there is no transition and no check.

**Convars**

| Convar | Default | Values | Meaning |
| --- | --- | --- | --- |
| `mp_solid_player` | `1` | `0`/`1` | Players block each other at all (stock: yes) |
| `mp_solid_hostages` | `1` | `0`/`1` | Hostages block movement (stock: yes) |
| `mp_solid_grenades` | `1` | `0`/`1` | Thrown grenades block movement (stock: yes) |
| `mp_players_nonsolid_time` | `0` | seconds | Spawn/respawn non-solid window (`0` = off, stock) |
| `mp_teammates_are_enemies` | `0` | `0`/`1` | Teammates are valid targets (stock: no) |

**Client compatibility.** `m_CollisionGroup` is already in the base-entity sendtable and already
replicated for every entity, so the client honours these groups without any change on its side: no
netprop was added, no sendtable changed, no new entity, no client command, and the convars are not
replicated. `m_flLastSpawnTime` is a plain server-side member.

**Limitations.**

* Solid or not is all-or-nothing for players: there is no teammate/enemy split and no per-player
  exclusion, because a collision group is a property of the entity. A per-team split would need the
  client to be told what its prediction must ignore, i.e. a client change, which the stock-client
  rule forbids.
* Damage and hitboxes are untouched: non-solid players and hostages can still be shot, knives still
  hit, and `mp_teammates_are_enemies` gates damage through the normal friendly-fire path. With the
  switch off, friendly fire keeps the stock 35% team-attack mitigation; with it on, that mitigation
  is skipped and damage is full.
* The unstick move is a teleport, so a player whose window expires while he is standing inside a
  teammate sees a small jump instead of a slide. If no free spot exists within 76 units (a crowded
  corner with walls around the pair), he is left where he is and the movement solver separates the
  two exactly as it did before this existed.
* A thrown grenade still *flies* through the world with its own engine-side trace, which uses
  `COLLISION_GROUP_NONE`; `mp_solid_grenades 0` stops it blocking players and stops the push-away,
  but if the projectile's own bounce off a player is still visible that is the projectile's flight
  trace, not player solidity.

**Files:** `game_shared/cstrike/cs_gamerules.cpp` (convars, `IsFriendlyFireOn`,
`PlayerRelationship`), `game_shared/cstrike/cs_gamerules.h`, `dlls/cstrike/cs_player.cpp`
(`UpdateSolidityCollisionGroup`, `PreThink`, `Spawn`), `dlls/cstrike/cs_player.h`,
`dlls/cstrike/hostage/cs_simple_hostage.cpp/.h`,
`game_shared/cstrike/basecsgrenade_projectile.cpp/.h`.

---

# 5. Economy control

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
  raises it.
* `mp_startmoney` — unbounded on the upper side: the convar used to be declared with a hard maximum
  of `16000`, so any larger start money was clamped before a player ever saw it. The minimum is
  still `800`. The effective ceiling remains `mp_maxmoney`, because that is what clamps the
  account: raising the start money above `mp_maxmoney` therefore also needs `mp_maxmoney` raised.
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

`mp_startmoney` itself is a stock CS:S convar, not one added by this port; only its upper bound was
removed.

**Client compatibility.** Money is already a networked player field rendered by the v34 buy menu and
HUD, so any value these convars produce is displayed correctly by a stock client. No convars are
replicated, no netprops, no protocol change.

**Files:** `game_shared/cstrike/cs_gamerules.cpp` (convars, `mp_startmoney` bounds, kill awards,
round-start cash), `dlls/cstrike/cs_player.cpp` (`AddAccount` cap).

---

# 6. Damage scaling

**Origin:** CS:GO `mp_damage_scale_ct_head`, `mp_damage_scale_ct_body`, `mp_damage_scale_t_head`,
`mp_damage_scale_t_body` and `mp_damage_headshot_only`
(`game/shared/cstrike15/cs_gamerules.cpp`).

**Why:** Counter-Strike: Source hardcodes its whole damage model: a headshot is exactly `4x` weapon
damage, a stomach hit `1.25x`, a leg hit `0.75x`, and there is no way to make a round headshot-only.
Servers running pistol-only, "no headshot" or asymmetric-team modes have no switch for any of it.
These are exactly the CS:GO convars that cover it, all of them multipliers or flags, so all of them
default to stock behaviour.

**What it does.**

* `mp_damage_scale_ct_head` / `mp_damage_scale_t_head` — multiplied into the damage of a bullet that
  hit that team's head, after the stock `4x` headshot multiplier.
* `mp_damage_scale_ct_body` / `mp_damage_scale_t_body` — the same for every other hitgroup of that
  team (chest, stomach, arms, legs), after the stock per-hitgroup multipliers.
* `mp_damage_headshot_only` — a player takes damage only from a bullet that hit his head. Everything
  else is dropped before any armour, force or `Damage` usermessage work happens: body shots, knife
  hits, explosions, fire and fall damage. `0` (default) keeps the stock damage model.

Scaling happens in `CCSPlayer::TraceAttack`, in the per-bullet path where `ptr->hitgroup` is known,
so shotgun pellets are scaled individually and a trace that hits the helmet is scaled as a head hit.
It is applied after the stock hitgroup multipliers and before armour reduction in
`CCSPlayer::OnTakeDamage`, which is the same order CS:GO uses (scale the shot, then let armour absorb
part of it). Because it lives in the trace path, only bullet damage is scaled; explosion and fall
damage are unaffected by the four multipliers, exactly like CS:GO, where the scale is a property of a
hitgroup.

**Convars**

| Convar | Default | Values | Meaning |
| --- | --- | --- | --- |
| `mp_damage_scale_ct_head` | `1.0` | `0`+ | Damage multiplier for a Counter-Terrorist headshot |
| `mp_damage_scale_ct_body` | `1.0` | `0`+ | Damage multiplier for the rest of a Counter-Terrorist |
| `mp_damage_scale_t_head` | `1.0` | `0`+ | Damage multiplier for a Terrorist headshot |
| `mp_damage_scale_t_body` | `1.0` | `0`+ | Damage multiplier for the rest of a Terrorist |
| `mp_damage_headshot_only` | `0` | `0`/`1` | Only headshot bullets deal damage |

**Client compatibility.** Damage is not a client-side decision: the server applies the multipliers and
the only thing the stock client receives is the resulting health change and the existing `Damage`
usermessage. No convar is replicated, no netprop and no usermessage is added, so a v34 client needs no
knowledge of any of this.

One consequence worth knowing: with `mp_damage_headshot_only 1` a client still receives blood, spark
and flinch feedback for a body hit, because that happens in the trace before the damage is dropped.
Only the health loss is suppressed.

**Files:** `game_shared/cstrike/cs_gamerules.cpp` (convars), `dlls/cstrike/cs_player.cpp`
(`TraceAttack` scaling, `OnTakeDamage` headshot gate).

---

# 7. Free armour

**Origin:** CS:GO `mp_free_armor` (`game/shared/cstrike15/cs_gamerules.cpp`).

**Why:** Counter-Strike: Source has no way to hand out armour: it can only be bought in the buy
zone. Pistol-only rounds, deathmatch-style configurations and any mode where the operator wants
everyone on an equal footing therefore have to script it. CS:GO's single convar covers the whole
case.

**What it does.** Every time a player spawns, `CCSGameRules::PlayerSpawn()` sets his armour to `100`
and, with `2`, gives him a helmet as well. Because it runs in `PlayerSpawn`, it applies to round
starts and to both respawn paths (`RoundRespawn` and feature 3's `DoRespawn`), and it runs after the
`game_player_equip` loadout, so maps that hand out items themselves still get it.

Two details that differ from a literal reading of the convar name:

* It **tops up** rather than filling only an empty vest. A player who survived the round with `40`
  armour comes back with `100`, which is what CS:GO does when armour carries over.
* With `1` it never removes a helmet the player already has; it simply does not add one.

`0` (default) is stock Counter-Strike: Source: nothing is granted, armour is still bought as usual.

**Convars**

| Convar | Default | Values | Meaning |
| --- | --- | --- | --- |
| `mp_free_armor` | `0` | `0`/`1`/`2` | Armour granted on spawn: none, kevlar, kevlar and helmet |

**Client compatibility.** Armour and the helmet flag are already networked player fields drawn by the
v34 HUD (the armour counter) and used by the client for its hit feedback, so a stock client shows the
granted armour exactly like bought armour. No convar is replicated, no netprop and no usermessage is
added.

Bots are unaffected: they still decide to buy armour on their own, so an operator who enables this
just sees them keep that money.

**Files:** `game_shared/cstrike/cs_gamerules.cpp` (convar, grant in `PlayerSpawn`).

---

# 8. Purchase restrictions

**Origin:** CS:GO `mp_buy_allow_guns`, `mp_buy_allow_grenades` and `mp_max_armor`
(`game/shared/cstrike15/cs_gamerules.cpp`).

**Why:** stock Counter-Strike: Source has no way to limit what may be bought beyond the map's buy
zones. Pistol-only nights, "no AWP" and "no grenades" servers therefore rely on external plugins or
on trust. These are the three CS:GO convars that cover the whole buy menu, and their defaults allow
everything stock CS:S allows.

**What it does.** Every purchase goes through `CCSPlayer::HandleCommand_Buy()`, which is the single
place the buy menu commands, the `buy` aliases, `autobuy`/`rebuy`, the per-item buy binds and the
bots end up (bots issue `buy <alias>` through `ClientCommand`, so they are covered by the same rule).
One check there applies the three convars:

* `mp_buy_allow_guns` — a bitfield over weapon groups. A group whose bit is clear cannot be bought.
  `255` (default) sets every defined bit and is therefore stock behaviour.

  | Bit | Value | Group |
  | --- | --- | --- |
  | 0 | `1` | Pistols |
  | 1 | `2` | Sub machine guns |
  | 2 | `4` | Rifles |
  | 3 | `8` | Shotguns |
  | 4 | `16` | Sniper rifles |
  | 5 | `32` | Machine guns |

  Bits 6 and 7 (`64`, `128`) are unused, exactly like CS:GO. The categories come from the weapon's own
  `CSWeaponType` in its weapon script, so the rule is data driven and does not list weapon names.
* `mp_buy_allow_grenades` — `0` refuses HE, flashbang and smoke purchases. `1` (default) is stock.
* `mp_max_armor` — caps armour purchases: `0` refuses vest and vest+helmet, `1` refuses only
  vest+helmet, `2` (default) is stock.

A refused purchase prints "This item cannot be bought on this server" in the centre of the screen
(suppressed for `autobuy`/`rebuy` feedback, like the other buy results) and returns `BUY_NOT_ALLOWED`.

**What it deliberately does not do.**

* It restricts **purchases only**. Armour can still arrive through `game_player_equip` or through
  feature 7's `mp_free_armor`, and a weapon picked up from the ground is not taken away. This is
  exactly the CS:GO behaviour of these convars.
* Defuse kits, nightvision and the tactical shield are not covered. CS:GO restricts those with
  `mp_items_prohibited` (a comma separated list of item indices), which is a different mechanism and
  not part of this port.
* Ammo (`primammo`/`secammo`) is not restricted: a player who already owns a weapon can still refill
  it, so a restriction never strands a player with an empty gun.
* The stock v34 buy menu still lists every item. The client cannot be told to hide entries without
  changing the client, so a restricted item is shown and simply refuses to buy. That is the same
  compromise as feature 3's buy-window rule and the only server-side option.

**Convars**

| Convar | Default | Values | Meaning |
| --- | --- | --- | --- |
| `mp_buy_allow_guns` | `255` | `0`–`255` | Weapon groups that may be bought (bitfield above) |
| `mp_buy_allow_grenades` | `1` | `0`/`1` | Whether grenades may be bought |
| `mp_max_armor` | `2` | `0`/`1`/`2` | Highest armour that may be bought |

**Client compatibility.** Nothing about a purchase is client-authoritative and none of these convars is
replicated. The stock client sends its usual `buy` command, receives the usual `BuyResult` handling
(account and weapon state, or a centre-screen refusal), and needs no knowledge of the rules.

**Files:** `game_shared/cstrike/cs_gamerules.cpp` (convars), `dlls/cstrike/cs_player.cpp`
(`IsBuyDisallowedByRules`, called from `HandleCommand_Buy`).

---

# 9. Halftime

**Origin:** CS:GO `mp_halftime` (`game/shared/cstrike15/cs_gamerules.cpp:18692`), the side swap in
`CCSGameRules::SwitchTeamsAtRoundReset()` (`:18239`) and `CCSPlayer`'s
`m_switchTeamsOnNextRoundReset` block (`game/server/cstrike15/cs_player.cpp:16815`).

**Why:** stock Counter-Strike: Source never swaps sides: a match is played on the same side from
start to finish, so an asymmetric map is never played both ways. CS:GO's halftime is the switch that
makes a full match fair, and it is a single convar.

**What it does.** When `mp_halftime` is `1` and `mp_maxrounds` is greater than `1`, the round restart
that follows the first half of the match (`m_iTotalRoundsPlayed == mp_maxrounds / 2`) does the
following, before anybody is spawned for the new round:

* Every player on CT or T is moved to the other side with `CCSPlayer::SwitchTeam()`, which drops the
  defuse kit, re-maps the player class to the corresponding model of the new side and refreshes the
  team counts. Dead players are swapped as well, so they return on the correct side; spectators and
  unassigned players are left alone.
* `m_iNumCTWins` and `m_iNumTerroristWins` are exchanged, so the score stays with the side that
  actually won the rounds, and `UpdateTeamScores()` publishes it.
* The loss ladder is **reset**, not exchanged: both consecutive-loss counters go to `0` and
  `m_iLoserBonus` goes back to `1400`, the value a fresh match starts with. CS:GO does the same thing
  with `mp_starting_losses` (a new half starts a clean streak), and in this tree the counters only
  feed the shared losing bonus of feature 5.
* Each swapped player is reset exactly like CS:GO's `SwitchTeamsAtRoundReset()` does it:
  `RemoveAllItems( true )` destroys weapons and armour, the account is set back to `mp_startmoney`,
  and `MarkAsNotReceivingMoneyNextRound()` skips the round's win/loss cash award (which runs later in
  the same restart), so the second half genuinely begins on start money instead of start money plus a
  bonus.

The swap happens during the restart rather than in a separate halftime phase: v34 has no match phase
system, so the CS:GO sequence "announce, freeze, swap at the round reset" collapses into "swap during
the round reset". A player therefore sees his side change as the new round starts, which is the moment
CS:GO applies it too.

`0` (default) is stock Counter-Strike: Source: no swap ever happens.

**Details and limits.**

* Assassination maps (`func_vip_safetyzone`) are skipped: they are built around a fixed VIP and have
  no meaningful side swap, so `mp_halftime` silently does nothing there.
* The trigger is an equality test, so it fires exactly once per match. `mp_restartgame` also counts as
  a played round, so an admin restart can trigger it early and consume it; lowering `mp_maxrounds`
  mid-match below twice the rounds already played means it never fires.
* `mp_humanteam` is applied earlier in the same restart (`MoveHumansToHumanTeam`) and is then undone
  by the swap: the two convars should not be combined.
* Odd `mp_maxrounds` floors the halfway point (`3` rounds swaps after round 1).
* Ammo and grenades are removed together with the weapons (`RemoveAllItems`), which is what CS:GO
  does at a side swap; `mp_free_armor` and feature 3's respawn policies then apply normally.

**Convars**

| Convar | Default | Values | Meaning |
| --- | --- | --- | --- |
| `mp_halftime` | `0` | `0`/`1` | Swap the teams after the first half of `mp_maxrounds` |

**Client compatibility.** The client already handles a team change: `CBasePlayer::ChangeTeam` fires
the stock `player_team` game event, the score comes from the replicated team scores, and the player
model follows the re-mapped class at respawn. No convar is replicated, no netprop and no usermessage
is added, so a v34 client needs no knowledge of halftime.

**Files:** `game_shared/cstrike/cs_gamerules.cpp` (convar, `SwapTeamsForHalftime`, the trigger in
`RestartRound`), `game_shared/cstrike/cs_gamerules.h` (declaration).
