#define FUSE_USE_VERSION 31
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>

#define MAX_PATH_LEN 1024
#define LOG_FILE "/var/log/it24.log"

static const char *host_dir = "/it24_host";
static const char *mount_dir = "/antink_mount";

void log_activity(const char *action, const char *filename) {
    time_t now;
    time(&now);
    char timestamp[20];
    strftime(timestamp, 20, "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    FILE *log_file = fopen(LOG_FILE, "a");
    if (log_file) {
        fprintf(log_file, "[%s] %s: %s\n", timestamp, action, filename);
        fclose(log_file);
    }
}

int is_dangerous(const char *filename) {
    return strstr(filename, "nafis") || strstr(filename, "kimcun");
}

void reverse_string(char *str) {
    if (!str) return;
    int len = strlen(str);
    for (int i = 0; i < len / 2; i++) {
        char temp = str[i];
        str[i] = str[len - i - 1];
        str[len - i - 1] = temp;
    }
}

void rot13(char *str) {
    if (!str) return;
    for (; *str; str++) {
        if (isalpha(*str)) {
            if ((*str >= 'a' && *str <= 'm') || (*str >= 'A' && *str <= 'M')) {
                *str += 13;
            } else {
                *str -= 13;
            }
        }
    }
}

static int antink_getattr(const char *path, struct stat *stbuf) {
    char full_path[MAX_PATH_LEN];
    snprintf(full_path, sizeof(full_path), "%s%s", host_dir, path);
    
    int res = lstat(full_path, stbuf);
    if (res == -1) return -errno;
    return 0;
}

static int antink_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi) {
    DIR *dp;
    struct dirent *de;
    char full_path[MAX_PATH_LEN];
    
    snprintf(full_path, sizeof(full_path), "%s%s", host_dir, path);
    dp = opendir(full_path);
    if (dp == NULL) return -errno;

    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        
        char display_name[MAX_PATH_LEN];
        strcpy(display_name, de->d_name);
        
        if (is_dangerous(display_name)) {
            reverse_string(display_name);
        }
        
        if (filler(buf, display_name, &st, 0)) break;
    }
    closedir(dp);
    return 0;
}

static int antink_open(const char *path, struct fuse_file_info *fi) {
    char full_path[MAX_PATH_LEN];
    snprintf(full_path, sizeof(full_path), "%s%s", host_dir, path);
    
    int res = open(full_path, fi->flags);
    if (res == -1) return -errno;
    
    close(res);
    log_activity("OPEN", path);
    return 0;
}

static int antink_read(const char *path, char *buf, size_t size, off_t offset,
                       struct fuse_file_info *fi) {
    char full_path[MAX_PATH_LEN];
    snprintf(full_path, sizeof(full_path), "%s%s", host_dir, path);
    
    int fd = open(full_path, O_RDONLY);
    if (fd == -1) return -errno;
    
    int res = pread(fd, buf, size, offset);
    if (res == -1) {
        close(fd);
        return -errno;
    }
    
    // Only apply ROT13 to non-dangerous text files
    if (!is_dangerous(path) && strstr(path, ".txt")) {
        buf[res] = '\0';
        rot13(buf);
        res = strlen(buf);
    }
    
    close(fd);
    log_activity("READ", path);
    return res;
}

static int antink_write(const char *path, const char *buf, size_t size,
                        off_t offset, struct fuse_file_info *fi) {
    char full_path[MAX_PATH_LEN];
    snprintf(full_path, sizeof(full_path), "%s%s", host_dir, path);
    
    int fd = open(full_path, O_WRONLY);
    if (fd == -1) return -errno;
    
    int res = pwrite(fd, buf, size, offset);
    if (res == -1) {
        close(fd);
        return -errno;
    }
    
    close(fd);
    log_activity("WRITE", path);
    return res;
}

static int antink_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char full_path[MAX_PATH_LEN];
    snprintf(full_path, sizeof(full_path), "%s%s", host_dir, path);
    
    int res = creat(full_path, mode);
    if (res == -1) return -errno;
    
    close(res);
    log_activity("CREATE", path);
    return 0;
}

static int antink_unlink(const char *path) {
    char full_path[MAX_PATH_LEN];
    snprintf(full_path, sizeof(full_path), "%s%s", host_dir, path);
    
    int res = unlink(full_path);
    if (res == -1) return -errno;
    
    log_activity("DELETE", path);
    return 0;
}

static struct fuse_operations antink_oper = {
    .getattr = antink_getattr,
    .readdir = antink_readdir,
    .open = antink_open,
    .read = antink_read,
    .write = antink_write,
    .create = antink_create,
    .unlink = antink_unlink,
};

int main(int argc, char *argv[]) {
    return fuse_main(argc, argv, &antink_oper, NULL);
}
