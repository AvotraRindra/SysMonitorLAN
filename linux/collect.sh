#!/usr/bin/env bash
# SysMonitor LAN - collecteur Linux sans installation (Bash, /proc, df).
# Sortie machine : OK,CPU,RAM,DISK (entiers de 0 a 100).
set -euo pipefail
export LC_ALL=C

fail() { printf 'ERREUR: %s\n' "$*" >&2; exit 1; }
[[ -r /proc/stat && -r /proc/meminfo ]] || fail 'Les fichiers /proc sont indisponibles.'

read_cpu() {
    local cpu user nice system idle iowait irq softirq steal guest guest_nice
    read -r cpu user nice system idle iowait irq softirq steal guest guest_nice < /proc/stat
    [[ "$cpu" == cpu ]] || return 1
    # /proc/stat peut omettre certains compteurs sur de vieux noyaux.
    local busy=$(( ${user:-0} + ${nice:-0} + ${system:-0} + ${irq:-0} + ${softirq:-0} + ${steal:-0} ))
    local total=$(( busy + ${idle:-0} + ${iowait:-0} ))
    printf '%s %s\n' "$busy" "$total"
}
read -r busy1 total1 < <(read_cpu) || fail 'Lecture CPU impossible.'
sleep 1
read -r busy2 total2 < <(read_cpu) || fail 'Seconde lecture CPU impossible.'
delta_total=$(( total2 - total1 ))
delta_busy=$(( busy2 - busy1 ))
(( delta_total > 0 && delta_busy >= 0 )) || fail 'Compteurs CPU incoherents.'
cpu=$(( (100 * delta_busy + delta_total / 2) / delta_total ))

mem_total=''
mem_available=''
while read -r key value unit; do
    case "$key" in
        MemTotal:) mem_total="$value" ;;
        MemAvailable:) mem_available="$value" ;;
    esac
done < /proc/meminfo
[[ "$mem_total" =~ ^[0-9]+$ && "$mem_available" =~ ^[0-9]+$ ]] || fail 'Lecture RAM impossible.'
(( mem_total > 0 && mem_available <= mem_total )) || fail 'Valeurs RAM invalides.'
ram=$(( (100 * (mem_total - mem_available) + mem_total / 2) / mem_total ))

# Pourcentage d'occupation de la partition racine (/).
disk=$(df -P / | awk 'NR==2 {gsub(/%/, "", $5); print $5}')
[[ "$disk" =~ ^[0-9]+$ ]] || fail 'Lecture du disque impossible.'
for n in "$cpu" "$ram" "$disk"; do
    (( n >= 0 && n <= 100 )) || fail 'Pourcentage hors limites.'
done
printf 'OK,%d,%d,%d\n' "$cpu" "$ram" "$disk"
