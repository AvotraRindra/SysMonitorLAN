#!/usr/bin/env bash
# Lance le programme C sur Linux avec un client ssh simule (sans vrai reseau).
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/project"/{config,linux,logs} "$tmp/mocks"
cp "$root/config/settings.conf" "$tmp/project/config/"
cat > "$tmp/project/config/machines.csv" <<'VM'
# NOM;UTILISATEUR;IPv4;PORT
Kali-VM;rindra;192.168.43.219;22
VM
cp "$root/linux/collect.sh" "$tmp/project/linux/"
cat > "$tmp/mocks/ssh" <<'MOCK'
#!/bin/sh
# Client SSH simule : teste le mode stdin du collecteur et la gestion des statuts.
[ "$#" -gt 3 ] || exit 33
head -n 1 >/dev/null  # consomme une ligne stdin du script transfere
case "${MOCK_MODE:-normal}" in
  normal) printf 'OK,12,23,34\n';;
  alert) printf 'OK,92,90,34\n';;
  unreachable) echo 'Connexion SSH refusee' >&2;exit 255;;
  invalid) printf 'OK,101,20,30\n';;
  *) exit 10;;
esac
MOCK
chmod +x "$tmp/mocks/ssh"
gcc -std=c11 -Wall -Wextra -Werror -O2 -o "$tmp/project/sysmonitor" "$root/src/sysmonitor.c"
cd "$tmp/project"
export PATH="$tmp/mocks:$PATH"
./sysmonitor --selftest
./sysmonitor --check
MOCK_MODE=normal ./sysmonitor --once > "$tmp/normal.txt"
grep -q 'NORMAL' "$tmp/normal.txt"
[[ $(wc -l < logs/events.log) -eq 1 ]]
MOCK_MODE=normal ./sysmonitor --once > /dev/null
[[ $(wc -l < logs/events.log) -eq 1 ]] # suppression alertes repetitives
set +e
MOCK_MODE=alert ./sysmonitor --once > "$tmp/alert.txt"
rc=$?
set -e
[[ "$rc" -eq 1 ]]
grep -q 'ALERTE' "$tmp/alert.txt"
[[ $(wc -l < logs/events.log) -eq 2 ]]
set +e
MOCK_MODE=alert ./sysmonitor --once > /dev/null
rc=$?
set -e
[[ "$rc" -eq 1 ]]
[[ $(wc -l < logs/events.log) -eq 2 ]]
set +e
MOCK_MODE=unreachable ./sysmonitor --once > "$tmp/offline.txt"
rc=$?
set -e
[[ "$rc" -eq 2 ]]
grep -q 'INACCESSIBLE' "$tmp/offline.txt"
[[ $(wc -l < logs/events.log) -eq 3 ]]
set +e
MOCK_MODE=invalid ./sysmonitor --once > "$tmp/invalid.txt"
rc=$?
set -e
[[ "$rc" -eq 2 ]]
grep -q 'ERREUR' "$tmp/invalid.txt"
[[ $(wc -l < logs/events.log) -eq 4 ]]
[[ $(wc -l < logs/measurements.csv) -eq 7 ]]
head -n 1 logs/measurements.csv | grep -q date_heure
printf 'OK : simulation SSH, transitions, anti-repetition, erreurs et journal CSV\n'
