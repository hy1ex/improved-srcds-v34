#!/bin/sh
#
# check_undefined.sh <shared-object> [allowlist]
#
# Fails when the game DLL carries an undefined symbol that the engine will not provide at
# load time. The SDK links server_i486.so as a shared object, which the linker happily emits
# with unresolved symbols; the engine then fails to dlopen it and segfaults right after
# "Console initialized." - which is what happened when CCSGameRules::IsIntermission() was
# called without ever being implemented.
#
# The engine does export a few hundred symbols to the game DLL, so -Wl,--no-undefined cannot
# be used directly. Instead every undefined symbol is compared against
# undefined_symbols.allow, the set observed on a build that is known to boot. A symbol that is
# not on that list means either a missing definition in our own code (fix the code) or a
# newly used engine export (verify it really exists in the engine, then add it).
#
# Regenerate the allowlist only after booting the build:
#   nm -D -u server_i486.so | sed -n 's/^ *U //p' | sed 's/@.*//' | sort -u \
#     > linux_sdk/undefined_symbols.allow
#
# Set UNDEFCHECK_SKIP=1 to bypass the check for a one-off build.
# Only sed, sort, comm and nm are used: the build chroot has no awk or grep.

set -e

SO="$1"
DIR=`cd "\`dirname "$0"\`" && pwd`
ALLOW="$2"
if [ -z "$ALLOW" ]; then
	ALLOW="$DIR/undefined_symbols.allow"
fi

if [ ! -f "$SO" ]; then
	echo "undefined symbols: '$SO' not found" >&2
	exit 1
fi

if [ ! -f "$ALLOW" ]; then
	echo "undefined symbols: allowlist '$ALLOW' not found" >&2
	exit 1
fi

TMP="$SO.undef.$$"
KNOWN="$SO.known.$$"
NEW="$SO.undefnew.$$"
trap 'rm -f "$TMP" "$KNOWN" "$NEW"' EXIT

nm -D -u "$SO" | sed -n 's/^ *U //p' | sed 's/@.*//' | sort -u > "$TMP"
sed -e '/^#/d' -e '/^$/d' "$ALLOW" | sort -u > "$KNOWN"
comm -23 "$TMP" "$KNOWN" > "$NEW"

if [ -s "$NEW" ]; then
	echo "" >&2
	echo "*** BUILD FAILED: server DLL has undefined symbols nothing provides ***" >&2
	echo "" >&2
	sed 's/^/    /' "$NEW" >&2
	echo "" >&2
	echo "The linker accepts these because a shared object may keep symbols unresolved," >&2
	echo "but the engine cannot dlopen such a DLL and dies right after \"Console initialized.\"." >&2
	echo "Declare the function where it belongs (a definition is missing in this tree), or," >&2
	echo "if the engine really exports it, add the name to linux_sdk/undefined_symbols.allow." >&2
	echo "" >&2
	exit 1
fi

echo "undefined symbols: ok (all `sed -e '/^#/d' -e '/^$/d' "$ALLOW" | wc -l` known engine exports)."
