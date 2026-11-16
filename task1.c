#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define MKDIR(p) _mkdir(p)
#else
#define MKDIR(p) mkdir(p, 0755)
#endif

#define ROOT "lab6_8"   

static void make_dir(const char *path) {
    if (MKDIR(path) == -1 && errno != EEXIST) {
        perror(path);
        exit(1);
    }
}

static void make_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (!f) {
        perror(path);
        exit(1);
    }
    if (content) fputs(content, f);
    fclose(f);
}

static void create_structure(void) {
    make_dir(ROOT);
    make_dir(ROOT "/.git");
    make_file(ROOT "/.git/config", "[core]\n\trepositoryformatversion = 0\n");
    make_dir(ROOT "/src");
    make_file(ROOT "/src/main.c", "int main(void) { return 0; }\n");
    make_dir(ROOT "/bin");
    make_dir(ROOT "/docs");
    make_file(ROOT "/docs/manual.txt", "Manual\n");
    make_file(ROOT "/docs/presentation.pdf", "%PDF-1.4\n");
    make_dir(ROOT "/img");
    make_file(ROOT "/img/cat.jpg", "cat");
    make_file(ROOT "/img/dog.png", "dog");
    make_file(ROOT "/README.md", "# lab6\n");
}

static int cmp_names(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static void tree(const char *path, const char *prefix, int *dirs, int *files) {
    DIR *d = opendir(path);
    if (!d) {
        perror(path);
        return;
    }

    char **names = NULL;
    size_t n = 0, cap = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;   
        if (n == cap) {
            cap = cap ? cap * 2 : 8;
            names = realloc(names, cap * sizeof(char *));
            if (!names) { perror("realloc"); exit(1); }
        }
        names[n++] = strdup(e->d_name);
    }
    closedir(d);
    qsort(names, n, sizeof(char *), cmp_names);

    for (size_t i = 0; i < n; i++) {
        int last = (i == n - 1);
        printf("%s%s%s\n", prefix, last ? "└── " : "├── ", names[i]);

        char full[4096];
        snprintf(full, sizeof(full), "%s/%s", path, names[i]);

        struct stat st;
        if (stat(full, &st) == -1) {
            perror(full);
        } else if (S_ISDIR(st.st_mode)) {
            (*dirs)++;
            char newprefix[4096];
            snprintf(newprefix, sizeof(newprefix), "%s%s", prefix,
                     last ? "    " : "│   ");
            tree(full, newprefix, dirs, files);
        } else {
            (*files)++;
        }
        free(names[i]);
    }
    free(names);
}

int main(void) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);   
#endif
    create_structure();

    int dirs = 0, files = 0;
    printf("%s\n", ROOT);
    tree(ROOT, "", &dirs, &files);
    printf("\n%d directories, %d files\n", dirs, files);
    return 0;
}
