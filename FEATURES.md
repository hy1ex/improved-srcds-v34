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
| `mp_use_respawn_waves` | `0` | [3. Respawn on death](#3-respawn-on-death) | 0=instantly, 1=team waves, 2=when the whole team is dead |
| `mp_respawnwavetime_ct` | `10` | [3. Respawn on death](#3-respawn-on-death) | seconds |
| `mp_respawnwavetime_t` | `10` | [3. Respawn on death](#3-respawn-on-death) | seconds |
| `mp_respawn_immunitytime` | `4` | [3. Respawn on death](#3-respawn-on-death) | seconds of damage immunity + model fade |
| `mp_respawn_immunity_fade` | `1` | [3. Respawn on death](#3-respawn-on-death) | 0=none, 1=fade in, 2=half transparent |
| `mp_respawn_weapons` | `0` | [3. Respawn on death](#3-respawn-on-death) | 0=default pistol, 1=random, 2=keep |
| `mp_buy_during_immunity` | `0` | [3. Respawn on death](#3-respawn-on-death) | 0=off, 1=both, 2=T, 3=CT: spawn immunity becomes the buy window, cut when it ends |
| `mp_buy_anywhere` | `0` | [3. Respawn on death](#3-respawn-on-death) | 0=stock, 1=both, 2=T, 3=CT (also opens the menu) |
| `mp_solid_player` | `-1` | [4. Player, hostage and grenade solidity](#4-player-hostage-and-grenade-solidity) | -1 = never touch collision groups, 0 = walk through every player, no push |
| `mp_solid_hostages` | `-1` | [4. Player, hostage and grenade solidity](#4-player-hostage-and-grenade-solidity) | -1 = never touch collision groups, 0 = walk through hostages |
| `mp_solid_grenades` | `-1` | [4. Player, hostage and grenade solidity](#4-player-hostage-and-grenade-solidity) | -1 = never touch collision groups, 0 = walk through thrown grenades |
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
| `mp_damage_vampiric_amount` | `0` | [6. Damage scaling](#6-damage-scaling) | fraction of damage dealt healed back to the attacker |
| `mp_free_armor` | `0` | [7. Free armour](#7-free-armour) | 0=none, 1=kevlar, 2=kevlar and helmet |
| `mp_buy_allow_guns` | `1` | [8. Purchase restrictions](#8-purchase-restrictions) | 0=nobody, 1=both teams, 2=T, 3=CT: who may buy guns |
| `mp_buy_allow_grenades_ct` | `7` | [8. Purchase restrictions](#8-purchase-restrictions) | bitfield: 1 HE, 2 flashbang, 4 smoke |
| `mp_buy_allow_grenades_t` | `7` | [8. Purchase restrictions](#8-purchase-restrictions) | bitfield: 1 HE, 2 flashbang, 4 smoke |
| `mp_buy_allow_machine_guns_ct` | `1` | [8. Purchase restrictions](#8-purchase-restrictions) | bitfield: 1 M249 |
| `mp_buy_allow_machine_guns_t` | `1` | [8. Purchase restrictions](#8-purchase-restrictions) | bitfield: 1 M249 |
| `mp_buy_allow_pistols_ct` | `15` | [8. Purchase restrictions](#8-purchase-restrictions) | bitfield: 1 USP, 2 P228, 4 Deagle, 8 Five-SeveN |
| `mp_buy_allow_pistols_t` | `15` | [8. Purchase restrictions](#8-purchase-restrictions) | bitfield: 1 Glock, 2 P228, 4 Deagle, 8 Dual Berettas |
| `mp_buy_allow_rifles_ct` | `7` | [8. Purchase restrictions](#8-purchase-restrictions) | bitfield: 1 FAMAS, 2 M4A1, 4 AUG |
| `mp_buy_allow_rifles_t` | `7` | [8. Purchase restrictions](#8-purchase-restrictions) | bitfield: 1 Galil, 2 AK-47, 4 SG552 |
| `mp_buy_allow_shotguns_ct` | `3` | [8. Purchase restrictions](#8-purchase-restrictions) | bitfield: 1 M3, 2 XM1014 |
| `mp_buy_allow_shotguns_t` | `3` | [8. Purchase restrictions](#8-purchase-restrictions) | bitfield: 1 M3, 2 XM1014 |
| `mp_buy_allow_smgs_ct` | `15` | [8. Purchase restrictions](#8-purchase-restrictions) | bitfield: 1 TMP, 2 MP5 Navy, 4 UMP-45, 8 P90 |
| `mp_buy_allow_smgs_t` | `15` | [8. Purchase restrictions](#8-purchase-restrictions) | bitfield: 1 MAC-10, 2 MP5 Navy, 4 UMP-45, 8 P90 |
| `mp_buy_allow_snipers_ct` | `7` | [8. Purchase restrictions](#8-purchase-restrictions) | bitfield: 1 Scout, 2 AWP, 4 SG550 |
| `mp_buy_allow_snipers_t` | `7` | [8. Purchase restrictions](#8-purchase-restrictions) | bitfield: 1 Scout, 2 AWP, 4 G3SG1 |
| `mp_max_armor_ct` | `2` | [8. Purchase restrictions](#8-purchase-restrictions) | 0=none, 1=kevlar, 2=kevlar and helmet |
| `mp_max_armor_t` | `2` | [8. Purchase restrictions](#8-purchase-restrictions) | 0=none, 1=kevlar, 2=kevlar and helmet |
| `mp_halftime` | `0` | [9. Halftime](#9-halftime) | 0 = never swap, 1 = swap at half of mp_maxrounds |
| `mp_randomspawn` | `0` | [10. Random deathmatch spawns](#10-random-deathmatch-spawns) | 0=default, 1=both teams, 2=Terrorists, 3=CTs |
| `mp_randomspawn_los` | `1` | [10. Random deathmatch spawns](#10-random-deathmatch-spawns) | 0/1, require the spot to be out of the enemy team's sight |
| `mp_randomspawn_dist` | `0` | [10. Random deathmatch spawns](#10-random-deathmatch-spawns) | minimum distance from a living enemy, 0 = off |
| `mp_dm_scoring` | `0` | [11. Deathmatch kill scoring](#11-deathmatch-kill-scoring) | 0/1, award CS:GO deathmatch points per kill |
| `mp_dm_kill_base_score` | `10` | [11. Deathmatch kill scoring](#11-deathmatch-kill-scoring) | base points per kill |
| `mp_hostages_max` | `0` | [12. Hostage rescue rules](#12-hostage-rescue-rules) | keep only N of the map's hostages; 0 = all |
| `mp_hostages_spawn_farthest` | `0` | [12. Hostage rescue rules](#12-hostage-rescue-rules) | 1 = keep the most spread-out set |
| `mp_hostages_spawn_same_every_round` | `1` | [12. Hostage rescue rules](#12-hostage-rescue-rules) | 0 = a new hostage set every round |
| `mp_hostages_spawn_force_positions` | *(empty)* | [12. Hostage rescue rules](#12-hostage-rescue-rules) | hostage indices to force, e.g. `0,2` |
| `mp_hostages_takedamage` | `1` | [12. Hostage rescue rules](#12-hostage-rescue-rules) | 0 = hostages cannot be hurt |
| `mp_hostages_rescuetowin` | `0` | [12. Hostage rescue rules](#12-hostage-rescue-rules) | rescues needed to win; 0 = every surviving hostage |
| `mp_hostages_run_speed_modifier` | `1.0` | [12. Hostage rescue rules](#12-hostage-rescue-rules) | movement force scale for a following hostage |
| `mp_only_cts_rescue_hostages` | `1` | [12. Hostage rescue rules](#12-hostage-rescue-rules) | 0 = Terrorists may take a hostage too |
| `mp_anyone_can_pickup_c4` | `0` | [13. C4 rules](#13-c4-rules) | 1 = any team may pick the bomb up |
| `mp_c4_cannot_be_defused` | `0` | [13. C4 rules](#13-c4-rules) | 1 = a planted bomb cannot be defused |
| `mp_plant_c4_anywhere` | `0` | [13. C4 rules](#13-c4-rules) | 1 = plant outside a bomb target |

| `mp_disconnect_kills_players` | `0` | [14. Disconnect kills](#14-disconnect-kills) | 1 = kill a living player when he disconnects |
| `mp_disconnect_kills_bots` | `0` | [14. Disconnect kills](#14-disconnect-kills) | 1 = do the same for bots; needs the switch above |
| `mp_drop_knife_enable` | `0` | [15. Knife drop](#15-knife-drop) | 1 = the `drop` command may drop the knife |
| `mp_drop_grenade_enable` | `0` | [17. Grenade drop](#17-grenade-drop) | 1 = the `drop` command may drop a grenade |
| `mp_weapon_prev_owner_touch_time` | `0.8` | [16. Weapon pickup timing](#16-weapon-pickup-timing) | seconds before the dropper may take his weapon back |
| `mp_weapon_next_owner_touch_time` | `0` | [16. Weapon pickup timing](#16-weapon-pickup-timing) | seconds before anybody may pick a dropped weapon up |
## Command index

| Command | Feature | Notes |
| --- | --- | --- |
| `dm_reset_spawns` | [10. Random deathmatch spawns](#10-random-deathmatch-spawns) | rebuild the generated spawn points from the current nav mesh |

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
  the kit until the next round. Every spawn also strips a leftover kit first, exactly like CS:GO
  (`if ( mp_defuser_allocation.GetInt() == DefuserAllocation::Random ) pPlayer->RemoveDefuser();`),
  so a kit bought last round cannot turn the "one random kit" round into a two-kit round.
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

**Origin:** CS:GO — `mp_respawn_on_death_ct` / `mp_respawn_on_death_t`, `mp_use_respawn_waves`,
`mp_respawnwavetime_ct` / `mp_respawnwavetime_t` and `mp_respawn_immunitytime`
(`game/shared/cstrike15/cs_gamerules.cpp`), the wave loop `CCSGameRules::CheckRespawnWaves()` and
`CCSGameRules::TeamExterminationCheck()`'s respawn guards, the spawn immunity in
`CCSPlayer::Spawn()` / `State_PreThink_ACTIVE()`, plus `mp_buy_anywhere` and
`mp_buy_during_immunity` (`game/shared/gamevars_shared.cpp`) for the buy rules.

`mp_respawn_weapons` and `mp_respawn_immunity_fade` are **not** CS:GO convars: they are this port's
own settings, kept because they cover what CS:GO does with its own loadout and render code. CS:GO's
`mp_buy_during_immunity` replaces this port's earlier `mp_respawn_buytime`, which had no CS:GO
counterpart.

**Why:** stock Counter-Strike: Source only ever brings players back at round restart. Deathmatch,
arena, gungame and warmup-style servers therefore have to fake respawns with round restarts, which
resets scores, money and objectives. This port adds real per-player respawns with a per-team wave
delay, an optional loadout policy, optional spawn immunity and a post-respawn buy window.

**What it does.**

* When a player dies, `CCSPlayer::Event_Killed` drops his weapons and remembers the loadout; the
  timing is owned by the game rules, exactly like CS:GO. `CCSGameRules::CheckRespawnWaves()` runs
  from `CCSGameRules::Think()` and keeps one respawn clock per team
  (`m_flNextRespawnWave[2]`):
  * `mp_use_respawn_waves 0` (**default**) — no waves at all, so a dead player comes back on the
    next frame, which is CS:GO's instant respawn (`GetRespawnWaveMaxLength()` returns `0` and the
    wave length is what the clock advances by).
  * `1` — the team respawns together once every `mp_respawnwavetime_<team>` seconds; players who
    die in between wait for the next wave.
  * `2` — nothing happens while a teammate is still alive; as soon as the whole team is dead the
    wave clock starts and the team comes back `mp_respawnwavetime_<team>` seconds later.
  The player is put back by `CCSPlayer::DoRespawn()`, which reuses the same `StopObserverMode` /
  `State_Transition( STATE_ACTIVE )` / `respawn()` path a round restart uses. The clocks are reset
  in `RestartRound()` and the loop is skipped during the freeze period and intermission.
* `CCSGameRules::TeamExterminationCheck()` carries CS:GO's `!bTsRespawn` / `!bCTsRespawn` guards,
  so eliminating a team that respawns no longer ends the round: the round keeps running and the
  dead team comes back. Without those guards a respawn server ended the round on the first wipe.
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
* `mp_respawn_immunitytime` grants damage immunity, armed in `CCSPlayer::Spawn()` the way CS:GO arms
  it, so it applies to **every** spawn of a player whose team respawns on death, not only to the
  respawns this feature brings back. Damage is dropped at the top of `CCSPlayer::OnTakeDamage`, so
  nothing (bullets, grenades, falls) can hurt the player during the window. As in CS:GO's
  `State_PreThink_ACTIVE()`, `PreThink()` notes when the player first moves
  (`m_bHasMovedSinceSpawn`) and then cuts the remaining immunity down to one second, so spawn
  protection cannot be carried into the fight. `0` disables it.
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
* `mp_buy_during_immunity` makes the spawn immunity window the buy window for the teams it names:
  * `0` — off, `mp_buytime` decides as stock (**default**),
  * `1` — both teams,
  * `2` — Terrorists only,
  * `3` — Counter-Terrorists only.
  It **replaces** the round's window rather than extending it, and cuts in both directions: while
  the immunity lasts the player may buy even after `mp_buytime` has run out, and the moment the
  immunity ends he is refused even while `mp_buytime` is still running. An exemption that only ever
  adds time can never take any away, which left the convar with no visible effect whenever the
  immunity ended inside the round's window - the usual case on a respawn server, where
  `mp_respawn_immunitytime 4` (cut to one second once the player moves) sits inside a
  `mp_buytime 15`. A cutoff is the only behaviour a server can enforce on its own: the stock v34
  client decides for itself whether to open the VGUI buy menu, and once `mp_buytime` has elapsed it
  cannot be opened again without a client change, so buying after that has to come from the
  `buy` / `autobuy` / `rebuy` commands and buy binds. The rule is checked in
  `CCSPlayer::IsBuyTimeAvailable()`, the one place that decides whether the window is open, so
  those commands, the ammo and equipment aliases and the networked `m_bInBuyZone` bit all obey the
  same cut. The refusal prints "You may only buy while spawn protected" instead of `#Cant_buy`,
  which counts whole minutes and would read "for 0 minutes" for a window of a few seconds. This only
  matters on a server that also sets `mp_respawn_immunitytime`, because that is what creates the
  window.
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
  convar cannot do that: the stock client's `buymenu` / `buyequip` commands compare `mp_buytime`
  with the networked round start on their own, so once the round's window has elapsed the menu
  cannot be opened again without a client change, whatever the server would allow.
  `mp_buy_during_immunity` is therefore a rule for the console commands and binds, not a way to
  keep the menu open past `mp_buytime`.
* **A player's ragdoll is cleared when he spawns** — ported from CS:GO `cs_player.cpp:1752`, where
  `CCSPlayer::Spawn()` removes the player's previous ragdoll with `UTIL_Remove( m_hRagdoll )`.
  Stock CS:S only drops the handle, so every death left a `cs_ragdoll` entity in the world until the
  round restarted or the edict limit forced a cleanup; on an auto-respawn or deathmatch server that
  piles bodies up within minutes and costs both server edicts and client ragdoll physics. This
  follows every spawn, including the respawns added by this feature, so a body disappears exactly
  when its owner comes back rather than on a timer. It is a behaviour of the port, not a switch:
  there is no convar, because CS:GO has none for it either.

**Convars**

| Convar | Default | Values | Meaning |
| --- | --- | --- | --- |
| `mp_respawn_on_death_ct` | `0` | `0`/`1` | Respawn dead CTs |
| `mp_respawn_on_death_t` | `0` | `0`/`1` | Respawn dead Terrorists |
| `mp_use_respawn_waves` | `0` | `0`-`2` | `0` instant respawn (CS:GO default), `1` team waves, `2` when the whole team is dead |
| `mp_respawnwavetime_ct` | `10` | seconds | CT respawn wave length |
| `mp_respawnwavetime_t` | `10` | seconds | Terrorist respawn delay |
| `mp_respawn_immunitytime` | `4` | seconds, `0` = off | Damage immunity and model fade at every spawn while the team respawns; cut to 1 s once the player moves |
| `mp_respawn_immunity_fade` | `1` | `0`-`2` | Immunity visual: `0` none, `1` fade in, `2` fixed half transparency |
| `mp_respawn_weapons` | `0` | `0`-`2` | Loadout policy after a respawn |
| `mp_buy_during_immunity` | `0` | `0`-`3` | Buy window is the spawn immunity window, cut when the immunity ends: `0` off (`mp_buytime`), `1` both teams, `2` T, `3` CT |
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
command. `m_flSpawnImmuneUntil`, `m_bHasMovedSinceSpawn`, `m_szRespawnWeapons`, `m_nRespawnWeaponCount` and
`m_nImmunityRenderAlpha` are plain server-side members on `CCSPlayer`, and `m_flNextRespawnWave[2]`
on `CCSGameRules`: not in `BEGIN_DATADESC`, not in the sendtable. The convars are not replicated.

**Risks / limitations.**

* Bots respawn through the same path. Their AI state is reset the way a round restart resets it,
  but a bot that respawns mid-round is not explicitly told to re-path; that is the main thing to
  watch on a bot server.
* Objective maps keep their normal rules: if respawns are enabled on a bomb map, a dead bomb
  carrier still drops the bomb (subject to feature 1) and the round can still be won or lost on
  objectives while players respawn.
* `mp_roundtime`, `mp_maxrounds`, `mp_winlimit` and the objective checks are unchanged. The one
  round-win rule that does change is elimination: while a wiped team respawns, "the other team is
  eliminated" no longer terminates the round (CS:GO's `TeamExterminationCheck` guards). Servers that want pure deathmatch should set a long round
  time and `ignore_round_win_conditions`-style rules themselves.
* `mp_buy_anywhere` lifts the buy *zone* only. The buy *window* still applies, and the round's
  other blockers (dead, VIP, `m_bCTCantBuy`/`m_bTCantBuy`) are unaffected.
* The buy-menu bit is the only buy-zone signal a stock client exposes. If a particular client build
  gates the menu on something else, `mp_buy_anywhere` keeps working for the console commands but the
  VGUI menu cannot be opened without a client DLL change, which the stock-client rule forbids.

**Files:** `game_shared/cstrike/cs_gamerules.cpp` (convars, `CheckRespawnWaves`,
`GetRespawnWaveMaxLength`, `TeamExterminationCheck`), `game_shared/cstrike/cs_gamerules.h`
(`m_flNextRespawnWave`), `dlls/cstrike/cs_player.cpp`
(`DoRespawn`, `RememberLoadoutForRespawn`, `RestoreLoadoutForRespawn`,
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

* `mp_solid_player` — `-1` (the default) never writes a player's collision group at all. The group
  the engine assigns at spawn stays, so players still block each other exactly as they do in stock,
  but the field belongs to whoever else wants it: a plugin can put one or two specific players in
  any group it chooses (the usual reason is a game mode where a single player walks through
  everybody) and this build will not revert it on the next frame. `0` puts every living player in
  `COLLISION_GROUP_DEBRIS_TRIGGER`, so no player blocks or pushes any other player, and `1` holds
  every living player in `COLLISION_GROUP_PLAYER` (stock). Those two are opt-in *and* authoritative:
  `CCSPlayer::UpdateSolidityCollisionGroup()` runs from `PreThink()` and, while the convar is `0` or
  `1`, keeps re-asserting the matching group, which is exactly why the hands-off value is the
  default. A runtime flip takes effect on the next frame with no respawn. Players inside a vehicle
  are skipped: the vehicle's own collision decides there. Switching back to `-1` restores a player
  that this build left non-solid to `COLLISION_GROUP_PLAYER` once — and only if the player is still
  in this build's non-solid group, so a group written by a plugin is never overwritten — and that
  transition runs the unstick check described after the list, as does the end of the non-solid
  window below.
* `mp_players_nonsolid_time` — for N seconds after any spawn or respawn the player is put in
  `COLLISION_GROUP_DEBRIS_TRIGGER`, so neither side is blocked *or* pushed. The timer starts in
  `CCSPlayer::Spawn()`, which covers players respawned by feature 3 as well. It only acts while
  `mp_solid_player` is `0` or `1`: with the default `-1` no group is written, so the window is
  inert and a plugin keeps control of the field.
* `mp_solid_hostages` — `-1` (the default) never writes the hostage's collision group, so the group
  the engine assigned stays, the hostage is as solid as in stock, and a plugin owns the field. `0`
  puts the hostage in `COLLISION_GROUP_DEBRIS_TRIGGER` at spawn, refreshed in
  `CHostage::HostageThink()` so a runtime change takes effect; `1` restores the engine's own group,
  `COLLISION_GROUP_NONE` — the group a hostage has in stock, *not* `COLLISION_GROUP_PLAYER`. Players
  walk through the hostage in the `0` case, the player's push-away does not fire, and the hostage's
  own `AvoidPhysicsProps()`/`PushawayThink` has nothing to shove. Switching from `0` to `-1` hands
  the hostage back on `COLLISION_GROUP_NONE` once, and only while it is still this build's non-solid
  group, so a group written by a plugin is never overwritten.
* `mp_solid_grenades` — `0` puts a thrown projectile in `COLLISION_GROUP_DEBRIS_TRIGGER` in
  `CBaseCSGrenadeProjectile::Spawn()`, which removes it from player movement and from the prop
  push-away; `1` leaves the group the base class assigned, which is the engine default and stock
  behaviour, and `-1` (the default) does not touch the group at all, so a plugin owns it. The switch
  is read when the grenade spawns, so it applies to grenades thrown after the change.
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
  `mp_players_nonsolid_time` runs out, `mp_solid_player` is set back to `1`, or `mp_solid_player` is
  switched to `-1` while this build still had him non-solid. If the player's box
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
| `mp_solid_player` | `-1` | `-1`/`0`/`1` | `-1`: leave collision groups alone (default, stock behaviour), `0`: walk through every player, `1`: players block |
| `mp_solid_hostages` | `-1` | `-1`/`0`/`1` | `-1`: leave collision groups alone (default, stock behaviour), `0`: walk through hostages, `1`: engine default group |
| `mp_solid_grenades` | `-1` | `-1`/`0`/`1` | `-1`: leave collision groups alone (default, stock behaviour), `0`: walk through thrown grenades, `1`: engine default group |
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
* `mp_startmoney` — unbounded in both directions: the convar used to be declared with a hard
  maximum of `16000`, so any larger start money was clamped before a player ever saw it, and a
  minimum of `800`, which made "start with nothing" modes impossible. Both bounds are now CS:GO's
  (`true, 0, false, 0`), so `mp_startmoney 0` is legal and the default stays `800`. The effective
  ceiling remains `mp_maxmoney`, because that is what clamps the account: raising the start money
  above `mp_maxmoney` therefore also needs `mp_maxmoney` raised.
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

`mp_startmoney` itself is a stock CS:S convar, not one added by this port; only its bounds were
changed, to the ones CS:GO uses.

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
* `mp_damage_scale_ct_body` / `mp_damage_scale_t_body` — the same for the other hitgroups of that
  team (chest, stomach, arms, legs), after the stock per-hitgroup multipliers. `HITGROUP_GENERIC`
  is left unscaled, exactly like CS:GO, where the scale lives inside its hitgroup switch cases.
* `mp_damage_headshot_only` — non-headshot **bullet** damage is zeroed in the trace, so a player only
  loses health to a bullet that hit his head. As in CS:GO this is a gun rule: explosions, fire, fall
  and knife damage still hurt, and the blocked hit spawns a spark instead of blood. `0` (default)
  keeps the stock damage model.

* `mp_damage_vampiric_amount` — the fraction of the health a victim actually loses that is healed
  back onto whoever hurt him, `0` (default) being stock and no healing at all. CS:GO's version
  (`cs_player.cpp:6540`) is ported as-is: the amount is the health removed after armour and clamping,
  the attacker is never healed by his own self-damage, and the heal is capped at **twice** his
  maximum health, so a full-health player can keep the extra. It applies to every source of damage a
  player takes, not only bullets, because that is where CS:GO applies it.

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
| `mp_damage_vampiric_amount` | `0` | `0`-`1`+ | Fraction of the damage dealt healed back onto the attacker |

**Client compatibility.** Damage is not a client-side decision: the server applies the multipliers and
the only thing the stock client receives is the resulting health change and the existing `Damage`
usermessage. No convar is replicated, no netprop and no usermessage is added, so a v34 client needs no
knowledge of any of this.

With `mp_damage_headshot_only 1` a body hit still shows the stock flinch and impact sound, but no
blood: the damage is zeroed before the bleed dispatch, and `g_pEffects->Sparks()` marks the hit the
way CS:GO does.

**Files:** `game_shared/cstrike/cs_gamerules.cpp` (convars), `dlls/cstrike/cs_player.cpp`
(`TraceAttack`: per-hitgroup scaling and the headshot-only rule).

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

While the convar is on, armour is **not for sale**, again like CS:GO: `CanAcquire()` answers
`NotAllowedForPurchase` for kevlar while `IsArmorFree()` is true and for the assault suit while
`IsHelmetFree()` is true, so a player cannot spend money on armour he is given anyway. With `2`
neither item can be bought; with `1` only the vest is blocked and the helmet can still be bought.
The check lives in `IsBuyDisallowedByRules()`, the single funnel every purchase passes through, so
the buy menu, the buy aliases and the bots all obey it.

`0` (default) is stock Counter-Strike: Source: nothing is granted, armour is still bought as usual.

**Convars**

| Convar | Default | Values | Meaning |
| --- | --- | --- | --- |
| `mp_free_armor` | `0` | `0`/`1`/`2` | Armour granted on spawn: none, kevlar, kevlar and helmet |

**Client compatibility.** Armour and the helmet flag are already networked player fields drawn by the
v34 HUD (the armour counter) and used by the client for its hit feedback, so a stock client shows the
granted armour exactly like bought armour. No convar is replicated, no netprop and no usermessage is
added.

Bots decide to buy armour on their own, and the refusal above catches that too: they get the armour
at spawn instead of paying for it, and keep the money.

**Files:** `game_shared/cstrike/cs_gamerules.cpp` (convar, grant in `PlayerSpawn`).

---

# 8. Purchase restrictions

**Why:** stock Counter-Strike: Source has no way to limit what may be bought beyond the map's buy
zones. Pistol-only nights, "no AWP" and "no grenades" servers therefore rely on external plugins or
on trust. These convars cover the whole buy menu, and every rule is set per team, so the two sides can
be given different shopping lists. Every default allows everything stock CS:S allows.

**What it does.** Every purchase goes through `CCSPlayer::HandleCommand_Buy()`, which is the single
place the buy menu commands, the `buy` aliases, `autobuy`/`rebuy`, the per-item buy binds and the
bots end up (bots issue `buy <alias>` through `ClientCommand`, so they are covered by the same rule).
One check there applies the convars:

* `mp_buy_allow_guns` — who may buy guns at all: `0` nobody, `1` both teams (default), `2`
  Terrorists only, `3` CT only. Grenades and armour are not guns and are governed by their own
  convars, so a server that only wants to stop rifles uses the rifle convar below instead.
* `mp_max_armor_ct` / `mp_max_armor_t` — the highest armour that team may buy: `0` nothing, `1`
  kevlar, `2` kevlar and helmet (default). The `mp_free_armor` refusal below applies on top.
* `mp_buy_allow_grenades_ct` / `mp_buy_allow_grenades_t` — one bit per grenade: `1` HE, `2`
  flashbang, `4` smoke. `7` (default) is all three.
* `mp_buy_allow_<group>_ct` / `mp_buy_allow_<group>_t` — one bit per weapon, for each of the six
  weapon groups. A weapon whose bit is clear cannot be bought by that team.

Only weapons a team can actually buy have a bit. The other side's own weapons (Glock, MAC-10, Galil,
AK-47, SG552, G3SG1 and their CT counterparts) can never be bought across teams anyway: the weapon
script's own team refuses them before these rules are reached. Bit positions follow the buy menu and
line up on both sides, so the same number means the same slot for either team and a value can be
copied between the `_ct` and `_t` convar of a group.

**Pistols** — `mp_buy_allow_pistols_ct`, `mp_buy_allow_pistols_t`, highest value `15`

| Value | CT | T |
| --- | --- | --- |
| `1` | USP | Glock |
| `2` | P228 | P228 |
| `4` | Desert Eagle | Desert Eagle |
| `8` | Five-SeveN | Dual Berettas |

**Sub machine guns** — `mp_buy_allow_smgs_ct`, `mp_buy_allow_smgs_t`, highest value `15`

| Value | CT | T |
| --- | --- | --- |
| `1` | TMP | MAC-10 |
| `2` | MP5 Navy | MP5 Navy |
| `4` | UMP-45 | UMP-45 |
| `8` | P90 | P90 |

**Rifles** — `mp_buy_allow_rifles_ct`, `mp_buy_allow_rifles_t`, highest value `7`

| Value | CT | T |
| --- | --- | --- |
| `1` | FAMAS | Galil |
| `2` | M4A1 | AK-47 |
| `4` | AUG | SG552 |

**Shotguns** — `mp_buy_allow_shotguns_ct`, `mp_buy_allow_shotguns_t`, highest value `3`: `1` M3
Super 90, `2` XM1014, the same for both teams.

**Sniper rifles** — `mp_buy_allow_snipers_ct`, `mp_buy_allow_snipers_t`, highest value `7`

| Value | CT | T |
| --- | --- | --- |
| `1` | Scout | Scout |
| `2` | AWP | AWP |
| `4` | SG550 | G3SG1 |

**Machine guns** — `mp_buy_allow_machine_guns_ct`, `mp_buy_allow_machine_guns_t`, highest value `1`:
`1` M249, the same for both teams.

A refused purchase prints "This item cannot be bought on this server" in the centre of the screen
(suppressed for `autobuy`/`rebuy` feedback, like the other buy results) and returns `BUY_NOT_ALLOWED`.

**What it deliberately does not do.**

* It restricts **purchases only**. Armour can still arrive through `game_player_equip` or through
  feature 7's `mp_free_armor`, and a weapon picked up from the ground is not taken away.
* Defuse kits, nightvision and the tactical shield are not covered: none of them is a gun, a grenade
  or armour, and the shield has no bit in any group.
* Ammo (`primammo`/`secammo`) is not restricted: a player who already owns a weapon can still refill
  it, so a restriction never strands a player with an empty gun.
* The stock v34 buy menu still lists every item. The client cannot be told to hide entries without
  changing the client, so a restricted item is shown and simply refuses to buy. That is the same
  compromise as feature 3's buy-window rule and the only server-side option.

**Convars**

| Convar | Default | Values | Meaning |
| --- | --- | --- | --- |
| `mp_buy_allow_guns` | `1` | `0`-`3` | `0` nobody, `1` both teams, `2` Terrorists, `3` CT |
| `mp_max_armor_ct` | `2` | `0`-`2` | Highest armour CT may buy |
| `mp_max_armor_t` | `2` | `0`-`2` | Highest armour Terrorists may buy |
| `mp_buy_allow_grenades_ct` | `7` | `0`-`7` | Grenades CT may buy (bitfield above) |
| `mp_buy_allow_grenades_t` | `7` | `0`-`7` | Grenades Terrorists may buy (bitfield above) |
| `mp_buy_allow_pistols_ct` | `15` | `0`-`15` | Pistols CT may buy (bitfield above) |
| `mp_buy_allow_pistols_t` | `15` | `0`-`15` | Pistols Terrorists may buy (bitfield above) |
| `mp_buy_allow_smgs_ct` | `15` | `0`-`15` | Sub machine guns CT may buy (bitfield above) |
| `mp_buy_allow_smgs_t` | `15` | `0`-`15` | Sub machine guns Terrorists may buy (bitfield above) |
| `mp_buy_allow_rifles_ct` | `7` | `0`-`7` | Rifles CT may buy (bitfield above) |
| `mp_buy_allow_rifles_t` | `7` | `0`-`7` | Rifles Terrorists may buy (bitfield above) |
| `mp_buy_allow_shotguns_ct` | `3` | `0`-`3` | Shotguns CT may buy (bitfield above) |
| `mp_buy_allow_shotguns_t` | `3` | `0`-`3` | Shotguns Terrorists may buy (bitfield above) |
| `mp_buy_allow_snipers_ct` | `7` | `0`-`7` | Sniper rifles CT may buy (bitfield above) |
| `mp_buy_allow_snipers_t` | `7` | `0`-`7` | Sniper rifles Terrorists may buy (bitfield above) |
| `mp_buy_allow_machine_guns_ct` | `1` | `0`/`1` | Machine guns CT may buy (bitfield above) |
| `mp_buy_allow_machine_guns_t` | `1` | `0`/`1` | Machine guns Terrorists may buy (bitfield above) |

**Client compatibility.** Nothing about a purchase is client-authoritative and none of these convars is
replicated. The stock client sends its usual `buy` command, receives the usual `BuyResult` handling
(account and weapon state, or a centre-screen refusal), and needs no knowledge of the rules.

**Files:** `game_shared/cstrike/cs_gamerules.cpp` (convars), `dlls/cstrike/cs_player.cpp`
(`IsWeaponAllowedForTeam` and `IsBuyDisallowedByRules`, called from `HandleCommand_Buy`).

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


# 10. Random deathmatch spawns

**Origin:** CS:GO, `game/shared/cstrike15/cs_gamerules.cpp` (convar definitions, line 18693;
`MaxNumPlayersOnTerrTeam`/`MaxNumPlayersOnCTTeam`, line 13701;
`IsSpawnPointHiddenFromOtherPlayers`, line 15978), `game/server/cstrike15/cs_player.cpp`
(`CCSPlayer::SelectSpawnSpot`, line 11622, and the deathmatch branch of
`CCSPlayer::EntSelectSpawnPoint`, line 11852), `game/shared/cstrike15/bot/bot_util.cpp`
(`UTIL_IsVisibleToTeam`, line 385, and `UTIL_IsRandomSpawnFarEnoughAwayFromTeam`, line 414).

**Why:** stock Counter-Strike: Source always spawns a player at the next
`info_player_terrorist`/`info_player_counterterrorist` point in entity order, which is the classic
"spawn at the same corner every round" behaviour and the reason deathmatch and arena servers cannot
use the stock spawn logic. CS:GO instead spawns at `info_deathmatch_spawn` points, avoiding spots the
enemy team is looking at and spots an enemy is standing next to. This port brings that selection
logic over as three opt-in convars.

**What it does.** With `mp_randomspawn` set to `1` (both teams), `2` (Terrorists only) or `3`
(Counter-Terrorists only), `CCSPlayer::EntSelectSpawnPoint()` (`dlls/cstrike/cs_player.cpp`) takes the
CS:GO branch instead of the per-team branch:

* A start point is picked by walking `info_deathmatch_spawn` entities a random number of times
  (1-10), so spawns are spread over the map instead of always starting at the same point.
* `CCSPlayer::SelectSpawnSpot()` then scans the DM spawn points and accepts the first one that is
  `IsSpawnPointValid()` (clear of players), not at the world origin, and out of the enemy team's
  sight. "Out of sight" is `CCSGameRules::IsSpawnPointHiddenFromOtherPlayers()`: a
  `UTIL_TraceLine` from every living Counter-Terrorist's eye position to the spawn point 32 units
  above the floor must be blocked.
* `UTIL_IsRandomSpawnFarEnoughAwayFromTeam()` adds the distance test: with `mp_randomspawn_dist`
  above `0`, a spot is rejected while every living enemy is closer than that distance to it.
* If every DM spawn point failed the sight test but some were otherwise valid, the last valid one is
  used anyway (CS:GO's "valid but unfortunately visible" fallback) rather than failing the spawn.
* With `mp_randomspawn_los 0` the sight and distance tests are skipped entirely, so any valid DM
  spawn point is taken - the same shortcut CS:GO takes.
* If a map has no `info_deathmatch_spawn` points at all, the player falls back to the stock team
  spawn point, so enabling `mp_randomspawn` on a stock map is harmless.
* A map can opt out of random spawns with `usenormalspawnsfordm` `1` on its `info_map_parameters`
  entity, which CS:GO also supports.

**Where the spawn points come from.** Maps that were never built for deathmatch have no
`info_deathmatch_spawn` entities, so the port generates them from the nav mesh the same way CS:GO
does (`server/cstrike15/cs_nav_mesh.cpp:280`, `CSNavMesh::MaintainDMSpawnPopulation`). Once per second,
while `mp_randomspawn` is not `0`, `MaintainDMSpawnPopulation()` in `game_shared/cstrike/cs_gamerules.cpp`
(stepped from `CCSGameRules::Think()`) tries to fill the population up to 50 points:

* A random nav area is taken from the mesh (`GetNavAreaByID`, 1 to `GetNavAreaCount()`). Areas smaller
  than 100x100 units are skipped, because a spawn point needs room for a player hull.
* The point is the area centre raised 16 units, so the player drops onto the floor instead of starting
  inside it.
* It is rejected when another generated point is closer than 300 units, when a player hull with 32
  units of extra head room is already inside world geometry (`UTIL_TraceHull` with
  `MASK_PLAYERSOLID`), or when `NavAreaBuildPath` cannot reach the nearest
  `info_player_counterterrorist` and then `info_player_terrorist`: a spawn point in an orphaned part of
  the map is useless.
* Ten areas are tried per point and ten points per second. After 100 consecutive failed attempts the
  target is lowered to what the map could actually support and the search stops, exactly like CS:GO, so
  a small map does not search forever.
* Generated points are ordinary `info_deathmatch_spawn` entities, and they are in the round-restart
  preserve list, so they survive round restarts and only disappear with the map.
* `disableautogenerateddmspawns` `1` on `info_map_parameters` turns generation off for a map that wants
  only its own spawn points.

**Rebuilding on a running map.** The `dm_reset_spawns` server command deletes every spawn point the
generator created and builds a fresh set from the nav mesh that is loaded now, so an operator can pick
up a re-run `nav_generate`/`nav_edit` without reloading the map. It resets the population target to 50
first, in case the earlier search had shrunk it to what the map used to support, then runs one
generation step immediately and reports how many points were removed and how many are placed; the rest
arrive over the following seconds. It refuses to run when `mp_randomspawn` is `0` (there would be
nothing to place), when the map sets `disableautogenerateddmspawns`, and when no nav mesh is loaded, so
a mistyped command cannot empty a map's spawn points. Mapper-placed `info_deathmatch_spawn` entities are
never touched: only generated ones are tracked and removed.

`0` (default) is stock Counter-Strike: Source: the stock team spawn points in entity order.

**Team size cap.** Because the team size limit is normally derived from the number of spawn points
(`CCSGameRules::TeamFull()`), a map that only has DM spawn points would report both teams as full at
zero players and nobody could join. `TeamFull()` now asks `MaxNumPlayersOnTerrTeam()` /
`MaxNumPlayersOnCTTeam()`, which use `MAX_PLAYERS` for any team that spawns at random DM spawn
points and the spawn point count for the others, exactly like CS:GO.

**Details and limits.**

* CS:GO marks these convars `FCVAR_REPLICATED` because its client can draw deathmatch spawn debug
  overlays. Here they are server-only (`FCVAR_NOTIFY` on `mp_randomspawn`), so nothing is sent to a
  stock client.
* The sight test always checks visibility from the Counter-Terrorist team, for both teams, because
  that is what CS:GO's `info_deathmatch_spawn` path does.
* `mp_randomspawn_dist` only has an effect while `mp_randomspawn_los` is `1`, as in CS:GO; the
  distance test is skipped with `0`.
* `CCSGameRules::MaxNumPlayersOnTerrTeam()`/`MaxNumPlayersOnCTTeam()` do not apply CS:GO's
  `m_iMaxNumTerrorists`/`m_iMaxNumCTs` caps, because those members do not exist in this tree; the
  team size is capped by `MAX_PLAYERS` (or the spawn point count) only.
* `info_deathmatch_spawn` is registered as a server-only point entity, so a map can place as many as
  it likes. Mapper-placed and generated points are used together; a map with neither has no random
  spawns and falls back to its team spawn points.
* Generation needs a loaded nav mesh. On a map without one the population stays at zero and spawns
  fall back to the team spawn points, so nothing breaks.
* The generator runs in `Think()` rather than in a nav mesh subclass, because the nav mesh class in
  this tree is the shared HL2 one. The result is the same: one attempt per second while the
  population is short.
* `func_block_dm_spawns` (CS:GO's brush that excludes areas from generation) is not ported; the world
  geometry test already rejects points inside solid brushes.

**Convars**

| Convar | Default | Values | Meaning |
| --- | --- | --- | --- |
| `mp_randomspawn` | `0` | `0`-`3` | 0 = stock spawns, 1 = both teams, 2 = Terrorists, 3 = Counter-Terrorists |
| `mp_randomspawn_los` | `1` | `0`/`1` | Only accept DM spawn points the enemy team cannot see |
| `mp_randomspawn_dist` | `0` | `>= 0` | Minimum distance a living enemy must be from the spot, `0` disables the test |

**Command**

| Command | Arguments | Meaning |
| --- | --- | --- |
| `dm_reset_spawns` | none | Delete the generated spawn points and build a new set from the current nav mesh |

**Client compatibility.** Spawn selection never leaves the server. `info_deathmatch_spawn` is a
server-only `CPointEntity` (like the stock `info_player_terrorist`), it sends no netprops and is never
created by a stock map, and none of the three convars is replicated. The client only sees the
`player_spawn` event it already handles, at whatever origin the server chose.

**Files:** `game_shared/cstrike/cs_gamerules.cpp` (convars, `IsSpawnPointHiddenFromOtherPlayers`,
`UTIL_IsRandomSpawnFarEnoughAwayFromTeam`, `MaxNumPlayersOn*Team`, `TeamFull`),
`game_shared/cstrike/cs_gamerules.h` (declarations), `game_shared/cstrike/bot/bot_util.h`
(`UTIL_IsRandomSpawnFarEnoughAwayFromTeam` declaration, next to the existing `UTIL_IsVisibleToTeam`),
`dlls/cstrike/cs_player.cpp` (`EntSelectSpawnPoint`, `SelectSpawnSpot`), `dlls/cstrike/mapinfo.cpp`
and `mapinfo.h` (`usenormalspawnsfordm` and `disableautogenerateddmspawns` keys on
`info_map_parameters`), and the generator in `game_shared/cstrike/cs_gamerules.cpp`
(`MaintainDMSpawnPopulation`, `IsDMSpawnBlockedByWorld`, `IsDMSpawnAreaReachable`,
`ResetDMSpawnState`, `dm_reset_spawns`).


# 11. Deathmatch kill scoring

**Origin:** CS:GO, `game/shared/cstrike15/cs_gamerules.cpp`
(`mp_dm_kill_base_score`, line 2346; `CCSGameRules::ScorePlayerKill`, line 12052;
`CCSGameRules::GetWeaponScoreForDeathmatch`, line 18728) and
`game/server/cstrike15/cs_player.cpp` (`CCSPlayer::AddDeathmatchKillScore`, line 7632).

**Why:** stock Counter-Strike: Source gives every kill one frag, so a deathmatch server cannot reward
skill with the knife or with cheap weapons, and it cannot make a kill worth more than a frag. CS:GO
scores deathmatch kills by weapon: the base score, more for the knife, and a bonus for cheap guns
because they are harder to use.

**What it does.** With `mp_dm_scoring 1`, `CCSGameRules::PlayerKilled()` asks
`CCSGameRules::GetDeathmatchKillScore()` for the score of the killing weapon and adds it to the
killer's frag count, after the one frag the base rules already gave:

* knife: `mp_dm_kill_base_score` **+ 10**
* a weapon cheaper than 1400: `mp_dm_kill_base_score` **+ 2** (pistols, the cheaper SMGs and shotguns)
* a weapon cheaper than 3500: `mp_dm_kill_base_score` **+ 1** (rifles, the AWP, the machine gun)
* anything else: `mp_dm_kill_base_score`

With the default `mp_dm_kill_base_score 10` a rifle kill is therefore worth 11 points and a pistol
kill 12, a knife kill 20.

`0` (default) is stock Counter-Strike: Source: one frag per kill, whatever the weapon.

**Details and limits.**

* The score goes into the frag count (`m_iFrags`), not a separate score. It is the only per-player
  number a stock v34 scoreboard displays, so `mp_dm_scoring` inflates the score column by design. It
  also means a team kill still costs a frag, and the round logic in this tree never reads the frag
  count, so rounds are unaffected.
* A team kill is not scored: the award sits in the enemy-kill branch of `PlayerKilled()`, next to the
  `#Killed_Teammate` penalty.
* The weapon is the killer's active weapon, and weapons without a `WeaponPrice` in their script score a
  single point, which is CS:GO's fallback for a weapon it cannot look up.
* CS:GO prints an award line in chat (`#Player_Point_Award_Killed_Enemy`). Those tokens do not exist
  in a v34 client's string tables, so no message is printed; the score column shows the result.
* CS:GO's `gg_leader` event, its contribution score field and its team-deathmatch score are not
  ported, because all three are client-side or per-mode features this tree does not have.
* The bonus-weapon multiplier of CS:GO's scoring is not ported: `PickRandomWeaponForDMBonus()` returns
  an invalid slot in the reference, so the bonus is unreachable there too.

**Convars**

| Convar | Default | Values | Meaning |
| --- | --- | --- | --- |
| `mp_dm_scoring` | `0` | `0`/`1` | Award CS:GO deathmatch points for kills |
| `mp_dm_kill_base_score` | `10` | `>= 0` | Base points per kill, before the weapon bonuses |

**Client compatibility.** `m_iFrags` is a stock `CBasePlayer` netprop that the HUD and scoreboard
already render, so nothing new is sent. `mp_dm_kill_base_score` is `FCVAR_REPLICATED` in CS:GO because
its client shows the value in the deathmatch HUD; here it is server-only, like every other ported
convar.

**Files:** `game_shared/cstrike/cs_gamerules.cpp` (both convars, `GetDeathmatchKillScore`, the award in
`PlayerKilled`), `game_shared/cstrike/cs_gamerules.h` (declaration).

# 12. Hostage rescue rules

**Origin:** CS:GO `game/shared/cstrike15/cs_gamerules.cpp` (`mp_hostages_takedamage`, line 1891;
`mp_hostages_rescuetowin`, line 1897) and `game/server/cstrike15/hostage/cs_simple_hostage.cpp`
(`mp_hostages_run_speed_modifier`, line 2114; `mp_only_cts_rescue_hostages`, lines 1648 and 1721).

**Why:** a stock CS:S hostage round assumes a full five-versus-five match on a map built for it. On a
deathmatch or bot server it breaks in ways the map cannot fix: the CTs must escort every hostage home
while being shot at, a single sprayed burst can kill a hostage and make the round unwinnable, and a
hostage that is dragged around at full speed cannot be caught by the half of the server that is
trying to stop the rescue. Each of these is a separate rule in CS:GO, so each is a separate switch
here.

**What it does.**

* `mp_hostages_takedamage` — `1` (default) is stock: hostages take damage, the shooter hears the pain
  sound, the `hostage_hurt` event fires and he loses money for it. `0` makes a hostage invulnerable:
  `CHostage::OnTakeDamage_Alive()` returns before any of that, so a stray bullet cannot kill a
  hostage, cannot make the round unwinnable and cannot cost the shooter anything. CS:GO does the same
  in one line (`return 1;` in place of the base-class call), but it does it after the hostage has
  already been removed from the round's win condition, which this tree does not, so the guard sits at
  the top of the function.
* `mp_hostages_rescuetowin` — `0` (default) keeps the stock CS:S rule: the CTs win only when no
  surviving hostage is left unrescued (the stock 50% check stays inside it, so a hostage that was
  killed does not make the round unwinnable). Any other value is CS:GO's meaning: N rescues win the
  round and the remaining live hostages may be left where they are. CS:GO's arithmetic is used for
  that case — `min(hostages, N)` rescues are needed once at most `hostages - N` are still alive — and
  the round still ends with `All_Hostages_Rescued` and its `hostage_rescued_all` event, because the
  client knows those names and nothing else.
* `mp_hostages_run_speed_modifier` — scales the force a following hostage applies to itself, `1.0`
  (default) being exactly the old behaviour; CS:GO clamps it to `0.1`-`1.5`. Below 1 the hostage
  trails behind its leader and can be caught, which is what the CS:GO setting exists for.
* `mp_only_cts_rescue_hostages` — `1` (default) is stock: only a Counter-Terrorist may take a hostage,
  and a Terrorist using one gets `#Only_CT_Can_Move_Hostages`. `0` lets a Terrorist take one as well
  (CS:GO's rule for the same switch), which is useful on hostage maps played as team deathmatch. The
  hostage is still only *rescued* in a rescue zone, and the CT use bonus and the touch push-away are
  unchanged.

* `mp_hostages_max` — `0` (default) keeps every hostage the map placed, which is stock. A positive
  number keeps only that many. CS:S has no hostage spawn points, so unlike CS:GO the pool is the
  map's own `hostage_entity` list: `SelectMapHostages()` chooses the subset, removes the rest from
  `g_Hostages` and the world, and runs at level load and again at every round start, because
  `CCSGameRules::CleanUpMap()` recreates every map entity and would otherwise put the removed
  hostages back. `m_iHostagesRemaining` is taken from what is left after that call.
* `mp_hostages_spawn_farthest` — `1` keeps the subset that is spread out furthest: CS:GO's metric is
  the sum of the pairwise 2D squared distances of the chosen hostages, maximised over every
  combination (CS:GO `cs_gamerules.cpp:8958-9010`). That is a combinatorial search, so it only runs
  when the map has at most 16 hostage entities; above that a greedy farthest-first pass (start
  nearest the map's average position, then repeatedly take the entity furthest from everything kept)
  produces the same kind of spread without the cost. `0` (default) is CS:GO's other fallback, a
  random pick without replacement (CS:GO `cs_gamerules.cpp:9020-9030`); CS:S has no per-hostage spawn
  weight, so every map hostage weighs the same.
* `mp_hostages_spawn_same_every_round` — `1` (default) picks the subset once and keeps it for the
  whole match, which is CS:GO's default and this tree's previous behaviour. `0` re-picks it at every
  round start, so a `mp_hostages_max` smaller than the map's hostage count gives a different set of
  hostages each round.
* `mp_hostages_spawn_force_positions` — a comma separated list of zero based indices into the map's
  hostage list (`0,2` or `1,6`). When set it wins over `mp_hostages_max` and the two pickers and
  keeps exactly the listed hostages; out of range indices are logged and skipped. CS:GO's string is
  used the same way at `cs_gamerules.cpp:8935`. Empty (default) means no forcing.

**Fixed with this port.** The earlier `PruneMapHostages()` ran only at level load, but
`CCSGameRules::CleanUpMap()` recreates all map entities at every round start, so the removed hostages
came back at the first round restart and `mp_hostages_max` only ever held for the opening round. The
selection now runs again straight after `CleanUpMap()`, which makes the limit hold for the whole
match.

**Convars**

| Convar | Default | Values | Meaning |
| --- | --- | --- | --- |
| `mp_hostages_max` | `0` | `0` or N | Keep only N of the map's hostages (`0` = all, stock) |
| `mp_hostages_spawn_farthest` | `0` | `0`/`1` | When limiting, keep the most spread-out set (`0` = random set) |
| `mp_hostages_spawn_same_every_round` | `1` | `0`/`1` | Same hostage set for the match (`0` = new set every round) |
| `mp_hostages_spawn_force_positions` | *(empty)* | index list | Force these hostage indices, e.g. `0,2` |
| `mp_hostages_takedamage` | `1` | `0`/`1` | Hostages can be hurt (stock: yes) |
| `mp_hostages_rescuetowin` | `0` | `0` or N | Rescues needed to win; `0` = every surviving hostage (stock) |
| `mp_hostages_run_speed_modifier` | `1.0` | `0.1`-`1.5` | Movement force scale for a following hostage (stock: `1.0`) |
| `mp_only_cts_rescue_hostages` | `1` | `0`/`1` | Only CTs may take a hostage (stock: yes) |

**Client compatibility.** All of these are server-side rules; nothing is networked. The hostage's own
speed is already an engine-side force, the rescue check runs in the game rules, and
`#Only_CT_Can_Move_Hostages` is an existing string in a v34 client's string tables, so the refusal
still appears when the switch is on.

**Files:** `game_shared/cstrike/cs_gamerules.cpp` (all eight convars,
`HostageRescueRoundEndCheck`, `SelectMapHostages`, `SelectFarthestHostages`, `SelectRandomHostages`,
`ApplyHostageSpawnSelection`), `dlls/cstrike/hostage/cs_simple_hostage.cpp` (`OnTakeDamage_Alive`,
`HostageUse`, `TrackPath`).

# 13. C4 rules

**Origin:** CS:GO `game/shared/cstrike15/weapon_c4.cpp` (`mp_plant_c4_anywhere`, line 57;
`mp_anyone_can_pickup_c4` and `mp_c4_cannot_be_defused` uses at lines 175, 495 and 1015) and
`game/shared/cstrike15/cs_gamerules.cpp` (the two convar definitions, lines 1909 and 1915).

**Why:** the bomb is the whole objective on a de_ map, and on a server that plays those maps as
deathmatch or as a warm-up the stock rules get in the way: the bomb is dead weight to a CT, a single
CT can undo a plant, and a plant is impossible outside the two painted zones. Each of those is a
CS:GO switch, so each is one here, and all three default to stock.

**What it does.**

* `mp_anyone_can_pickup_c4` — `0` (default) is stock: `CCSPlayer::Weapon_CanUse()` refuses `weapon_c4`
  for anybody but a Terrorist. `1` lets any player pick the dropped bomb up, which is CS:GO's
  exception at `weapon_c4.cpp:175`. Nothing else changes: the bomb is still given to a Terrorist at
  round start, and the CT who picks it up gets an ordinary weapon slot that he loses when the round
  restarts.
* `mp_c4_cannot_be_defused` — `0` (default) is stock. `1` refuses every defuse attempt in
  `CPlantedC4::Use()` before the progress bar, the `bomb_begindefuse` event and the freeze in place,
  so once the bomb is down the round can only end in the explosion. CS:GO checks the same switch in
  `CanDefuse()` (`weapon_c4.cpp:1015`). A CT pressing USE gets no message, exactly as in CS:GO: the
  bar simply never appears.
* `mp_plant_c4_anywhere` — `0` (default) is stock, planting inside a `func_bomb_target` only. `1`
  replaces every `m_bInBombZone` test in `CC4::PrimaryAttack()` with "in a bomb zone *or* the switch
  is on" (CS:GO `weapon_c4.cpp:1666`), so the plant can start and complete anywhere the player is
  standing on solid ground. The two stock refusals stay: standing on a player, a pushable prop or a
  breakable still aborts the arm, and `#C4_Plant_Must_Be_On_Ground` is still printed. The plant event
  carries whatever bomb site index the player last stood in, which is `-1` outside a zone.

**Convars**

| Convar | Default | Values | Meaning |
| --- | --- | --- | --- |
| `mp_anyone_can_pickup_c4` | `0` | `0`/`1` | Any team may pick the C4 up (`0` = Terrorists only, stock) |
| `mp_c4_cannot_be_defused` | `0` | `0`/`1` | A planted C4 cannot be defused by anyone (`0` = stock) |
| `mp_plant_c4_anywhere` | `0` | `0`/`1` | The C4 may be planted outside a bomb target (`0` = stock) |

**Client compatibility.** All three are server-side rules. The bomb's own networked state
(`m_flC4Blow`, `m_flDefuseLength`, `m_flDefuseCountDown`) is untouched, so the planted-bomb HUD and
the beeping stay correct; with `mp_c4_cannot_be_defused` the defuse countdown is simply never
started, and with `mp_plant_c4_anywhere` the plant animation, beeps and `bomb_planted` event all run
from the same code that runs inside a zone.

**Files:** `game_shared/cstrike/cs_gamerules.cpp` (all three convars),
`game_shared/cstrike/weapon_c4.cpp` (`CC4::PrimaryAttack`, `CPlantedC4::Use`),
`dlls/cstrike/cs_player.cpp` (`Weapon_CanUse`).


# 14. Disconnect kills

**Origin:** CS:GO `game/shared/cstrike15/cs_gamerules.cpp` — convar definitions at lines 1957 and
1963, use in `CCSGameRules::ClientDisconnected()` at lines 6937-6952.

**Why:** a player who disconnects while alive is removed from the world without dying: nothing drops,
the kill feed and the stats never see the death, and a round can be left with a player who simply
evaporated. CS:GO kills the player first so every system sees an ordinary death, and two switches
control whether that happens for players and for bots.

**What it does.**

* `mp_disconnect_kills_players` — `0` (default) is stock: `CCSGameRules::ClientDisconnected()` calls
  the base class and the player is removed without a death. `1` kills a living player before the base
  class runs, with the same self-inflicted, zero damage, `DMG_NEVERGIB` hit CS:GO builds
  (`CTakeDamageInfo info( pPlayer, pPlayer, 0.0f, DMG_NEVERGIB )`), so `CCSPlayer::Event_Killed()`
  runs the ordinary death path: weapons drop under the `mp_death_drop_*` rules, the ragdoll and the
  kill feed record the death, and a pending `mp_respawn_on_death_*` respawn is discarded together with
  the client.
* `mp_disconnect_kills_bots` — `0` (default) is stock: bots are never killed for this. `1` applies the
  same kill to bots and only has an effect while `mp_disconnect_kills_players` is on, exactly as
  CS:GO documents it.

CS:GO defaults to `1` and `0`; both default to `0` here because stock Counter-Strike: Source does not
kill on disconnect (rule 3). A server that wants CS:GO's behaviour sets
`mp_disconnect_kills_players 1`.

**Convars**

| Convar | Default | Values | Meaning |
| --- | --- | --- | --- |
| `mp_disconnect_kills_players` | `0` | `0`/`1` | Kill a living player when he disconnects (`0` = stock removal) |
| `mp_disconnect_kills_bots` | `0` | `0`/`1` | Kill a bot when it disconnects; needs `mp_disconnect_kills_players` |

**Client compatibility.** Server-side only: the death is an ordinary one, so the client's existing
death camera, ragdoll, kill feed and scoreboard need no new message.

**Files:** `game_shared/cstrike/cs_gamerules.cpp` (both convars,
`CCSGameRules::ClientDisconnected`).


# 15. Knife drop

**Origin:** CS:GO `game/server/cstrike15/cs_player.cpp` (convar at line 214, use in the `drop`
command at line 14451) and `game/shared/cstrike15/weapon_knife.cpp` (`CKnife::CanDrop()` at line 640
always returns false).

**Why:** in stock CS:S pressing the drop key with the knife in hand does nothing, and there is no
switch to change that. CS:GO keeps the same refusal in the weapon class but lets the `drop` command
bypass it while a server switch is on.

**What it does.** `mp_drop_knife_enable 0` (default) is stock: the `drop` command ignores a knife.
`1` lets the command call `CSWeaponDrop()` on it, so the knife becomes an ordinary world weapon with
the usual throw velocity and can be picked up again. `CKnife::CanDrop()` stays `false` in both cases,
so no other path can drop a knife — the command is the one exception, as in CS:GO. The player is left
with his pistol and primary until he picks the knife back up, which is also CS:GO's behaviour.

**Convars**

| Convar | Default | Values | Meaning |
| --- | --- | --- | --- |
| `mp_drop_knife_enable` | `0` | `0`/`1` | Allow the `drop` command to drop a knife (`0` = stock refusal) |

**Client compatibility.** Server-side: the knife already has a world model and the ordinary weapon
pickup path handles a dropped one; nothing is networked.

**Files:** `dlls/cstrike/cs_player.cpp` (convar and the `drop` branch of `ClientCommand`).


# 16. Weapon pickup timing

**Origin:** CS:GO `game/shared/cstrike15/weapon_csbase.cpp` — convar definitions at lines 1752-1753
and the delays set in `CWeaponCSBase::Drop()` at lines 1799-1801.

**Why:** CS:S hardcodes the 0.8 seconds during which a player cannot take back the weapon he just
dropped, and has no window at all in which a freshly dropped weapon is untouchable for everybody.
CS:GO exposes both, because a weapon picked up in the same frame it was thrown can leave the client
with a stale world model.

**What it does.**

* `mp_weapon_prev_owner_touch_time` — default `0.8`, the exact constant the code used in
  `CWeaponCSBase::Drop()`, so stock behaviour is unchanged. It is read when a weapon is dropped and
  checked in `DefaultTouch()`; the same value still gates `IsRemoveable()`, so the world weapon
  manager does not clean up a weapon inside the window. CS:GO's default is `1.5`.
* `mp_weapon_next_owner_touch_time` — default `0`, stock: anybody may pick a just-dropped weapon up
  immediately. Above zero, nobody may pick it up for that many seconds, which is the delay CS:GO
  hardcodes as `0.1` in `CWeaponCSBase::Drop()` and exposes as a convar (default `1.3`, used for its
  battle royale item drops). `0` stays the default here because that is stock CS:S.

**Convars**

| Convar | Default | Values | Meaning |
| --- | --- | --- | --- |
| `mp_weapon_prev_owner_touch_time` | `0.8` | seconds, `>= 0` | Delay before the previous owner may pick the weapon up again |
| `mp_weapon_next_owner_touch_time` | `0` | seconds, `>= 0` | Delay before anybody may pick a just-dropped weapon up |

**Client compatibility.** Server-side: both values are read only on the server's drop and touch path,
and a dropped weapon is still an ordinary networked weapon.

**Files:** `game_shared/cstrike/weapon_csbase.cpp` (both convars, `CWeaponCSBase::Drop`,
`CWeaponCSBase::DefaultTouch`), `game_shared/cstrike/weapon_csbase.h` (`m_nextOwnerTouchTime`).


# 17. Grenade drop

**Origin:** CS:GO `game/server/cstrike15/cs_player.cpp` — convar at line 216, use in the `drop` command
at lines 14388-14410.

**Why:** in stock CS:S the drop key refuses a grenade in hand, so a player carrying a spare flashbang
cannot hand it to a teammate. CS:GO has a switch for it, and its `drop` command builds a fresh grenade
entity, takes one round out of the player's stack and throws that.

**What it does.** `mp_drop_grenade_enable 0` (default) is stock: the `drop` command ignores grenades.
`1` drops the grenade the player is holding with the same `CSWeaponDrop()` call the death drop already
uses for `mp_death_drop_grenade`, so the grenade becomes an ordinary world weapon that anybody may
pick up. CS:S keeps one grenade per weapon entity with one round in it, so there is no stack to
decrement: the whole grenade is thrown, which is the honest CS:S equivalent of CS:GO's "throw one out
of the stack". The player is left holding whatever else he owns.

**Convars**

| Convar | Default | Values | Meaning |
| --- | --- | --- | --- |
| `mp_drop_grenade_enable` | `0` | `0`/`1` | Allow the `drop` command to drop a grenade (`0` = stock refusal) |

**Client compatibility.** Server-side: a dropped grenade is a stock networked weapon with a world
model, and the ordinary pickup path (which the death drop already feeds) handles it. Nothing new is
sent.

**Files:** `dlls/cstrike/cs_player.cpp` (convar and the `drop` branch of `ClientCommand`).
