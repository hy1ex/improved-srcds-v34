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
