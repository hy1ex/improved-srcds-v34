FROM debian:bookworm-slim

# 1. Tooling to fetch the archived packages
RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates curl \
    && rm -rf /var/lib/apt/lists/*

# 2. The original build environment: i386 Debian Etch (GCC 3.4.6, glibc 2.3.6),
#    unpacked into a chroot at /opt/etch. Same package set as the CI workflow.
RUN mkdir -p /opt/etch /tmp/debs \
    && cd /tmp/debs \
    && BASE=http://archive.debian.org/debian \
    && for f in \
         pool/main/g/gcc-3.4/gcc-3.4-base_3.4.6-5_i386.deb \
         pool/main/g/gcc-3.4/cpp-3.4_3.4.6-5_i386.deb \
         pool/main/g/gcc-3.4/gcc-3.4_3.4.6-5_i386.deb \
         pool/main/g/gcc-3.4/g++-3.4_3.4.6-5_i386.deb \
         pool/main/g/gcc-3.4/libstdc++6-dev_3.4.6-5_i386.deb \
         pool/main/g/gcc-4.1/gcc-4.1-base_4.1.1-21_i386.deb \
         pool/main/g/gcc-4.1/libgcc1_4.1.1-21_i386.deb \
         pool/main/g/gcc-4.1/libstdc++6_4.1.1-21_i386.deb \
         pool/main/g/gcc-3.3/libstdc++5_3.3.6-15_i386.deb \
         pool/main/b/binutils/binutils_2.17-3+etch1_i386.deb \
         pool/main/g/glibc/libc6_2.3.6.ds1-13etch10+b1_i386.deb \
         pool/main/g/glibc/libc6-dev_2.3.6.ds1-13etch10+b1_i386.deb \
         pool/main/l/linux-kernel-headers/linux-kernel-headers_2.6.18-7_i386.deb \
         pool/main/b/bash/bash_3.1dfsg-8_i386.deb \
         pool/main/m/make-dfsg/make_3.81-2_i386.deb \
         pool/main/c/coreutils/coreutils_5.97-5.3_i386.deb \
         pool/main/s/sed/sed_4.1.5-1_i386.deb \
         pool/main/d/debianutils/debianutils_2.17_i386.deb \
         pool/main/f/findutils/findutils_4.2.28-1etch1_i386.deb \
         pool/main/g/gzip/gzip_1.3.5-15+etch1_i386.deb \
         pool/main/n/ncurses/libncurses5_5.5-5_i386.deb \
         pool/main/a/acl/libacl1_2.2.41-1_i386.deb \
         pool/main/a/attr/libattr1_2.4.32-1_i386.deb \
         pool/main/libs/libselinux/libselinux1_1.32-3_i386.deb \
         pool/main/libs/libsepol/libsepol1_1.14-2_i386.deb \
       ; do curl -fsSLO --retry 5 --retry-delay 5 "$BASE/$f"; done \
    && for d in *.deb; do dpkg-deb -x "$d" /opt/etch; done \
    && rm -rf /tmp/debs

# 3. The stock linux_sdk/Makefile expects Valve's historical crosstool
#    (i686-unknown-linux-gnu-gcc etc. and static libs under /opt/crosstool/...).
#    Point those names at the extracted GCC 3.4.6 via symlinks so that the
#    Makefile works unmodified.
RUN mkdir -p /opt/etch/usr/local/bin \
             /opt/etch/build \
             /opt/etch/tmp \
             /opt/etch/opt/crosstool/gcc-3.4.6-glibc-2.3.6/i686-unknown-linux-gnu/i686-unknown-linux-gnu/lib \
             /opt/etch/opt/crosstool/gcc-3.4.6-glibc-2.3.6/i686-unknown-linux-gnu/lib/gcc/i686-unknown-linux-gnu/3.4.6 \
    && chmod 1777 /opt/etch/tmp \
    && ln -s /usr/bin/gcc-3.4 /opt/etch/usr/local/bin/i686-unknown-linux-gnu-gcc \
    && ln -s /usr/bin/g++-3.4 /opt/etch/usr/local/bin/i686-unknown-linux-gnu-g++ \
    && ln -s /usr/bin/ar /opt/etch/usr/local/bin/i686-unknown-linux-gnu-ar \
    && ln -s /usr/lib/gcc/i486-linux-gnu/3.4.6/libstdc++.a /opt/etch/opt/crosstool/gcc-3.4.6-glibc-2.3.6/i686-unknown-linux-gnu/i686-unknown-linux-gnu/lib/libstdc++.a \
    && ln -s /usr/lib/gcc/i486-linux-gnu/3.4.6/libgcc_eh.a /opt/etch/opt/crosstool/gcc-3.4.6-glibc-2.3.6/i686-unknown-linux-gnu/lib/gcc/i686-unknown-linux-gnu/3.4.6/libgcc_eh.a

# 4. Build script: runs inside the chroot after `docker run`
RUN printf '%s\n' \
      '#!/bin/bash' \
      'set -eux' \
      'export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin' \
      'cd /build/linux_sdk' \
      'mkdir -p obj/server_i486 obj/server_i486/common obj/server_i486/game_shared obj/server_i486/game_shared/cstrike obj/server_i486/game_shared/cstrike/bot obj/server_i486/game_shared/hl2 obj/server_i486/game_shared/portal obj/server_i486/public obj/server_i486/public/haptics obj/server_i486/public/keyframe obj/server_i486/public/tier0 obj/server_i486/public/vgui_controls obj/server_i486/cstrike obj/server_i486/cstrike/bot obj/server_i486/cstrike/bot/states obj/server_i486/cstrike/hostage obj/server_i486/hl2_dll' \
      'make -j"${JOBS:-4}"' \
      'if [ -n "${HOST_UID:-}" ] && [ -n "${HOST_GID:-}" ]; then chown -R "$HOST_UID:$HOST_GID" /build/linux_sdk/obj /build/linux_sdk/server_i486.so || echo "warning: could not set ownership of build outputs"; fi' \
    > /opt/etch/usr/local/bin/build \
    && chmod +x /opt/etch/usr/local/bin/build

# /opt/etch/build is where the repository is bind-mounted at `docker run` time
ENTRYPOINT ["chroot", "/opt/etch"]
CMD ["/usr/local/bin/build"]
