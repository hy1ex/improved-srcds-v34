# Improved srcds v34
Improved Counter-Strike Source v34 Dedicated Server also known as srcds fix in CSS v34 community.
## Description
This is the open source server part of the game Counter-Strike: Source v34 based on the Source SDK Base 2006 and Source Engine 2007 with changes for the latest build of the game as well as some improvements.
## Improvements
* Physics fixes for 100 tickrate
* Fixed some console commands leading to the crash of the server
* Added new console commands that extend gameplay and fix bugs
* Optimization using SSE
## ConVars
* sv_maxusrcmdprocessticks <16> - Maximum number of client-issued usrcmd ticks that can be replayed in packet loss conditions, 0 to allow no restrictions
* sv_maxusrcmdprocessticks_warning <-1> - Print a warning when user commands get dropped due to insufficient usrcmd ticks allocated, number of seconds to throttle, negative disabled
* sv_maxusrcmdprocessticks_holdaim <1> - Hold client aim for multiple server sim ticks when client-issued usrcmd contains multiple actions (0: off; 1: hold this server tick; 2+: hold multiple ticks)
* sv_player_usercommand_timeout <3> - After this many seconds without a usercommand from a player, the server will RunNullCommand as if client sends an empty command
* sv_usercmd_custom_random_seed <1> - When enabled server will populate an additional random seed independent of the client
* sv_max_usercmd_move_magnitude <1000> - Maximum move magnitude that can be requested by client
* cs_enable_player_physics_box <1> - Allow to create physical objects for players
* mp_ignore_round_win_conditions <0> - Ignore conditions which would end the current round
* mp_round_restart_delay <5.0> - Number of seconds to delay before restarting a round after a win
* sv_enablebunnyhopping <0> - Allow player speed to exceed maximum running speed
* sv_shootpositionfix <1> - Fixes shoot position. https://github.com/ValveSoftware/source-sdk-2013/pull/442
## Building
The server binary (`linux_sdk/server_i486.so`) is a **32-bit i386** Linux library and must be compiled with **GCC 3.4.6 or lower** — the code uses pre-C++11 headers such as `<new.h>` which newer compilers no longer ship.

You can build it in two ways:

* **with Docker** — works on any machine, no toolchain setup required (step-by-step guide below)
* **with a native GCC 3.4.6 installation** — see [Building natively](#building-natively)

You can also let GitHub Actions build it without any local setup: **Actions** → **Build and Release** → **Run workflow**.

### Building with Docker (step by step)

The repository's [`Dockerfile`](Dockerfile) builds an image with the original-era toolchain — i386 Debian Etch with GCC 3.4.6 and glibc 2.3.6, fetched from archive.debian.org — and compiles the code inside a chroot of it. This is the same environment the GitHub Actions workflow uses.

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

### Building natively

To compile, you need to use <b>GCC 3.4.6</b> or lower. First, go to the <b>linux_sdk</b> directory and open the <b>Makefile</b>, then edit it for your system. Type <b>make</b> to build. 
After compilation, the binary file will be located in the working directory.
