# SysMonitor LAN

**Application de supervision d'un petit parc informatique Linux, pilotée depuis Windows.**

SysMonitor LAN permet à un poste administrateur **Windows** de contrôler les ressources de plusieurs machines virtuelles ou ordinateurs **Linux** (Kali Linux, Debian, Ubuntu, etc.) à travers un réseau local. Le programme affiche les mesures du **processeur (CPU)**, de la **mémoire RAM** et de l'**espace disque**, détecte les dépassements de seuils et enregistre un historique.

Le projet est développé en **langage C**, avec un collecteur **Shell Bash (`.sh`)** exécuté à distance grâce à **SSH**. Il s'agit d'un projet pédagogique d'administration système et réseau (niveau L1), conçu pour un laboratoire de petite taille.

## Fonctionnalités

- Diagnostic ponctuel ou surveillance continue d'une ou plusieurs machines Linux.
- Collecte de l'utilisation du CPU, de la RAM et de la partition racine `/`.
- Affichage des états **NORMAL**, **ALERTE**, **INACCESSIBLE** ou **ERREUR** dans la console Windows.
- Alertes en couleur et notification sonore Windows facultative, lors des changements d'état.
- Historique des mesures dans un fichier CSV et journal des changements d'état.
- Ajout et configuration des machines avec un assistant SSH interactif.
- Authentification automatique par clé SSH après la configuration initiale.
- Mode de démonstration et autotests du programme C, sans machine distante.

## Prérequis

**Sur le poste Windows (machine de supervision) :**

- Windows 10 ou une version compatible ;
- un compilateur C sous Windows : **GCC/MinGW-w64** ou **MSVC** (`cl`) ;
- le **client OpenSSH**, avec les commandes `ssh`, `scp` et `ssh-keygen` disponibles ;
- un accès réseau à la ou aux machines Linux à superviser.

**Sur chaque machine Linux supervisée :**

- Bash et les commandes système usuelles utilisées par le collecteur (`/proc`, `df`, etc.) ;
- le **serveur SSH** en fonctionnement (`sshd`) ;
- un compte Linux autorisé à se connecter par SSH.

> Le serveur SSH **n'est pas nécessaire sur Windows** : c'est Windows qui se connecte aux machines Linux. Une connexion Internet n'est pas nécessaire pour la supervision si tous les prérequis sont déjà installés et que les machines communiquent sur le réseau.

## Installation et lancement rapide

### 1. Récupérer le projet

Clonez le dépôt ou téléchargez son archive ZIP, puis extrayez-la dans un dossier de votre choix sous Windows. Ouvrez **l'Invite de commandes (CMD)** dans le dossier contenant `build.bat` et `src\`.

### 2. Compiler l'application

```bat
build.bat
```

Le script détecte GCC ou MSVC et compile le programme C. Si la compilation réussit, le fichier **`SysMonitor.exe`** est créé à la racine du projet.

> Si le message « Aucun compilateur C trouvé » apparaît, installez ou configurez GCC/MinGW-w64 ou ouvrez le terminal de développement Visual Studio. Le dépôt contient le code source, pas un exécutable Windows précompilé.

### 3. Démarrer SysMonitor LAN

```bat
SysMonitor.exe
```

Le **menu principal interactif** s'affiche :

```text
===== SYSMONITOR LAN =====
1. Diagnostiquer une fois
2. Surveiller en continu
3. Lister les VM
4. Verifier la configuration
5. Configuration guidee SSH / ajout VM
0. Quitter
Votre choix :
```

Saisissez le numéro d'une option puis appuyez sur **Entrée**.

**À la toute première utilisation, sélectionnez l'option `5` pour enregistrer et configurer une machine Linux.** Vous pourrez ensuite utiliser `1` pour le diagnostic ou `2` pour la surveillance en continu.

## Explication des options du menu

### 1 — Diagnostiquer une fois

SysMonitor établit une connexion SSH avec **chaque VM enregistrée**, récupère les mesures CPU/RAM/disque, affiche l'état de la machine, compare les valeurs aux seuils configurés et écrit la mesure dans l'historique CSV.

Cette option exécute **un seul cycle** et revient ensuite au menu. Elle convient à une vérification rapide ou à une démonstration devant un enseignant.

### 2 — Surveiller en continu

SysMonitor répète le diagnostic de toutes les machines à intervalles réguliers (5 secondes par défaut entre deux cycles complets). Il affiche les changements d'état et conserve les résultats dans les journaux.

Pour arrêter la surveillance, utilisez **`Ctrl+C`** : cela interrompt l'application et ramène à l'invite de commandes Windows. Relancez `SysMonitor.exe` pour retrouver le menu.

### 3 — Lister les VM

Affiche les machines déjà enregistrées dans `config/machines.csv` : nom, utilisateur Linux, adresse IPv4 et port SSH. Cette option **ne teste pas** leur disponibilité sur le réseau.

### 4 — Vérifier la configuration

Vérifie la lecture et la validité des paramètres, affiche la liste des VM enregistrées et les seuils de supervision.

Cette opération **ne réalise pas de connexion SSH**. Pour vérifier la communication avec les machines, utilisez ensuite l'option `1`.

### 5 — Configuration guidée SSH / ajout VM

Permet d'ajouter une nouvelle machine Linux ou de reconfigurer l'authentification SSH d'une machine déjà enregistrée. L'assistant vous demande :

1. le **nom** à attribuer à la VM ;
2. le **nom d'utilisateur Linux** ;
3. son **adresse IPv4** joignable depuis Windows ;
4. le **port SSH**, généralement `22`.

Il peut créer une **clé SSH dédiée** (`%USERPROFILE%\.ssh\sysmonitor_ed25519`), transmettre **uniquement sa clé publique** à Linux et l'ajouter aux clés autorisées. Lors des premiers échanges, le mot de passe du compte Linux peut être demandé. **Aucun mot de passe n'est enregistré dans le CSV.**

Avant d'accepter une clé d'hôte lors de la première connexion, **vérifiez son empreinte directement sur la VM Linux**. L'assistant vérifie ensuite la connexion SSH automatique et n'enregistre la nouvelle machine que si le test réussit.

> Si la clé privée possède une phrase secrète, chargez-la dans `ssh-agent` afin que la supervision puisse fonctionner sans interaction. Ne publiez jamais la clé privée sur GitHub.

### 0 — Quitter

Ferme proprement le menu de l'application.

## Préparer une VM Linux pour la première utilisation

Une seule préparation sur la VM est nécessaire avant de pouvoir l'ajouter dans le menu. Dans le terminal Linux :

```bash
hostname -I
systemctl is-active ssh
```

La première commande permet de retrouver l'adresse IP ; la deuxième indique si le serveur SSH fonctionne. S'il est déjà installé mais arrêté :

```bash
sudo systemctl start ssh
```

Pour relever l'empreinte de la clé d'hôte SSH et la comparer à celle affichée lors de la connexion depuis Windows :

```bash
ssh-keygen -lf /etc/ssh/ssh_host_ed25519_key.pub
```

La machine doit être accessible depuis Windows via le réseau de VirtualBox ou celui de votre laboratoire. Un réseau **NAT avec redirection de port**, **pont** ou **hôte uniquement** peut convenir selon votre configuration, à condition que Windows puisse réellement joindre l'adresse et le port SSH choisis. L'assistant ne peut pas activer à distance un serveur SSH inexistant ou inaccessible.

**Parcours conseillé pour une première démonstration :**

1. Préparez une VM Linux dont le serveur SSH est actif.
2. Dans CMD Windows : `build.bat`, puis `SysMonitor.exe`.
3. Choisissez **`5`** et suivez l'assistant jusqu'à la confirmation de la connexion.
4. Choisissez **`3`** pour vérifier que la VM figure dans la liste.
5. Choisissez **`1`** pour afficher ses mesures réelles.
6. Choisissez **`2`** pour observer la surveillance en continu et les alertes.

## Configuration et journaux

Les machines enregistrées sont définies dans **`config/machines.csv`**. Le fichier est volontairement sans machine préconfigurée au départ afin de pouvoir tester le projet sur n'importe quel réseau autorisé.

Chaque enregistrement utilise le format :

```text
NOM;UTILISATEUR_LINUX;IPv4;PORT_SSH
Kali-VM;utilisateur;192.168.1.50;22
```

**Cet exemple est fictif.** Utilisez l'adresse réelle de votre VM. Le menu `5` peut créer la ligne automatiquement.

Les paramètres de **`config/settings.conf`** comprennent :

| Paramètre | Valeur initiale | Description |
|---|---:|---|
| `CPU_THRESHOLD` | `85` | Seuil d'alerte CPU (%) |
| `RAM_THRESHOLD` | `85` | Seuil d'alerte RAM (%) |
| `DISK_THRESHOLD` | `85` | Seuil d'alerte disque (%) |
| `INTERVAL_SECONDS` | `5` | Pause entre les cycles complets (secondes) |
| `SSH_TIMEOUT_SECONDS` | `6` | Délai de connexion SSH (secondes) |
| `ENABLE_SOUND` | `1` | Notification sonore Windows (`1` oui / `0` non) |

Les mesures sont conservées dans **`logs/measurements.csv`**. Les changements d'état sont inscrits dans **`logs/events.log`**. Les valeurs sont laissées vides pour une machine inaccessible ou une mesure invalide : elles ne sont pas artificiellement remplacées par `0 %`.

## Tests et utilisation en ligne de commande (facultatif)

Le **menu interactif** est le mode d'utilisation recommandé. Il existe aussi des paramètres pour les utilisateurs souhaitant automatiser les tests :

| Commande | Effet |
|---|---|
| `SysMonitor.exe --demo` | Affiche des exemples simulés, sans SSH |
| `SysMonitor.exe --selftest` | Teste la logique principale du programme C |
| `SysMonitor.exe --check` | Vérifie la configuration locale |
| `SysMonitor.exe --list` | Liste les VM configurées |
| `SysMonitor.exe --setup` | Ouvre directement l'assistant SSH |
| `SysMonitor.exe --once` | Lance un diagnostic réel unique |
| `SysMonitor.exe --watch` | Lance la surveillance réelle en continu |

Les modes `--demo` et `--selftest` peuvent être essayés **sans VM Linux**. Ils ne prouvent pas la connectivité réseau : pour valider une supervision réelle, configurez une VM et lancez l'option `1`.

Les instructions de tests supplémentaires sont disponibles dans [`docs/TESTS.md`](docs/TESTS.md).

## Organisation du dépôt

```text
SysMonitorLAN/
├── README.md                 # Présentation et utilisation
├── build.bat                 # Compilation de l'application C sous Windows
├── run.bat                   # Lancement, compilation si nécessaire
├── src/
│   └── sysmonitor.c         # Menu, collecte distante, alertes, journaux
├── linux/
│   └── collect.sh           # Collecte CPU, RAM et disque des machines Linux
├── config/
│   ├── machines.csv         # Liste des machines à superviser
│   └── settings.conf        # Seuils et paramètres
├── logs/                    # Historique local (généré à l'exécution)
├── tests/                   # Tests C/Bash et scénarios simulés
├── SECURITY.md              # Précautions de sécurité
└── LICENSE                  # Licence du projet
```

Le script `linux/collect.sh` est **transmis par SSH à l'exécution**. Il n'est pas nécessaire de copier ni de lancer manuellement ce fichier sur chaque VM à chaque cycle.

## Dépannage courant

| Problème | Vérification conseillée |
|---|---|
| `build.bat` ne trouve pas de compilateur | Installer/configurer GCC ou MSVC dans le `PATH` |
| `SysMonitor.exe` ne démarre pas | Exécuter `build.bat` puis ouvrir CMD dans le dossier du projet |
| Aucune VM n'apparaît | Commencer par l'option `5` |
| `Permission denied (publickey,password)` | Vérifier l'utilisateur, la clé autorisée sur Linux et l'option `5` |
| `Connection refused` | Vérifier que `sshd` fonctionne **sur Linux** et écoute au bon port |
| `Connection timed out` | Contrôler l'adresse IP et la configuration du réseau virtuel |
| `Host key verification failed` | Vérifier l'empreinte depuis Linux et corriger la configuration de confiance sans désactiver la vérification |
| Aucune alerte sonore | Vérifier les réglages audio Windows et `ENABLE_SOUND` ; l'alerte visuelle reste disponible |
