#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#define ZIP_ID "1hi_GDdP51Kn2JJMw02WmCOxuc3qrXzh5"
#define ZIP_FILE "anomali.zip"
#define EXTRACT_DIR "sampel_anomali"
#define IMAGE_DIR "image"
#define LOG_FILE "conversion.log"


void run_shell(const char *cmd) {
    printf("[shell] %s\n", cmd);
    int status = system(cmd);
    if (status != 0) {
        fprintf(stderr, "[ERROR] Perintah gagal: %s\n", cmd);
        exit(1);
    }
}


void get_timestamp(char *date_str, char *time_str) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(date_str, 16, "%Y-%m-%d", tm_info);
    strftime(time_str, 16, "%H:%M:%S", tm_info);
}

void convert_hex_files(const char *src_dir, const char *out_dir) {
    DIR *dp = opendir(src_dir);
    if (!dp) {
        perror("opendir");
        exit(1);
    }

    mkdir(out_dir, 0755); // Buat folder image
    FILE *logf = fopen(LOG_FILE, "a");
    if (!logf) {
        perror("log open");
        exit(1);
    }

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        if (de->d_type != DT_REG || strstr(de->d_name, ".txt") == NULL)
            continue;

        char src_file[512], dst_file[512];
        snprintf(src_file, sizeof(src_file), "%s/%s", src_dir, de->d_name);

        FILE *fp = fopen(src_file, "r");
        if (!fp) continue;

        fseek(fp, 0, SEEK_END);
        long len = ftell(fp);
        rewind(fp);
        char *hex_str = malloc(len + 1);
        fread(hex_str, 1, len, fp);
        hex_str[len] = '\0';
        fclose(fp);

        size_t bin_len = strlen(hex_str) / 2;
        unsigned char *bin_data = malloc(bin_len);
        for (size_t i = 0; i < bin_len; ++i)
            sscanf(&hex_str[i * 2], "%2hhx", &bin_data[i]);

        char date[20], timebuf[20];
        get_timestamp(date, timebuf);

        char base[256];
        strncpy(base, de->d_name, sizeof(base));
        base[strlen(base) - 4] = '\0'; // hilangkan .txt

        snprintf(dst_file, sizeof(dst_file), "%s/%s_image_%s_%s.png", out_dir, base, date, timebuf);

        FILE *outf = fopen(dst_file, "wb");
        fwrite(bin_data, 1, bin_len, outf);
        fclose(outf);

        fprintf(logf, "[%s][%s]: Successfully converted hexadecimal text %s to %s.\n",
                date, timebuf, de->d_name, strrchr(dst_file, '/') + 1);

        free(hex_str);
        free(bin_data);
    }

    fclose(logf);
    closedir(dp);
}

const char *root_dir = IMAGE_DIR;

int x_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    char fullpath[512];
    snprintf(fullpath, sizeof(fullpath), "%s%s", root_dir, path);
    return lstat(fullpath, stbuf);
}

int x_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
              off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    (void) offset; (void) fi; (void) flags;
    char fullpath[512];
    snprintf(fullpath, sizeof(fullpath), "%s%s", root_dir, path);
    DIR *dp = opendir(fullpath);
    if (!dp) return -errno;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    struct dirent *de;
    while ((de = readdir(dp)) != NULL)
        filler(buf, de->d_name, NULL, 0, 0);

    closedir(dp);
    return 0;
}

int x_open(const char *path, struct fuse_file_info *fi) {
    char fullpath[512];
    snprintf(fullpath, sizeof(fullpath), "%s%s", root_dir, path);
    int fd = open(fullpath, fi->flags);
    if (fd == -1) return -errno;
    close(fd);
    return 0;
}

int x_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void) fi;
    char fullpath[512];
    snprintf(fullpath, sizeof(fullpath), "%s%s", root_dir, path);
    FILE *fp = fopen(fullpath, "rb");
    if (!fp) return -errno;
    fseek(fp, offset, SEEK_SET);
    size_t res = fread(buf, 1, size, fp);
    fclose(fp);
    return res;
}

static struct fuse_operations ops = {
    .getattr = x_getattr,
    .readdir = x_readdir,
    .open    = x_open,
    .read    = x_read,
};

int main(int argc, char *argv[]) {

    if (access(ZIP_FILE, F_OK) != 0) {
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "curl -L -o %s 'https://drive.google.com/uc?export=download&id=%s'", ZIP_FILE, ZIP_ID);
        run_shell(cmd);
    }


    char mkdir_cmd[256];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", EXTRACT_DIR);
    run_shell(mkdir_cmd);

    char unzip_cmd[256];
    snprintf(unzip_cmd, sizeof(unzip_cmd), "unzip -o -q %s -d %s", ZIP_FILE, EXTRACT_DIR);
    run_shell(unzip_cmd);


    char rm_cmd[128];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -f %s", ZIP_FILE);
    run_shell(rm_cmd);


    convert_hex_files(EXTRACT_DIR, IMAGE_DIR);

 
    return fuse_main(argc, argv, &ops, NULL);
}
