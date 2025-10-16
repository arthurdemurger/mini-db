#!/usr/bin/env bash
set -euo pipefail

DB="classic.db"
ROOT=1
TMPDIR="$(mktemp -d)"
cleanup() { rm -rf "$TMPDIR"; }
trap cleanup EXIT

# --- Layout (128 bytes) -------------------------------------------------------
# [0..31]   name  (32 bytes, UTF-8/ASCII, NUL-padded)
# [32]      age   (1 byte, unsigned)
# [33..64]  city  (32 bytes, NUL-padded)
# [65..127] note  (63 bytes, NUL-padded)

# write exactly SIZE bytes: truncate or NUL-pad
pad_to() {
  # $1=string, $2=size
  local s="$1" size="$2"
  # write at most 'size' bytes (truncate if longer)
  printf "%s" "$s" | LC_ALL=C head -c "$size" || true
  # compute original byte length
  local n
  n=$(printf "%s" "$s" | wc -c | awk '{print $1}')
  if [ "$n" -lt "$size" ]; then
    dd if=/dev/zero bs=1 count=$((size - n)) status=none
  fi
}

# write a single byte (0..255), using octal escape (portable)
write_byte() {
  local dec="$1"
  if [ "$dec" -lt 0 ] || [ "$dec" -gt 255 ]; then
    echo "age out of range: $dec" >&2; exit 1
  fi
  printf "%b" "\\$(printf '%03o' "$dec")"
}

make_record() {
  # $1=name, $2=age, $3=city, $4=note  -> writes 128 bytes to stdout
  local name="$1" age="$2" city="$3" note="$4"
  pad_to "$name" 32
  write_byte "$age"
  pad_to "$city" 32
  pad_to "$note" 63
}

write_record_file() {
  # $1=outfile, $2=name, $3=age, $4=city, $5=note
  local out="$1"
  make_record "$2" "$3" "$4" "$5" > "$out"
  # ensure exactly 128 bytes
  local bytes
  bytes=$(wc -c < "$out")
  if [ "$bytes" -lt 128 ]; then
    dd if=/dev/zero bs=1 count=$((128 - bytes)) status=none >> "$out"
  elif [ "$bytes" -gt 128 ]; then
    # truncate
    : > "$out.tmp"
    head -c 128 "$out" > "$out.tmp"
    mv "$out.tmp" "$out"
  fi
}

echo "[1/11] cleanup"
rm -f "$DB"

echo "[2/11] create table @page $ROOT"
./mdb "$DB" create $ROOT

echo "[3/11] build 3 classic records (name/age/city/note) -> 128 bytes each"
R1="$TMPDIR/alice.bin"
R2="$TMPDIR/bob.bin"
R3="$TMPDIR/carol.bin"

write_record_file "$R1" "Alice" 30 "Paris" "Loves baguettes"
write_record_file "$R2" "Bob"   25 "Lyon"  "Enjoys silk history"
write_record_file "$R3" "Carol" 28 "Tokyo" "Karaoke on Fridays"

echo "[4/11] insert them"
ID1=$(./mdb "$DB" insert $ROOT "$R1"); echo "  Alice -> $ID1"
ID2=$(./mdb "$DB" insert $ROOT "$R2"); echo "  Bob   -> $ID2"
ID3=$(./mdb "$DB" insert $ROOT "$R3"); echo "  Carol -> $ID3"

echo "[5/11] scan (should list 3 IDs)"
./mdb "$DB" scan $ROOT

echo "[6/11] get one row and show first lines of hex (Alice)"
./mdb "$DB" get "$ID1" | head -n 2

echo "[7/11] update Bob’s note"
R2U="$TMPDIR/bob_update.bin"
write_record_file "$R2U" "Bob" 25 "Lyon" "Now learning databases!"
./mdb "$DB" update "$ID2" "$R2U"
./mdb "$DB" get "$ID2" | head -n 2

echo "[8/11] delete Carol"
./mdb "$DB" delete "$ID3"
./mdb "$DB" scan $ROOT

echo "[9/11] inspect + dump"
./mdb "$DB" inspect $ROOT || true
./mdb "$DB" dump row "$ID1" | head -n 10 || true
./mdb "$DB" dump page 1 | head -n 20 || true

echo "[10/11] validate"
./mdb "$DB" validate $ROOT

echo "[11/11] Classic example done ✓"
