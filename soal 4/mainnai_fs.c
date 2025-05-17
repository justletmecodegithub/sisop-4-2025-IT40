#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <zlib.h>

#define BLOCK_SIZE 1024
#define AES_KEY_SIZE 32
#define AES_IV_SIZE 16

// Struktur konfigurasi
struct {
    const char *rootdir;
    const char *mountdir;
} maimai_config;

// Fungsi bantuan untuk path
char* build_path(const char *path) {
    char *fullpath = malloc(strlen(maimai_config.rootdir) + strlen(path) + 1);
    sprintf(fullpath, "%s%s", maimai_config.rootdir, path);
    return fullpath;
}

// Fungsi untuk Starter Chiho (a)
char* starter_transform_path(const char *path, int add_mai) {
    if (strstr(path, "/starter/") != path) return NULL;
    
    const char *filename = path + strlen("/starter/");
    if (*filename == '\0') return NULL;
    
    char *newpath = malloc(strlen(maimai_config.rootdir) + strlen(path) + 5);
    if (add_mai) {
        sprintf(newpath, "%s%s.mai", maimai_config.rootdir, path);
    } else {
        sprintf(newpath, "%s%s", maimai_config.rootdir, path);
    }
    return newpath;
}

// Fungsi untuk Metropolis Chiho (b)
char* metro_transform_path(const char *path) {
    if (strstr(path, "/metro/") != path) return NULL;
    
    const char *filename = path + strlen("/metro/");
    if (*filename == '\0') return NULL;
    
    char *newpath = malloc(strlen(maimai_config.rootdir) + strlen(path) + 1);
    sprintf(newpath, "%s%s", maimai_config.rootdir, path);
    return newpath;
}

void metro_transform_content(char *content, size_t size) {
    for (size_t i = 0; i < size; i++) {
        content[i] = (content[i] + (i % 256)) % 256;
    }
}

// Fungsi untuk Dragon Chiho (c) - ROT13
void dragon_rot13(char *str, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if ((str[i] >= 'a' && str[i] <= 'm') || (str[i] >= 'A' && str[i] <= 'M')) {
            str[i] += 13;
        } else if ((str[i] >= 'n' && str[i] <= 'z') || (str[i] >= 'N' && str[i] <= 'Z')) {
            str[i] -= 13;
        }
    }
}

// Fungsi untuk Black Rose Chiho (d) - Tidak ada transformasi
char* blackrose_transform_path(const char *path) {
    if (strstr(path, "/blackrose/") != path) return NULL;
    
    const char *filename = path + strlen("/blackrose/");
    if (*filename == '\0') return NULL;
    
    char *newpath = malloc(strlen(maimai_config.rootdir) + strlen(path) + 1);
    sprintf(newpath, "%s%s", maimai_config.rootdir, path);
    return newpath;
}

// Fungsi untuk Heaven Chiho (e) - AES-256-CBC
void heaven_encrypt(const char *in, size_t in_len, char *out, unsigned char *iv) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    unsigned char key[AES_KEY_SIZE];
    memset(key, 0xAA, AES_KEY_SIZE);
    
    EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv);
    int len;
    EVP_EncryptUpdate(ctx, (unsigned char*)out, &len, (unsigned char*)in, in_len);
    EVP_EncryptFinal_ex(ctx, (unsigned char*)out + len, &len);
    EVP_CIPHER_CTX_free(ctx);
}

void heaven_decrypt(const char *in, size_t in_len, char *out, unsigned char *iv) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    unsigned char key[AES_KEY_SIZE];
    memset(key, 0xAA, AES_KEY_SIZE);
    
    EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv);
    int len;
    EVP_DecryptUpdate(ctx, (unsigned char*)out, &len, (unsigned char*)in, in_len);
    EVP_DecryptFinal_ex(ctx, (unsigned char*)out + len, &len);
    EVP_CIPHER_CTX_free(ctx);
}

// Fungsi untuk Youth Chiho (f) - Gzip compression
int youth_compress(const char *in, size_t in_len, char *out, size_t out_len) {
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    
    if (deflateInit(&strm, Z_DEFAULT_COMPRESSION) != Z_OK) {
        return -1;
    }
    
    strm.next_in = (Bytef *)in;
    strm.avail_in = in_len;
    strm.next_out = (Bytef *)out;
    strm.avail_out = out_len;
    
    if (deflate(&strm, Z_FINISH) != Z_STREAM_END) {
        deflateEnd(&strm);
        return -1;
    }
    
    int compressed_size = out_len - strm.avail_out;
    deflateEnd(&strm);
    return compressed_size;
}

int youth_decompress(const char *in, size_t in_len, char *out, size_t out_len) {
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    
    if (inflateInit(&strm) != Z_OK) {
        return -1;
    }
    
    strm.next_in = (Bytef *)in;
    strm.avail_in = in_len;
    strm.next_out = (Bytef *)out;
    strm.avail_out = out_len;
    
    if (inflate(&strm, Z_FINISH) != Z_STREAM_END) {
        inflateEnd(&strm);
        return -1;
    }
    
    int decompressed_size = out_len - strm.avail_out;
    inflateEnd(&strm);
    return decompressed_size;
}

// Fungsi untuk 7sRef Chiho (g)
char* sevsref_transform_path(const char *path) {
    if (strstr(path, "/7sref/") != path) return NULL;
    
    const char *filename = path + strlen("/7sref/");
    if (*filename == '\0') return NULL;
    
    const char *underscore = strchr(filename, '_');
    if (!underscore) return NULL;
    
    int area_len = underscore - filename;
    char area[32];
    strncpy(area, filename, area_len);
    area[area_len] = '\0';
    
    const char *real_filename = underscore + 1;
    
    char *newpath = malloc(strlen(maimai_config.rootdir) + 32 + strlen(real_filename) + 2);
    sprintf(newpath, "%s/%s/%s", maimai_config.rootdir, area, real_filename);
    return newpath;
}

// Implementasi operasi FUSE
static int maimai_getattr(const char *path, struct stat *stbuf,
                         struct fuse_file_info *fi) {
    (void) fi;
    memset(stbuf, 0, sizeof(struct stat));
    
    char *fullpath = NULL;
    
    if (strstr(path, "/7sref/") == path) {
        fullpath = sevsref_transform_path(path);
    } else if (strstr(path, "/starter/") == path) {
        fullpath = starter_transform_path(path, 1);
    } else {
        fullpath = build_path(path);
    }
    
    if (!fullpath) return -ENOENT;
    
    int res = lstat(fullpath, stbuf);
    free(fullpath);
    
    if (res == -1) return -errno;
    return 0;
}

static int maimai_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi,
                         enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;
    
    char *fullpath = build_path(path);
    DIR *dp = opendir(fullpath);
    if (!dp) {
        free(fullpath);
        return -errno;
    }
    
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    
    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        if (strstr(path, "/starter") == path && strstr(de->d_name, ".mai")) {
            char name[256];
            strncpy(name, de->d_name, strlen(de->d_name) - 4);
            name[strlen(de->d_name) - 4] = '\0';
            filler(buf, name, NULL, 0, 0);
        } else {
            filler(buf, de->d_name, NULL, 0, 0);
        }
    }
    
    closedir(dp);
    free(fullpath);
    return 0;
}

static int maimai_open(const char *path, struct fuse_file_info *fi) {
    char *fullpath = NULL;
    
    if (strstr(path, "/7sref/") == path) {
        fullpath = sevsref_transform_path(path);
    } else if (strstr(path, "/starter/") == path) {
        fullpath = starter_transform_path(path, 1);
    } else {
        fullpath = build_path(path);
    }
    
    if (!fullpath) return -ENOENT;
    
    int res = open(fullpath, fi->flags);
    free(fullpath);
    
    if (res == -1) return -errno;
    
    close(res);
    return 0;
}

static int maimai_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
    (void) fi;
    
    char *fullpath = NULL;
    int fd;
    
    if (strstr(path, "/7sref/") == path) {
        fullpath = sevsref_transform_path(path);
    } else if (strstr(path, "/starter/") == path) {
        fullpath = starter_transform_path(path, 1);
    } else {
        fullpath = build_path(path);
    }
    
    if (!fullpath) return -ENOENT;
    
    fd = open(fullpath, O_RDONLY);
    if (fd == -1) {
        free(fullpath);
        return -errno;
    }
    
    int res = pread(fd, buf, size, offset);
    if (res == -1) res = -errno;
    
    close(fd);
    
    // Transformasi konten berdasarkan area
    if (res > 0) {
        if (strstr(path, "/metro/") == path) {
            metro_transform_content(buf, res);
        } else if (strstr(path, "/dragon/") == path) {
            dragon_rot13(buf, res);
        } else if (strstr(path, "/heaven/") == path) {
            unsigned char iv[AES_IV_SIZE];
            if (offset == 0 && size >= AES_IV_SIZE) {
                memcpy(iv, buf, AES_IV_SIZE);
                char *decrypted = malloc(res - AES_IV_SIZE);
                heaven_decrypt(buf + AES_IV_SIZE, res - AES_IV_SIZE, decrypted, iv);
                memcpy(buf, decrypted, res - AES_IV_SIZE);
                res -= AES_IV_SIZE;
                free(decrypted);
            }
        } else if (strstr(path, "/youth/") == path) {
            char *decompressed = malloc(size * 4);
            int decompressed_size = youth_decompress(buf, res, decompressed, size * 4);
            if (decompressed_size > 0) {
                memcpy(buf, decompressed, decompressed_size);
                res = decompressed_size;
            }
            free(decompressed);
        }
    }
    
    free(fullpath);
    return res;
}

static int maimai_write(const char *path, const char *buf, size_t size,
                       off_t offset, struct fuse_file_info *fi) {
    (void) fi;
    
    char *fullpath = NULL;
    int fd;
    
    if (strstr(path, "/7sref/") == path) {
        fullpath = sevsref_transform_path(path);
    } else if (strstr(path, "/starter/") == path) {
        fullpath = starter_transform_path(path, 1);
    } else {
        fullpath = build_path(path);
    }
    
    if (!fullpath) return -ENOENT;
    
    fd = open(fullpath, O_WRONLY);
    if (fd == -1) {
        free(fullpath);
        return -errno;
    }
    
    // Transformasi konten sebelum menulis
    if (strstr(path, "/heaven/") == path) {
        unsigned char iv[AES_IV_SIZE];
        RAND_bytes(iv, AES_IV_SIZE);
        
        char *encrypted = malloc(size + AES_IV_SIZE);
        memcpy(encrypted, iv, AES_IV_SIZE);
        heaven_encrypt(buf, size, encrypted + AES_IV_SIZE, iv);
        
        int res = pwrite(fd, encrypted, size + AES_IV_SIZE, offset);
        free(encrypted);
        if (res == -1) res = -errno;
        close(fd);
        free(fullpath);
        return res > 0 ? res - AES_IV_SIZE : res;
    } else if (strstr(path, "/youth/") == path) {
        char *compressed = malloc(size + 1024);
        int compressed_size = youth_compress(buf, size, compressed, size + 1024);
        
        int res = -EIO;
        if (compressed_size > 0) {
            res = pwrite(fd, compressed, compressed_size, offset);
            if (res == -1) res = -errno;
        }
        
        free(compressed);
        close(fd);
        free(fullpath);
        return res;
    } else {
        int res = pwrite(fd, buf, size, offset);
        if (res == -1) res = -errno;
        close(fd);
        free(fullpath);
        return res;
    }
}

static int maimai_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char *fullpath = NULL;
    
    if (strstr(path, "/7sref/") == path) {
        fullpath = sevsref_transform_path(path);
    } else if (strstr(path, "/starter/") == path) {
        fullpath = starter_transform_path(path, 1);
    } else {
        fullpath = build_path(path);
    }
    
    if (!fullpath) return -ENOENT;
    
    int res = open(fullpath, fi->flags, mode);
    if (res == -1) {
        free(fullpath);
        return -errno;
    }
    
    close(res);
    free(fullpath);
    return 0;
}

static int maimai_unlink(const char *path) {
    char *fullpath = NULL;
    
    if (strstr(path, "/7sref/") == path) {
        fullpath = sevsref_transform_path(path);
    } else if (strstr(path, "/starter/") == path) {
        fullpath = starter_transform_path(path, 1);
    } else {
        fullpath = build_path(path);
    }
    
    if (!fullpath) return -ENOENT;
    
    int res = unlink(fullpath);
    free(fullpath);
    
    if (res == -1) return -errno;
    return 0;
}

static struct fuse_operations maimai_oper = {
    .getattr = maimai_getattr,
    .readdir = maimai_readdir,
    .open = maimai_open,
    .read = maimai_read,
    .write = maimai_write,
    .create = maimai_create,
    .unlink = maimai_unlink,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <rootdir> <mountpoint> [FUSE options]\n", argv[0]);
        return 1;
    }
    
    maimai_config.rootdir = realpath(argv[1], NULL);
    maimai_config.mountdir = argv[2];
    
    // Pindahkan rootdir dan mountdir ke akhir argumen untuk FUSE
    argv[1] = argv[2];
    argv[2] = NULL;
    argc = 2;
    
    return fuse_main(argc, argv, &maimai_oper, NULL);
}
