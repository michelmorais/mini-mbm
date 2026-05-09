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

    distribution.h — Public C API for the mini-mbm asset-packaging library.

    Assets are stored in a SQLite database (.asset) whose schema is compatible
    with the asset_packager.lua editor tool.  Content BLOBs are optionally
    encrypted with AES-128-CBC; the key is derived from a user password via
    PBKDF2-HMAC-SHA256 (100 000 iterations, 16-byte random salt stored in the
    metadata table).

    Schema (same as asset_packager.lua):
        metadata  (key TEXT PRIMARY KEY, value TEXT)
        dumped_folder (path TEXT)
        paths     (id INTEGER PRIMARY KEY AUTOINCREMENT, path TEXT)
        assets    (id INTEGER PRIMARY KEY AUTOINCREMENT,
                   name TEXT, category TEXT, content BLOB,
                   id_path INTEGER REFERENCES paths(id) ON DELETE CASCADE)

    Metadata rows added by this library:
        version   = "2"
        encrypted = "1"      (only when a password is set)
        salt      = <32 hex chars>  (only when a password is set)

    Encrypted BLOB format:  IV(16 bytes) || AES-128-CBC-padded(content)
*/

#ifndef DISTRIBUTION_H
#define DISTRIBUTION_H

#include <stddef.h>

#ifdef _WIN32
    #ifdef DISTRIBUTION_BUILD_DLL
        #define DIST_API __declspec(dllexport)
    #else
        #define DIST_API __declspec(dllimport)
    #endif
#else
    #define DIST_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque context — create with distribution_create(), free with distribution_destroy(). */
typedef struct DISTRIBUTION_CTX DISTRIBUTION_CTX;

/*
 * Allocate a new context.  Returns NULL on allocation failure.
 */
DIST_API DISTRIBUTION_CTX *distribution_create(void);

/*
 * Free all resources held by ctx.  Passing NULL is a no-op.
 */
DIST_API void distribution_destroy(DISTRIBUTION_CTX *ctx);

/*
 * Set the encryption password.  Pass NULL or "" to disable encryption.
 * Must be called before distribution_save_asset(); has no effect on extraction
 * (the correct password is read from the database metadata row).
 *
 * The password is copied internally; the caller does not need to keep it alive.
 */
DIST_API void distribution_set_password(DISTRIBUTION_CTX *ctx, const char *password);

/*
 * Add a file or directory to the pending asset list.
 *   - If file_or_folder is a regular file, it is added directly.
 *   - If it is a directory, the entire tree is walked recursively.
 * The first call that passes a directory sets the "base path"; all
 * subsequent paths stored in the database are relative to this base.
 * If file_or_folder is a file, its directory is used as the base path
 * (for that specific file only — the file name is recorded under the
 * root path "").
 *
 * Returns 1 on success, 0 on error (see distribution_last_error()).
 */
DIST_API int distribution_add_asset(DISTRIBUTION_CTX *ctx, const char *file_or_folder);

/*
 * Write all collected assets to output_path as a SQLite database.
 * Creates or overwrites the file.  Encrypts content BLOBs when a
 * password has been set.
 *
 * Returns 1 on success, 0 on error.
 */
DIST_API int distribution_save_asset(DISTRIBUTION_CTX *ctx, const char *output_path);

/*
 * Open asset_path (a .asset SQLite database), decrypt content if needed
 * (using the password previously set via distribution_set_password()), and
 * extract all files into dest_folder, recreating the original sub-directory
 * structure.  dest_folder is created if it does not exist.
 *
 * The password stored in ctx must match the one used at save time;
 * if the asset was saved without a password, no password is required.
 *
 * Returns 1 on success, 0 on error.
 */
DIST_API int distribution_extract_asset(DISTRIBUTION_CTX *ctx,
                                         const char       *asset_path,
                                         const char       *dest_folder);

/*
 * Return a human-readable description of the last error on ctx.
 * The pointer is valid until the next call on the same ctx or until
 * distribution_destroy() is called.  Never returns NULL.
 */
DIST_API const char *distribution_last_error(DISTRIBUTION_CTX *ctx);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* DISTRIBUTION_H */
