# Soal 2
Dikerjakan oleh I Gede Bagus Saka Sinatrya/5027241088.

Tugas utama kita dalam kode ini adalah
- Menampilkan file utuh Baymax.jpeg di folder mount_dir, yang di mana aslinya file tersebut terpecah dalam folder relics
- Mendukung operasi normal file, seperti membuka,menyalin, dan lainnya
- Menangani penulisan (write). Seperti pada saat menambahkan file baru ke mount_dir file tersebut akan langsung tercepah di relics
- Menangani penghapusan (unlink). Jika menghapus di mount_dir, seluruh file di relics yang terkait akan ikut terhapus
- Mencatat aktivitas ke activity.log

## Penjelasan dan Struktur Kodenya

### Library dan Define
```c
#define FUSE_USE_VERSION 35

#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>

#define MAX_PART_SIZE 1024
#define RELIC_DIR "/home/ricardo/Sisop/m4/soal_2/relics"
#define LOG_FILE "/home/ricardo/Sisop/m4/soal_2/activity.log"
#define MAX_FILENAME_LEN 255
#define MAX_LOG_LEN 1024
#define MAX_PATH_LEN 1024
```

### Fungsi dari activity.log
```c
void log_activity(const char *action, const char *details) {
    time_t now = time(NULL); 
    struct tm *tm_info = localtime(&now); 
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    FILE *log = fopen(LOG_FILE, "a");
    if (log) {
        fprintf(log, "[%s] %s: %s\n", timestamp, action, details);
        fclose(log);
    }
}
```
- `time_t now = time(NULL); ` : mengambil waktu sekarang dalam bentuk timestamp.
- ` struct tm *tm_info = localtime(&now); `: Mengubah timestamp now menjadi struktur waktu lokal.
- `strftime` : mengubah tm_info menjadi string waktu berdasarkan format yang diberikan

### Menentukan rormat nama file yang sesuai
```c
static int is_fragment_file(const char *f) {
    const char *dot = strrchr(f, '.');
    return dot && strlen(dot) == 4 &&
           isdigit(dot[1]) && isdigit(dot[2]) && isdigit(dot[3]);
}
```
- `strrchr(f, '.')`: mencari kemunculan titik (.) terakhir dalam string f.
- `isdigit` : menentukan jumlah karakter setelah titik.

### Menyalin bagian nama file asli (tanpa format .###) ke variabel base
```c
static int get_base_filename(const char *frag, char *base) {
    const char *dot = strrchr(frag, '.');
    if (!is_fragment_file(frag)) return -1;
    size_t len = dot - frag;
    if (len >= MAX_FILENAME_LEN) return -1;
    memcpy(base, frag, len);
    base[len] = '\0';
    return 0;
}
```
- `const char *dot = strrchr(frag, '.');` : menemukan titik terakhir darri frag.
- `size_t len = dot - frag;` : menghitung panjang bagian nama sebelum titik.
  
### Menghitung jumlah file pecahan yang ada untuk file tertentu
```c
static int count_file_parts(const char *base_filename) {
    char path[MAX_PATH_LEN];
    int count = 0;
    while (1) {
        snprintf(path, sizeof(path), "%s/%s.%03d", RELIC_DIR, base_filename, count);
        if (access(path, F_OK) != 0) break;
        count++;
    }
    return count;
}
```
- `snprintf` : menghasilkan path file sesui dengan format.
- `if (access(path, F_OK) != 0) break;` : mengecek apakah file ada.

### Menghitung total ukuran dari seluruh file pecahan
```c
static int get_file_size(const char *base_filename) {
    char path[MAX_PATH_LEN];
    struct stat st;
    int total_size = 0;
    for (int i = 0; ; i++) {
        snprintf(path, sizeof(path), "%s/%s.%03d", RELIC_DIR, base_filename, i);
        if (stat(path, &st) != 0) break;
        total_size += st.st_size;
    }
    return total_size;
}
```
- `struct stat st;` : struktur untuk menyimpan informasi file dari stat() seperti ukuran file, waktu modifikasi, dll.
- ` for (int i = 0; ; i++)` : loop untuk mencari fragmen.
- ` total_size += st.st_size;` : menambahkan ukuran file saat ini ke total dan ukuran total file akan dikembalikan dengan `  return total_size;`.

### Mengambil atribut dari sebuah file atau direktori
```c
static int xmp_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) { // mengecek apakah path adalah direktori root
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }
    //jika bukan root
    char base_filename[MAX_FILENAME_LEN + 1];
    strncpy(base_filename, path + 1, MAX_FILENAME_LEN);
    base_filename[MAX_FILENAME_LEN] = '\0';
    char first_part[MAX_PATH_LEN];
    snprintf(first_part, sizeof(first_part), "%s/%s.000", RELIC_DIR, base_filename);
    if (access(first_part, F_OK) == 0) {
        stbuf->st_mode = S_IFREG | 0644;
        stbuf->st_nlink = 1; // jumlah hard link 1 karena virtual
        stbuf->st_size = get_file_size(base_filename);
        return 0;
    }
    return -ENOENT;
}
```

### Menampilkan file di mount_dir
```c
static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi, 
                      enum fuse_readdir_flags flags) 
{
    (void) offset;
    (void) fi;
    (void) flags;

    if (strcmp(path, "/") != 0)
        return -ENOENT;

    // Isi entri khusus . (current dir) dan .. (parent dir) ke buffer FUSE.
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    DIR *dp = opendir(RELIC_DIR);
    if (!dp) {
        perror("opendir");
        return -errno;
    }

    struct dirent *de;
    char listed[MAX_FILENAME_LEN][MAX_FILENAME_LEN];
    int listed_count = 0;

    while ((de = readdir(dp)) != NULL) { // loop semua entri di direktori relics
        if (!is_fragment_file(de->d_name)) continue;

        char base[MAX_FILENAME_LEN];
        if (get_base_filename(de->d_name, base) != 0) continue;

        int duplicate = 0; 
        for (int i = 0; i < listed_count; i++) {
            if (strcmp(listed[i], base) == 0) {
                duplicate = 1;
                break;
            }
        }

        if (!duplicate) {
            char part_path[MAX_PATH_LEN];
            snprintf(part_path, sizeof(part_path), "%s/%s.000", RELIC_DIR, base);

            if (access(part_path, F_OK) == 0) {
                strcpy(listed[listed_count++], base);
                filler(buf, base, NULL, 0, 0);
            }
        }
    }

    closedir(dp);
    return 0;
}
```

### Memastikan file virtual ada saat di buka
```c
static int xmp_open(const char *path, struct fuse_file_info *fi) {
    char base_filename[MAX_FILENAME_LEN + 1];
    strncpy(base_filename, path + 1, MAX_FILENAME_LEN); // mwnyalin nama file dari path
    base_filename[MAX_FILENAME_LEN] = '\0';
    char first_part[MAX_PATH_LEN];
    snprintf(first_part, sizeof(first_part), "%s/%s.000", RELIC_DIR, base_filename); // menyusun path lengkap ke file fragmen pertama
    if (access(first_part, F_OK) != 0) return -ENOENT; // mengecek apakah file fragmen ada
    return 0;
}
```
- `path + 1` : ambil string mulai dari indeks ke-1 (lewat`/`)

### Membaca dan menggabungkan file virtual
```c
static int xmp_read(const char *path, char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi)
{
    char base_filename[MAX_FILENAME_LEN + 1];
    strncpy(base_filename, path + 1, MAX_FILENAME_LEN);
    base_filename[MAX_FILENAME_LEN] = '\0';

    log_activity("READ", base_filename);

    size_t total_read = 0;
    int part_num = 0;
    off_t cur_offset = 0;

    // loop membaca file pecahan sampai jumlah byte yang diminta (size) terpenuhi
    while (total_read < size) { 
        char part_path[MAX_PATH_LEN];
        snprintf(part_path, sizeof(part_path),
                "%s/%s.%03d", RELIC_DIR, base_filename, part_num);

        FILE *fp = fopen(part_path, "rb");
        if (!fp) break;

        // menghitunng ukuran file pecahan
        fseek(fp, 0, SEEK_END);
        size_t part_size = ftell(fp);
        rewind(fp);

        if (offset >= cur_offset + (off_t)part_size) {
            cur_offset += part_size;
            fclose(fp);
            part_num++;
            continue;
        }

        // menghitung posisi awal dalam pecahan yang perlu di baca
        off_t read_off = (offset > cur_offset) ? 
                        (offset - cur_offset) : 0;

        size_t remain = part_size - read_off; // banyak bite yang tersisa
        size_t to_read = (remain < (size - total_read)) ?  // jumlaah byte yang akan dibaca
                        remain : (size - total_read);

        fseek(fp, read_off, SEEK_SET);
        size_t n = fread(buf + total_read, 1, to_read, fp);
        fclose(fp);

        if (n == 0) break;

        // memperbaharui status pembacaan
        total_read += n;
        offset += n;
        part_num++;
        cur_offset += part_size;
    }

    return total_read;
}
```

### Membuat file baru di mount_dir
```c
static int xmp_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void) fi;
    char base[MAX_FILENAME_LEN + 1];
    strncpy(base, path + 1, MAX_FILENAME_LEN);
    base[MAX_FILENAME_LEN] = '\0';

    mkdir(RELIC_DIR, 0755); // membuat folder relic jika belum ada dan permissionnya

    char part_path[MAX_PATH_LEN];
    snprintf(part_path, sizeof(part_path), "%s/%s.000", RELIC_DIR, base);

    if (access(part_path, F_OK) == 0) {
        printf("File %s already exists.\n", base);
        return -EEXIST;
    }

    // membuka atau membuat file .000 untuk di tulis
    int fd = open(part_path, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (fd == -1) return -errno;
    close(fd);

    char log_msg[MAX_LOG_LEN];
    snprintf(log_msg, sizeof(log_msg), "%s -> %s.000", base, base);
    log_activity("WRITE", log_msg);
    return 0;
}
```
- `O_CREAT` : buat file jika belum ada.
- `O_TRUNC` : kosongkan file jika sudah ada

### Penulisan data ke file 
```c
static int xmp_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    char base_filename[MAX_FILENAME_LEN + 1];
    strncpy(base_filename, path + 1, MAX_FILENAME_LEN);
    base_filename[MAX_FILENAME_LEN] = '\0';

    size_t total_written = 0; // jumlah byte yang berhasil di tulis
    int part_num = offset / MAX_PART_SIZE;
    size_t part_offset = offset % MAX_PART_SIZE;

    char log_msg[MAX_LOG_LEN] = ""; // buffer untuk isi log
    char part_info[MAX_FILENAME_LEN + 8]; // menyimpan nama fragmen
    int log_full = 0; // flag log jika terlalu panjang

    while (size > 0) {
        char part_path[MAX_PATH_LEN];
        snprintf(part_path, sizeof(part_path), "%s/%s.%03d", RELIC_DIR, base_filename, part_num);

        //memastikan direktori relics ada
        struct stat st = {0}; 
        if (stat(RELIC_DIR, &st) == -1) {
            mkdir(RELIC_DIR, 0755);
        }

        FILE *fp = fopen(part_path, "r+b");
        if (!fp) {
            fp = fopen(part_path, "wb");
        }
        if (!fp) {
            perror("Error opening fragment file");
            return -errno;
        }

        fseek(fp, part_offset, SEEK_SET);

        size_t to_write = (size > MAX_PART_SIZE - part_offset) ? (MAX_PART_SIZE - part_offset) : size; // hitung berapa byte yang bisa ditulis ke fragmen
        size_t bytes_written = fwrite(buf, 1, to_write, fp);
        fclose(fp);

        if (bytes_written <= 0) break;

        snprintf(part_info, sizeof(part_info), "%s.%03d", base_filename, part_num); // format nama frragmen


        if (log_full == 0) {
            if (total_written > 0) strncat(log_msg, ", ", sizeof(log_msg) - strlen(log_msg) - 1);

            if (strlen(log_msg) + strlen(part_info) < sizeof(log_msg) - 5) {
                strncat(log_msg, part_info, sizeof(log_msg) - strlen(log_msg) - 1);
            } else {
                strncat(log_msg, "...", sizeof(log_msg) - strlen(log_msg) - 1);
                log_full = 1;
            }
        }

        // update variabel untuk iterasi berikutnya
        buf += bytes_written;
        size -= bytes_written;
        total_written += bytes_written;
        part_num++;
        part_offset = 0;
    }

    if (total_written > 0) {
        char full_log_msg[MAX_LOG_LEN];

        // format log
        if (strlen(base_filename) + 4 < sizeof(full_log_msg)) {
            strncpy(full_log_msg, base_filename, sizeof(full_log_msg) - 1);
            strncat(full_log_msg, " -> ", sizeof(full_log_msg) - strlen(full_log_msg) - 1);

            if (strlen(log_msg) + strlen(full_log_msg) < sizeof(full_log_msg) - 1) {
                strncat(full_log_msg, log_msg, sizeof(full_log_msg) - strlen(full_log_msg) - 1);
            } else {
                strncat(full_log_msg, "...", sizeof(full_log_msg) - strlen(full_log_msg) - 1);
            }
        } else {
            strncpy(full_log_msg, "Log to Long", sizeof(full_log_msg) - 1);
        }

        log_activity("WRITE", full_log_msg);
    }

    return total_written;
}
```

### Menangani penghapusan file
```c
static int xmp_unlink(const char *path) {
    char base_filename[MAX_FILENAME_LEN + 1];
    strncpy(base_filename, path + 1, MAX_FILENAME_LEN);
    base_filename[MAX_FILENAME_LEN] = '\0';
    int last_part = count_file_parts(base_filename) - 1;
    if (last_part < 0) return -ENOENT;

    // loop dari awal sampe fragmen terakhir
    for (int i = 0; i <= last_part; i++) {
        char part_path[MAX_PATH_LEN];
        snprintf(part_path, sizeof(part_path), "%s/%s.%03d", RELIC_DIR, base_filename, i);
        unlink(part_path); // hapus file dengan unlink()
    }

    // catat ke log
    char log_msg[MAX_LOG_LEN];
    snprintf(log_msg, sizeof(log_msg), "%s.000 - %s.%03d", base_filename, base_filename, last_part);
    log_activity("DELETE", log_msg);
    return 0;
}
```
- `count_file_parts(` : mengetahui jumlah fragmen.

### Struktur utama untuk semua operasi FUSE yang kita buat
```c
static struct fuse_operations xmp_oper = {
    .getattr = xmp_getattr,
    .readdir = xmp_readdir,
    .open    = xmp_open,
    .read    = xmp_read,
    .write   = xmp_write,
    .unlink  = xmp_unlink,
    .create  = xmp_create,
};
```
### Fungsi utama pada kode ini
```c
int main(int argc, char *argv[]) {
    umask(0); // menonaktifkan file permission mask

    struct fuse_args args = FUSE_ARGS_INIT(0, NULL); 

    fuse_opt_add_arg(&args, argv[0]);

    if (argc > 1) // mengecek apakah ada mountpoint yang diberikan
        fuse_opt_add_arg(&args, argv[1]);
    else {
        fprintf(stderr, "Usage: %s <mountpoint>\n", argv[0]);
        return 1;
    }

    // mengatur jumlah maksimum thread idle = 16
    fuse_opt_add_arg(&args, "-o");
    fuse_opt_add_arg(&args, "max_idle_threads=16");

    return fuse_main(args.argc, args.argv, &xmp_oper, NULL);
}
```

## Beberapa Masalah yang terjadi

![Image](https://github.com/user-attachments/assets/9b730811-3528-4a3e-8237-55619e0af610)


![Image](https://github.com/user-attachments/assets/93c72c99-382f-4001-8e85-b5f3742bd184)


![Image](https://github.com/user-attachments/assets/8bcf6867-1823-454d-9ddf-364811a7b8d5)


![Image](https://github.com/user-attachments/assets/31523f7e-bba1-4d13-ad04-5ab851cdc2db)


![Image](https://github.com/user-attachments/assets/3fcdf078-9389-47e1-ad6e-1a0940e5d025)


![Image](https://github.com/user-attachments/assets/b70ee5e7-a890-4ee4-918b-e7a9719de894)


## Revisi
#### Copy belum tercatat pada activity.log
![Image](https://github.com/user-attachments/assets/d1991d9f-3bed-49f8-9982-3369a10b9fa9)
Mohon maaf mas/mba untuk revisi saya hanya mendapatkan hasil seperti ini dan belum mendapatkan format yang sesuai. 
