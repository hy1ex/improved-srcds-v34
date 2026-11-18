# Improved srcds v34

Open-source dedicated server for **Counter-Strike: Source, build v34** — the
Source SDK Base 2006 / Source Engine 2007 `server_i486.so`, rebuilt and
maintained for the current game build: hardened against the exploits that have
accumulated over the years, fixed for high-tickrate play, and extended with the
server-side game features players and operators expect from newer builds.

The whole fix set is in the code. There is no "stock behavior" build and no
runtime toggle: every improvement described below is what this binary does.

## Contents

* [Requirements](#requirements)
* [Installing](#installing)
* [What is in this build](#what-is-in-this-build)
* [Documentation](#documentation)
  * [Feature ports — `docs/FEATURES.md`](#feature-ports--docsfeaturesmd)
  * [Security and crash fixes — `docs/FIXES.md`](#security-and-crash-fixes--docsfixesmd)
  * [Landed upstream backports — `docs/BACKPORT-REPORT.md`](#landed-upstream-backports--docsbackport-reportmd)
* [Core console variables](#core-console-variables)
* [Building](#building)
* [Repository layout](#repository-layout)
* [License](#license)

## Requirements

* A **Counter-Strike: Source v34** dedicated server installation. The engine is
  expected to provide `tier0_i486.so`, `vstdlib_i486.so` and the usual VGUI /
  filesystem libraries next to the game DLL.
* **32-bit i386** Linux. `server_i486.so` is an `ELF 32-bit LSB shared object`
  and cannot be loaded by a 64-bit engine build (the loader rejects the ELF
  class, and even a matching class would need an ABI-identical engine and
  `tier0`/`tier1`/`vstdlib`).
* A glibc at least as new as the one it was linked against (2.3.6). The
  `libstdc++`/`libgcc_eh` code is linked statically, so no C++ runtime package
  is needed on the host.

## Installing

1. Build the binary ([Building](#building)) or take `server_i486.so` from a
   [release](../../releases) — each release ships `server_i486.so`,
   `SHA256SUMS.txt` and `build-info.txt`.
2. Verify the checksum and the file type:

   ```bash
   sha256sum -c SHA256SUMS.txt
   file server_i486.so
   # server_i486.so: ELF 32-bit LSB shared object, Intel 80386, ...
   ```

3. Replace the game DLL in the server's game directory — the file the engine
   loads as the server module, normally `cstrike/bin/server_i486.so`. Keep a
   copy of the original so the change can be reverted.
4. Restart the server. It should reach `Console initialized.` and load a map
   exactly as before; if the DLL does not load, the engine logs the failing
   module and the missing symbol name.

The binary carries debug info and is not stripped, so a crash still produces
useful backtraces.

## What is in this build

* **100-tickrate physics** — the physics environment simulates at the server's
  real tick interval instead of the stock hardwired 15 ms, so movement,
  prediction and lag compensation stay correct at `sv_maxupdaterate 100`.
* **Anti-exploit input hardening** — client usercommands are validated
  (NaN/INF view angles, capped move magnitude), replayed tick bursts are
  budgeted (`sv_maxusrcmdprocessticks`), idle or silent clients fall back to a
  null command instead of stalling the server, and the server generates its own
  usercmd random seed.
* **Crash fixes** — stale game-UI handles, null gamerules on damage, missing
  studio headers, spectator loops and other paths that let a client or a map
  change take the whole server down.
* **Security** — debug and report commands are restricted to the server
  console, sprays are validated before they are broadcast, and the server-side
  protections of [ProtectCmds](https://github.com/PowerTakeDev/ProtectCmds)
  are ported into the source (the engine-level ones still need the extension).
* **Upstream backports** — fixes found by diffing this tree against
  source-sdk-2013, css-community and nillerusr 2017 are ported function by
  function, without touching the network protocol. See
  [`docs/BACKPORT-REPORT.md`](docs/BACKPORT-REPORT.md).
* **Game features** — 48 ports from CS:GO and later CS:S builds: respawn and
  deathmatch modes, economy and purchase control, damage scaling, match flow,
  team auto-balance and more. See [`docs/FEATURES.md`](docs/FEATURES.md).
* **SSE math** — built with `-O3 -msse3 -mfpmath=sse` (`-ffast-math`), as the
  original SDK makefile intends.

## Documentation

Three documents accompany the code. They are the reference for every change in
this tree; the summaries below are only an entry point.

| Document | Contents |
|---|---|
| [`docs/FEATURES.md`](docs/FEATURES.md) | 48 feature ports, 157 console variables, 1 console command — each with its origin in CS:GO / later CS:S, the reasoning, the code and the files touched |
| [`docs/FIXES.md`](docs/FIXES.md) | 14 security and crash fixes with the stock code, the fixed code and the exploit each one closes |
| [`docs/BACKPORT-REPORT.md`](docs/BACKPORT-REPORT.md) | 82 landed upstream backports, oldest first, each mapped to its commit |

### Feature ports — [`docs/FEATURES.md`](docs/FEATURES.md)

Everything ported from CS:GO or from later Counter-Strike: Source builds, one
section per feature with its convar index and its source. Nothing here changes
the network protocol, so stock clients keep working.

| | | |
|---|---|---|
| [1. Death drop control](docs/FEATURES.md#1-death-drop-control) | [2. Defuse kit allocation](docs/FEATURES.md#2-defuse-kit-allocation) | [3. Respawn on death](docs/FEATURES.md#3-respawn-on-death) |
| [4. Player, hostage and grenade solidity](docs/FEATURES.md#4-player-hostage-and-grenade-solidity) | [5. Economy control](docs/FEATURES.md#5-economy-control) | [6. Damage scaling](docs/FEATURES.md#6-damage-scaling) |
| [7. Free armour](docs/FEATURES.md#7-free-armour) | [8. Purchase restrictions](docs/FEATURES.md#8-purchase-restrictions) | [9. Halftime](docs/FEATURES.md#9-halftime) |
| [10. Random deathmatch spawns](docs/FEATURES.md#10-random-deathmatch-spawns) | [11. Deathmatch kill scoring](docs/FEATURES.md#11-deathmatch-kill-scoring) | [12. Hostage rescue rules](docs/FEATURES.md#12-hostage-rescue-rules) |
| [13. C4 rules](docs/FEATURES.md#13-c4-rules) | [14. Disconnect kills](docs/FEATURES.md#14-disconnect-kills) | [15. Knife drop](docs/FEATURES.md#15-knife-drop) |
| [16. Weapon pickup timing](docs/FEATURES.md#16-weapon-pickup-timing) | [17. Grenade drop](docs/FEATURES.md#17-grenade-drop) | [18. Tagging scale](docs/FEATURES.md#18-tagging-scale-adapted) |
| [19. Friendly fire damage reduction](docs/FEATURES.md#19-friendly-fire-damage-reduction) | [20. Cash awards](docs/FEATURES.md#20-cash-awards) | [21. C4 handout](docs/FEATURES.md#21-c4-handout) |
| [22. Recoil and accuracy](docs/FEATURES.md#22-recoil-and-accuracy) | [23. +USE weapon pickup](docs/FEATURES.md#23-use-weapon-pickup-adapted) | [24. Grenade carry limits](docs/FEATURES.md#24-grenade-carry-limits) |
| [25. Round and hostage rescue time](docs/FEATURES.md#25-round-time-per-objective-type-and-hostage-rescue-time) | [26. HE grenade scaling](docs/FEATURES.md#26-he-grenade-scaling) | [27. Fall damage](docs/FEATURES.md#27-fall-damage) |
| [28. Per-match gun purchase limit](docs/FEATURES.md#28-per-match-gun-purchase-limit) | [29. Economy and streak controls](docs/FEATURES.md#29-economy-and-streak-controls) | [30. Map-placed weapons](docs/FEATURES.md#30-map-placed-weapons) |
| [31. Team assignment](docs/FEATURES.md#31-team-assignment) | [32. Aim punch scale](docs/FEATURES.md#32-aim-punch-scale) | [33. Bullet hit log](docs/FEATURES.md#33-bullet-hit-log-adapted) |
| [34. Suicide delay](docs/FEATURES.md#34-suicide-delay) | [35. Self-inflicted miss damage](docs/FEATURES.md#35-self-inflicted-miss-damage) | [36. Friendly fire bullet penetration](docs/FEATURES.md#36-friendly-fire-bullet-penetration) |
| [37. Money logging](docs/FEATURES.md#37-money-logging) | [38. Spectator cap](docs/FEATURES.md#38-spectator-cap) | [39. Automatic team pick](docs/FEATURES.md#39-automatic-team-pick) |
| [40. Default loadouts](docs/FEATURES.md#40-default-loadouts) | [41. Deathmatch kill healing](docs/FEATURES.md#41-deathmatch-kill-healing) | [42. Shorthanded win bonus](docs/FEATURES.md#42-shorthanded-win-bonus) |
| [43. Suicide penalty](docs/FEATURES.md#43-suicide-penalty) | [44. Match-end behaviour](docs/FEATURES.md#44-match-end-behaviour) | [45. Early match clinch](docs/FEATURES.md#45-early-match-clinch) |
| [46. Auto-balance delay and warning](docs/FEATURES.md#46-auto-balance-delay-and-warning) | [47. Turbo-physics +USE gates](docs/FEATURES.md#47-turbo-physics-use-gates) | [48. Spray validation and decal gate](docs/FEATURES.md#48-spray-validation-and-decal-gate) |

The same file starts with a **convar index** (every convar, its default and the
feature it belongs to) and a **command index**, so a setting can be looked up
without knowing which feature introduced it.

### Security and crash fixes — [`docs/FIXES.md`](docs/FIXES.md)

Fourteen fixes, each documented with the vulnerable code, the patched code and
the failure it prevents:

1. `CGameUI::Deactivate` — stale player handle crash
2. `Studio_BoneIndexByName` — null studio header crash
3. `CBaseEntity::TakeDamage` — null gamerules crash
4. `CBasePlayer::FindNextObserverTarget` — spectator infinite loop
5. `spec_goto` — unvalidated spectator teleport
6. Client usercmd value validation (anti-exploit)
7. Aim lock across replayed ticks (`sv_maxusrcmdprocessticks_holdaim`)
8. Server-generated usercmd random seed
9. Usercmd queue rework — packet loss, flood and idle protection
10. `se_shootpositionfix` — shoot position correction (pre-move eye latch)
11. Physics follows the server tick interval (100-tick support)
12. CS: player physics box toggle
13. Chat flood fix (`say`)
14. Server-console-only debug/report commands

The document ends with the full list of commands that fix 14 moves behind the
server console, with the source file and line for each.

### Landed upstream backports — [`docs/BACKPORT-REPORT.md`](docs/BACKPORT-REPORT.md)

82 backports from source-sdk-2013, css-community and nillerusr 2017, each in
its own commit with the diff, the upstream reference and a compatibility
rating. The bulk are null-guards, bounds checks and stale-state fixes in the
physics, animation, entity-I/O and save/restore paths, plus lag-compensation
corrections (pose parameters restored around a backtrack, `cl_interp` honoured,
horizontal-only teleport guard).

## Core console variables

The convars below belong to the netcode and physics work in this tree; the
~150 feature convars live in the [convar index](docs/FEATURES.md#convar-index).

**Netcode / movement**

* `sv_maxusrcmdprocessticks <16>` — Maximum number of client-issued usrcmd ticks that can be replayed in packet loss conditions, 0 to allow no restrictions
* `sv_maxusrcmdprocessticks_warning <-1>` — Print a warning when user commands get dropped due to insufficient usrcmd ticks allocated, number of seconds to throttle, negative disabled
* `sv_maxusrcmdprocessticks_holdaim <1>` — Hold client aim for multiple server sim ticks when client-issued usrcmd contains multiple actions (0: off; 1: hold this server tick; 2+: hold multiple ticks)
* `sv_player_usercommand_timeout <3>` — After this many seconds without a usercommand from a player, the server will RunNullCommand as if client sends an empty command
* `sv_usercmd_custom_random_seed <1>` — When enabled server will populate an additional random seed independent of the client
* `sv_max_usercmd_move_magnitude <1000>` — Maximum move magnitude that can be requested by client
* `sv_enablebunnyhopping <0>` — Allow player speed to exceed maximum running speed
* `se_shootpositionfix <0>` — Fire from the eye position the player aimed from (start of the command) instead of the post-movement one. Off by default; enable it with `se_shootpositionfix 1` in `server.cfg`. The default has to stay `0`: a replicated convar left at its default is not sent to connecting clients, so ClientMod clients would keep their own `0` and predict hits without the fix.

**Gameplay**

* `cs_enable_player_physics_box <1>` — Allow to create physical objects for players
* `mp_ignore_round_win_conditions <0>` — Ignore conditions which would end the current round
* `mp_round_restart_delay <5.0>` — Number of seconds to delay before restarting a round after a win

## Building

The build needs a **GCC 3.4.6-or-older** toolchain (the code includes
pre-C++11 headers such as `<new.h>`) and must produce a **32-bit i386** binary
to match the game's `tier0_i486.so` / `vstdlib_i486.so`. There are four ways
to build `src/linux_sdk/server_i486.so`:

| Method | Best for |
|---|---|
| [Docker](#building-with-docker) | Any machine, no toolchain setup |
| [Local chroot, no Docker/root](#building-locally-without-docker-or-root) | Cloud runners and locked-down machines |
| [GitHub Actions](#building-with-github-actions) | No local setup at all |
| [Native GCC 3.4.6](#building-natively) | Distro maintainers, hacking on the code |

The first three all use the same environment: an **i386 Debian Etch** sysroot
(GCC 3.4.6, glibc 2.3.6) assembled from archive.debian.org and entered through
a chroot. Every ordinary build produces

```text
src/linux_sdk/server_i486.so          the game DLL
src/linux_sdk/server_i486.so_map.txt  linker map (when linked in this tree)
```

and `make` verifies the result with `src/linux_sdk/check_undefined.sh`, which
fails the build when the DLL refers to a symbol the engine does not export —
the failure mode that otherwise shows up as a segfault right after
`Console initialized.`

### Building with Docker

The repository's [`Dockerfile`](Dockerfile) builds an image with the original
toolchain and compiles inside a chroot of it. This is the same environment the
GitHub Actions workflow uses.

**1. Install [Docker](https://docs.docker.com/get-docker/)** if you don't have it yet.

**2. Clone the repository** and enter it (skip if you already have it):

```bash
git clone https://github.com/hy1ex/improved-srcds-v34.git
cd improved-srcds-v34
```

**3. Build the image** (the first run downloads the toolchain, later runs reuse the cache):

```bash
docker build --platform linux/amd64 -t srcds-v34-builder .
```

**4. Compile:**

```bash
docker run --rm \
    --platform linux/amd64 \
    -v "$PWD":/opt/etch/build \
    -e HOST_UID=$(id -u) -e HOST_GID=$(id -g) \
    srcds-v34-builder
```

* **Windows (PowerShell):** `docker run --rm --platform linux/amd64 -v "${PWD}:/opt/etch/build" srcds-v34-builder`
* `HOST_UID`/`HOST_GID` fix ownership of the build output on Linux; on macOS/Windows Docker Desktop they can be omitted (`id -u` does not exist in PowerShell either).
* The `--platform linux/amd64` flag makes sure the x86_64 image is used even on ARM Macs.
* Optional: add `-e JOBS=8` to change the build parallelism (default `4`).
* Running the command again only rebuilds what changed. For a clean rebuild, first delete the outputs (`rm -rf src/linux_sdk/obj src/linux_sdk/server_i486.so`), then run it again.

**5. Verify** the result:

```bash
file src/linux_sdk/server_i486.so
# src/linux_sdk/server_i486.so: ELF 32-bit LSB shared object, Intel 80386, ...
```

### Building locally without Docker or root

The same toolchain, unpacked by hand and entered through an **unprivileged user
namespace** chroot. No root, no `sudo`, no Docker daemon — it works on cloud
build runners and locked-down machines, and this is how the binary is built and
verified here. Host prerequisites: `curl`, `dpkg-deb` and `unshare`
(util-linux).

**1. Extract the toolchain** — the same 25 packages the [`Dockerfile`](Dockerfile)
lists:

```bash
ETCH=./etch                       # chroot tree, any writable location
BASE=http://archive.debian.org/debian
mkdir -p "$ETCH/tmp/debs"
PKGS=$(grep -oE 'pool/[^ \\]+_i386\.deb' Dockerfile)
( cd "$ETCH/tmp/debs" &&
  for f in $PKGS; do curl -fsSLO --retry 5 --retry-delay 5 "$BASE/$f"; done )
for d in "$ETCH"/tmp/debs/*.deb; do dpkg-deb -x "$d" "$ETCH"; done
rm -rf "$ETCH/tmp/debs"
```

**2. Create the toolchain symlinks** the stock `src/linux_sdk/Makefile` expects
(Valve's historical crosstool names pointing at the extracted GCC 3.4.6):

```bash
mkdir -p "$ETCH/usr/local/bin" \
  "$ETCH/opt/crosstool/gcc-3.4.6-glibc-2.3.6/i686-unknown-linux-gnu/i686-unknown-linux-gnu/lib" \
  "$ETCH/opt/crosstool/gcc-3.4.6-glibc-2.3.6/i686-unknown-linux-gnu/lib/gcc/i686-unknown-linux-gnu/3.4.6"
chmod 1777 "$ETCH/tmp"
ln -sf /usr/bin/gcc-3.4 "$ETCH/usr/local/bin/i686-unknown-linux-gnu-gcc"
ln -sf /usr/bin/g++-3.4 "$ETCH/usr/local/bin/i686-unknown-linux-gnu-g++"
ln -sf /usr/bin/ar      "$ETCH/usr/local/bin/i686-unknown-linux-gnu-ar"
ln -sf /usr/lib/gcc/i486-linux-gnu/3.4.6/libstdc++.a \
  "$ETCH/opt/crosstool/gcc-3.4.6-glibc-2.3.6/i686-unknown-linux-gnu/i686-unknown-linux-gnu/lib/libstdc++.a"
ln -sf /usr/lib/gcc/i486-linux-gnu/3.4.6/libgcc_eh.a \
  "$ETCH/opt/crosstool/gcc-3.4.6-glibc-2.3.6/i686-unknown-linux-gnu/lib/gcc/i686-unknown-linux-gnu/3.4.6/libgcc_eh.a"
```

(The symlink targets are paths **inside** the chroot, so they stay absolute.)

**3. Copy the source into the chroot** and smoke-test the compiler:

```bash
mkdir -p "$ETCH/build"
tar -C . --exclude=.git --exclude=./src/linux_sdk/obj \
    --exclude=./src/linux_sdk/server_i486.so -cf - . | tar -C "$ETCH/build" -xf -

unshare --map-root-user chroot "$ETCH" /usr/bin/gcc-3.4 --version
```

**4. Build.** `unshare --map-root-user` maps your user to root *inside the new
user namespace only*, which is exactly enough to `chroot` without being root
on the host:

```bash
JOBS=$(nproc) unshare --map-root-user chroot "$ETCH" /bin/bash -c '
  set -eux
  export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
  cd /build/src/linux_sdk
  make -j"$JOBS"
'
```

`make` creates all `obj/` subdirectories itself (the `dirs` target is an
order-only prerequisite of every compile rule), so no preparation of the
build tree is needed — not even for `make -j`. The result lands in the chroot
copy — fetch it and verify:

```bash
cp "$ETCH/build/src/linux_sdk/server_i486.so" src/linux_sdk/
file src/linux_sdk/server_i486.so
# src/linux_sdk/server_i486.so: ELF 32-bit LSB shared object, Intel 80386, ...
```

Notes:

* The build runs against a **copy** of the tree: edit freely, re-run step 3
  after pulling new changes, and copy the `.so` back when done.
* `JOBS` is passed through the environment as shown because Etch's
  `coreutils` is too old to provide `nproc` *inside* the chroot.
* A full clean build takes a few minutes at `-j4`; rebuilds only recompile
  changed files and the files that include a changed header (`-MMD -MP`
  dependency files, kept under `src/linux_sdk/obj/`).
* `make` links the prebuilt `tier1_i486.a`, `mathlib_i486.a` and
  `choreoobjects_i486.a` archives that ship with the tree. Run `make libs` to
  rebuild them from source after editing `tier1/`, `mathlib/` or
  `game/shared/` (that needs the `i686-unknown-linux-gnu-ar` symlink).

### Building with GitHub Actions

No local setup needed: the [workflow](.github/workflows/build.yml) compiles the
code in the same Debian Etch chroot used by the Docker image. It runs
automatically on every push to `main` and on pull requests (the Etch packages
are cached between runs), and every run uploads its `server_i486.so`,
`SHA256SUMS.txt`, linker map and `build-info.txt` as a downloadable artifact.

A manual run (**Actions** → **Build and Release** → **Run workflow**) adds a few
options:

| Input | Meaning |
|---|---|
| `publish` | Create the GitHub release. Unticked, the run only builds. |
| `tag` | Release tag; empty generates `v34-<date>-r<run>`. |
| `prerelease` | Mark the release as a pre-release. |
| `jobs` | Parallel compile jobs (default `4`). |
| `libs` | Also rebuild `tier1`/`mathlib`/`choreoobjects` from source. |
| `extra_cflags` | Extra compiler flags, e.g. `-g` for debug info. |

Nothing is ever published implicitly: the release job only runs when `publish`
is ticked.

### Building natively

To compile, you need to use **GCC 3.4.6** or lower. Go to `src/linux_sdk`, edit
the compiler paths in the `Makefile` for your system, and run `make`. The
binary lands in the working directory. `make`, `make -j8`, `make libs` and
`make clean` are the supported targets.

## Repository layout

```text
src/
  linux_sdk/        makefiles, the prebuilt tier1/mathlib/choreoobjects
                    archives, the engine libraries it links against, and
                    check_undefined.sh with its symbol allowlist
  game/server/      game DLL sources (entities, physics, AI, players)
  game/shared/      code shared with the client: rule sets, weapons, movement
  public/           SDK headers and the interfaces the engine calls
  common/, tier1/, mathlib/
docs/
  FEATURES.md       the 48 feature ports and their convars
  FIXES.md          the 14 security and crash fixes, with code
  BACKPORT-REPORT.md the 82 landed upstream backports
Dockerfile          the GCC 3.4.6 / glibc 2.3.6 build environment
.github/workflows/  CI: build on push/PR, optional release
```

## License

The source is covered by the [Source 1 SDK license](LICENSE) (`LICENSE`) and,
where applicable, Valve's terms for the Source SDK. Bundled third-party
components and their notices are listed in
[`thirdpartylegalnotices.txt`](thirdpartylegalnotices.txt).
