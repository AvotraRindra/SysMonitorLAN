# Sécurité — SysMonitor LAN

- Utiliser ce projet uniquement sur des machines Linux **dont l'accès vous est autorisé**.
- **Aucun mot de passe** n'est stocké dans `machines.csv`, dans le programme ou dans les journaux. Un mot de passe peut être demandé **par le client SSH** lors de la configuration initiale.
- Le programme génère une **clé SSH dédiée** `%USERPROFILE%\.ssh\sysmonitor_ed25519` ; **la clé privée reste sur Windows** et ne doit jamais être copiée sur GitHub, une clé USB partagée ou dans les VM. Seule la clé publique `.pub` est transférée.
- Une clé privée sans phrase secrète facilite les démonstrations mais augmente le risque en cas d'accès au compte Windows. Pour un usage prolongé, utiliser une phrase secrète et un agent SSH (`ssh-agent`).
- L'utilisateur doit **comparer l'empreinte** du serveur SSH avec celle vérifiée sur sa VM avant d'accepter la première connexion. Pendant la surveillance, `StrictHostKeyChecking=yes` est imposé.
- Le serveur OpenSSH de Windows n'est pas nécessaire : Windows utilise le **client**. Les droits du compte Kali peuvent rester non privilégiés (lecture de `/proc`, `df`).
- Le contenu Bash de collecte ne modifie pas la VM. Les noms, utilisateurs, IPv4 et ports du CSV sont validés pour éviter l'injection de commandes.
- Les journaux locaux peuvent contenir les noms/IP des machines ; ils sont ignorés dans le dépôt.
- Ne pas utiliser un compte Linux administrateur pour ce petit laboratoire si un compte ordinaire suffit.
