#!/usr/bin/env bash
# Séquence de démonstration. Lancer ./build/vcu dans un autre terminal avant.
set -u
cd "$(dirname "$0")/.."
source .venv/bin/activate

BMS=""
HMI=""
cleanup() { kill $BMS $HMI 2>/dev/null; }
trap cleanup EXIT

python tools/bms_node.py & BMS=$!
sleep 0.5
python tools/hmi_node.py --run-seconds 60 & HMI=$!

echo "== Attente de l'état RUN"
sleep 3

echo "== Coupure du BMS (défaut et DTC attendus)"
kill $BMS; wait $BMS 2>/dev/null
sleep 2

echo "== Retour du BMS"
python tools/bms_node.py & BMS=$!
sleep 1

echo "== UDS : lecture des DTC, session étendue, effacement"
python tools/uds_tester.py 19 02 FF
python tools/uds_tester.py 10 03
python tools/uds_tester.py 14 FF FF FF
python tools/uds_tester.py 19 01 FF
