#!/usr/bin/env bash
set -euo pipefail

DB="classic.db"
ROOT=1
TMPDIR="$(mktemp -d)"
cleanup() { rm -rf "$TMPDIR"; }
trap cleanup EXIT

# ---- Record layout (128 bytes) ----------------------------------------------
# [0..31] name (NUL-padded), [32] age (u8), [33..64] city (NUL-padded), [65..127] note (NUL-padded)
# Spec for pretty printing (generic formatter in CLI)
SPEC='name:0:32:s,age:32:1:u8,city:33:32:s,note:65:63:s'

# --- helpers to build 128B records -------------------------------------------
pad_to() {
  local s="$1" size="$2"
  printf "%s" "$s" | LC_ALL=C head -c "$size" || true
  local n; n=$(printf "%s" "$s" | wc -c | awk '{print $1}')
  if [ "$n" -lt "$size" ]; then dd if=/dev/zero bs=1 count=$((size - n)) status=none; fi
}

write_byte() {
  local dec="$1"
  if [ "$dec" -lt 0 ] || [ "$dec" -gt 255 ]; then echo "age out of range: $dec" >&2; exit 1; fi
  printf "%b" "\\$(printf '%03o' "$dec")"
}

make_record() {
  local name="$1" age="$2" city="$3" note="$4"
  pad_to "$name" 32
  write_byte "$age"
  pad_to "$city" 32
  pad_to "$note" 63
}

write_record_file() {
  local out="$1"
  make_record "$2" "$3" "$4" "$5" > "$out"
  local bytes; bytes=$(wc -c < "$out")
  if [ "$bytes" -lt 128 ]; then dd if=/dev/zero bs=1 count=$((128 - bytes)) status=none >> "$out"
  elif [ "$bytes" -gt 128 ]; then : > "$out.tmp"; head -c 128 "$out" > "$out.tmp"; mv "$out.tmp" "$out"; fi
}

echo "[1/10] cleanup"
rm -f "$DB"

echo "[2/10] create table @page $ROOT"
./mdb "$DB" create $ROOT

echo "[3/10] build 3 classic records (name/age/city/note) -> 128 bytes each"
R1="$TMPDIR/alice.bin"; R2="$TMPDIR/bob.bin"; R3="$TMPDIR/carol.bin"
write_record_file "$R1" "Alice" 30 "Paris" "Loves baguettes"
write_record_file "$R2" "Bob"   25 "Lyon"  "Enjoys silk history"
write_record_file "$R3" "Carol" 28 "Tokyo" "Karaoke on Fridays"

echo "[4/10] insert them"
ID1=$(./mdb "$DB" insert $ROOT "$R1"); echo "  Alice -> $ID1"
ID2=$(./mdb "$DB" insert $ROOT "$R2"); echo "  Bob   -> $ID2"
ID3=$(./mdb "$DB" insert $ROOT "$R3"); echo "  Carol -> $ID3"

echo "[5/10] pretty list (table view)"
./mdb "$DB" listf $ROOT "$SPEC"

echo "[6/10] pretty get (Alice)"
./mdb "$DB" getf "$ID1" "$SPEC"

echo "[7/10] update Bob’s note (and show table again)"
R2U="$TMPDIR/bob_update.bin"
write_record_file "$R2U" "Bob" 25 "Lyon" "Now learning databases!"
./mdb "$DB" update "$ID2" "$R2U"
./mdb "$DB" listf $ROOT "$SPEC"

echo "[8/10] delete Carol (and show table again)"
./mdb "$DB" delete "$ID3"
./mdb "$DB" listf $ROOT "$SPEC"

echo "[9/10] validate integrity"
./mdb "$DB" validate $ROOT

echo "[10/10] persistence spot-check (re-read pretty table)"
./mdb "$DB" listf $ROOT "$SPEC"

echo "Classic example (pretty) done ✓"
