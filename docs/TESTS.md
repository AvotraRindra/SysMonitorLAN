# Plan de tests et soutenance — SysMonitor LAN Option B

## Tests automatisés

**Linux :**

```bash
bash tests/test_collector.sh
bash tests/test_integration.sh
```

Ils vérifient la syntaxe et la sortie du collecteur Bash, la compilation C sur Linux et les scénarios simulés (normal, seuil dépassé, SSH indisponible, sortie invalide), ainsi que l'anti-répétition et les journaux.

**Windows :**

```bat
tests\test_windows.bat
```

Compile l'application et lance les autotests C, la démonstration et la vérification locale. Cela ne prouve pas la connectivité réelle : il faut tester SSH sur le réseau du laboratoire.

## Validation du nouvel assistant (Windows)

| ID | Action | Résultat attendu |
| --- | --- | --- |
| S01 | Premier lancement `--check` avec CSV vide | Aucune VM, suggestion `--setup` |
| S02 | `--setup` > Ajouter la VM | Demande nom, utilisateur, IPv4, port |
| S03 | Nom/IP invalides | Refus sans écriture CSV |
| S04 | VM autorisée + serveur SSH actif | Copie de clé publique, autorisation, réponse `SYSMONITOR_SSH_OK` |
| S05 | `--once` après S04 | CPU, RAM, disque réels sans mot de passe |
| S06 | Refaire `--setup` sur une machine connue | Reconfiguration sans doublon CSV |
| S07 | Machine inaccessible | Pas de succès mensonger ni de nouvelle entrée CSV |
| S08 | `--demo` sans réseau | Démonstration simulée autonome |

**Note :** les tests automatisés Linux ne prouvent pas que l'assistant Windows fonctionne sur le matériel du professeur. Une vérification native Windows est nécessaire.

## Tests manuels à réaliser sur Windows + Kali

| ID | Manipulation | Résultat attendu |
| --- | --- | --- |
| T01 | `SysMonitor.exe --check` | configuration valide, VM affichées si ajoutees |
| T02 | `SysMonitor.exe --demo` | NORMAL, ALERTE et INACCESSIBLE simulés |
| T03 | `SysMonitor.exe --once` avec VM active | mesures CPU/RAM/disque réelles |
| T04 | Modifier `RAM_THRESHOLD=1` puis `--once` | état ALERTE et entrée events.log (si RAM >= 1 %) |
| T05 | Répéter `--once` sans changement | pas d'alerte dupliquée dans events.log |
| T06 | Couper SSH sur la VM autorisée (puis le réactiver) | INACCESSIBLE, pas de fausses valeurs CPU/RAM/DISK |
| T07 | Deux VM configurées | deux lignes par cycle, bilan correct |
| T08 | `--watch` sur plusieurs cycles | actualisation et historique CSV |
| T09 | `Ctrl+C` | arrêt du programme |
| T10 | Retirer temporairement l'accès réseau d'une VM | erreur gérée et affichée |

**Après T04 : remettre `RAM_THRESHOLD=85`** ou votre seuil normal. Les tests peuvent aussi être réalisés en adaptant les seuils sans générer de charge importante sur le PC.

## Pour la soutenance

1. Présenter le problème : un administrateur suit plusieurs VM Linux depuis Windows.
2. Présenter le schéma : Windows/C -> SSH -> Kali/Bash.
3. Montrer `--list` et `--once` avec de vraies valeurs.
4. Montrer une alerte en abaissant temporairement un seuil.
5. Montrer les journaux `logs/measurements.csv` et `logs/events.log`.
6. Présenter les limites : le son dépend de Windows, la connectivité SSH doit fonctionner, projet de labo et non outil industriel.

Privilégier des captures de vos propres VM et démontrer qu'une coupure réseau est reconnue comme **INACCESSIBLE**.
