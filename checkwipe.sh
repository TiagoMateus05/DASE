#!/bin/sh
set -u

f="build/disks/$1.img"
[ -f "$f" ] || { echo "no such image: $f"; exit 1; }

apparent=$(stat -f%z "$f")
alloc_blocks=$(stat -f%b "$f")
allocated=$((alloc_blocks * 512))

human() {
    awk -v b="$1" 'BEGIN {
        split("B KiB MiB GiB TiB", u, " ")
        i = 1
        while (b >= 1024 && i < 5) { b /= 1024; i++ }
        printf "%.1f %s", b, u[i]
    }'
}

echo "== $f  ($(human "$apparent"), $apparent bytes; $(human "$allocated") allocated on disk)"
echo "-- first 32 bytes"
xxd -l 32 "$f"
echo "-- last 32 bytes"
tail -c 32 "$f" | xxd
echo "-- non-zero content?"

# Fast path: nothing is actually allocated -- every byte reads back as a
# hole, which the filesystem already guarantees is zero. No I/O needed.
if [ "$allocated" -eq 0 ]; then
    echo "   image is entirely zero (sparse, 0 bytes allocated)"
    exit 0
fi

# cmp against /dev/zero can never report "equal" on its own: /dev/zero has
# no EOF, so cmp always ends by hitting EOF on $f first and reports THAT
# as the difference -- even when every byte compared so far matched. That
# was the original bug: the "entirely zero" branch could never fire.
# -n bounds both sides to the same explicit length, so a real match
# reports success instead.
CHUNK=$((1024 * 1024)) # 1 MiB, matches the wipe's own write chunk size
SMALL_LIMIT=$((256 * 1024 * 1024))

if [ "$apparent" -le "$SMALL_LIMIT" ]; then
    # Cheap enough to check exhaustively in one shot.
    if cmp -s -n "$apparent" "$f" /dev/zero; then
        echo "   image is entirely zero ($apparent bytes, full scan)"
    else
        echo "   NOT all zero (full scan):"
        cmp -n "$apparent" "$f" /dev/zero 2>&1 | head -1
    fi
    exit 0
fi

# Large image: comparing the full logical length against /dev/zero is what
# was actually hanging here -- for a multi-GB (or sparse multi-TB) image
# that's just as many read()s whether or not the content is zero. Sample
# instead: first chunk, last chunk, and evenly spaced points across the
# file. Same compromise NIST SP 800-88 allows for verifying a wipe --
# bounded runtime, not exhaustive.
NUM_SAMPLES=32
last_chunk_off=$(((apparent / CHUNK - 1) * CHUNK))
echo "   image is $(human "$apparent") logical; sampling $NUM_SAMPLES x $(human "$CHUNK") chunks instead of a full scan"

fail=0
i=0
while [ "$i" -lt "$NUM_SAMPLES" ]; do
    off=$((i * last_chunk_off / (NUM_SAMPLES - 1)))
    off=$(((off / CHUNK) * CHUNK)) # align to a chunk boundary
    off_mb=$((off / CHUNK))
    # cmp's own skip1 doesn't lseek() on this system -- it reads and
    # discards a byte at a time, so cost grows with the offset (a skip of
    # ~264 MiB alone burned several seconds of CPU). dd's skip= does a
    # real seek and stays flat at any offset, so use it to position and
    # hand cmp only the chunk itself.
    if ! dd if="$f" bs="$CHUNK" skip="$off_mb" count=1 2>/dev/null | cmp -s -n "$CHUNK" - /dev/zero; then
        echo "   NOT all zero: chunk at byte $off differs"
        dd if="$f" bs="$CHUNK" skip="$off_mb" count=1 2>/dev/null | cmp -n "$CHUNK" - /dev/zero 2>&1 | head -1
        fail=1
        break
    fi
    i=$((i + 1))
done

[ "$fail" -eq 0 ] && echo "   sampled $NUM_SAMPLES x $(human "$CHUNK") chunks across $(human "$apparent") -- all zero (not exhaustive)"
