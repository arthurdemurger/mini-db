#!/usr/bin/env bash
set -euo pipefail

DB="demo.db"
ROOT=1
REC0="rec0.bin"
REC1="rec1.bin"

echo "[1/11] cleanup"
rm -f "$DB" "$REC0" "$REC1" /tmp/mdb_*.txt

echo "[2/11] create & init table @page $ROOT"
./mdb "$DB" create $ROOT

echo "[3/11] prepare 2 payloads (128 bytes each)"
dd if=/dev/zero    of="$REC0" bs=128 count=1 status=none
dd if=/dev/urandom of="$REC1" bs=128 count=1 status=none

echo "[4/11] insert two rows"
ID0=$(./mdb "$DB" insert $ROOT "$REC0")
ID1=$(./mdb "$DB" insert $ROOT "$REC1")
echo "  -> ID0=$ID0 ID1=$ID1"

echo "[5/11] scan should list 2 IDs"
./mdb "$DB" scan $ROOT | tee /tmp/mdb_scan1.txt
grep -qx "$ID0" /tmp/mdb_scan1.txt
grep -qx "$ID1" /tmp/mdb_scan1.txt

echo "[6/11] get/update/get"
./mdb "$DB" get "$ID0" | head -n1 > /tmp/get0_before.txt
./mdb "$DB" update "$ID0" "$REC1"
./mdb "$DB" get "$ID0" | head -n1 > /tmp/get0_after.txt
if diff -q /tmp/get0_before.txt /tmp/get0_after.txt >/dev/null; then
  echo "ERROR: content did not change"; exit 1
else
  echo "  -> content changed OK"
fi

echo "[7/11] delete ID1 and rescan"
./mdb "$DB" delete "$ID1"
./mdb "$DB" scan $ROOT | tee /tmp/mdb_scan2.txt
if grep -qx "$ID1" /tmp/mdb_scan2.txt; then
  echo "ERROR: $ID1 still present after delete"; exit 1
else
  echo "  -> $ID1 removed OK"
fi

echo "[8/11] validate integrity"
./mdb "$DB" validate $ROOT

echo "[9/11] persistence check (new shell state)"
./mdb "$DB" scan $ROOT | grep -qx "$ID0"
./mdb "$DB" get "$ID0" > /dev/null
./mdb "$DB" validate $ROOT

echo "[10/11] visual inspection (structure + one record)"
echo "--------------------------------------------------"
echo "→ Table layout:"
./mdb "$DB" inspect $ROOT || echo "(inspect unavailable)"
echo
echo "→ Dump first row (ID=$ID0):"
./mdb "$DB" dump row "$ID0" | head -n 10 || echo "(dump unavailable)"
echo
echo "→ Hex of page 1 (first 256 bytes):"
./mdb "$DB" dump page 1 | head -n 20 || echo "(dump unavailable)"
echo "--------------------------------------------------"

echo "[11/11] scenario completed"
echo "All good ✅"
