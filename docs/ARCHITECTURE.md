# Architecture technique — Option B, avec assistant de configuration

```text
                      Windows 10 (administrateur)
                  +--------------------------------+
                  | SysMonitor.exe (C)             |
                  |  - menu et surveillance        |
                  |  - client ssh.exe              |
                  |  - comparateur de seuils       |
                  |  - alertes et journaux CSV     |
                  +--------------+-----------------+
                                 |
                         SSH sortant, port 22
                                 |
                +----------------+----------------+
                |                                 |
        VM Kali Linux                      Autre VM Linux
       sshd + bash -s                       sshd + bash -s
          /proc, df                           /proc, df
                |                                 |
          OK,CPU,RAM,DISK                    OK,CPU,RAM,DISK
```

## Pourquoi cette architecture ?

Le programme tourne sur **Windows** ; Kali fournit les mesures à la demande. Contrairement à une architecture à agent, le collecteur `.sh` n'a pas besoin d'être installé ou planifié dans chaque VM : Windows envoie son contenu dans l'entrée standard de `ssh ... "bash -s"`.

## Déroulement d'un cycle

1. Le programme C charge `config/settings.conf` et `config/machines.csv`.
2. Il vérifie les champs (notamment IPv4 numériques, ports valides et identifiants sûrs).
3. Pour chaque VM, il lance `ssh.exe` avec `BatchMode=yes`, validation stricte de la clé d'hôte et délais de connexion.
4. Le programme reçoit la sortie `OK,CPU,RAM,DISK` du script Bash.
5. Il vérifie les pourcentages (0..100) et compare aux seuils configurés.
6. Il affiche l'état en couleur et enregistre la mesure (CSV).
7. Si l'état a changé, il l'inscrit dans `events.log` et demande éventuellement une notification sonore Windows.
8. En mode `--watch`, il attend puis recommence.

## Code C et code Bash

- `src/sysmonitor.c` : utilisation des fichiers (`fopen`, `fprintf`), structures, boucles, tableaux, parsing et fonctions Windows de console.
- `linux/collect.sh` : lecture de `/proc/stat` deux fois (différence des compteurs), `/proc/meminfo` pour l'utilisation RAM et `df -P /` pour le disque racine.

Le shell utilise une seule seconde d'échantillonnage CPU ; les statistiques ne sont pas des mesures instantanées. La RAM utilisée est définie ici comme `MemTotal - MemAvailable`, ce qui exclut une grande partie de la mémoire récupérable.

## Limites

- Chaque machine est surveillée séquentiellement ; pour de nombreux postes une approche parallèle serait plus rapide.
- L'application est conçue pour un **petit laboratoire local** (maximum 32 machines), pas pour une supervision industrielle.
- Les processus SSH sont lancés via le shell local `_popen` ; **toutes les valeurs de configuration utilisées dans la commande sont strictement validées**. Le chemin du projet est relatif au dossier du programme ; le script est transmis via la redirection d'entrée.
- Les journaux résident sur la machine Windows et ne sont pas destinés à un dépôt Git public.
