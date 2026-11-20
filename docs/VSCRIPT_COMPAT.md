# VScript compatibility

This file tracks what the server-side VScript subsystem in `src/game/server/vscript/` can run,
compared with the VScript surface documented for current Steam CS:S. A symbol is only "implemented"
when code exists for it, and a gap states the observable difference instead of being quietly
dropped.

The subsystem lives on the `vscript` branch. `main` stays stock CS:S: no Squirrel in the
tree, no VScript convars, no script hooks.

## Runtime model

* One Squirrel VM per map, created in `LevelInitPreEntity` only when `sv_vscript_enable` is `1`,
  destroyed in `LevelShutdownPreEntity`.
* Script files are read through `filesystem->ReadFile` from `scripts/vscripts/`, which also finds
  files packed inside a map's BSP pakfile. `.nut` is appended when the caller omits an extension.
* Scopes: one global (root table) and one per entity that carries scripts. A scope table delegates
  to the global table, so global functions are visible from entity scopes.
* Entity scripts arrive from any entity's `vscripts` keyvalue (not only `logic_script`), which is
  how community maps attach theirs.
* Entity references are `CBaseEntity` instances wrapping a handle-table entry over an `EHANDLE`; a
  destroyed entity makes `IsValid()` false and method calls fail with a logged script error rather
  than touching freed memory.
* The VM never adds replicated state: bindings may only read or write fields the v34 send tables
  already declare, and may only spawn entity classes compiled into this DLL.

## Implemented — global functions

| Symbol | Notes |
| --- | --- |
| `print`, `printl`, `Msg`, `Warning` | server console only; values are converted to text |
| `Time`, `FrameTime` | `gpGlobals->curtime` / `frametime` |
| `RandomInt`, `RandomFloat` | |
| `Vector(x, y, z)` | returns a `Vector` instance with `x`, `y`, `z` and `+ - * /` operators |
| `GetMapName` | |
| `IncludeScript(file)`, `DoIncludeScript(file)` | run in the caller's scope; see gaps for the scope argument |
| `PrecacheModel`, `PrecacheSound` | server resources only, same timing rules as map precache |
| `EntFire(target, input, value, delay, activator)` | delay 0 is deferred to the end of the frame |
| `EntFireByHandle(entity, input, value, delay, activator)` | |
| `AddThinkToEnt(entity, function)` | the function's return value is the delay to the next call |
| `ListenToGameEvent(event, function, context)`, `StopListeningToGameEvent(handle)` | events v34 actually fires |
| `__CollectGameEventCallbacks()` | registers every `OnGameEvent_<name>` in the calling scope |

## Implemented — `Entities` table

| Symbol | Notes |
| --- | --- |
| `Entities.FindByName(prev, name)` | |
| `Entities.FindByClassname(prev, classname)` | |
| `Entities.FindByNameWithin(prev, name, origin, radius)` | |
| `Entities.FindByClassnameWithin(prev, classname, origin, radius)` | |
| `Entities.FindInSphere(prev, origin, radius)` | |
| `Entities.First()`, `Entities.Next(entity)` | ascending entindex |
| `Entities.CreateByClassname(classname)` | classes compiled into the DLL only; call `DispatchSpawn` through `EntFire` or the entity's own inputs |

## Implemented — `Convars`

`Convars.GetStr`, `GetFloat`, `GetInt`, and `SetValue` (writes require `sv_vscript_allow_convars 1`).

## Implemented — `NetProps`

`GetPropInt` / `SetPropInt`, `GetPropFloat` / `SetPropFloat`, `GetPropBool` / `SetPropBool`,
`GetPropVector` / `SetPropVector`, `GetPropString` / `SetPropString`, `GetPropEntity` /
`SetPropEntity`. Names are resolved by walking the entity's server send table; a missing name or a
type mismatch raises a script error that names the entity class, and nothing is written.

## Implemented — `CBaseEntity` methods

`IsValid`, `GetName`, `SetName`, `GetClassname`, `entindex`, `GetOrigin`, `SetOrigin`, `GetAngles`,
`SetAngles`, `GetForwardVector`, `GetHealth`, `SetHealth`, `GetMaxHealth`, `SetMaxHealth`,
`GetVelocity`, `SetVelocity`, `GetTeamNumber`, `SetTeam` (players), `IsAlive`, `IsPlayer`,
`GetModelName`, `SetModel`, `Kill`, `TakeDamage(amount, attacker)`, `ValidateScriptScope`,
`GetScriptScope`, `GetEyePosition`, `GetPlayerName`.

## Implemented — hooks C++ calls into scripts

Global scope: `OnMapSpawn()`, `OnPlayerSpawn(player)`, `OnPlayerKilled(victim, attacker, weapon)`,
`OnRoundStart()`, `OnRoundEnd()`, `OnGameEvent_<name>(event)`.

Entity scope: `Precache()`, `OnPostSpawn()`, `ConnectOutputs()`, think functions, and any function a
`logic_script` input calls.

## Entity integration

| Symbol | Notes |
| --- | --- |
| `vscripts` keyvalue on any entity | space-separated `.nut` list, loaded at spawn |
| `logic_script` | `Group` keyvalue accepted and ignored; inputs `RunScriptCode`, `RunScriptFile`, `CallScriptFunction` |
| `script_execute <file>` | `FCVAR_CHEAT`, global scope |
| `script_reload` | re-runs every map-shipped script |
| `script_help [filter]` | lists bindings and entity methods |

## Gaps

### Impossible with a 2007 engine and a stock v34 client

* Client-side VScript: client `script_*` commands, script HUDs/panels, client entity scopes,
  material proxies. The v34 client has no script code and the protocol carries none.
* New replicated state for scripts: scripts can only touch sendprops v34 already declares.
* New entity classes or client assets that exist only server-side.
* `script_debug`, the remote debugger and profiler integration.
* Async or threaded APIs, filesystem writes and network access from scripts.
* Valve's `ScriptClassDesc_t` ABI and functor-based binding templates (this is C++98 with no
  variadic templates; bindings are hand-registered native functions).

### Not implemented yet

| Symbol | Status |
| --- | --- |
| `Input<Name>()` entity input hooks | not implemented; needs interception in `CBaseEntity::AcceptInput`, which several classes override without chaining |
| `OnTakeDamage` entity callback | not implemented; `OnTakeDamage` is virtual and overridden in many classes, so the only reliable hook is a per-class one |
| `DoIncludeScript(file, scope)` | the second argument is ignored; the file always runs in the calling scope |
| `SpawnEntityFromTable`, `__KeyValueFromString` | not implemented |
| `TraceLine`, `TraceHull`, `IntersectRayWithBox` | not implemented |
| `EmitSound`, `ClientPrint`, `ScreenShake`, particle effects | not implemented |
| `NetProps` array accessors (`GetPropIntArray`, ...) | not implemented; scalar props only |
| `SetContext` / `GetContext` | not implemented |
| Save/restore | script state is not serialised; scopes and handles are dropped, and a restored map starts with no scripts until it is re-entered |
| Preemptive timeouts | the VM is single-threaded: a runaway loop stalls the server frame. `sv_vscript_warn_ms` reports slow calls but cannot interrupt them |
| `SendGlobalGameEvent` | not implemented |
| Script-defined classes as entity types | impossible; `Entities.CreateByClassname` only resolves classes compiled into the DLL |

### Behaviour differences worth knowing

* `Vector` is a Squirrel class with `x`/`y`/`z` slots and `+ - * /` operators; it is not a native
  type, so `typeof` reports `"Vector"` through `_typeof` but Squirrel's own type system still sees
  an instance.
* `GetAngles` returns pitch/yaw/roll packed into a `Vector`, not a `QAngle` type.
* Event payloads are built from a fixed list of keys CS:S events use (`userid`, `attacker`,
  `victim`, `weapon`, `headshot`, `health`, `team`, ...) because the v34 `IGameEvent` interface has
  no key enumeration. A key outside that list is absent from the table.
* A script error is logged as `[VScript] <source>:<line>: <message>` and ends the current call; the
  VM and the entity stay alive.
