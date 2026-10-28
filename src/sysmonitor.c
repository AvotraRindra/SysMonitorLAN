#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
/* SysMonitor LAN - Option B: supervision SSH de VM Linux depuis Windows.
   C11, bibliotheque standard + Win32 (console / temps / chemins).
   Aucune commande manuelle executee sur les VM pendant la collecte. */
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define popen_read _popen
#define pclose_read _pclose
#define make_dir(p) _mkdir(p)
#define sleep_secs(s) Sleep((DWORD)((s) * 1000U))
#define COLLECTOR_PATH "linux\\collect.sh"
#else
#include <unistd.h>
#include <sys/stat.h>
#define popen_read popen
#define pclose_read pclose
#define make_dir(p) mkdir((p), 0755)
#define sleep_secs(s) sleep(s)
#define COLLECTOR_PATH "linux/collect.sh"
#endif

#define MAX_HOSTS 32
#define MAX_LINE 512
#define MAX_COMMAND 4096
#define MAX_NAME 64

typedef struct
{
    char name[MAX_NAME];
    char user[MAX_NAME];
    char ip[32];
    int port;
} Host;
typedef struct
{
    int cpu, ram, disk, interval, timeout, sound;
} Settings;
typedef enum
{
    NORMAL,
    ALERT,
    UNREACHABLE,
    ERROR_STATE
} State;
typedef struct
{
    int cpu, ram, disk;
} Metrics;

static Host hosts[MAX_HOSTS];
static int host_count = 0;
static Settings cfg = {85, 85, 85, 15, 6, 1};

static void trim(char *s)
{
    char *start = s;
    size_t n;
    while (isspace((unsigned char)*start))
        start++;
    if (start != s)
        memmove(s, start, strlen(start) + 1);
    n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1]))
        s[--n] = '\0';
}
static int safe_name(const char *s)
{
    size_t i, n = strlen(s);
    if (n < 1 || n >= MAX_NAME)
        return 0;
    if (!(isalnum((unsigned char)s[0]) || s[0] == '_'))
        return 0;
    for (i = 0; i < n; i++)
        if (!(isalnum((unsigned char)s[i]) || s[i] == '_' || s[i] == '-' || s[i] == '.'))
            return 0;
    return 1;
}
static int valid_ip(const char *ip)
{
    int n = 0, parts = 0;
    const char *p = ip;
    int octets = 0;
    if (!p || !*p || strlen(p) > 15)
        return 0;
    while (*p)
    {
        if (!isdigit((unsigned char)*p))
            return 0;
        n = 0;
        parts = 0;
        while (isdigit((unsigned char)*p))
        {
            n = n * 10 + (*p++ - '0');
            if (++parts > 3 || n > 255)
                return 0;
        }
        octets++;
        if (octets == 4)
            return *p == '\0';
        if (*p++ != '.')
            return 0;
    }
    return 0;
}
static int percent(const char *s, int *out)
{
    char *end;
    long x;
    if (!s || !isdigit((unsigned char)*s))
        return 0;
    errno = 0;
    x = strtol(s, &end, 10);
    if (errno || *end || x < 0 || x > 100)
        return 0;
    *out = (int)x;
    return 1;
}
static int positive_int(const char *s, int lo, int hi, int *out)
{
    char *end;
    long x;
    if (!s || !isdigit((unsigned char)*s))
        return 0;
    errno = 0;
    x = strtol(s, &end, 10);
    if (errno || *end || x < lo || x > hi)
        return 0;
    *out = (int)x;
    return 1;
}
static int read_settings(void)
{
    FILE *f = fopen("config/settings.conf", "r");
    char line[MAX_LINE];
    int ln = 0;
    if (!f)
    {
        fprintf(stderr, "ERREUR : config/settings.conf introuvable.\n");
        return 0;
    }
    while (fgets(line, sizeof(line), f))
    {
        char *sep, *k, *v;
        int value = 0, *dest = NULL, lo = 1, hi = 100;
        ln++;
        if (!strchr(line, '\n') && !feof(f))
        {
            fprintf(stderr, "Parametre trop long ligne %d\n", ln);
            fclose(f);
            return 0;
        }
        trim(line);
        if (!line[0] || line[0] == '#')
            continue;
        sep = strchr(line, '=');
        if (!sep)
        {
            fprintf(stderr, "Format invalide config ligne %d\n", ln);
            fclose(f);
            return 0;
        }
        *sep = 0;
        k = line;
        v = sep + 1;
        trim(k);
        trim(v);
        if (!strcmp(k, "CPU_THRESHOLD"))
            dest = &cfg.cpu;
        else if (!strcmp(k, "RAM_THRESHOLD"))
            dest = &cfg.ram;
        else if (!strcmp(k, "DISK_THRESHOLD"))
            dest = &cfg.disk;
        else if (!strcmp(k, "INTERVAL_SECONDS"))
        {
            dest = &cfg.interval;
            lo = 2;
            hi = 3600;
        }
        else if (!strcmp(k, "SSH_TIMEOUT_SECONDS"))
        {
            dest = &cfg.timeout;
            lo = 1;
            hi = 30;
        }
        else if (!strcmp(k, "ENABLE_SOUND"))
        {
            dest = &cfg.sound;
            lo = 0;
            hi = 1;
        }
        else
        {
            fprintf(stderr, "Parametre inconnu : %s\n", k);
            fclose(f);
            return 0;
        }
        if (!positive_int(v, lo, hi, &value))
        {
            fprintf(stderr, "Valeur incorrecte pour %s (ligne %d)\n", k, ln);
            fclose(f);
            return 0;
        }
        *dest = value;
    }
    fclose(f);
    return 1;
}
static int read_hosts(void)
{
    FILE *f = fopen("config/machines.csv", "r");
    char line[MAX_LINE];
    int ln = 0;
    if (!f)
    {
        fprintf(stderr, "ERREUR : config/machines.csv introuvable.\n");
        return 0;
    }
    host_count = 0;
    while (fgets(line, sizeof(line), f))
    {
        char *columns[4], *p, *sep;
        int i, j, port = 0;
        Host *h;
        ln++;
        if (!strchr(line, '\n') && !feof(f))
        {
            fprintf(stderr, "Ligne trop longue : %d\n", ln);
            fclose(f);
            return 0;
        }
        trim(line);
        if (!line[0] || line[0] == '#')
            continue;
        p = line;
        for (i = 0; i < 3; i++)
        {
            sep = strchr(p, ';');
            if (!sep)
                break;
            *sep = 0;
            columns[i] = p;
            p = sep + 1;
        }
        if (i != 3 || strchr(p, ';'))
        {
            fprintf(stderr, "CSV invalide ligne %d (nom;user;IPv4;port)\n", ln);
            fclose(f);
            return 0;
        }
        columns[3] = p;
        for (i = 0; i < 4; i++)
            trim(columns[i]);
        if (!safe_name(columns[0]) || !safe_name(columns[1]) || !valid_ip(columns[2]) || !positive_int(columns[3], 1, 65535, &port))
        {
            fprintf(stderr, "Machine invalide ligne %d (nom, utilisateur, IPv4, port).\n", ln);
            fclose(f);
            return 0;
        }
        if (host_count >= MAX_HOSTS)
        {
            fprintf(stderr, "Maximum %d machines.\n", MAX_HOSTS);
            fclose(f);
            return 0;
        }
        for (j = 0; j < host_count; j++)
            if (!strcmp(hosts[j].name, columns[0]))
            {
                fprintf(stderr, "Nom de machine en double ligne %d\n", ln);
                fclose(f);
                return 0;
            }
        h = &hosts[host_count++];
        snprintf(h->name, sizeof(h->name), "%s", columns[0]);
        snprintf(h->user, sizeof(h->user), "%s", columns[1]);
        snprintf(h->ip, sizeof(h->ip), "%s", columns[2]);
        h->port = port;
    }
    fclose(f);
    return 1; /* Aucune machine : assistant --setup disponible. */
}
static int parse_metrics(const char *s, Metrics *m)
{
    int values[3], i;
    const char *p;
    const char *end;
    if (strncmp(s, "OK,", 3))
        return 0;
    p = s + 3;
    for (i = 0; i < 3; i++)
    {
        char num[8];
        size_t len;
        end = (i < 2) ? strchr(p, ',') : p + strlen(p);
        if (!end || (i == 2 && strchr(p, ',')))
            return 0;
        len = (size_t)(end - p);
        if (len < 1 || len >= sizeof(num))
            return 0;
        memcpy(num, p, len);
        num[len] = '\0';
        if (!percent(num, &values[i]))
            return 0;
        p = end + (i < 2 ? 1 : 0);
    }
    m->cpu = values[0];
    m->ram = values[1];
    m->disk = values[2];
    return 1;
}
static State evaluate(Metrics m)
{
    return m.cpu >= cfg.cpu || m.ram >= cfg.ram || m.disk >= cfg.disk ? ALERT : NORMAL;
}
static const char *state_name(State s)
{
    switch (s)
    {
    case NORMAL:
        return "NORMAL";
    case ALERT:
        return "ALERTE";
    case UNREACHABLE:
        return "INACCESSIBLE";
    default:
        return "ERREUR";
    }
}
static void timestamp(char *out, size_t n)
{
    time_t now = time(NULL);
    struct tm *p = localtime(&now);
    if (!p || !strftime(out, n, "%Y-%m-%d %H:%M:%S", p))
        snprintf(out, n, "DATE_INCONNUE");
}
static void color_state(State s)
{
#ifdef _WIN32
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (out == INVALID_HANDLE_VALUE)
        return;
    SetConsoleTextAttribute(out, (WORD)(s == NORMAL ? 10 : (s == ALERT ? 12 : 14)));
#else
    fputs(s == NORMAL ? "\033[32m" : (s == ALERT ? "\033[31m" : "\033[33m"), stdout);
#endif
}
static void reset_color(void)
{
#ifdef _WIN32
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (out != INVALID_HANDLE_VALUE)
        SetConsoleTextAttribute(out, 7);
#else
    fputs("\033[0m", stdout);
#endif
}
static void alert_sound(void)
{
    if (!cfg.sound)
        return;
#ifdef _WIN32
    MessageBeep(MB_ICONEXCLAMATION);
#else
    fputc('\a', stdout);
    fflush(stdout);
#endif
}
static int ensure_logs(void)
{
    if (make_dir("logs") != 0 && errno != EEXIST)
    {
        perror("logs");
        return 0;
    }
    if (make_dir("logs/states") != 0 && errno != EEXIST)
    {
        perror("logs/states");
        return 0;
    }
    return 1;
}
static void csv_log(const Host *h, State st, Metrics m)
{
    FILE *f;
    char dt[32];
    timestamp(dt, sizeof(dt));
    f = fopen("logs/measurements.csv", "a+");
    if (!f)
    {
        perror("measurements.csv");
        return;
    }
    if (fseek(f, 0, SEEK_END) == 0 && ftell(f) == 0)
        fputs("date_heure;machine;ip;cpu;ram;disque;etat\n", f);
    fprintf(f, "%s;%s;%s;", dt, h->name, h->ip);
    if (st == UNREACHABLE || st == ERROR_STATE)
        fprintf(f, ";;;");
    else
        fprintf(f, "%d;%d;%d;", m.cpu, m.ram, m.disk);
    fprintf(f, "%s\n", state_name(st));
    fclose(f);
}
static int get_last_state(const Host *h, char *out, size_t n)
{
    char path[160];
    FILE *f;
    snprintf(path, sizeof(path), "logs/states/%.63s.txt", h->name);
    f = fopen(path, "r");
    if (!f)
        return 0;
    if (!fgets(out, (int)n, f))
    {
        fclose(f);
        return 0;
    }
    fclose(f);
    trim(out);
    return 1;
}
static void handle_transition(const Host *h, State st)
{
    char previous[40] = {0}, dt[32], path[160];
    const char *newstate = state_name(st);
    int known = get_last_state(h, previous, sizeof(previous));
    FILE *f;
    if (known && !strcmp(previous, newstate))
        return;
    timestamp(dt, sizeof(dt));
    if ((f = fopen("logs/events.log", "a")))
    {
        fprintf(f, "%s;%s;%s;%s\n", dt, h->name, known ? previous : "PREMIERE_MESURE", newstate);
        fclose(f);
    }
    else
        perror("events.log");
    snprintf(path, sizeof(path), "logs/states/%.63s.txt", h->name);
    if ((f = fopen(path, "w")))
    {
        fprintf(f, "%s\n", newstate);
        fclose(f);
    }
    else
        perror("etat machine");
    if (st != NORMAL)
        alert_sound();
}
/* Cle dediee creee par l'assistant Windows, sinon comportement SSH habituel. */
static const char *ssh_identity_args(void)
{
#ifdef _WIN32
    static char args[1200];
    char key[MAX_PATH * 2];
    const char *home = getenv("USERPROFILE");
    if (!home || !*home || strlen(home) > 1000)
        return "";
    snprintf(key, sizeof(key), "%s\\.ssh\\sysmonitor_ed25519", home);
    if (GetFileAttributesA(key) != INVALID_FILE_ATTRIBUTES)
    {
        snprintf(args, sizeof(args), "-i \"%s\" -o IdentitiesOnly=yes", key);
        return args;
    }
#endif
    return "";
}

static State collect_ssh(const Host *h, Metrics *m, char *why, size_t whyn)
{
    char command[MAX_COMMAND], line[MAX_LINE], detail[192] = {0};
    FILE *f;
    int rc, have = 0, invalid = 0;
    Metrics temp;
    /* Les champs issus du CSV sont valides contre l'injection cmd.exe. */
    snprintf(command, sizeof(command),
             "ssh -T -p %d -o BatchMode=yes -o StrictHostKeyChecking=yes "
             "-o ConnectTimeout=%d -o ServerAliveInterval=5 -o ServerAliveCountMax=1 "
             "-o LogLevel=ERROR %s %s@%s \"bash -s\" < \"%s\" 2>&1",
             h->port, cfg.timeout, ssh_identity_args(), h->user, h->ip, COLLECTOR_PATH);
    f = popen_read(command, "r");
    if (!f)
    {
        snprintf(why, whyn, "Impossible de lancer ssh");
        return UNREACHABLE;
    }
    while (fgets(line, sizeof(line), f))
    {
        trim(line);
        if (!strncmp(line, "OK,", 3))
        {
            if (parse_metrics(line, &temp))
            {
                have = 1;
                *m = temp;
            }
            else
                invalid = 1;
        }
        else if (line[0] && !detail[0])
            snprintf(detail, sizeof(detail), "%.180s", line);
    }
    rc = pclose_read(f);
    if (rc != 0)
    {
        snprintf(why, whyn, "SSH a echoue (code %d): %.130s", rc, detail[0] ? detail : "verifier reseau, cle et serveur SSH");
        return UNREACHABLE;
    }
    if (invalid || !have)
    {
        snprintf(why, whyn, "Mesures absentes ou invalides : %.130s", detail);
        return ERROR_STATE;
    }
    *why = 0;
    return evaluate(*m);
}
static void print_result(const Host *h, State st, Metrics m, const char *detail)
{
    printf("%-17s %-15s ", h->name, h->ip);
    color_state(st);
    printf("%-12s", state_name(st));
    reset_color();
    if (st == NORMAL || st == ALERT)
    {
        printf(" CPU=%3d%% RAM=%3d%% DISQUE=%3d%%", m.cpu, m.ram, m.disk);
        if (st == ALERT)
            printf(" [seuils CPU:%d RAM:%d DISK:%d]", cfg.cpu, cfg.ram, cfg.disk);
    }
    else
        printf(" %s", detail);
    putchar('\n');
}
static int run_cycle(void)
{
    int i, norm = 0, alrt = 0, off = 0, err = 0;
    char dt[32];
    timestamp(dt, sizeof(dt));
    printf("\n===== SYSMONITOR LAN | %s =====\n", dt);
    for (i = 0; i < host_count; i++)
    {
        Metrics m = {0, 0, 0};
        char why[256] = {0};
        State s = collect_ssh(&hosts[i], &m, why, sizeof(why));
        print_result(&hosts[i], s, m, why);
        csv_log(&hosts[i], s, m);
        handle_transition(&hosts[i], s);
        if (s == NORMAL)
            norm++;
        else if (s == ALERT)
            alrt++;
        else if (s == UNREACHABLE)
            off++;
        else
            err++;
    }
    printf("Bilan: %d normal | %d alerte | %d inaccessible | %d erreur\n", norm, alrt, off, err);
    fflush(stdout);
    return off || err ? 2 : alrt ? 1
                                 : 0;
}
static void usage(const char *name)
{
    printf("SysMonitor LAN - Option B (C + Bash, Windows pilote Linux par SSH)\n"
           "Usage : %s [--setup|--check|--list|--once|--watch|--demo|--selftest|--help]\n",
           name);
}
static int selftest(void)
{
    Metrics m;
    int failures = 0;
#define CHECK(expr)                        \
    do                                     \
    {                                      \
        if (!(expr))                       \
        {                                  \
            printf("[ECHEC] %s\n", #expr); \
            failures++;                    \
        }                                  \
    } while (0)
    CHECK(valid_ip("192.168.43.219"));
    CHECK(!valid_ip("192.168.0.999"));
    CHECK(!valid_ip("127.0.0.1 & del x"));
    CHECK(!valid_ip("1.2.3"));
    CHECK(safe_name("kali_user-1"));
    CHECK(!safe_name("user&whoami"));
    CHECK(!safe_name("-oCommand"));
    CHECK(parse_metrics("OK,10,50,100", &m));
    CHECK(m.cpu == 10 && m.ram == 50 && m.disk == 100);
    CHECK(!parse_metrics("OK,10,250,30", &m));
    CHECK(!parse_metrics("OK,10,20,30;bad", &m));
    m.cpu = 85;
    m.ram = 20;
    m.disk = 5;
    CHECK(evaluate(m) == ALERT);
    m.cpu = 10;
    m.ram = 30;
    m.disk = 50;
    CHECK(evaluate(m) == NORMAL);
    printf("Autotests C : %s\n", failures ? "ECHEC" : "OK");
    return failures ? 1 : 0;
#undef CHECK
}
static void demo(void)
{
    Host a = {"Kali-VM", "rindra", "192.168.43.219", 22};
    Metrics m = {12, 40, 60};
    puts("Mode demonstration : aucune connexion ni fichier modifie.");
    print_result(&a, NORMAL, m, "");
    m.cpu = 93;
    m.ram = 88;
    m.disk = 61;
    print_result(&a, ALERT, m, "");
    print_result(&a, UNREACHABLE, m, "SSH inaccessible (simulation)");
}
static void list_hosts(void)
{
    int i;
    printf("Machines configurees (%d) :\n", host_count);
    for (i = 0; i < host_count; i++)
        printf("  %s -> %s@%s:%d\n", hosts[i].name, hosts[i].user, hosts[i].ip, hosts[i].port);
}
static int run_mode(const char *mode)
{
    if (!strcmp(mode, "--check"))
    {
        puts("Configuration : OK");
        list_hosts();
        if (host_count == 0)
            puts("Aucune VM ajoutee : lancez SysMonitor.exe --setup.");
        printf("Seuils CPU=%d%% RAM=%d%% DISK=%d%%, intervalle=%ds, SSH timeout=%ds, son=%d\n", cfg.cpu, cfg.ram, cfg.disk, cfg.interval, cfg.timeout, cfg.sound);
        puts("Verification distante non effectuee. Tester ensuite --once.");
        return 0;
    }
    if (!strcmp(mode, "--list"))
    {
        list_hosts();
        return 0;
    }
    if (!strcmp(mode, "--once") && host_count == 0)
    {
        puts("Aucune VM. Lancez SysMonitor.exe --setup pour ajouter une machine.");
        return 2;
    }
    if (!strcmp(mode, "--once"))
        return run_cycle();
    if (!strcmp(mode, "--watch") && host_count == 0)
    {
        puts("Aucune VM. Lancez --setup d'abord.");
        return 2;
    }
    if (!strcmp(mode, "--watch"))
    {
        puts("Surveillance active, Ctrl+C pour arreter.");
        for (;;)
        {
            run_cycle();
            sleep_secs((unsigned)cfg.interval);
        }
    }
    usage("SysMonitor.exe");
    return 2;
}
/* Assistant de premiere installation : aucun mot de passe conserve.
   Tous les arguments utilises pour les appels externes proviennent de
   champs valides (nom/utilisateur/IP/port) ou de chemins Windows cites. */
#ifdef _WIN32
static int ask_input(const char *question, char *out, size_t max)
{
    printf("%s", question);
    fflush(stdout);
    if (!fgets(out, (int)max, stdin))
        return 0;
    if (!strchr(out, '\n') && !feof(stdin))
    {
        int c;
        while ((c = getchar()) != '\n' && c != EOF)
        {
        }
        puts("Saisie trop longue.");
        return 0;
    }
    trim(out);
    return 1;
}

static int run_external(char *command)
{
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD rc = 1;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    if (!CreateProcessA(NULL, command, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
    {
        printf("[ERREUR] Impossible de lancer la commande (Windows: %lu).\n", (unsigned long)GetLastError());
        return 0;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    if (!GetExitCodeProcess(pi.hProcess, &rc))
        rc = 1;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return rc == 0;
}

static int ssh_paths(char *priv, size_t privcap, char *pub, size_t pubcap)
{
    const char *home = getenv("USERPROFILE");
    char dir[1100];
    if (!home || !*home || strlen(home) > 900)
    {
        puts("[ERREUR] USERPROFILE introuvable ou trop long.");
        return 0;
    }
    snprintf(dir, sizeof(dir), "%s\\.ssh", home);
    if (!CreateDirectoryA(dir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        printf("[ERREUR] Impossible de creer %s\n", dir);
        return 0;
    }
    if (snprintf(priv, privcap, "%s\\sysmonitor_ed25519", dir) >= (int)privcap ||
        snprintf(pub, pubcap, "%s.pub", priv) >= (int)pubcap)
        return 0;
    return 1;
}

static int confirm_step(const char *question)
{
    char answer[32];
    return ask_input(question, answer, sizeof(answer)) &&
           (!strcmp(answer, "o") || !strcmp(answer, "O") ||
            !strcmp(answer, "oui") || !strcmp(answer, "OUI"));
}

static int append_machine(const Host *h)
{
    FILE *f = fopen("config/machines.csv", "a");
    if (!f)
    {
        perror("machines.csv");
        return 0;
    }
    /* Ligne vide acceptee : evite une ligne collee a un commentaire existant. */
    {
        int good = fprintf(f, "\n%s;%s;%s;%d\n", h->name, h->user, h->ip, h->port) >= 0;
        if (fclose(f) != 0)
            good = 0;
        if (!good)
        {
            puts("[ERREUR] Echec d'ecriture de machines.csv.");
            return 0;
        }
    }
    return 1;
}

static int setup_wizard(void)
{
    char action[32], name[64], user[64], ip[32], portstr[16];
    char key[1200], pub[1250], cmd[3500], remote[200];
    int existing = 0, port = 22, i;
    Host h;
    puts("\n======= ASSISTANT SYSMONITOR LAN =======");
    puts("Windows supervise des VM Linux par SSH.");
    puts("Le mot de passe Linux peut etre demande pendant CETTE installation.");
    puts("Il ne sera ni enregistre ni ajoute a machines.csv.\n");
    puts("1. Ajouter et configurer une nouvelle VM");
    if (host_count)
        puts("2. Reconfigurer l'acces SSH d'une VM existante");
    puts("0. Annuler");
    if (!ask_input("Choix : ", action, sizeof(action)))
        return 2;
    if (!strcmp(action, "0"))
        return 0;
    if (!strcmp(action, "2") && host_count)
    {
        char buf[32];
        int k;
        for (i = 0; i < host_count; i++)
            printf("%d. %s (%s@%s:%d)\n", i + 1, hosts[i].name, hosts[i].user, hosts[i].ip, hosts[i].port);
        if (!ask_input("Numero : ", buf, sizeof(buf)) || !positive_int(buf, 1, host_count, &k))
        {
            puts("Numero invalide.");
            return 2;
        }
        h = hosts[k - 1];
        existing = 1;
    }
    else if (!strcmp(action, "1"))
    {
        if (host_count >= MAX_HOSTS)
        {
            puts("Maximum de VM atteint.");
            return 2;
        }
        if (!ask_input("Nom de la VM (ex: Kali-VM) : ", name, sizeof(name)) || !safe_name(name))
        {
            puts("Nom invalide. Utilisez lettres, chiffres, tiret, point ou _.");
            return 2;
        }
        for (i = 0; i < host_count; i++)
            if (!strcmp(hosts[i].name, name))
            {
                puts("Cette VM existe deja : choisissez 'Reconfigurer'.");
                return 2;
            }
        if (!ask_input("Utilisateur Linux (ex: rindra) : ", user, sizeof(user)) || !safe_name(user))
        {
            puts("Utilisateur invalide.");
            return 2;
        }
        if (!ask_input("IPv4 de la VM (ex: 192.168.43.219) : ", ip, sizeof(ip)) || !valid_ip(ip))
        {
            puts("Adresse IPv4 invalide.");
            return 2;
        }
        if (!ask_input("Port SSH [22] : ", portstr, sizeof(portstr)))
            return 2;
        if (portstr[0] && !positive_int(portstr, 1, 65535, &port))
        {
            puts("Port invalide.");
            return 2;
        }
        memset(&h, 0, sizeof(h));
        snprintf(h.name, sizeof(h.name), "%s", name);
        snprintf(h.user, sizeof(h.user), "%s", user);
        snprintf(h.ip, sizeof(h.ip), "%s", ip);
        h.port = port;
    }
    else
    {
        puts("Choix invalide.");
        return 2;
    }
    if (!ssh_paths(key, sizeof(key), pub, sizeof(pub)))
        return 2;
    printf("\nMachine cible : %s -> %s@%s:%d\n", h.name, h.user, h.ip, h.port);
    puts("Verifier sur la VM : systemctl is-active ssh et hostname -I.");
    puts("Empreinte a comparer sur Kali :");
    puts("  ssh-keygen -lf /etc/ssh/ssh_host_ed25519_key.pub");
    if (!confirm_step("L'IP est correcte et vous pouvez verifier l'empreinte [o/N] ? "))
    {
        puts("Annule avant toute connexion.");
        return 1;
    }
    if (GetFileAttributesA(key) == INVALID_FILE_ATTRIBUTES)
    {
        puts("\n[1/4] Creation d'une CLE DEDIEE a SysMonitor dans votre profil Windows.");
        puts("Choisissez une phrase secrete (recommandee avec ssh-agent) ou");
        puts("Entree deux fois pour une cle sans phrase secrete sur un PC de demonstration protege.");
        snprintf(cmd, sizeof(cmd), "ssh-keygen -t ed25519 -f \"%s\" -C SysMonitorLAN", key);
        if (!run_external(cmd))
        {
            puts("[ERREUR] Generation de la cle impossible.");
            return 2;
        }
    }
    else
        puts("\n[1/4] Cle dediee existante reutilisee, sans ecrasement.");
    if (GetFileAttributesA(pub) == INVALID_FILE_ATTRIBUTES)
    {
        puts("[ERREUR] Cle publique introuvable. Verifiez la paire de cles SSH.");
        return 2;
    }
    printf("\n[2/4] Transfert UNIQUEMENT de la cle publique vers %s.\n", h.ip);
    puts("Au premier acces SSH, comparez l'empreinte de Kali AVANT de taper yes.");
    puts("Il est normal que Kali demande son mot de passe pendant l'installation.");
    snprintf(remote, sizeof(remote), "sysmonitor_setup_%s.pub", h.name);
    snprintf(cmd, sizeof(cmd), "scp -P %d -o StrictHostKeyChecking=ask -o ConnectTimeout=%d \"%s\" %s@%s:%s",
             h.port, cfg.timeout, pub, h.user, h.ip, remote);
    if (!run_external(cmd))
    {
        puts("[ERREUR] Copie de la cle publique impossible : verifier SSH/reseau/identifiants.");
        return 2;
    }
    puts("\n[3/4] Autorisation de la cle publique dans ~/.ssh/authorized_keys.");
    snprintf(cmd, sizeof(cmd), "ssh -p %d -o StrictHostKeyChecking=yes %s@%s \"umask 077; mkdir -p ~/.ssh; chmod 700 ~/.ssh; touch ~/.ssh/authorized_keys; if ! grep -Fqx -f ~/%s ~/.ssh/authorized_keys; then cat ~/%s >> ~/.ssh/authorized_keys; fi; chmod 600 ~/.ssh/authorized_keys; rm -f ~/%s\"",
             h.port, h.user, h.ip, remote, remote, remote);
    if (!run_external(cmd))
    {
        puts("[ERREUR] Impossible d'autoriser la cle sur Kali. Le fichier public peut rester dans HOME.");
        return 2;
    }
    puts("\n[4/4] Verification de l'acces non-interactif (BatchMode=yes).");
    snprintf(cmd, sizeof(cmd), "ssh -T -p %d -i \"%s\" -o IdentitiesOnly=yes -o BatchMode=yes -o StrictHostKeyChecking=yes -o ConnectTimeout=%d %s@%s \"echo SYSMONITOR_SSH_OK\"",
             h.port, key, cfg.timeout, h.user, h.ip);
    if (!run_external(cmd))
    {
        puts("[ERREUR] Connexion sans mot de passe impossible.");
        puts("Si votre cle a une phrase secrete, chargez-la dans ssh-agent puis reessayez --setup.");
        puts("Aucune nouvelle VM n'a ete enregistree.");
        return 2;
    }
    if (!existing)
    {
        if (!append_machine(&h))
            return 2;
        hosts[host_count++] = h;
    }
    puts("\n[SUCCES] La VM est configuree et autorisee.");
    puts("Prochaine commande : SysMonitor.exe --once");
    return 0;
}
#else
static int setup_wizard(void)
{
    puts("L'assistant est reserve a Windows. Les tests C/Linux restent disponibles.");
    return 2;
}
#endif

static int go_to_project_dir(void)
{
#ifdef _WIN32
    char path[4096];
    DWORD n = GetModuleFileNameA(NULL, path, (DWORD)sizeof(path));
    char *slash;
    if (n == 0 || n >= sizeof(path))
        return 0;
    slash = strrchr(path, '\\');
    if (!slash)
        return 0;
    *slash = 0;
    return SetCurrentDirectoryA(path) != 0;
#else
    return 1;
#endif
}
int main(int argc, char **argv)
{
    int ret;
    char choice[32];
    if (!go_to_project_dir())
    {
        fputs("Impossible de trouver le dossier du programme.\n", stderr);
        return 2;
    }
    if (argc > 1 && !strcmp(argv[1], "--help"))
    {
        usage(argv[0]);
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "--selftest"))
        return selftest();
    if (argc > 1 && !strcmp(argv[1], "--demo"))
    {
        demo();
        return 0;
    }
    if (!read_settings() || !read_hosts())
        return 2;
    {
        FILE *collector = fopen(COLLECTOR_PATH, "r");
        if (!collector)
        {
            fprintf(stderr, "ERREUR : %s introuvable.\n", COLLECTOR_PATH);
            return 2;
        }
        fclose(collector);
    }
    if (!ensure_logs())
        return 2;
    if (argc > 1 && !strcmp(argv[1], "--setup"))
        return setup_wizard();
    if (argc > 1)
        return run_mode(argv[1]);
    for (;;)
    {
        puts("\n===== SYSMONITOR LAN =====\n1. Diagnostiquer une fois\n2. Surveiller en continu\n3. Lister les VM\n4. Verifier la configuration\n5. Configuration guidee SSH / ajout VM\n0. Quitter");
        printf("Votre choix : ");
        fflush(stdout);
        if (!fgets(choice, sizeof(choice), stdin))
            return 0;
        trim(choice);
        if (!strcmp(choice, "0"))
            return 0;
        if (!strcmp(choice, "1"))
            ret = run_cycle();
        else if (!strcmp(choice, "2"))
            ret = run_mode("--watch");
        else if (!strcmp(choice, "3"))
        {
            list_hosts();
            ret = 0;
        }
        else if (!strcmp(choice, "4"))
            ret = run_mode("--check");
        else if (!strcmp(choice, "5"))
            ret = setup_wizard();
        else
        {
            puts("Choix invalide.");
            continue;
        }
        if (ret)
            printf("Code de diagnostic : %d (0=normal, 1=alerte, 2=erreur/inaccessible)\n", ret);
    }
}
