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
Mohon maaf mas/mba untuk revisi saya hanya mendapatkan hasil seperti ini dan belum mendapatkan format yang sesuai. Saya juga cukup bingung terkait proses copy ini dan menentukan format copy yang menyertakan direktori tujuan dari file yang kita copy. 

# Soal 3
Dikerjakan oleh Muhammad Ahsani Taqwiim rakhman / 5027241099

tugas utama:
Membuat sistem pendeteksi dan penanganan file yang mengandung kata kunci "nafis" atau "kimcun" menggunakan Docker dan FUSE, dengan fitur keamanan tambahan seperti pembalikan nama file dan enkripsi isi file.

## Dockerfile
```c

FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    fuse \
    libfuse-dev \
    pkg-config \
    build-essential \
    inotify-tools

WORKDIR /app
COPY . /app

RUN gcc -Wall anthk.c `pkg-config fuse --cflags --libs` -o antinkfs

CMD mkdir -p /it24_host /antink_mount /var/log && \
    touch /var/log/it24.log && \
    /app/antinkfs /antink_mount -o allow_other -f

```
`fuse`: Paket untuk mendukung FUSE (Filesystem in Userspace).

`libfuse-dev`: Library development untuk FUSE.

`pkg-config`: Membantu kompilasi program dengan dependensi eksternal.

`build-essential`: Berisi tools kompilasi seperti `gcc` dan `make`.

`inotify-tools`: Untuk memantau perubahan file secara real-time (opsional, tergantung kebutuhan logger).

Menetapkan `/app` sebagai direktori kerja dan Menyalin semua file dari direktori build ke `/app` di dalam container.

Mengompilasi file `antink.c` (program FUSE) menjadi binary `antinkfs`.

`pkg-config fuse --cflags --libs`: Otomatis menambahkan flag kompilasi dan linking untuk FUSE.

`-Wall`: Mengaktifkan peringatan kompilasi untuk debugging.

Membuat direktori:

`/it24_host`: Untuk menyimpan file asli (bind mount dari host).

`/antink_mount`: Mount point FUSE.

`/var/log`: Untuk menyimpan log.

Membuat file log:

`touch` `/var/log/it24.log`: File log aktivitas sistem.

Menjalankan FUSE:

`/app/antinkfs /antink_mount`: Memount sistem file FUSE ke /antink_mount.

Flag:

`-o allow_other`: Izinkan pengguna lain (selain root) mengakses mount point.

`-f`: Menjalankan FUSE di latar depan (foreground) untuk logging.

## docker-compose.yml

```c
version: '3.8'

services:
  antink-server:
    build: .
    container_name: antink-server
    privileged: true
    volumes:
      - ./it24_host:/it24_host:rw
      - ./antink_mount:/antink_mount:rw
      - ./antink_logs:/var/log:rw

  antink-logger:
    image: ubuntu:22.04
    container_name: antink-logger
    depends_on:
      antink-server:
        condition: service_started
    volumes:
      - ./antink_logs:/var/log:rw
    command: ["sh", "-c", "tail -f /var/log/it24.log"]
```
Gunakan docker-compose untuk mengelola dua container:

`antink-server`: Menjalankan sistem file FUSE.

`antink-logger`: Memantau log secara real-time.

Siapkan bind mount untuk:

`it24_host`: Menyimpan file asli.

`antink_mount`: Mount point FUSE.

`antink-logs`: Menyimpan log aktivitas.

## Antink.c

```c

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

```
Deteksi File Berbahaya:

Jika nama file mengandung "nafis" atau "kimcun", balikkan namanya (misal: `nafis.txt` → `txt.sifan`).

Enkripsi Konten:

File normal: Enkripsi isi dengan ROT13 saat dibaca.

File berbahaya: Biarkan isi asli tanpa enkripsi.

Logging:

Catat semua aktivitas (baca, tulis, hapus) ke `/var/log/it24.log`.

Isolasi:

Perubahan file hanya berlaku di dalam container antink-server, tidak memengaruhi direktori host.

## Output

start up docker:
![Screenshot 2025-05-23 215855](https://github.com/user-attachments/assets/af5c2095-674c-474e-b91e-96230fdbb39f)

deteksi jika file berbahaya:
![Screenshot 2025-05-23 215929](https://github.com/user-attachments/assets/ba1d2282-398b-4948-a16e-b312d89a98de)

Output jika file normal:
![Screenshot 2025-05-23 215955](https://github.com/user-attachments/assets/43ae5627-f92f-4605-9a5a-40c24f067367)

output jika file berbahaya:
![Screenshot 2025-05-23 220027](https://github.com/user-attachments/assets/c85b7b21-55d6-4177-919b-d876e7d346e3)

pencatatan aktifitas:
![Screenshot 2025-05-23 220047](https://github.com/user-attachments/assets/12ba176f-5201-42e2-a00a-e0defa63eb18)

# Soal 4

Tugas utama:

Starter Chiho: File di `fuse_dir/starter` disimpan di `chiho/starter` dengan ekstensi .mai (tanpa .mai di FUSE).

Metropolis Chiho: Karakter nama file di-shift secara berurutan: +1, +2, +3, dst. (mod 256).

Dragon Chiho: Enkripsi isi file dengan ROT13 di direktori asli.

Black Rose Chiho: Simpan data dalam format biner mentah.

Heaven Chiho: Enkripsi isi dengan AES-256-CBC (gunakan OpenSSL) dengan IV dinamis.

Youth Chiho: Kompresi file menggunakan `gzip` (lib zlib).

7sRef Chiho: Gateway untuk mengakses file di area lain dengan format `[area]_[nama_file]`.

```c
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


struct {
    const char *rootdir;
    const char *mountdir;
} maimai_config;


char* build_path(const char *path) {
    char *fullpath = malloc(strlen(maimai_config.rootdir) + strlen(path) + 1);
    sprintf(fullpath, "%s%s", maimai_config.rootdir, path);
    return fullpath;
}


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

void dragon_rot13(char *str, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if ((str[i] >= 'a' && str[i] <= 'm') || (str[i] >= 'A' && str[i] <= 'M')) {
            str[i] += 13;
        } else if ((str[i] >= 'n' && str[i] <= 'z') || (str[i] >= 'N' && str[i] <= 'Z')) {
            str[i] -= 13;
        }
    }
}


char* blackrose_transform_path(const char *path) {
    if (strstr(path, "/blackrose/") != path) return NULL;
    
    const char *filename = path + strlen("/blackrose/");
    if (*filename == '\0') return NULL;
    
    char *newpath = malloc(strlen(maimai_config.rootdir) + strlen(path) + 1);
    sprintf(newpath, "%s%s", maimai_config.rootdir, path);
    return newpath;
}

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
    

    argv[1] = argv[2];
    argv[2] = NULL;
    argc = 2;
    
    return fuse_main(argc, argv, &maimai_oper, NULL);
}
```
Starter Chiho:

File disimpan dengan ekstensi .mai di direktori asli, tapi tanpa ekstensi di mount point FUSE.

Metropolis Chiho:

Konten file di-shift per byte: +1, +2, +3, ... (mod 256).

Dragon Chiho:

Enkripsi isi file dengan ROT13.

Black Rose Chiho:

Tidak ada transformasi (data biner mentah).

Heaven Chiho:

Enkripsi dengan AES-256-CBC (IV acak) saat disimpan.

Youth Chiho:

Kompresi isi file dengan `gzip`.

7sRef Chiho:

Gateway untuk mengakses file di chiho lain dengan format:
`/7sref/[area]_[file]` → `/[area]/[file]`.

Fungsi Utama:
Transformasi Path/Isi:
Setiap chiho memiliki fungsi khusus untuk mengubah path atau konten file (misal: starter_transform_path(), dragon_rot13()).

Operasi FUSE:

`getattr`, `readdir`: Menangani metadata dan direktori.

`open`, `read`, `write`: Membaca/menulis file dengan transformasi sesuai chiho.

`create`, `unlink`: Membuat/hapus file.

## output

starter:
![Screenshot 2025-05-23 220803](https://github.com/user-attachments/assets/22443d21-3b37-4648-8559-60c3ff2bf49c)

Metro:
![Screenshot 2025-05-23 220857](https://github.com/user-attachments/assets/889f0e53-eb6b-4b77-8e08-c41adf072c81)

Dragon:
![Screenshot 2025-05-23 221004](https://github.com/user-attachments/assets/1b421bae-086e-41d4-a5a9-874f76d34674)






