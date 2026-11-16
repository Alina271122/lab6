#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <errno.h>
#include <limits.h>
#include <getopt.h>
#include <dirent.h>
#include <unistd.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define MKDIR(p) _mkdir(p)
#define REALPATH(p, buf) _fullpath(buf, p, PATH_MAX)
#else
#define MKDIR(p) mkdir(p, 0755)
#define REALPATH(p, buf) realpath(p, buf)
#endif
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#include <sys/stat.h>
#include <sys/types.h>

static FILE *logf;
static char dest_real[PATH_MAX];

static void logmsg(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    putchar('\n');

    va_start(ap, fmt);
    vfprintf(logf, fmt, ap);
    va_end(ap);
    fputc('\n', logf);
    fflush(logf);
}

static const char *category(const char *name) {
    const char *dot = strrchr(name, '.');
    if (!dot || dot == name) return "Other";
    if (!strcasecmp(dot, ".txt") || !strcasecmp(dot, ".md")) return "Documents";
    if (!strcasecmp(dot, ".c") || !strcasecmp(dot, ".h") || !strcasecmp(dot, ".go")) return "Sources";
    if (!strcasecmp(dot, ".jpg") || !strcasecmp(dot, ".png")) return "Images";
    if (!strcasecmp(dot, ".pdf")) return "PDFs";
    return "Other";
}

static int ensure_dir(const char *path) {
    if (MKDIR(path) == 0) {
        logmsg("Створено директорію: %s", path);
        return 0;
    }
    if (errno == EEXIST) return 0;
    logmsg("Помилка створення %s: %s", path, strerror(errno));
    return -1;
}

static int copy_file(const char *from, const char *to) {
    FILE *in = fopen(from, "rb");
    if (!in) return -1;
    FILE *out = fopen(to, "wb");
    if (!out) { fclose(in); return -1; }
    char buf[8192];
    size_t n;
    int rc = 0;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        if (fwrite(buf, 1, n, out) != n) { rc = -1; break; }
    fclose(in);
    if (fclose(out) != 0) rc = -1;
    if (rc == 0) rc = remove(from);
    return rc;
}

static void move_file(const char *src, const char *name, const char *dest) {
    const char *cat = category(name);
    char dir[PATH_MAX], target[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s/%s", dest, cat);
    if (ensure_dir(dir) == -1) return;
    snprintf(target, sizeof(target), "%s/%s", dir, name);

    struct stat st;
    if (stat(target, &st) == 0) {
        logmsg("Помилка: %s вже існує, пропущено %s", target, src);
        return;
    }
    if (rename(src, target) == 0 ||
        (errno == EXDEV && copy_file(src, target) == 0))
        logmsg("Переміщено: %s -> %s", src, target);
    else
        logmsg("Помилка переміщення %s: %s", src, strerror(errno));
}

static void scan(const char *path, const char *dest) {
    DIR *d = opendir(path);
    if (!d) {
        logmsg("Помилка відкриття %s: %s", path, strerror(errno));
        return;
    }
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;   // приховані та . / ..
        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", path, e->d_name);

        struct stat st;
        if (stat(full, &st) == -1) {
            logmsg("Помилка stat %s: %s", full, strerror(errno));
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            char real[PATH_MAX];
            if (REALPATH(full, real) && !strcmp(real, dest_real)) continue;
            scan(full, dest);
        } else if (S_ISREG(st.st_mode)) {
            move_file(full, e->d_name, dest);
        }
    }
    closedir(d);
}

int main(int argc, char *argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    const char *source = NULL, *dest = NULL;
    static struct option opts[] = {
        {"source", required_argument, 0, 's'},
        {"dest",   required_argument, 0, 'd'},
        {0, 0, 0, 0}
    };
    int c;
    while ((c = getopt_long(argc, argv, "", opts, NULL)) != -1) {
        if (c == 's') source = optarg;
        else if (c == 'd') dest = optarg;
        else {
            fprintf(stderr, "Використання: %s --source=<dir> --dest=<dir>\n", argv[0]);
            return 1;
        }
    }
    if (!source || !dest) {
        fprintf(stderr, "Використання: %s --source=<dir> --dest=<dir>\n", argv[0]);
        return 1;
    }

    logf = fopen("sort_report.log", "w");
    if (!logf) { perror("sort_report.log"); return 1; }

    struct stat st;
    if (stat(source, &st) == -1 || !S_ISDIR(st.st_mode)) {
        logmsg("Помилка: %s не є директорією", source);
        fclose(logf);
        return 1;
    }

    logmsg("--- Програма сортування запущена ---");
    if (ensure_dir(dest) == -1) { fclose(logf); return 1; }
    if (!REALPATH(dest, dest_real)) dest_real[0] = '\0';

    scan(source, dest);

    logmsg("Сортування завершено! Див. 'sort_report.log'.");
    fclose(logf);
    return 0;
}
