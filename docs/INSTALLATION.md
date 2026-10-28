# Installation guidée pour le professeur et l'équipe (Windows -> Linux)

## Ce qui est automatisé

La **configuration SSH** et l'enregistrement de la machine sont assistés par `SysMonitor.exe --setup`. Le programme crée une paire de clés si besoin, copie la clé publique sur Linux, l'autorise pour le compte Linux choisi, valide la connexion sans mot de passe, puis ajoute la machine au CSV. L'assistant ne conserve aucun mot de passe.

Le professeur doit cependant disposer **d'un PC Windows avec un compilateur C, d'une VM Linux accessible et de son accès SSH initial**. Aucun logiciel ne peut configurer une VM à laquelle le poste administrateur n'a aucun accès préalable.

## A. Préparer Linux une seule fois

Dans votre VM Kali/Debian/Ubuntu, avec votre compte autorisé :

```bash
hostname -I
systemctl is-active ssh
ssh-keygen -lf /etc/ssh/ssh_host_ed25519_key.pub
```

Si OpenSSH Server existe mais est arrêté :

```bash
sudo systemctl start ssh
```

Si `sshd` est absent, installer le paquet `openssh-server` depuis une source autorisée avant de poursuivre. Vérifier que la VM est joignable depuis Windows (adaptateur VirtualBox en NAT avec redirection, pont ou réseau hôte approprié ; l'IP doit être celle joignable par Windows).

## B. Compiler et tester sur Windows

Ouvrir **CMD** dans le dossier extrait. **Il n'est pas nécessaire d'être administrateur pour la supervision ou la création de la clé dans le profil Windows.**

```bat
ssh -V
build.bat
SysMonitor.exe --selftest
SysMonitor.exe --demo
```

Si GCC ou MSVC n'est pas disponible, installer/configurer un compilateur C Windows. Ce ZIP fournit les **sources**, pas un exécutable Windows.

## C. Configurer par assistant

```bat
SysMonitor.exe --setup
```

1. Choisir `1. Ajouter et configurer une nouvelle VM`.
2. Fournir un nom sans espaces (ex. `Kali-VM`), le compte Linux, l'IPv4 et le port 22.
3. Comparer l'empreinte SSH présentée lors de la première connexion avec l'empreinte affichée **directement sur la VM**, puis accepter seulement si elles correspondent.
4. Accepter la génération d'une clé dédiée `%USERPROFILE%\.ssh\sysmonitor_ed25519` si elle n'existe pas. Ne jamais écraser les clés existantes. Une phrase secrète nécessite `ssh-agent` pour l'exécution automatisée ; pour un laboratoire de démonstration à faible risque, une clé sans phrase secrète peut être utilisée sur un poste sécurisé.
5. Le transfert de la clé publique et son ajout à `~/.ssh/authorized_keys` se font automatiquement ; SSH peut demander le **mot de passe Linux** durant ces opérations initiales.
6. Le test final doit afficher `SYSMONITOR_SSH_OK`. Alors la nouvelle VM est ajoutée à `config/machines.csv`.

Si le test final échoue, l'assistant ne déclare pas la machine configurée. Vérifier réseau, compte Linux, permissions `~/.ssh` et agent de clé, puis réessayer.

**Surveillance sans commandes manuelles dans Kali :** après l'installation, les collectes sont initiées et affichées depuis Windows.

## D. Ajouter un autre Linux ou reconfigurer

Relancer `SysMonitor.exe --setup` et choisir `1` pour ajouter une autre VM, ou `2` pour autoriser de nouveau la clé sur une machine déjà dans le CSV. Une seule clé publique Windows peut être autorisée sur plusieurs VM de laboratoire. Pour corriger une IP, modifier d'abord `config/machines.csv`.

## E. Exécuter

```bat
SysMonitor.exe --check
SysMonitor.exe --once
SysMonitor.exe --watch
```

- `--demo` et `--selftest` fonctionnent sans clé et sans VM ; ils ne représentent pas un test réseau réel.
- Dans `logs/measurements.csv`, les données invalides / VM inaccessibles laissent les pourcentages **vides**.
- Arrêter `--watch` par `Ctrl+C`.

## F. Problèmes fréquents

| Problème | Action conseillée |
|---|---|
| `ssh` introuvable sur Windows | Activer OpenSSH **Client** sur Windows ; `sshd` Windows n'est pas requis |
| `gcc` ou `cl` introuvable | Installer/configurer un compilateur C Windows |
| `Connection refused` | Activer `sshd` **sur Kali** et vérifier le port |
| `Connection timed out` | Vérifier IP actuelle, mode réseau VirtualBox, accès au réseau |
| `Host key verification failed` | Contrôler l'empreinte via Kali ; ne pas désactiver la vérification |
| `Permission denied (publickey,password)` | Vérifier l'utilisateur Kali et sa politique d'authentification |
| `BatchMode=yes` refuse la clé | Vérifier clé publique dans `authorized_keys`, permissions et ssh-agent si phrase secrète |
| `scp` refuse le transfert avec certains anciens serveurs | Vérifier le sous-système SFTP sur Kali ; une solution manuelle d'autorisation est possible |
| Pas de bip | Vérifier sons Windows ; l'alerte visuelle reste fonctionnelle |

## G. Alternative manuelle si l'assistant n'arrive pas à copier la clé

Sur Windows, **sans publier la clé privée**, afficher uniquement la clé publique :

```bat
type "%USERPROFILE%\.ssh\sysmonitor_ed25519.pub"
```

Sur la VM, un utilisateur autorisé peut ajouter **cette seule ligne publique** à `~/.ssh/authorized_keys` avec des permissions sûres (`~/.ssh` en `700`, fichier en `600`). Tester ensuite depuis Windows :

```bat
ssh -i "%USERPROFILE%\.ssh\sysmonitor_ed25519" -o IdentitiesOnly=yes -o BatchMode=yes UTILISATEUR@IP_DU_KALI "echo SYSMONITOR_SSH_OK"
```

Ne jamais envoyer le fichier `sysmonitor_ed25519` sans extension `.pub`.
