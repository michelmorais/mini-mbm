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

    distribution.cpp — Asset packaging library implementation.

    Encryption:  AES-128-CBC (plusaes, header-only).
    Key derivation: PBKDF2-HMAC-SHA256, 100 000 iterations, 16-byte random salt.
    Random bytes: sqlite3_randomness() — adequate CSPRNG on all platforms.
    Directory walking: POSIX dirent (system) / dirent-1-13 shim (MSVC).
    No engine dependency — links only sqlite3 (compiled in) and plusaes (header).
*/

#include "sha256.h"
#include <distribution.h>

/* ---- platform includes ---- */
#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #include <sys/stat.h>
    #define PATH_SEP     '\\'
    #define PATH_SEP_STR "\\"
    #define DIST_MKDIR(p) _mkdir(p)
    #define DIST_STAT    struct _stat
    #define DIST_STAT_FN _stat
#else
    #include <sys/stat.h>
    #include <unistd.h>
    #define PATH_SEP     '/'
    #define PATH_SEP_STR "/"
    #define DIST_MKDIR(p) mkdir((p), 0755)
    #define DIST_STAT    struct stat
    #define DIST_STAT_FN stat
#endif

#include <dirent.h>
#include <sqlite3.h>
#include <plusaes/plusaes.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

/* -------------------------------------------------------------------------- */
/* Internal types                                                              */
/* -------------------------------------------------------------------------- */

struct FileEntry {
    std::string rel_path; /* relative sub-directory from asset root, e.g. "sprites" or "" */
    std::string name;     /* plain filename, e.g. "hero.spt" */
    std::string category; /* "image" | "lua-script" | "mesh" | "audio" | lowercase-ext */
};

struct DISTRIBUTION_CTX {
    std::string          password;
    std::string          base_folder; /* first add_asset root */
    std::vector<FileEntry> files;
    char                 last_error[512];
};

/* -------------------------------------------------------------------------- */
/* Helpers                                                                     */
/* -------------------------------------------------------------------------- */

static void set_error(DISTRIBUTION_CTX *ctx, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(ctx->last_error, sizeof(ctx->last_error) - 1, fmt, ap);
    va_end(ap);
}

/* Lowercase extension from a filename, without the dot.  Returns "" when none. */
static std::string get_extension(const std::string &name)
{
    const size_t dot = name.rfind('.');
    if (dot == std::string::npos || dot + 1 == name.size())
        return "";
    std::string ext = name.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return static_cast<char>(::tolower(c)); });
    return ext;
}

static std::string get_category(const std::string &name)
{
    const std::string ext = get_extension(name);
    if (ext.empty())
        return "";
    /* image */
    static const char *IMG[] = { "png","jpeg","jpg","bmp","gif","psd","pic","pnm","hdr","tga","tif","tiff","webp", nullptr };
    for (int i = 0; IMG[i]; ++i)
        if (ext == IMG[i]) return "image";
    /* lua-script */
    if (ext == "lua") return "lua-script";
    /* mesh */
    static const char *MESH[] = { "msh","spt","ptl","tile","fnt","mbm", nullptr };
    for (int i = 0; MESH[i]; ++i)
        if (ext == MESH[i]) return "mesh";
    /* audio */
    static const char *AUD[] = {
        "aa","aac","aax","act","aiff","alac","amr","ape","au","awb","dct","dss","dvf",
        "flac","gsm","iklax","ivs","m4a","m4b","m4p","mmf","mp3","mpc","msv","nmf","nsf",
        "ogg","oga","mogg","opus","ra","rm","raw","rf64","sln","tta","voc","vox","wav",
        "wma","wv","webm","cda","3gp","8svx", nullptr };
    for (int i = 0; AUD[i]; ++i)
        if (ext == AUD[i]) return "audio";
    return ext; /* unknown — store the extension itself */
}

/* Create all directories in path (mkdir -p style). */
static bool mkdirs(const std::string &path)
{
    if (path.empty()) return true;
    std::string cur;
    cur.reserve(path.size());
    for (size_t i = 0; i < path.size(); ++i) {
        const char c = path[i];
        cur += c;
        if ((c == '/' || c == '\\' || i + 1 == path.size()) && !cur.empty()) {
            DIST_STAT st;
            if (DIST_STAT_FN(cur.c_str(), &st) != 0) {
                if (DIST_MKDIR(cur.c_str()) != 0 && errno != EEXIST)
                    return false;
            }
        }
    }
    return true;
}

/* Normalise separators to forward slash for DB storage. */
static std::string to_fwd_slash(std::string s)
{
    for (char &c : s)
        if (c == '\\') c = '/';
    return s;
}

/* Compute relative path of child under base.  Both must be normalised. */
static std::string relative_path(const std::string &base, const std::string &child)
{
    if (child.size() <= base.size()) return "";
    size_t start = base.size();
    if (child[start] == '/' || child[start] == '\\') ++start;
    const std::string rel = child.substr(start);
    /* Keep only the directory portion (not the filename). */
    const size_t last_sep = rel.rfind('/');
    if (last_sep == std::string::npos) return "";
    return rel.substr(0, last_sep);
}

/* -------------------------------------------------------------------------- */
/* PBKDF2-HMAC-SHA256                                                          */
/* Derives out_len bytes of key material from password+salt.                  */
/* -------------------------------------------------------------------------- */

static void hmac_sha256(const uint8_t *key,  size_t key_len,
                         const uint8_t *data, size_t data_len,
                         uint8_t *out /* SHA256_BLOCK_SIZE bytes */)
{
    uint8_t k_buf[64] = {0};

    /* Step 1 — if key > 64 bytes, hash it */
    if (key_len > 64) {
        SHA256_CTX ctx;
        sha256_init(&ctx);
        sha256_update(&ctx, key, key_len);
        sha256_final(&ctx, k_buf);
    } else {
        memcpy(k_buf, key, key_len);
    }

    /* Step 2 — ipad / opad */
    uint8_t ipad[64], opad[64];
    for (int i = 0; i < 64; ++i) {
        ipad[i] = static_cast<uint8_t>(k_buf[i] ^ 0x36u);
        opad[i] = static_cast<uint8_t>(k_buf[i] ^ 0x5cu);
    }

    /* Step 3 — inner hash: SHA256(ipad || data) */
    uint8_t inner[SHA256_BLOCK_SIZE];
    {
        SHA256_CTX ctx;
        sha256_init(&ctx);
        sha256_update(&ctx, ipad, 64);
        sha256_update(&ctx, data, data_len);
        sha256_final(&ctx, inner);
    }

    /* Step 4 — outer hash: SHA256(opad || inner) */
    {
        SHA256_CTX ctx;
        sha256_init(&ctx);
        sha256_update(&ctx, opad, 64);
        sha256_update(&ctx, inner, SHA256_BLOCK_SIZE);
        sha256_final(&ctx, out);
    }
}

/*
 * PBKDF2-HMAC-SHA256.
 * out_len must be <= SHA256_BLOCK_SIZE (32) for this single-block implementation.
 * For AES-128 we only ever request 16 bytes, so one block is sufficient.
 */
static void pbkdf2_hmac_sha256(const uint8_t *password, size_t password_len,
                                const uint8_t *salt,     size_t salt_len,
                                uint32_t       iterations,
                                uint8_t       *out,      size_t out_len)
{
    /* Block index i = 1, encoded big-endian */
    uint8_t salt_plus_i[256 + 4];
    if (salt_len > sizeof(salt_plus_i) - 4)
        salt_len = sizeof(salt_plus_i) - 4;
    memcpy(salt_plus_i, salt, salt_len);
    salt_plus_i[salt_len + 0] = 0;
    salt_plus_i[salt_len + 1] = 0;
    salt_plus_i[salt_len + 2] = 0;
    salt_plus_i[salt_len + 3] = 1; /* block index = 1 */

    /* U1 = HMAC(password, salt || INT32BE(1)) */
    uint8_t U[SHA256_BLOCK_SIZE];
    hmac_sha256(password, password_len, salt_plus_i, salt_len + 4, U);

    /* T = U1 */
    uint8_t T[SHA256_BLOCK_SIZE];
    memcpy(T, U, SHA256_BLOCK_SIZE);

    /* T ^= U2, U3, ... */
    for (uint32_t iter = 1; iter < iterations; ++iter) {
        uint8_t Un[SHA256_BLOCK_SIZE];
        hmac_sha256(password, password_len, U, SHA256_BLOCK_SIZE, Un);
        for (int j = 0; j < SHA256_BLOCK_SIZE; ++j)
            T[j] ^= Un[j];
        memcpy(U, Un, SHA256_BLOCK_SIZE);
    }

    const size_t copy = out_len < SHA256_BLOCK_SIZE ? out_len : SHA256_BLOCK_SIZE;
    memcpy(out, T, copy);
}

/* -------------------------------------------------------------------------- */
/* Key derivation — derives a 16-byte AES-128 key from ctx->password + salt  */
/* -------------------------------------------------------------------------- */

#define PBKDF2_ITERATIONS 100000u

static void derive_key(const std::string &password,
                        const uint8_t     *salt,  /* 16 bytes */
                        uint8_t           *key_out /* 16 bytes */)
{
    pbkdf2_hmac_sha256(
        reinterpret_cast<const uint8_t *>(password.data()), password.size(),
        salt, 16,
        PBKDF2_ITERATIONS,
        key_out, 16);
}

/* -------------------------------------------------------------------------- */
/* Hex encode / decode                                                         */
/* -------------------------------------------------------------------------- */

static std::string bytes_to_hex(const uint8_t *data, size_t len)
{
    static const char HEX[] = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        out += HEX[(data[i] >> 4) & 0xf];
        out += HEX[data[i] & 0xf];
    }
    return out;
}

static bool hex_to_bytes(const char *hex, uint8_t *out, size_t out_len)
{
    for (size_t i = 0; i < out_len; ++i) {
        unsigned int hi = 0, lo = 0;
        const char ch = hex[i * 2];
        const char cl = hex[i * 2 + 1];
        if      (ch >= '0' && ch <= '9') hi = static_cast<unsigned>(ch - '0');
        else if (ch >= 'a' && ch <= 'f') hi = static_cast<unsigned>(ch - 'a' + 10);
        else if (ch >= 'A' && ch <= 'F') hi = static_cast<unsigned>(ch - 'A' + 10);
        else return false;
        if      (cl >= '0' && cl <= '9') lo = static_cast<unsigned>(cl - '0');
        else if (cl >= 'a' && cl <= 'f') lo = static_cast<unsigned>(cl - 'a' + 10);
        else if (cl >= 'A' && cl <= 'F') lo = static_cast<unsigned>(cl - 'A' + 10);
        else return false;
        out[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return true;
}

/* -------------------------------------------------------------------------- */
/* Encrypt / decrypt a blob                                                    */
/* Encrypted format: IV(16 bytes) || AES-128-CBC-padded(data)                 */
/* -------------------------------------------------------------------------- */

static bool encrypt_blob(const uint8_t *key,              /* 16 bytes */
                          const std::vector<uint8_t> &plain,
                          std::vector<uint8_t>       &cipher_out)
{
    /* Generate a random 16-byte IV using SQLite's CSPRNG */
    uint8_t iv_bytes[16];
    sqlite3_randomness(16, iv_bytes);

    const unsigned long enc_size = plusaes::get_padded_encrypted_size(static_cast<unsigned long>(plain.size()));
    cipher_out.resize(16 + enc_size);

    memcpy(cipher_out.data(), iv_bytes, 16);

    const unsigned char (*iv_ptr)[16] = reinterpret_cast<const unsigned char (*)[16]>(iv_bytes);

    plusaes::Error err = plusaes::encrypt_cbc(
        plain.data(),          static_cast<unsigned long>(plain.size()),
        key,                   16,
        iv_ptr,
        cipher_out.data() + 16, enc_size,
        true /* padding */);

    return err == plusaes::Error::kErrorOk;
}

static bool decrypt_blob(const uint8_t *key,              /* 16 bytes */
                          const std::vector<uint8_t> &cipher,
                          std::vector<uint8_t>       &plain_out)
{
    if (cipher.size() < 16) return false;

    const unsigned char (*iv_ptr)[16] = reinterpret_cast<const unsigned char (*)[16]>(cipher.data());

    const unsigned long enc_size = static_cast<unsigned long>(cipher.size() - 16);
    plain_out.resize(enc_size);

    unsigned long padded_size = 0;
    plusaes::Error err = plusaes::decrypt_cbc(
        cipher.data() + 16, enc_size,
        key, 16,
        iv_ptr,
        plain_out.data(), enc_size,
        &padded_size);

    if (err != plusaes::Error::kErrorOk) return false;

    /* Trim padding: actual size = enc_size - padded_size */
    const size_t actual = enc_size - static_cast<size_t>(padded_size);
    plain_out.resize(actual);
    return true;
}

/* -------------------------------------------------------------------------- */
/* Directory walking                                                           */
/* -------------------------------------------------------------------------- */

static bool is_directory(const std::string &path)
{
    DIST_STAT st;
    return DIST_STAT_FN(path.c_str(), &st) == 0 && (st.st_mode & S_IFDIR) != 0;
}

static bool is_regular_file(const std::string &path)
{
    DIST_STAT st;
    return DIST_STAT_FN(path.c_str(), &st) == 0 && (st.st_mode & S_IFREG) != 0;
}

/* Walk dir recursively, push (absolute_file_path) into result. */
static void walk_dir(const std::string &dir, std::vector<std::string> &result)
{
    DIR *dp = opendir(dir.c_str());
    if (!dp) return;

    dirent *entry;
    while ((entry = readdir(dp)) != nullptr) {
        if (entry->d_name[0] == '.') continue; /* skip . and .. */
        const std::string child = dir + PATH_SEP_STR + entry->d_name;
        if (is_directory(child))
            walk_dir(child, result);
        else if (is_regular_file(child))
            result.push_back(child);
    }
    closedir(dp);
}

/* -------------------------------------------------------------------------- */
/* sqlite3 helpers                                                             */
/* -------------------------------------------------------------------------- */

static int exec_sql(sqlite3 *db, const char *sql, char **errmsg)
{
    return sqlite3_exec(db, sql, nullptr, nullptr, errmsg);
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                  */
/* -------------------------------------------------------------------------- */

extern "C" {

DIST_API DISTRIBUTION_CTX *distribution_create(void)
{
    DISTRIBUTION_CTX *ctx = new (std::nothrow) DISTRIBUTION_CTX();
    if (ctx)
        ctx->last_error[0] = '\0';
    return ctx;
}

DIST_API void distribution_destroy(DISTRIBUTION_CTX *ctx)
{
    delete ctx;
}

DIST_API void distribution_set_password(DISTRIBUTION_CTX *ctx, const char *password)
{
    if (!ctx) return;
    ctx->password = (password && password[0] != '\0') ? password : "";
}

DIST_API int distribution_add_asset(DISTRIBUTION_CTX *ctx, const char *file_or_folder)
{
    if (!ctx || !file_or_folder) return 0;

    std::string path(file_or_folder);
    /* Normalise path separators */
    for (char &c : path)
        if (c == '\\') c = '/';
    /* Strip trailing separator */
    while (!path.empty() && (path.back() == '/' || path.back() == '\\'))
        path.pop_back();

    if (is_directory(path)) {
        /* Set base folder on first directory add */
        if (ctx->base_folder.empty())
            ctx->base_folder = path;

        std::vector<std::string> files;
        walk_dir(path, files);
        for (const auto &f : files) {
            std::string fwd = to_fwd_slash(f);
            std::string base_fwd = to_fwd_slash(path);
            FileEntry e;
            e.rel_path = relative_path(base_fwd, fwd);
            e.name     = fwd.substr(fwd.rfind('/') + 1);
            e.category = get_category(e.name);
            ctx->files.push_back(std::move(e));
        }
    } else if (is_regular_file(path)) {
        std::string fwd = to_fwd_slash(path);
        FileEntry e;
        e.rel_path = "";
        e.name     = fwd.substr(fwd.rfind('/') + 1);
        e.category = get_category(e.name);
        ctx->files.push_back(std::move(e));

        if (ctx->base_folder.empty()) {
            const size_t sep = fwd.rfind('/');
            ctx->base_folder = (sep != std::string::npos) ? path.substr(0, sep) : ".";
        }
    } else {
        set_error(ctx, "path not found or not accessible: %s", file_or_folder);
        return 0;
    }

    return 1;
}

DIST_API int distribution_save_asset(DISTRIBUTION_CTX *ctx, const char *output_path)
{
    if (!ctx || !output_path) return 0;

    /* Remove existing file so we start fresh */
    remove(output_path);

    sqlite3 *db = nullptr;
    if (sqlite3_open(output_path, &db) != SQLITE_OK) {
        set_error(ctx, "cannot open database: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 0;
    }

    /* Create schema */
    char *errmsg = nullptr;
    const char *schema =
        "BEGIN;"
        "CREATE TABLE IF NOT EXISTS metadata(key TEXT PRIMARY KEY, value TEXT);"
        "CREATE TABLE IF NOT EXISTS dumped_folder(path TEXT);"
        "CREATE TABLE IF NOT EXISTS paths(id INTEGER PRIMARY KEY AUTOINCREMENT, path TEXT);"
        "CREATE TABLE IF NOT EXISTS assets("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT, category TEXT, content BLOB,"
        "  id_path INTEGER REFERENCES paths(id) ON DELETE CASCADE);"
        "COMMIT;";

    if (exec_sql(db, schema, &errmsg) != SQLITE_OK) {
        set_error(ctx, "schema error: %s", errmsg);
        sqlite3_free(errmsg);
        sqlite3_close(db);
        return 0;
    }

    /* --- metadata --- */
    const bool encrypt = !ctx->password.empty();
    uint8_t salt[16]   = {0};
    uint8_t key[16]    = {0};

    if (encrypt) {
        sqlite3_randomness(16, salt);
        derive_key(ctx->password, salt, key);
    }

    sqlite3_stmt *meta_stmt = nullptr;
    sqlite3_prepare_v2(db, "INSERT OR REPLACE INTO metadata(key,value) VALUES(?,?);", -1, &meta_stmt, nullptr);

    auto insert_meta = [&](const char *k, const char *v) {
        sqlite3_bind_text(meta_stmt, 1, k, -1, SQLITE_STATIC);
        sqlite3_bind_text(meta_stmt, 2, v, -1, SQLITE_STATIC);
        sqlite3_step(meta_stmt);
        sqlite3_reset(meta_stmt);
    };

    insert_meta("version", "2");
    if (encrypt) {
        insert_meta("encrypted", "1");
        const std::string salt_hex = bytes_to_hex(salt, 16);
        insert_meta("salt", salt_hex.c_str());
    }
    if (!ctx->base_folder.empty())
        insert_meta("base_folder", ctx->base_folder.c_str());

    sqlite3_finalize(meta_stmt);

    /* --- dumped_folder --- */
    if (!ctx->base_folder.empty()) {
        sqlite3_stmt *df_stmt = nullptr;
        sqlite3_prepare_v2(db, "INSERT INTO dumped_folder(path) VALUES(?);", -1, &df_stmt, nullptr);
        sqlite3_bind_text(df_stmt, 1, ctx->base_folder.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(df_stmt);
        sqlite3_finalize(df_stmt);
    }

    /* --- paths (collect unique rel_paths) --- */
    std::vector<std::string> unique_paths;
    for (const auto &fe : ctx->files) {
        bool found = false;
        for (const auto &p : unique_paths)
            if (p == fe.rel_path) { found = true; break; }
        if (!found) unique_paths.push_back(fe.rel_path);
    }

    sqlite3_stmt *path_stmt = nullptr;
    sqlite3_prepare_v2(db, "INSERT INTO paths(path) VALUES(?);", -1, &path_stmt, nullptr);
    for (const auto &p : unique_paths) {
        sqlite3_bind_text(path_stmt, 1, p.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(path_stmt);
        sqlite3_reset(path_stmt);
    }
    sqlite3_finalize(path_stmt);

    /* --- assets --- */
    sqlite3_stmt *asset_stmt = nullptr;
    sqlite3_prepare_v2(db,
        "INSERT INTO assets(name,category,content,id_path) "
        "SELECT ?,?,?,(SELECT id FROM paths WHERE path=?);",
        -1, &asset_stmt, nullptr);

    exec_sql(db, "BEGIN;", nullptr);

    int ok = 1;
    for (const auto &fe : ctx->files) {
        /* Resolve full file path */
        std::string full;
        if (!ctx->base_folder.empty()) {
            full = ctx->base_folder;
            if (!fe.rel_path.empty())
                full += PATH_SEP_STR + fe.rel_path;
            full += PATH_SEP_STR + fe.name;
        } else {
            full = fe.name;
        }
        /* Replace forward slashes back to OS sep */
#ifdef _WIN32
        for (char &c : full) if (c == '/') c = '\\';
#endif

        FILE *fp = fopen(full.c_str(), "rb");
        if (!fp) {
            set_error(ctx, "cannot open file: %s", full.c_str());
            ok = 0;
            break;
        }

        /* Read file */
        fseek(fp, 0, SEEK_END);
        const long fsz = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        std::vector<uint8_t> raw(static_cast<size_t>(fsz));
        if (fsz > 0 && fread(raw.data(), 1, static_cast<size_t>(fsz), fp) != static_cast<size_t>(fsz)) {
            fclose(fp);
            set_error(ctx, "read error: %s", full.c_str());
            ok = 0;
            break;
        }
        fclose(fp);

        /* Optionally encrypt */
        std::vector<uint8_t> blob;
        if (encrypt) {
            if (!encrypt_blob(key, raw, blob)) {
                set_error(ctx, "encryption error for: %s", fe.name.c_str());
                ok = 0;
                break;
            }
        } else {
            blob = std::move(raw);
        }

        sqlite3_bind_text(asset_stmt, 1, fe.name.c_str(),     -1, SQLITE_STATIC);
        sqlite3_bind_text(asset_stmt, 2, fe.category.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_blob(asset_stmt, 3, blob.data(), static_cast<int>(blob.size()), SQLITE_STATIC);
        sqlite3_bind_text(asset_stmt, 4, fe.rel_path.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(asset_stmt);
        sqlite3_reset(asset_stmt);
    }

    exec_sql(db, ok ? "COMMIT;" : "ROLLBACK;", nullptr);
    sqlite3_finalize(asset_stmt);
    sqlite3_close(db);

    if (!ok) remove(output_path);
    return ok;
}

DIST_API int distribution_extract_asset(DISTRIBUTION_CTX *ctx,
                                          const char       *asset_path,
                                          const char       *dest_folder)
{
    if (!ctx || !asset_path || !dest_folder) return 0;

    sqlite3 *db = nullptr;
    if (sqlite3_open_v2(asset_path, &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        set_error(ctx, "cannot open database: %s", asset_path);
        sqlite3_close(db);
        return 0;
    }

    /* Read metadata */
    bool encrypted = false;
    std::string salt_hex;
    {
        sqlite3_stmt *st = nullptr;
        sqlite3_prepare_v2(db, "SELECT key,value FROM metadata;", -1, &st, nullptr);
        while (sqlite3_step(st) == SQLITE_ROW) {
            const char *k = reinterpret_cast<const char *>(sqlite3_column_text(st, 0));
            const char *v = reinterpret_cast<const char *>(sqlite3_column_text(st, 1));
            if (k && v) {
                if (strcmp(k, "encrypted") == 0 && strcmp(v, "1") == 0) encrypted = true;
                if (strcmp(k, "salt") == 0) salt_hex = v;
            }
        }
        sqlite3_finalize(st);
    }

    uint8_t salt[16] = {0};
    uint8_t aes_key[16] = {0};
    if (encrypted) {
        if (ctx->password.empty()) {
            set_error(ctx, "asset is encrypted but no password was set");
            sqlite3_close(db);
            return 0;
        }
        if (salt_hex.size() < 32 || !hex_to_bytes(salt_hex.c_str(), salt, 16)) {
            set_error(ctx, "corrupt or missing salt in metadata");
            sqlite3_close(db);
            return 0;
        }
        derive_key(ctx->password, salt, aes_key);
    }

    /* Create destination folder */
    {
        std::string dest(dest_folder);
        if (!mkdirs(dest)) {
            set_error(ctx, "cannot create destination folder: %s", dest_folder);
            sqlite3_close(db);
            return 0;
        }
    }

    /* Extract all assets */
    sqlite3_stmt *st = nullptr;
    sqlite3_prepare_v2(db,
        "SELECT a.name, a.content, p.path "
        "FROM assets a "
        "LEFT JOIN paths p ON p.id = a.id_path;",
        -1, &st, nullptr);

    int ok = 1;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char *name = reinterpret_cast<const char *>(sqlite3_column_text(st, 0));
        const void *blob = sqlite3_column_blob(st, 1);
        const int   blen = sqlite3_column_bytes(st, 1);
        const char *rel  = reinterpret_cast<const char *>(sqlite3_column_text(st, 2));

        if (!name || !blob || blen <= 0) continue;

        /* Build output directory */
        std::string out_dir = dest_folder;
        if (rel && rel[0] != '\0') {
            out_dir += PATH_SEP_STR;
            out_dir += rel;
#ifdef _WIN32
            for (char &c : out_dir) if (c == '/') c = '\\';
#endif
        }
        if (!mkdirs(out_dir)) {
            set_error(ctx, "cannot create directory: %s", out_dir.c_str());
            ok = 0;
            break;
        }

        std::string out_file = out_dir + PATH_SEP_STR + name;

        /* Decrypt if needed */
        std::vector<uint8_t> plain;
        if (encrypted) {
            const std::vector<uint8_t> cipher(
                static_cast<const uint8_t *>(blob),
                static_cast<const uint8_t *>(blob) + blen);
            if (!decrypt_blob(aes_key, cipher, plain)) {
                set_error(ctx, "decryption failed for: %s (wrong password?)", name);
                ok = 0;
                break;
            }
        } else {
            plain.assign(
                static_cast<const uint8_t *>(blob),
                static_cast<const uint8_t *>(blob) + blen);
        }

        FILE *fp = fopen(out_file.c_str(), "wb");
        if (!fp) {
            set_error(ctx, "cannot write: %s", out_file.c_str());
            ok = 0;
            break;
        }
        if (!plain.empty())
            fwrite(plain.data(), 1, plain.size(), fp);
        fclose(fp);
    }

    sqlite3_finalize(st);
    sqlite3_close(db);
    return ok;
}

DIST_API const char *distribution_last_error(DISTRIBUTION_CTX *ctx)
{
    if (!ctx) return "null context";
    return ctx->last_error[0] ? ctx->last_error : "(no error)";
}

} /* extern "C" */
