#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "dlx.h"

int main(int argc, char const** argv)
{
    dlx_t dlx = dlx_new();
    int ncols = -1;
    int row = 0;
    char line[1024];
    while (fgets(line, sizeof(line), stdin) != NULL) {
        if (line[strlen(line)-1] == '\n')
            line[strlen(line)-1] = '\0';
        char const* p = line + strspn(line, " ");
        if (*p == '\0' || *p == '#')
            continue;
        int col = 0;
        for (; *p != '\0'; ++col) {
            if (*p++ == '1')
                dlx_set(dlx, row, col);
            while (*p == ' ')
                ++p;
        }
        if (ncols >= 0 && col != ncols)
            fprintf(stderr, "WARNING: row %d has %d columns rather than %d\n", row, col, ncols);
        ncols = col;
        ++row;
    }

    void prt(int row[], int n) {
        for (int i = 0; i < n; ++i)
            printf(" %d", row[i]);
        printf("\n");
    }
    dlx_forall_cover(dlx, prt);
    dlx_clear(dlx);
    return 0;
}
