#!/usr/bin/env bash
# Builds One-Pass PSI's dependencies (RELIC, OpenSSL) from pinned sources into
# out/install.
#
# Usage: ./build.sh [options] [target...]
#   targets : deps (default: relic + openssl + verify) | relic | openssl
#             | verify | clean
#   -j N    : parallel jobs (default: nproc)
#   -f      : force rebuild even if the install stamp is up to date
#   -v      : stream build output instead of writing it to out/log/
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="$ROOT/out"
SRC_DIR="$OUT/src"
DEPS_DIR="$OUT/deps"
PREFIX="$OUT/install"
LOG_DIR="$OUT/log"
STAMP_DIR="$PREFIX/.stamps"

RELIC_REPO="https://github.com/relic-toolkit/relic.git"
RELIC_COMMIT="9fc7356e3c304ec3b0f2f5c34c8ff4a12bac3783"

OPENSSL_REPO="https://github.com/openssl/openssl.git"
OPENSSL_TAG="OpenSSL_1_1_1w"
OPENSSL_COMMIT="e04bd3433fd84e1861bf258ea37928d9845e6a86"

# RELIC configuration of ALOS22: upstream preset/x64-pbc-bls12-381.sh, then
# cmake -DTIMER=HREAL -DBN_PRECI=3072 -DMULTI=PTHREAD -DEP_METHD='JACOB;LWNAF;COMBS;INTER;SSWUM'.
RELIC_FLAGS=(
    # preset/x64-pbc-bls12-381.sh
    -DWSIZE=64
    -DRAND=UDEV
    -DSHLIB=OFF
    -DCHECK=off
    -DVERBS=off
    -DARITH=x64-asm-6l
    -DFP_PRIME=381
    "-DFP_METHD=INTEG;INTEG;INTEG;MONTY;JMPDS;JMPDS;SLIDE"
    "-DCFLAGS=-O3 -funroll-loops -fomit-frame-pointer -finline-small-functions -march=native -mtune=native"
    -DFP_PMERS=off
    -DFP_QNRES=on
    "-DFPX_METHD=INTEG;INTEG;LAZYR"
    -DEP_PLAIN=off
    -DEP_SUPER=off
    "-DPP_METHD=LAZYR;OATEP"
    # ALOS22 overrides
    -DTIMER=HREAL
    -DBN_PRECI=3072
    -DMULTI=PTHREAD
    "-DEP_METHD=JACOB;LWNAF;COMBS;INTER;SSWUM"
    # static library only, no RELIC test/bench binaries
    -DCMAKE_BUILD_TYPE=Release -DSTLIB=ON -DSTBIN=OFF -DTESTS=0 -DBENCH=0
)

OPENSSL_FLAGS=(
    no-shared no-tests
    enable-ec_nistp_64_gcc_128
    --libdir=lib
)

JOBS="$(nproc)"
FORCE=0
VERBOSE=0
TARGETS=()

usage() { sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'; exit "${1:-0}"; }

while [[ $# -gt 0 ]]; do
    case "$1" in
        -j)        JOBS="$2"; shift 2 ;;
        -j*)       JOBS="${1#-j}"; shift ;;
        -f)        FORCE=1; shift ;;
        -v)        VERBOSE=1; shift ;;
        -h|--help) usage ;;
        -*)        echo "unknown option: $1" >&2; usage 1 ;;
        *)         TARGETS+=("$1"); shift ;;
    esac
done
[[ ${#TARGETS[@]} -eq 0 ]] && TARGETS=(deps)

info() { printf '\033[1;34m[%s]\033[0m %s\n' "$1" "$2"; }
die()  { printf '\033[1;31m[error]\033[0m %s\n' "$*" >&2; exit 1; }

run() {
    local name="$1"; shift
    if [[ $VERBOSE -eq 1 ]]; then
        "$@"
        return
    fi
    local log="$LOG_DIR/$name.log"
    if ! "$@" >>"$log" 2>&1; then
        tail -n 30 "$log" >&2
        die "$name failed (full log: $log)"
    fi
}

fetch() {
    local name="$1" repo="$2" ref="$3" commit="$4"
    local dir="$SRC_DIR/$name"
    if [[ -d "$dir/.git" ]] && [[ "$(git -C "$dir" rev-parse HEAD 2>/dev/null)" == "$commit" ]]; then
        return
    fi
    info "$name" "fetching ${ref} (${commit:0:12})"
    rm -rf "$dir"
    mkdir -p "$dir"
    run "$name" git -C "$dir" init -q
    run "$name" git -C "$dir" fetch -q --depth 1 "$repo" "$ref"
    run "$name" git -C "$dir" -c advice.detachedHead=false checkout -q FETCH_HEAD
    local got
    got="$(git -C "$dir" rev-parse HEAD)"
    [[ "$got" == "$commit" ]] || die "$name: expected $commit, got $got"
}

stamp_of() { printf '%s\n' "$@" | sha256sum | cut -d' ' -f1; }

up_to_date() {
    local name="$1" stamp="$2"
    [[ $FORCE -eq 0 && -f "$STAMP_DIR/$name" && "$(cat "$STAMP_DIR/$name")" == "$stamp" ]]
}

build_relic() {
    local stamp
    stamp="$(stamp_of "$RELIC_COMMIT" "${RELIC_FLAGS[@]}")"
    if up_to_date relic "$stamp"; then
        info relic "up to date (${RELIC_COMMIT:0:12})"
        return
    fi
    : >"$LOG_DIR/relic.log"
    fetch relic "$RELIC_REPO" "$RELIC_COMMIT" "$RELIC_COMMIT"

    local bdir="$DEPS_DIR/relic"
    rm -rf "$bdir" "$PREFIX/include/relic" "$PREFIX"/lib/librelic*
    info relic "configuring"
    run relic cmake -S "$SRC_DIR/relic" -B "$bdir" \
        -DCMAKE_INSTALL_PREFIX="$PREFIX" "${RELIC_FLAGS[@]}"
    info relic "building (-j$JOBS)"
    run relic cmake --build "$bdir" -j "$JOBS"
    run relic cmake --install "$bdir"

    [[ -f "$PREFIX/lib/librelic_s.a" && -f "$PREFIX/include/relic/relic.h" ]] \
        || die "relic: install incomplete"
    echo "$stamp" >"$STAMP_DIR/relic"
    info relic "installed -> $PREFIX"
}

build_openssl() {
    local stamp
    stamp="$(stamp_of "$OPENSSL_COMMIT" "${OPENSSL_FLAGS[@]}")"
    if up_to_date openssl "$stamp"; then
        info openssl "up to date ($OPENSSL_TAG)"
        return
    fi
    : >"$LOG_DIR/openssl.log"
    fetch openssl "$OPENSSL_REPO" "refs/tags/$OPENSSL_TAG" "$OPENSSL_COMMIT"

    local bdir="$DEPS_DIR/openssl"
    rm -rf "$bdir" "$PREFIX/include/openssl" "$PREFIX"/lib/lib{crypto,ssl}.* "$PREFIX/lib/pkgconfig"/{libcrypto,libssl,openssl}.pc
    mkdir -p "$bdir"
    info openssl "configuring ($OPENSSL_TAG)"
    (cd "$bdir" && run openssl "$SRC_DIR/openssl/config" \
        --prefix="$PREFIX" --openssldir="$PREFIX/ssl" "${OPENSSL_FLAGS[@]}")
    info openssl "building (-j$JOBS)"
    run openssl make -C "$bdir" -j "$JOBS"
    run openssl make -C "$bdir" install_sw

    [[ -f "$PREFIX/lib/libcrypto.a" && -f "$PREFIX/include/openssl/evp.h" ]] \
        || die "openssl: install incomplete"
    echo "$stamp" >"$STAMP_DIR/openssl"
    info openssl "installed -> $PREFIX"
}

verify() {
    local src="$DEPS_DIR/smoke_test.c" exe="$DEPS_DIR/smoke_test"
    info verify "building smoke test"
    : >"$LOG_DIR/verify.log"
    cat >"$src" <<'EOF'
#include <stdio.h>
#include <string.h>

#include <relic/relic.h>
#include <openssl/evp.h>
#include <openssl/opensslv.h>

#define CHECK(cond, what)                                   \
    do {                                                    \
        if (!(cond)) {                                      \
            fprintf(stderr, "FAIL: %s\n", what);            \
            return 1;                                       \
        }                                                   \
        printf("  ok  %s\n", what);                         \
    } while (0)

static int test_relic(void) {
    bn_t a, b, n, k;
    g1_t p, p2;
    g2_t q, q2;
    gt_t e1, e2, e3;
    uint8_t buf[12 * RLC_PC_BYTES + 1];

    bn_null(a); bn_null(b); bn_null(n); bn_null(k);
    g1_null(p); g1_null(p2); g2_null(q); g2_null(q2);
    gt_null(e1); gt_null(e2); gt_null(e3);

    {
        bn_new(a); bn_new(b); bn_new(n); bn_new(k);
        g1_new(p); g1_new(p2); g2_new(q); g2_new(q2);
        gt_new(e1); gt_new(e2); gt_new(e3);

        pc_get_ord(n);
        bn_rand_mod(a, n);
        bn_rand_mod(b, n);

        g1_rand(p);
        int len = g1_size_bin(p, 1);
        g1_write_bin(buf, len, p, 1);
        g1_read_bin(p2, buf, len);
        CHECK(g1_cmp(p, p2) == RLC_EQ, "G1 serialize round-trip");
        printf("        |G1| = %d bytes (compressed)\n", len);

        g2_rand(q);
        len = g2_size_bin(q, 1);
        g2_write_bin(buf, len, q, 1);
        g2_read_bin(q2, buf, len);
        CHECK(g2_cmp(q, q2) == RLC_EQ, "G2 serialize round-trip");
        printf("        |G2| = %d bytes (compressed)\n", len);

        g1_mul(p2, p, a);
        g2_mul(q2, q, b);
        pc_map(e1, p2, q2);
        pc_map(e2, p, q);
        bn_mul(k, a, b);
        bn_mod(k, k, n);
        gt_exp(e3, e2, k);
        CHECK(gt_cmp(e1, e3) == RLC_EQ, "pairing bilinearity");
        CHECK(gt_is_unity(e2) == 0, "pairing non-degeneracy");

        len = gt_size_bin(e1, 0);
        gt_write_bin(buf, len, e1, 0);
        gt_read_bin(e2, buf, len);
        CHECK(gt_cmp(e1, e2) == RLC_EQ, "GT serialize round-trip");
        printf("        |GT| = %d bytes\n", len);

        const uint8_t msg[] = "one-pass-psi";
        g1_map(p, msg, sizeof(msg));
        g1_map(p2, msg, sizeof(msg));
        CHECK(g1_cmp(p, p2) == RLC_EQ && g1_is_valid(p), "G1 hash-to-curve");
    }
    bn_free(a); bn_free(b); bn_free(n); bn_free(k);
    g1_free(p); g1_free(p2); g2_free(q); g2_free(q2);
    gt_free(e1); gt_free(e2); gt_free(e3);
    return 0;
}

static int test_openssl(void) {
    static const uint8_t expect[32] = {
        0x3a, 0x98, 0x5d, 0xa7, 0x4f, 0xe2, 0x25, 0xb2, 0x04, 0x5c, 0x17,
        0x2d, 0x6b, 0xd3, 0x90, 0xbd, 0x85, 0x5f, 0x08, 0x6e, 0x3e, 0x9d,
        0x52, 0x5b, 0x46, 0xbf, 0xe2, 0x45, 0x11, 0x43, 0x15, 0x32};
    uint8_t out[32];
    unsigned int len = 0;
    int ok = EVP_Digest("abc", 3, out, &len, EVP_sha3_256(), NULL);
    CHECK(ok == 1 && len == 32 && memcmp(out, expect, 32) == 0, "SHA3-256 test vector");
    return 0;
}

int main(void) {
    if (core_init() != RLC_OK || pc_param_set_any() != RLC_OK) {
        fprintf(stderr, "FAIL: RELIC init\n");
        return 1;
    }
    printf("RELIC (security level %d bits)\n", pc_param_level());
    int rc = test_relic();
    core_clean();
    if (rc) return rc;

    printf("%s\n", OPENSSL_VERSION_TEXT);
    return test_openssl();
}
EOF
    run verify cc -O2 -I"$PREFIX/include" "$src" \
        "$PREFIX/lib/librelic_s.a" "$PREFIX/lib/libcrypto.a" -lgmp -lpthread -ldl -o "$exe"
    "$exe" || die "smoke test failed (source kept at $src)"
    rm -f "$src" "$exe"
}

for t in "${TARGETS[@]}"; do
    case "$t" in
        clean) info clean "removing $OUT"; rm -rf "$OUT"; continue ;;
        deps|relic|openssl|verify) ;;
        *) die "unknown target: $t" ;;
    esac
    mkdir -p "$SRC_DIR" "$DEPS_DIR" "$PREFIX" "$LOG_DIR" "$STAMP_DIR"
    case "$t" in
        deps)    build_relic; build_openssl; verify ;;
        relic)   build_relic ;;
        openssl) build_openssl ;;
        verify)  verify ;;
    esac
done
