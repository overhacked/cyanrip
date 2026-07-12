#!/bin/bash
# Rips the disc images in fixtures/ and verifies the outputs.
# Usage: test_images.sh <cyanrip-binary> <fixtures-dir>
set -u

CRIP=$(realpath "$1")
FIX=$(realpath "$2")
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# libcdio pairs .cue and .bin files by basename, so stage a copy per sheet
cp "$FIX"/*.cue "$FIX"/cdda.nrg "$WORK"/
cp "$FIX"/cdda.bin "$WORK"/basic.bin
cp "$FIX"/cdda.bin "$WORK"/pregap.bin
cp "$FIX"/mixed.bin "$WORK"/mixed.bin

FAILS=0
FFPROBE=$(command -v ffprobe || true)

fail() {
    echo "FAIL: $*"
    FAILS=$((FAILS + 1))
}

# rip <name> <image> [extra cyanrip args...]
rip() {
    local name=$1 img=$2
    shift 2
    if ! timeout 60 "$CRIP" -d "$WORK/$img" -N -A -U -s 0 -P 0 -o flac \
            -D "$WORK/out_$name" -F '{track}' -L log -M sheet "$@" \
            > "$WORK/$name.log" 2>&1; then
        fail "$name: cyanrip exited with $? (log follows)"
        cat "$WORK/$name.log"
    fi
}

# expect <name> <file[:duration[:sample_fmt]]...>
expect() {
    local name=$1 out="$WORK/out_$1" f dur fmt have
    shift

    have=$(cd "$out" 2>/dev/null && ls | sort | tr '\n' ' ')
    want=$(printf '%s\n' "$@" | cut -d: -f1 | sort | tr '\n' ' ')
    if [ "$have" != "$want" ]; then
        fail "$name: outputs [ $have] != expected [ $want]"
        return
    fi

    for spec in "$@"; do
        IFS=: read -r f dur fmt <<< "$spec"

        case "$f" in *.flac)
            [ "$(head -c4 "$out/$f")" = "fLaC" ] || fail "$name: $f is not FLAC"
        esac

        [ -z "$FFPROBE" ] && continue
        if [ -n "$dur" ]; then
            local d
            d=$("$FFPROBE" -v error -show_entries format=duration -of csv=p=0 "$out/$f")
            awk "BEGIN { exit !(($d - $dur)^2 < 0.01) }" || \
                fail "$name: $f duration $d != $dur"
        fi
        if [ -n "$fmt" ]; then
            local sf
            sf=$("$FFPROBE" -v error -select_streams a -show_entries stream=sample_fmt -of csv=p=0 "$out/$f")
            sf=${sf%,}
            [ "$sf" = "$fmt" ] || fail "$name: $f sample_fmt $sf != $fmt"
        fi
    done
}

# Info-only mode on every image type
for img in basic.cue pregap.cue mixed.cue cdda.nrg; do
    timeout 60 "$CRIP" -d "$WORK/$img" -I -N -A -U -P 0 > "$WORK/info.log" 2>&1 || \
        fail "info $img: cyanrip exited with $?"
done

rip basic basic.cue
expect basic 1.flac:4 2.flac:4 log.log sheet.cue

# Track 1 HTOA stays unmerged by default, track 2 pregap merges into track 1
rip pregap_def pregap.cue
expect pregap_def 1.flac:3 2.flac:2 3.flac:1 log.log sheet.cue

# HTOA becomes track 0, track 2 pregap becomes a track of its own
rip pregap_track pregap.cue -p 1=track -p 2=track
expect pregap_track 0.flac:2 1.flac:2 2.flac:1 3.flac:2 4.flac:1 log.log sheet.cue

rip pregap_drop pregap.cue -p 2=drop
expect pregap_drop 1.flac:2 2.flac:2 3.flac:1 log.log sheet.cue

# Data track must be skipped, and produce no stray file
rip mixed mixed.cue
expect mixed 2.flac:2 3.flac:2 log.log sheet.cue

# Data track selected via rip indices
rip mixed_idx mixed.cue -l 1,2
expect mixed_idx 2.flac:2 log.log sheet.cue

# NRG with a DAOX pregap on track 2
rip nrg cdda.nrg
expect nrg 1.flac:3 2.flac:3 log.log sheet.cue

# HDCD decoding pipeline, output must be 24-bit (s32)
rip hdcd basic.cue -H
expect hdcd 1.flac:4:s32 2.flac:4:s32 log.log sheet.cue

# Forced deemphasis pipeline
rip deemph basic.cue -E
expect deemph 1.flac:4 2.flac:4 log.log sheet.cue

# Album cover art: written out per format and embedded in every track
rip art basic.cue -C "Front=$FIX/art.png"
expect art 1.flac:4 2.flac:4 Front.png log.log sheet.cue
if [ -n "$FFPROBE" ]; then
    for f in 1 2; do
        nb=$("$FFPROBE" -v error -select_streams v -show_entries stream=codec_name \
             -of csv=p=0 "$WORK/out_art/$f.flac" | grep -c .)
        [ "$nb" -eq 1 ] || fail "art: $f.flac has $nb embedded pictures, wanted 1"
    done
fi

# Naming schemes: subdirectories, conditionals, tag values with escaped
# separators, and disc number prefixes
rip scheme basic.cue -c 2/3 -a 'album=CI Album:date=2020-01-01' \
    -D "$WORK/out_scheme/{album} ({year})" \
    -F '{if #totaldiscs# > #1#|disc|.}{track} - {title}' \
    -t '1=title=One\: Two' -t '2=title=Three'
d="$WORK/out_scheme/CI Album (2020)"
[ -f "$d/2.1 - One∶ Two.flac" ] || \
    fail "scheme: missing '2.1 - One∶ Two.flac', have: $(ls "$d" 2>/dev/null | tr '\n' ' ')"
[ -f "$d/2.2 - Three.flac" ] || fail "scheme: missing '2.2 - Three.flac'"

# Schemes sending multiple tracks to one file must be warned about
rip collide basic.cue -F '{album}'
grep -q "resolve to the same file" "$WORK/collide.log" || \
    fail "collide: expected a filename collision warning"

# Encoder init failure (file name too long) must fail cleanly, not
# crash in cleanup on the uninitialized encoder mutex/thread
longname=$(printf 'x%.0s' $(seq 1 300))
timeout 60 "$CRIP" -d "$WORK/basic.cue" -N -A -U -s 0 -P 0 -K -o flac \
    -D "$WORK/out_longname" -F "$longname" -L log -M sheet \
    > "$WORK/longname.log" 2>&1
ec=$?
[ "$ec" -eq 1 ] || fail "longname: expected clean failure (1), got exit $ec"

if [ "$FAILS" -gt 0 ]; then
    echo "$FAILS check(s) failed"
    exit 1
fi
echo "all image tests passed"
