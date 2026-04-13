/*
 * festum.c — testa simplex
 *
 * Pars subiecti pro 'bash'. Testa interactiva et scriptoria.
 * C99 POSIX, nullae dependentiae externae.
 *
 * Facultates:
 *   - Mandata interna: cd, exit, export, unset, echo
 *   - Tubuli (|)
 *   - Redirectiones (>, >>, <)
 *   - Variabiles ($VAR, ${VAR}, $?)
 *   - Citatio (simplici et duplici)
 *   - Semicolones et commentaria (#)
 *   - Cursus in fundo (&)
 *   - Signa (SIGINT, SIGCHLD)
 *   - Completio tabulaturae (pliculae et mandata in PATH)
 *   - Editio lineae (cursores, historia, Ctrl-A/E/K/U/W/D/L)
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <termios.h>
#include <dirent.h>

/* ============================================================
 * I. LIMITES ET CONSTANTIAE
 * ============================================================ */

#define LIM_LINEA       4096
#define LIM_SIGNA       256
#define LIM_TUBULI      32
#define LIM_VIA         4096
#define LIM_HISTORIA    256
#define LIM_COMPLETIONES 512

/* ============================================================
 * II. TYPI
 * ============================================================ */

/* redirectio — descriptio redirectionis */
typedef struct {
    int genus;              /* 0=nihil, 1=exitus(>), 2=adde(>>), 3=initus(<) */
    char *via;              /* iter plicae */
} redirectio_t;

/* mandatum — mandatum simplex dissectum */
typedef struct {
    char *argumenta[LIM_SIGNA];
    int num_argumenta;
    redirectio_t in_red;    /* < */
    redirectio_t ex_red;    /* > vel >> */
} mandatum_t;

/* linea_dissecta — catena mandatorum cum tubulis */
typedef struct {
    mandatum_t mandata[LIM_TUBULI];
    int num_mandata;
    int in_fundo;           /* & */
} linea_dissecta_t;

/* ============================================================
 * III. STATUS GLOBALIS
 * ============================================================ */

static int status_ultimus;          /* $? */
static volatile sig_atomic_t infans_mutatus;

/* ============================================================
 * IV. SIGNA
 * ============================================================ */

static void tracta_sigchld(int sig)
{
    (void)sig;
    infans_mutatus = 1;
}

static void tracta_sigint(int sig)
{
    (void)sig;
    /* testa ipsa ignorat — infantes accipiunt */
    write(STDOUT_FILENO, "\n", 1);
}

/* mete infantes in fundo ne zombi fiant */
static void mete_infantes(void)
{
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0)
        (void)0;
    infans_mutatus = 0;
}

/* ============================================================
 * V. AUXILIARIA
 * ============================================================ */

/*
 * expande_variabiles — expandit $VAR, ${VAR}, $? in chorda.
 * reddit chordam novam (vocans liberet per free()).
 */
static char *expande_variabiles(const char *fons)
{
    static char alveus[LIM_LINEA];
    int i   = 0;
    int o   = 0;
    int lon = (int)strlen(fons);

    while (i < lon && o < LIM_LINEA - 1) {
        if (fons[i] == '\\' && i + 1 < lon) {
            /* character effugitus */
            alveus[o++] = fons[++i];
            i++;
            continue;
        }
        if (fons[i] == '\'') {
            /* citatio simplex — nihil expanditur */
            i++;
            while (i < lon && fons[i] != '\'' && o < LIM_LINEA - 1)
                alveus[o++] = fons[i++];
            if (i < lon)
                i++; /* praetermitte ' */
            continue;
        }
        if (fons[i] != '$') {
            alveus[o++] = fons[i++];
            continue;
        }

        /* $... */
        i++;
        if (i >= lon) {
            alveus[o++] = '$';
            break;
        }

        if (fons[i] == '?') {
            /* $? — status ultimus */
            char num[16];
            snprintf(num, sizeof(num), "%d", status_ultimus);
            for (int k = 0; num[k] && o < LIM_LINEA - 1; k++)
                alveus[o++] = num[k];
            i++;
            continue;
        }

        if (fons[i] == '$') {
            /* $$ — PID */
            char num[16];
            snprintf(num, sizeof(num), "%d", (int)getpid());
            for (int k = 0; num[k] && o < LIM_LINEA - 1; k++)
                alveus[o++] = num[k];
            i++;
            continue;
        }

        /* ${NOMEN} vel $NOMEN */
        int cum_bracchiis = 0;
        if (fons[i] == '{') {
            cum_bracchiis = 1;
            i++;
        }

        char nomen[256];
        int n = 0;
        while (i < lon && n < 255) {
            if (cum_bracchiis) {
                if (fons[i] == '}') {
                    i++;
                    break;
                }
            } else {
                if (!isalnum((unsigned char)fons[i]) && fons[i] != '_')
                    break;
            }
            nomen[n++] = fons[i++];
        }
        nomen[n] = '\0';

        const char *val = getenv(nomen);
        if (val) {
            for (int k = 0; val[k] && o < LIM_LINEA - 1; k++)
                alveus[o++] = val[k];
        }
    }

    alveus[o] = '\0';
    return strdup(alveus);
}

/*
 * disseca_signa — dividit lineam in signa (tokens).
 * tractat citationes simplices et duplices, effugitiones.
 * reddit numerum signorum.
 */
static int disseca_signa(const char *linea, char **signa, int lim)
{
    int n   = 0;
    int i   = 0;
    int lon = (int)strlen(linea);

    while (i < lon && n < lim) {
        /* praetermitte spatia */
        while (i < lon && isspace((unsigned char)linea[i]))
            i++;
        if (i >= lon || linea[i] == '#')
            break;

        /* operatores speciales */
        if (linea[i] == '|' || linea[i] == ';' || linea[i] == '&') {
            char op[3] = {linea[i], '\0', '\0'};
            if (linea[i] == '>' && i + 1 < lon && linea[i+1] == '>') {
                op[1] = '>';
                i++;
            }
            i++;
            signa[n++] = strdup(op);
            continue;
        }
        if (linea[i] == '>' || linea[i] == '<') {
            char op[3] = {linea[i], '\0', '\0'};
            if (linea[i] == '>' && i + 1 < lon && linea[i+1] == '>') {
                op[1] = '>';
                i++;
            }
            i++;
            signa[n++] = strdup(op);
            continue;
        }

        /* signum ordinarium — cum citationibus */
        char alveus[LIM_LINEA];
        int o = 0;

        while (i < lon && o < LIM_LINEA - 1) {
            char c = linea[i];

            /* spatium vel operator terminat signum */
            if (
                !( c == '\'' || c == '"' || c == '\\') &&
                (
                    isspace((unsigned char)c) ||
                    c == '|' || c == ';' || c == '&' ||
                    c == '>' || c == '<' || c == '#'
                )
            )
                break;

            if (c == '\\' && i + 1 < lon) {
                alveus[o++] = linea[++i];
                i++;
                continue;
            }

            if (c == '\'') {
                i++;
                while (i < lon && linea[i] != '\'')
                    alveus[o++] = linea[i++];
                if (i < lon)
                    i++;
                continue;
            }

            if (c == '"') {
                i++;
                while (i < lon && linea[i] != '"') {
                    if (linea[i] == '\\' && i + 1 < lon) {
                        i++;
                        alveus[o++] = linea[i++];
                    } else {
                        alveus[o++] = linea[i++];
                    }
                }
                if (i < lon)
                    i++;
                continue;
            }

            alveus[o++] = linea[i++];
        }

        alveus[o] = '\0';
        if (o > 0)
            signa[n++] = strdup(alveus);
    }

    return n;
}

/*
 * disseca_lineam — dissecit lineam in structuram linea_dissecta_t.
 * reddit 0 si bene, -1 si error.
 */
static int disseca_lineam(const char *linea_cruda, linea_dissecta_t *ld)
{
    memset(ld, 0, sizeof(*ld));

    /* expande variabiles primum */
    char *expansa = expande_variabiles(linea_cruda);
    if (!expansa)
        return -1;

    /* disseca in signa */
    char *signa[LIM_SIGNA];
    int num_signa = disseca_signa(expansa, signa, LIM_SIGNA);
    free(expansa);

    if (num_signa == 0)
        return 0;

    int m = 0;  /* index mandati currentis */
    ld->num_mandata = 1;

    for (int i = 0; i < num_signa; i++) {
        char *s = signa[i];

        if (strcmp(s, "|") == 0) {
            /* tubulus novus */
            free(s);
            m++;
            if (m >= LIM_TUBULI) {
                fprintf(stderr, "festum: nimis multi tubuli\n");
                goto purga;
            }
            ld->num_mandata = m + 1;
            continue;
        }

        if (strcmp(s, ";") == 0) {
            /* semicolon — non supportatur in hac versione simplici */
            /* tractamus ut finem lineae */
            free(s);
            break;
        }

        if (strcmp(s, "&") == 0) {
            free(s);
            ld->in_fundo = 1;
            continue;
        }

        if (strcmp(s, ">") == 0) {
            free(s);
            if (i + 1 < num_signa) {
                ld->mandata[m].ex_red.genus = 1;
                ld->mandata[m].ex_red.via   = signa[++i];
            }
            continue;
        }

        if (strcmp(s, ">>") == 0) {
            free(s);
            if (i + 1 < num_signa) {
                ld->mandata[m].ex_red.genus = 2;
                ld->mandata[m].ex_red.via   = signa[++i];
            }
            continue;
        }

        if (strcmp(s, "<") == 0) {
            free(s);
            if (i + 1 < num_signa) {
                ld->mandata[m].in_red.genus = 3;
                ld->mandata[m].in_red.via   = signa[++i];
            }
            continue;
        }

        /* argumentum ordinarium */
        mandatum_t *md = &ld->mandata[m];
        if (md->num_argumenta < LIM_SIGNA - 1)
            md->argumenta[md->num_argumenta++] = s;
        else
            free(s);
    }

    return 0;

purga:
    for (int i = 0; i < num_signa; i++)
        free(signa[i]);
    return -1;
}

/*
 * libera_lineam — liberat memoriam lineae dissectae.
 */
static void libera_lineam(linea_dissecta_t *ld)
{
    for (int i = 0; i < ld->num_mandata; i++) {
        mandatum_t *m = &ld->mandata[i];
        for (int j = 0; j < m->num_argumenta; j++)
            free(m->argumenta[j]);
        if (m->in_red.via)
            free(m->in_red.via);
        if (m->ex_red.via)
            free(m->ex_red.via);
    }
}

/* ============================================================
 * VI. TILDE
 * ============================================================ */

/*
 * expande_tildem — expandit ~ ad directorium domesticum.
 * reddit chordam novam (vocans liberet) vel NULL si non expandendum.
 */
static char *expande_tildem(const char *via)
{
    if (via[0] != '~')
        return NULL;

    const char *domus = getenv("HOME");
    if (!domus) {
        struct passwd *pw = getpwuid(getuid());
        if (pw)
            domus = pw->pw_dir;
    }
    if (!domus)
        return NULL;

    if (via[1] == '\0')
        return strdup(domus);

    if (via[1] == '/') {
        char alveus[LIM_VIA];
        snprintf(alveus, sizeof(alveus), "%s%s", domus, via + 1);
        return strdup(alveus);
    }

    return NULL;
}

/* ============================================================
 * VII. MANDATA INTERNA
 * ============================================================ */

/* reddit 1 si mandatum internum tractatum est, 0 aliter */
static int exsequere_internum(mandatum_t *m)
{
    if (m->num_argumenta == 0)
        return 1;

    const char *nomen = m->argumenta[0];

    /* exit */
    if (strcmp(nomen, "exit") == 0) {
        int codex = 0;
        if (m->num_argumenta > 1)
            codex = atoi(m->argumenta[1]);
        libera_lineam(&(linea_dissecta_t){.mandata = {*m}, .num_mandata = 1});
        exit(codex);
    }

    /* cd */
    if (strcmp(nomen, "cd") == 0) {
        const char *dest = NULL;
        char *expandata  = NULL;

        if (m->num_argumenta < 2) {
            dest = getenv("HOME");
            if (!dest)
                dest = "/";
        } else {
            expandata = expande_tildem(m->argumenta[1]);
            dest      = expandata ? expandata : m->argumenta[1];
        }

        if (chdir(dest) != 0) {
            fprintf(stderr, "festum: cd: %s: %s\n", dest, strerror(errno));
            status_ultimus = 1;
        } else {
            status_ultimus = 0;
        }
        free(expandata);
        return 1;
    }

    /* export — NOMEN=PRETIUM */
    if (strcmp(nomen, "export") == 0) {
        for (int i = 1; i < m->num_argumenta; i++) {
            char *par = m->argumenta[i];
            char *eq  = strchr(par, '=');
            if (eq) {
                *eq = '\0';
                setenv(par, eq + 1, 1);
                *eq = '=';
            } else {
                /* export sine pretio — nihil agit */
            }
        }
        status_ultimus = 0;
        return 1;
    }

    /* unset */
    if (strcmp(nomen, "unset") == 0) {
        for (int i = 1; i < m->num_argumenta; i++)
            unsetenv(m->argumenta[i]);
        status_ultimus = 0;
        return 1;
    }

    /* echo */
    if (strcmp(nomen, "echo") == 0) {
        for (int i = 1; i < m->num_argumenta; i++) {
            if (i > 1)
                putchar(' ');
            fputs(m->argumenta[i], stdout);
        }
        putchar('\n');
        status_ultimus = 0;
        return 1;
    }

    /* pwd */
    if (strcmp(nomen, "pwd") == 0) {
        char alveus[LIM_VIA];
        if (getcwd(alveus, sizeof(alveus)))
            printf("%s\n", alveus);
        else
            perror("festum: pwd");
        status_ultimus = 0;
        return 1;
    }

    return 0;
}

/* ============================================================
 * VIII. EXECUTIO
 * ============================================================ */

/*
 * applica_redirectiones — aperit plicas et dirigit fd.
 * vocatur in processu filio post fork().
 */
static void applica_redirectiones(mandatum_t *m)
{
    if (m->in_red.genus == 3) {
        int fd = open(m->in_red.via, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "festum: %s: %s\n", m->in_red.via, strerror(errno));
            _exit(1);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    if (m->ex_red.genus == 1) {
        int fd = open(m->ex_red.via, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            fprintf(stderr, "festum: %s: %s\n", m->ex_red.via, strerror(errno));
            _exit(1);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    } else if (m->ex_red.genus == 2) {
        int fd = open(m->ex_red.via, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd < 0) {
            fprintf(stderr, "festum: %s: %s\n", m->ex_red.via, strerror(errno));
            _exit(1);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
}

/*
 * exsequere_lineam — exsequitur lineam dissectam.
 * tractat tubulos, redirectiones, fundos.
 */
static void exsequere_lineam(linea_dissecta_t *ld)
{
    if (ld->num_mandata == 0)
        return;

    /* mandatum simplex sine tubulis — proba internum */
    if (ld->num_mandata == 1 && !ld->in_fundo) {
        if (exsequere_internum(&ld->mandata[0]))
            return;
    }

    /* mandatum simplex sine tubulis — fork + exec */
    if (ld->num_mandata == 1 && ld->mandata[0].num_argumenta > 0) {
        mandatum_t *m = &ld->mandata[0];
        m->argumenta[m->num_argumenta] = NULL;

        pid_t pid = fork();
        if (pid < 0) {
            perror("festum: fork");
            status_ultimus = 1;
            return;
        }

        if (pid == 0) {
            /* filius */
            signal(SIGINT, SIG_DFL);
            applica_redirectiones(m);
            execvp(m->argumenta[0], m->argumenta);
            fprintf(
                stderr, "festum: %s: %s\n",
                m->argumenta[0], strerror(errno)
            );
            _exit(127);
        }

        /* parens */
        if (ld->in_fundo) {
            printf("[%d]\n", (int)pid);
            status_ultimus = 0;
        } else {
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status))
                status_ultimus = WEXITSTATUS(status);
            else
                status_ultimus = 128;
        }
        return;
    }

    /* tubuli — catena mandatorum */
    int num = ld->num_mandata;
    int tubi[LIM_TUBULI - 1][2];
    pid_t pids[LIM_TUBULI];

    /* crea tubulos */
    for (int i = 0; i < num - 1; i++) {
        if (pipe(tubi[i]) < 0) {
            perror("festum: pipe");
            status_ultimus = 1;
            return;
        }
    }

    /* crea filios */
    for (int i = 0; i < num; i++) {
        mandatum_t *m = &ld->mandata[i];
        m->argumenta[m->num_argumenta] = NULL;

        pids[i] = fork();
        if (pids[i] < 0) {
            perror("festum: fork");
            status_ultimus = 1;
            return;
        }

        if (pids[i] == 0) {
            /* filius */
            signal(SIGINT, SIG_DFL);

            /* redirectio tubi */
            if (i > 0) {
                dup2(tubi[i-1][0], STDIN_FILENO);
            }
            if (i < num - 1) {
                dup2(tubi[i][1], STDOUT_FILENO);
            }

            /* claude omnes tubos */
            for (int j = 0; j < num - 1; j++) {
                close(tubi[j][0]);
                close(tubi[j][1]);
            }

            applica_redirectiones(m);
            execvp(m->argumenta[0], m->argumenta);
            fprintf(
                stderr, "festum: %s: %s\n",
                m->argumenta[0], strerror(errno)
            );
            _exit(127);
        }
    }

    /* parens claudit omnes tubos */
    for (int i = 0; i < num - 1; i++) {
        close(tubi[i][0]);
        close(tubi[i][1]);
    }

    /* exspecta omnes filios */
    if (!ld->in_fundo) {
        for (int i = 0; i < num; i++) {
            int status;
            waitpid(pids[i], &status, 0);
            if (i == num - 1) {
                if (WIFEXITED(status))
                    status_ultimus = WEXITSTATUS(status);
                else
                    status_ultimus = 128;
            }
        }
    }
}

/* ============================================================
 * IX. INCITAMENTUM (PROMPT)
 * ============================================================ */

/*
 * longitudo_incitamenti — reddit longitudinem incitamenti in characteribus.
 */
static int longitudo_incitamenti(void)
{
    char dir[LIM_VIA];
    const char *domus = getenv("HOME");

    if (!getcwd(dir, sizeof(dir)))
        strcpy(dir, "?");

    if (domus && strncmp(dir, domus, strlen(domus)) == 0) {
        size_t ld = strlen(domus);
        if (dir[ld] == '\0')
            return (int)strlen("festum ~$ ");
        if (dir[ld] == '/')
            return (int)(strlen("festum ~$ ") + strlen(dir + ld));
    }
    return (int)(strlen("festum $ ") + strlen(dir));
}

/*
 * scribe_incitamentum — scribit incitamentum ad stderr.
 */
static void scribe_incitamentum(void)
{
    char dir[LIM_VIA];
    const char *domus = getenv("HOME");

    if (!getcwd(dir, sizeof(dir)))
        strcpy(dir, "?");

    if (domus && strncmp(dir, domus, strlen(domus)) == 0) {
        size_t ld = strlen(domus);
        if (dir[ld] == '\0') {
            fprintf(stderr, "festum ~$ ");
        } else if (dir[ld] == '/') {
            fprintf(stderr, "festum ~%s$ ", dir + ld);
        } else {
            fprintf(stderr, "festum %s$ ", dir);
        }
    } else {
        fprintf(stderr, "festum %s$ ", dir);
    }
}

/* ============================================================
 * X. HISTORIA
 * ============================================================ */

static char *historia[LIM_HISTORIA];
static int num_historia;

static void adde_historiam(const char *linea)
{
    if (!linea[0])
        return;
    /* ne duplices ultimam */
    if (num_historia > 0 && strcmp(historia[num_historia - 1], linea) == 0)
        return;

    if (num_historia >= LIM_HISTORIA) {
        free(historia[0]);
        memmove(historia, historia + 1, (LIM_HISTORIA - 1) * sizeof(char *));
        num_historia--;
    }
    historia[num_historia++] = strdup(linea);
}

/* ============================================================
 * XI. COMPLETIO TABULATURAE
 * ============================================================ */

/*
 * extrahe_verbum — extrahit verbum sub cursore pro completione.
 * reddit initium verbi in linea. *lon_verbi = longitudo.
 */
static int extrahe_verbum(const char *linea, int cursor, int *lon_verbi)
{
    int init = cursor;
    while (init > 0 && linea[init - 1] != ' ')
        init--;
    *lon_verbi = cursor - init;
    return init;
}

/*
 * est_positio_mandati — reddit 1 si cursor in prima positione mandati est
 * (ante primum spatium vel post | vel ;).
 */
static int est_positio_mandati(const char *linea, int init_verbi)
{
    int i = init_verbi - 1;
    while (i >= 0 && linea[i] == ' ')
        i--;
    if (i < 0)
        return 1;
    if (linea[i] == '|' || linea[i] == ';')
        return 1;
    return 0;
}

/*
 * comple_plicas — complet nomina plicarum.
 * reddit numerum completionum inventarum.
 */
static int comple_plicas(
    const char *praefixum, int lon,
    char **completiones, int lim
) {
    int n = 0;
    char directorium[LIM_VIA] = ".";
    const char *basis = praefixum;
    int lon_basis = lon;

    /* si praefixum continet '/', separa directorium */
    const char *ult_sep = NULL;
    for (int i = lon - 1; i >= 0; i--) {
        if (praefixum[i] == '/') {
            ult_sep = praefixum + i;
            break;
        }
    }

    if (ult_sep) {
        int lon_dir = (int)(ult_sep - praefixum);
        if (lon_dir == 0) {
            strcpy(directorium, "/");
        } else {
            if (lon_dir >= LIM_VIA)
                lon_dir = LIM_VIA - 1;
            memcpy(directorium, praefixum, lon_dir);
            directorium[lon_dir] = '\0';
        }
        basis     = ult_sep + 1;
        lon_basis = lon - lon_dir - 1;
    }

    DIR *d = opendir(directorium);
    if (!d)
        return 0;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && n < lim) {
        if (ent->d_name[0] == '.' && (lon_basis == 0 || basis[0] != '.'))
            continue;
        if (lon_basis > 0 && strncmp(ent->d_name, basis, lon_basis) != 0)
            continue;

        char via_plena[LIM_VIA];
        struct stat st;

        if (ult_sep) {
            snprintf(
                via_plena, sizeof(via_plena), "%s/%s",
                directorium, ent->d_name
            );
        } else {
            snprintf(via_plena, sizeof(via_plena), "%s", ent->d_name);
        }

        /* adde '/' si directorium */
        char nomen[LIM_VIA];
        if (ult_sep) {
            snprintf(
                nomen, sizeof(nomen), "%.*s/%s",
                (int)(ult_sep - praefixum), praefixum, ent->d_name
            );
        } else {
            snprintf(nomen, sizeof(nomen), "%s", ent->d_name);
        }

        char stat_via[LIM_VIA];
        snprintf(
            stat_via, sizeof(stat_via), "%s/%s",
            directorium, ent->d_name
        );
        if (stat(stat_via, &st) == 0 && S_ISDIR(st.st_mode)) {
            size_t l = strlen(nomen);
            if (l + 1 < LIM_VIA) {
                nomen[l]     = '/';
                nomen[l + 1] = '\0';
            }
        }

        completiones[n++] = strdup(nomen);
    }
    closedir(d);
    return n;
}

/*
 * comple_mandata — complet nomina mandatorum in PATH.
 * reddit numerum completionum.
 */
static int comple_mandata(
    const char *praefixum, int lon,
    char **completiones, int lim
) {
    int n = 0;
    const char *via_env = getenv("PATH");
    if (!via_env)
        return 0;

    char via_copia[LIM_LINEA];
    strncpy(via_copia, via_env, sizeof(via_copia) - 1);
    via_copia[sizeof(via_copia) - 1] = '\0';

    char *salvatum = NULL;
    char *dir      = strtok_r(via_copia, ":", &salvatum);
    while (dir && n < lim) {
        DIR *d = opendir(dir);
        if (d) {
            struct dirent *ent;
            while ((ent = readdir(d)) != NULL && n < lim) {
                if (ent->d_name[0] == '.')
                    continue;
                if (strncmp(ent->d_name, praefixum, lon) != 0)
                    continue;

                /* ne duplices */
                int iam = 0;
                for (int i = 0; i < n; i++) {
                    if (strcmp(completiones[i], ent->d_name) == 0) {
                        iam = 1;
                        break;
                    }
                }
                if (!iam)
                    completiones[n++] = strdup(ent->d_name);
            }
            closedir(d);
        }
        dir = strtok_r(NULL, ":", &salvatum);
    }

    /* adde etiam mandata interna */
    static const char *interna[] = {
        "cd", "exit", "export", "unset", "echo", "pwd", NULL
    };
    for (int i = 0; interna[i] && n < lim; i++) {
        if (strncmp(interna[i], praefixum, lon) == 0) {
            int iam = 0;
            for (int j = 0; j < n; j++) {
                if (strcmp(completiones[j], interna[i]) == 0) {
                    iam = 1;
                    break;
                }
            }
            if (!iam)
                completiones[n++] = strdup(interna[i]);
        }
    }

    return n;
}

/*
 * praefixum_commune — invenit praefixum commune omnium completionum.
 * reddit longitudinem praefixi communis post praefixum originale.
 */
static int praefixum_commune(char **completiones, int n, int lon_praef)
{
    if (n <= 0)
        return 0;
    int lon_prim = (int)strlen(completiones[0]);
    int communis = lon_prim;

    for (int i = 1; i < n; i++) {
        int lon_i = (int)strlen(completiones[i]);
        if (lon_i < communis)
            communis = lon_i;
        for (int j = lon_praef; j < communis; j++) {
            if (completiones[0][j] != completiones[i][j]) {
                communis = j;
                break;
            }
        }
    }
    return communis - lon_praef;
}

/* ============================================================
 * XII. EDITOR LINEAE (TERMIOS)
 * ============================================================ */

static struct termios terminus_pristinus;
static int terminus_mutatus;

static void restitue_terminum(void)
{
    if (terminus_mutatus) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminus_pristinus);
        terminus_mutatus = 0;
    }
}

static void pone_modum_crudum(void)
{
    if (!terminus_mutatus) {
        tcgetattr(STDIN_FILENO, &terminus_pristinus);
        terminus_mutatus = 1;
        atexit(restitue_terminum);
    }

    struct termios crudus = terminus_pristinus;
    crudus.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    crudus.c_oflag &= ~(OPOST);
    crudus.c_cflag |= CS8;
    crudus.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    crudus.c_cc[VMIN]  = 1;
    crudus.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &crudus);
}

/* scribe chordam ad terminalem */
static void term_scribe(const char *s, int n)
{
    (void)write(STDERR_FILENO, s, n);
}

static void term_scribe_s(const char *s)
{
    term_scribe(s, (int)strlen(s));
}

/*
 * repinge_lineam — repingit lineam ab initio post incitamentum.
 */
static void repinge_lineam(
    const char *linea, int lon, int cursor,
    int lon_incit
) {
    char alveus[64];
    /* move ad initium lineae */
    term_scribe_s("\r");
    /* purga lineam */
    term_scribe_s("\x1b[K");
    /* rescribe incitamentum */
    scribe_incitamentum();
    /* scribe lineam */
    term_scribe(linea, lon);
    /* pone cursorem */
    int pos = lon_incit + cursor;
    snprintf(alveus, sizeof(alveus), "\r\x1b[%dC", pos);
    term_scribe_s(alveus);
}

/*
 * lege_lineam_interactivam — legit lineam cum editione plena.
 * reddit chordam staticam vel NULL (EOF).
 */
static char *lege_lineam_interactivam(void)
{
    static char linea[LIM_LINEA];
    int lon     = 0;
    int cursor  = 0;
    int lon_incit = longitudo_incitamenti();
    int idx_hist  = num_historia;
    char salvata[LIM_LINEA] = "";

    scribe_incitamentum();
    linea[0] = '\0';

    pone_modum_crudum();

    for (;;) {
        unsigned char c;
        int r = (int)read(STDIN_FILENO, &c, 1);
        if (r <= 0) {
            /* EOF */
            restitue_terminum();
            if (lon == 0)
                return NULL;
            linea[lon] = '\0';
            return linea;
        }

        /* Ctrl-D — EOF si linea vacua */
        if (c == 4) {
            if (lon == 0) {
                restitue_terminum();
                return NULL;
            }
            continue;
        }

        /* Ctrl-C — purga lineam */
        if (c == 3) {
            term_scribe_s("^C\r\n");
            restitue_terminum();
            scribe_incitamentum();
            lon      = 0;
            cursor   = 0;
            linea[0] = '\0';
            idx_hist = num_historia;
            pone_modum_crudum();
            continue;
        }

        /* Enter */
        if (c == '\r' || c == '\n') {
            term_scribe_s("\r\n");
            restitue_terminum();
            linea[lon] = '\0';
            return linea;
        }

        /* Backspace (127 vel 8) */
        if (c == 127 || c == 8) {
            if (cursor > 0) {
                memmove(linea + cursor - 1, linea + cursor, lon - cursor);
                cursor--;
                lon--;
                linea[lon] = '\0';
                repinge_lineam(linea, lon, cursor, lon_incit);
            }
            continue;
        }

        /* Tab — completio */
        if (c == '\t') {
            int lon_verbi;
            int init_verbi  = extrahe_verbum(linea, cursor, &lon_verbi);
            char *praefixum = linea + init_verbi;

            char *completiones[LIM_COMPLETIONES];
            int num_compl;

            if (est_positio_mandati(linea, init_verbi))
                num_compl = comple_mandata(
                    praefixum, lon_verbi,
                    completiones, LIM_COMPLETIONES
                );
            else
                num_compl = comple_plicas(
                    praefixum, lon_verbi,
                    completiones, LIM_COMPLETIONES
                );

            /* adde semper plicas etiam in positione mandati */
            if (est_positio_mandati(linea, init_verbi)) {
                int n2 = comple_plicas(
                    praefixum, lon_verbi,
                    completiones + num_compl,
                    LIM_COMPLETIONES - num_compl
                );
                num_compl += n2;
            }

            if (num_compl == 1) {
                /* completio unica — insere residuum */
                const char *compl = completiones[0];
                int lon_compl     = (int)strlen(compl);
                int addendum      = lon_compl - lon_verbi;

                if (addendum > 0 && lon + addendum < LIM_LINEA) {
                    /* spatium post si non directorium */
                    int cum_spatio = (compl[lon_compl - 1] != '/') ? 1 : 0;
                    memmove(
                        linea + cursor + addendum + cum_spatio,
                        linea + cursor,
                        lon - cursor
                    );
                    memcpy(linea + cursor, compl + lon_verbi, addendum);
                    if (cum_spatio)
                        linea[cursor + addendum] = ' ';
                    lon += addendum + cum_spatio;
                    cursor += addendum + cum_spatio;
                    linea[lon] = '\0';
                    repinge_lineam(linea, lon, cursor, lon_incit);
                }
            } else if (num_compl > 1) {
                /* praefixum commune */
                int addendum = praefixum_commune(
                    completiones, num_compl,
                    lon_verbi
                );
                if (addendum > 0 && lon + addendum < LIM_LINEA) {
                    memmove(
                        linea + cursor + addendum,
                        linea + cursor, lon - cursor
                    );
                    memcpy(
                        linea + cursor,
                        completiones[0] + lon_verbi, addendum
                    );
                    lon += addendum;
                    cursor += addendum;
                    linea[lon] = '\0';
                    repinge_lineam(linea, lon, cursor, lon_incit);
                } else {
                    /* ostende omnes completiones */
                    term_scribe_s("\r\n");
                    for (int i = 0; i < num_compl; i++) {
                        term_scribe_s(completiones[i]);
                        term_scribe_s("  ");
                    }
                    term_scribe_s("\r\n");
                    repinge_lineam(linea, lon, cursor, lon_incit);
                }
            }

            for (int i = 0; i < num_compl; i++)
                free(completiones[i]);
            continue;
        }

        /* Ctrl-A — initium lineae */
        if (c == 1) {
            cursor = 0;
            repinge_lineam(linea, lon, cursor, lon_incit);
            continue;
        }

        /* Ctrl-E — finis lineae */
        if (c == 5) {
            cursor = lon;
            repinge_lineam(linea, lon, cursor, lon_incit);
            continue;
        }

        /* Ctrl-K — dele ad finem */
        if (c == 11) {
            lon        = cursor;
            linea[lon] = '\0';
            repinge_lineam(linea, lon, cursor, lon_incit);
            continue;
        }

        /* Ctrl-U — dele ad initium */
        if (c == 21) {
            memmove(linea, linea + cursor, lon - cursor);
            lon -= cursor;
            cursor     = 0;
            linea[lon] = '\0';
            repinge_lineam(linea, lon, cursor, lon_incit);
            continue;
        }

        /* Ctrl-W — dele verbum retro */
        if (c == 23) {
            int dest = cursor;
            while (dest > 0 && linea[dest - 1] == ' ')
                dest--;
            while (dest > 0 && linea[dest - 1] != ' ')
                dest--;
            memmove(linea + dest, linea + cursor, lon - cursor);
            lon -= cursor - dest;
            cursor     = dest;
            linea[lon] = '\0';
            repinge_lineam(linea, lon, cursor, lon_incit);
            continue;
        }

        /* Ctrl-L — purga terminale */
        if (c == 12) {
            term_scribe_s("\x1b[H\x1b[2J");
            repinge_lineam(linea, lon, cursor, lon_incit);
            continue;
        }

        /* sequentia effugitionis (cursores, etc.) */
        if (c == 27) {
            unsigned char seq[3];
            if (read(STDIN_FILENO, &seq[0], 1) <= 0)
                continue;
            if (seq[0] == '[') {
                if (read(STDIN_FILENO, &seq[1], 1) <= 0)
                    continue;

                switch (seq[1]) {
                case 'A': /* sursum — historia */
                    if (idx_hist > 0) {
                        if (idx_hist == num_historia)
                            memcpy(salvata, linea, lon + 1);
                        idx_hist--;
                        strncpy(linea, historia[idx_hist], LIM_LINEA - 1);
                        linea[LIM_LINEA - 1] = '\0';
                        lon = (int)strlen(linea);
                        cursor = lon;
                        repinge_lineam(linea, lon, cursor, lon_incit);
                    }
                    break;
                case 'B': /* deorsum — historia */
                    if (idx_hist < num_historia) {
                        idx_hist++;
                        if (idx_hist == num_historia) {
                            memcpy(linea, salvata, strlen(salvata) + 1);
                        } else {
                            strncpy(linea, historia[idx_hist], LIM_LINEA - 1);
                            linea[LIM_LINEA - 1] = '\0';
                        }
                        lon    = (int)strlen(linea);
                        cursor = lon;
                        repinge_lineam(linea, lon, cursor, lon_incit);
                    }
                    break;
                case 'C': /* dextrorsum */
                    if (cursor < lon) {
                        cursor++;
                        repinge_lineam(linea, lon, cursor, lon_incit);
                    }
                    break;
                case 'D': /* sinistrorsum */
                    if (cursor > 0) {
                        cursor--;
                        repinge_lineam(linea, lon, cursor, lon_incit);
                    }
                    break;
                case 'H': /* Home */
                    cursor = 0;
                    repinge_lineam(linea, lon, cursor, lon_incit);
                    break;
                case 'F': /* End */
                    cursor = lon;
                    repinge_lineam(linea, lon, cursor, lon_incit);
                    break;
                case '3': /* Delete */
                    read(STDIN_FILENO, &seq[2], 1); /* ~ */
                    if (cursor < lon) {
                        memmove(
                            linea + cursor, linea + cursor + 1,
                            lon - cursor - 1
                        );
                        lon--;
                        linea[lon] = '\0';
                        repinge_lineam(linea, lon, cursor, lon_incit);
                    }
                    break;
                case '1': /* Home (alternativum) */
                    read(STDIN_FILENO, &seq[2], 1);
                    cursor = 0;
                    repinge_lineam(linea, lon, cursor, lon_incit);
                    break;
                case '4': /* End (alternativum) */
                    read(STDIN_FILENO, &seq[2], 1);
                    cursor = lon;
                    repinge_lineam(linea, lon, cursor, lon_incit);
                    break;
                }
            }
            continue;
        }

        /* character ordinarius impressibilis */
        if (c >= 32 && lon < LIM_LINEA - 1) {
            memmove(linea + cursor + 1, linea + cursor, lon - cursor);
            linea[cursor] = c;
            cursor++;
            lon++;
            linea[lon] = '\0';
            repinge_lineam(linea, lon, cursor, lon_incit);
        }
    }
}

/*
 * lege_lineam — legit lineam ex flumine (non-interactivum).
 * reddit NULL ad finem.
 */
static char *lege_lineam_simplicem(FILE *flumen)
{
    static char alveus[LIM_LINEA];
    if (!fgets(alveus, sizeof(alveus), flumen))
        return NULL;

    size_t lon = strlen(alveus);
    if (lon > 0 && alveus[lon - 1] == '\n')
        alveus[lon - 1] = '\0';

    return alveus;
}

/* ============================================================
 * XIII. PRINCIPALE
 * ============================================================ */

int main(int argc, char **argv)
{
    /* tracta signa */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = tracta_sigint;
    sa.sa_flags   = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = tracta_sigchld;
    sa.sa_flags   = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    /* determina flumen fontis */
    FILE *flumen     = stdin;
    int interactivum = isatty(STDIN_FILENO);

    if (argc > 1) {
        flumen = fopen(argv[1], "r");
        if (!flumen) {
            fprintf(stderr, "festum: %s: %s\n", argv[1], strerror(errno));
            return 1;
        }
        interactivum = 0;
    }

    /* ansa principalis */
    char *linea;
    for (;;) {
        if (infans_mutatus)
            mete_infantes();

        if (interactivum)
            linea = lege_lineam_interactivam();
        else
            linea = lege_lineam_simplicem(flumen);

        if (!linea)
            break;

        /* praetermitte lineas vacuas */
        if (linea[0] == '\0' || linea[0] == '#')
            continue;

        if (interactivum)
            adde_historiam(linea);

        linea_dissecta_t ld;
        if (disseca_lineam(linea, &ld) == 0 && ld.num_mandata > 0) {
            exsequere_lineam(&ld);
            libera_lineam(&ld);
        }
    }

    if (interactivum)
        fprintf(stderr, "\n");

    if (flumen != stdin)
        fclose(flumen);

    /* libera historiam */
    for (int i = 0; i < num_historia; i++)
        free(historia[i]);

    return status_ultimus;
}
