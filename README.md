# Improved srcds v34

Open-source dedicated server for **Counter-Strike: Source build v34** — the
Source SDK Base 2006 / Source Engine 2007 server binary, updated for the
current game build, hardened against known exploits, and fixed for high
tickrate play.

## About

This repository contains the server part (`server_i486.so`) of Counter-Strike:
Source v34. It compiles to a **32-bit i386** shared library and runs on a
stock v34 dedicated server installation.

Changes are applied directly to the code: the **`BUGFIXED`** compile switch
was removed (commit `7fdd22f`), so the fixed code is the only code — there is
no flag to toggle and no “stock behavior” build anymore. The original fix set
is documented with its original code, fixed code and rationale in
**[FIXES.md](FIXES.md)** (kept for archive purposes), and the ongoing port of
fixes from newer Source-engine trees is tracked in
**[UPSTREAM-PORT-REPORT.md](UPSTREAM-PORT-REPORT.md)**.

## What's improved

* **100-tickrate physics** — the physics environment now simulates at the
  server's actual tick interval instead of the stock hardwired 15 ms.
* **Anti-exploit input hardening** — client-supplied usercommands are
  validated (NaN/INF angles, move magnitudes capped by
  `sv_max_usercmd_move_magnitude`), replay bursts are budgeted
  (`sv_maxusrcmdprocessticks`), idle clients time out safely, and the server
  generates its own usercmd random seed.
* **Crash fixes** — null-gamerules damage, stale game-UI handles, missing
  studio headers and other paths that let a client or a map change take the
  server down (see [FIXES.md](FIXES.md)).
* **Security** — debug and report commands are restricted to the server
  console; the server-side protections of the
  [ProtectCmds](https://github.com/PowerTakeDev/ProtectCmds) extension are
  ported into the source (the engine-level ones still need the extension
  itself).
* **Upstream backports** — engine-layer fixes found by diffing this tree
  against source-sdk-2013, css-community and nillerusr 2017 (guards, bounds
  checks, trace and lag-compensation fixes) are ported function by function
  without touching the network protocol — see
  [UPSTREAM-PORT-REPORT.md](UPSTREAM-PORT-REPORT.md) for the analysis and the
  commits that have landed.
* **Gameplay fixes** — `sv_shootpositionfix` (Valve's shoot-position
  correction), configurable round flow, bunnyhop speed cap, and the original
  list below:
  * Physics fixes for 100 tickrate
  * Fixed some console commands leading to the crash of the server
  * Added new console commands that extend gameplay and fix bugs
  * Optimization using SSE

## Console variables

**Netcode / movement**

* `sv_maxusrcmdprocessticks <16>` — Maximum number of client-issued usrcmd ticks that can be replayed in packet loss conditions, 0 to allow no restrictions
* `sv_maxusrcmdprocessticks_warning <-1>` — Print a warning when user commands get dropped due to insufficient usrcmd ticks allocated, number of seconds to throttle, negative disabled
* `sv_maxusrcmdprocessticks_holdaim <1>` — Hold client aim for multiple server sim ticks when client-issued usrcmd contains multiple actions (0: off; 1: hold this server tick; 2+: hold multiple ticks)
* `sv_player_usercommand_timeout <3>` — After this many seconds without a usercommand from a player, the server will RunNullCommand as if client sends an empty command
* `sv_usercmd_custom_random_seed <1>` — When enabled server will populate an additional random seed independent of the client
* `sv_max_usercmd_move_magnitude <1000>` — Maximum move magnitude that can be requested by client
* `sv_shootpositionfix <1>` — Fixes shoot position. https://github.com/ValveSoftware/source-sdk-2013/pull/442
* `sv_enablebunnyhopping <0>` — Allow player speed to exceed maximum running speed

**Gameplay**

* `cs_enable_player_physics_box <1>` — Allow to create physical objects for players
* `mp_ignore_round_win_conditions <0>` — Ignore conditions which would end the current round
* `mp_round_restart_delay <5.0>` — Number of seconds to delay before restarting a round after a win

All fixes, including the ConVars above, are documented in
[FIXES.md](FIXES.md).

## Building

The build needs a **GCC 3.4.6-or-older** toolchain (the code includes
pre-C++11 headers such as `<new.h>`) and must produce a **32-bit i386** binary
to match the game's `tier0_i486.so` / `vstdlib_i486.so`. There are three ways
to build `linux_sdk/server_i486.so`:

| Method | Best for |
|---|---|
| [Docker](#building-with-docker-step-by-step) | Any machine, no toolchain setup |
| [Local chroot, no Docker/root](#building-locally-without-docker-or-root) | Cloud runners and locked-down machines |
| [GitHub Actions](#building-with-github-actions) | No local setup at all |
| [Native GCC 3.4.6](#building-natively) | Distro maintainers, hacking on the code |

### Building with Docker (step by step)

The repository's [`Dockerfile`](Dockerfile) builds an image with the
original-era toolchain — i386 Debian Etch with GCC 3.4.6 and glibc 2.3.6,
fetched from archive.debian.org — and compiles the code inside a chroot of it.
This is the same environment the GitHub Actions workflow uses.

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
* Running the command again only rebuilds what changed. For a clean rebuild, first delete the outputs (`rm -rf linux_sdk/obj linux_sdk/server_i486.so`), then run it again.

**5. The result** is `linux_sdk/server_i486.so`. Verify it:

```bash
file linux_sdk/server_i486.so
# linux_sdk/server_i486.so: ELF 32-bit LSB shared object, Intel 80386, ...
```

### Building locally without Docker or root

The same toolchain as the Docker image — the Debian Etch / GCC 3.4.6
packages — but unpacked by hand and entered through an **unprivileged user
namespace** chroot. This needs no root, no `sudo` and no Docker daemon, which
is why it is the method that works on cloud build runners: the cloud runner
used for this repository has none of those, and this is exactly how
`server_i486.so` is built and verified there. It runs on any Linux where
unprivileged user namespaces are enabled. Host prerequisites: `curl`,
`dpkg-deb` and `unshare` (util-linux).

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

**2. Create the toolchain symlinks** the stock `linux_sdk/Makefile` expects
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
tar -C . --exclude=.git --exclude=./linux_sdk/obj \
    --exclude=./linux_sdk/server_i486.so -cf - . | tar -C "$ETCH/build" -xf -

unshare --map-root-user chroot "$ETCH" /usr/bin/gcc-3.4 --version
```

**4. Build.** `unshare --map-root-user` maps your user to root *inside the new
user namespace only*, which is exactly enough to `chroot` without being root
on the host:

```bash
JOBS=$(nproc) unshare --map-root-user chroot "$ETCH" /bin/bash -c '
  set -eux
  export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
  cd /build/linux_sdk
  make -j"$JOBS"
'
```

`make` creates all `obj/` subdirectories itself (its `-mkdir -p` rules), so
no preparation of the build tree is needed. The result lands in the chroot
copy — fetch it and verify:

```bash
cp "$ETCH/build/linux_sdk/server_i486.so" linux_sdk/
file linux_sdk/server_i486.so
# linux_sdk/server_i486.so: ELF 32-bit LSB shared object, Intel 80386, ...
```

Notes:

* The build runs against a **copy** of the tree: edit freely, re-run step 3
  after pulling new changes, and copy the `.so` back when done.
* `JOBS` is passed through the environment as shown because Etch's
  `coreutils` is too old to provide `nproc` *inside* the chroot.
* A full clean build takes a few minutes at `-j4`; rebuilds only recompile
  changed files.

### Building with GitHub Actions

No local setup needed: **Actions** → **Build and Release** → **Run workflow**.
The workflow compiles the code in the same Debian Etch chroot and publishes a
release containing `server_i486.so` and a `SHA256SUMS.txt`. You can optionally
enter a release tag (otherwise one is generated) and tick the prerelease box.

### Building natively

To compile, you need to use <b>GCC 3.4.6</b> or lower. First, go to the <b>linux_sdk</b> directory and open the <b>Makefile</b>, then edit it for your system. Type <b>make</b> to build. 
After compilation, the binary file will be located in the working directory.

## Fix documentation

* **[UPSTREAM-PORT-REPORT.md](UPSTREAM-PORT-REPORT.md)** — the living
  analysis document: how this tree diverges from newer Source-engine trees
  (source-sdk-2013, css-community, nillerusr 2017), why upstream changed
  each function, which backports have landed, and what is still queued.
  Every landed batch has its own commit with a detailed explanation.
* **[FIXES.md](FIXES.md)** — archive of the original fix set: the security
  fixes ported from ProtectCmds (versions 1.0.2 and 1.0.10), including which
  ones still require the extension itself, plus the original `BUGFIXED`-
  guarded changes with their stock and fixed code. Kept for reference only;
  the `BUGFIXED` switch it describes no longer exists in this tree.
