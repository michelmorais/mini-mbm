/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|------------------------------------------------------------------------------------------------------------------------|

    main.cpp — CLI entry point for the distribution tool.

    Usage:
        distribution pack   <source_folder> <output.asset> [--password <pwd>]
        distribution extract <input.asset>  <dest_folder>  [--password <pwd>]
*/

#include <distribution.h>
#include <cstdio>
#include <cstring>

static void usage(const char *argv0)
{
    fprintf(stderr,
        "Usage:\n"
        "  %s pack    <source_folder> <output.asset> [--password <pwd>]\n"
        "  %s extract <input.asset>   <dest_folder>  [--password <pwd>]\n",
        argv0, argv0);
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        usage(argv[0]);
        return 1;
    }

    const char *subcmd  = argv[1];
    const char *arg2    = argv[2];
    const char *arg3    = argv[3];
    const char *password = nullptr;

    /* Parse optional --password <pwd> */
    for (int i = 4; i + 1 < argc; ++i) {
        if (strcmp(argv[i], "--password") == 0) {
            password = argv[i + 1];
            ++i;
        }
    }

    DISTRIBUTION_CTX *ctx = distribution_create();
    if (!ctx) {
        fprintf(stderr, "error: failed to create distribution context\n");
        return 1;
    }

    if (password && password[0] != '\0')
        distribution_set_password(ctx, password);

    int result = 0;

    if (strcmp(subcmd, "pack") == 0) {
        if (!distribution_add_asset(ctx, arg2)) {
            fprintf(stderr, "error: %s\n", distribution_last_error(ctx));
            distribution_destroy(ctx);
            return 1;
        }
        if (!distribution_save_asset(ctx, arg3)) {
            fprintf(stderr, "error: %s\n", distribution_last_error(ctx));
            result = 1;
        } else {
            printf("packed: %s -> %s\n", arg2, arg3);
        }
    } else if (strcmp(subcmd, "extract") == 0) {
        if (!distribution_extract_asset(ctx, arg2, arg3)) {
            fprintf(stderr, "error: %s\n", distribution_last_error(ctx));
            result = 1;
        } else {
            printf("extracted: %s -> %s\n", arg2, arg3);
        }
    } else {
        fprintf(stderr, "error: unknown sub-command '%s'\n", subcmd);
        usage(argv[0]);
        result = 1;
    }

    distribution_destroy(ctx);
    return result;
}
