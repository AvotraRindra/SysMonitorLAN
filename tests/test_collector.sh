#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
bash -n "$root/linux/collect.sh"
output=$(bash "$root/linux/collect.sh")
[[ "$output" =~ ^OK,([0-9]+),([0-9]+),([0-9]+)$ ]] || {
  printf 'ECHEC format collecteur : %q\n' "$output" >&2
  exit 1
}
for n in "${BASH_REMATCH[@]:1}"; do
    ((n >= 0 && n <= 100)) || { echo 'ECHEC : pourcentage hors limites' >&2; exit 1; }
done
printf 'OK : collecteur Linux = %s\n' "$output"
# Le mode SSH fonctionne avec bash -s et stdin, sans agent installe :
remote_style=$(bash -s < "$root/linux/collect.sh")
[[ "$remote_style" =~ ^OK,[0-9]+,[0-9]+,[0-9]+$ ]] || exit 1
printf 'OK : execution via bash -s et stdin\n'
