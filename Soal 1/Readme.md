Soal Nomer 1
Dikerjakan oleh 5027241024 -

## **Header & Definisi**

```c
#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
// ... (library lainnya)

#define ZIP_ID "1hi_GDdP51Kn2JJMw02WmCOxuc3qrXzh5"
#define ZIP_FILE "anomali.zip"
#define EXTRACT_DIR "sampel_anomali"
#define IMAGE_DIR "image"
#define LOG_FILE "conversion.log"
FUSE library untuk membuat filesystem virtual

Define konstanta untuk ID Google Drive, nama file, dan direktori

## Fungsi Utilitas

```c
void run_shell(const char *cmd) {
    printf("[shell] %s\n", cmd);
    int status = system(cmd);
    if (status != 0) exit(1); // Handle error
}
```c
Menjalankan perintah shell (e.g., curl, unzip)

Jika gagal, program berhenti

```c
void get_timestamp(char *date_str, char *time_str) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(date_str, 16, "%Y-%m-%d", tm_info); // Format tanggal
    strftime(time_str, 16, "%H:%M:%S", tm_info); // Format waktu
}
```
Ambil timestamp untuk nama file dan log

Konversi Heksadesimal ke PNG
```c
void convert_hex_files(const char *src_dir, const char *out_dir) {
    DIR *dp = opendir(src_dir);
    mkdir(out_dir, 0755); // Buat folder 'image'
    FILE *logf = fopen(LOG_FILE, "a"); // Buka log file

    while ((de = readdir(dp)) != NULL) {
        if (de->d_type != DT_REG || !strstr(de->d_name, ".txt")) continue;

        // Baca isi file .txt
        FILE *fp = fopen(src_file, "r");
        fread(hex_str, 1, len, fp); 

        // Konversi heksa ke biner
        unsigned char *bin_data = malloc(bin_len);
        for (size_t i = 0; i < bin_len; ++i)
            sscanf(&hex_str[i * 2], "%2hhx", &bin_data[i]);

        // Simpan sebagai PNG
        snprintf(dst_file, "...%s_image_%s_%s.png", base, date, timebuf);
        FILE *outf = fopen(dst_file, "wb");
        fwrite(bin_data, 1, bin_len, outf);

        // Catat di log
        fprintf(logf, "[%s][%s]: Success...\n", date, timebuf, de->d_name);
    }
}
```
Loop file .txt di folder sampel

Konversi heksa ke biner per 2 karakter

Nama file menggunakan timestamp

Log setiap konversi sukses

FUSE Operations
```c
int x_getattr(const char *path, struct stat *stbuf, ...) {
    char fullpath[512];
    snprintf(fullpath, "%s%s", root_dir, path); // Gabung path
    return lstat(fullpath, stbuf); // Baca atribut file
}

int x_readdir(const char *path, void *buf, fuse_fill_dir_t filler, ...) {
    DIR *dp = opendir(fullpath);
    while ((de = readdir(dp)) != NULL)
        filler(buf, de->d_name, NULL, 0, 0); // List direktori
}

int x_read(const char *path, char *buf, size_t size, ...) {
    FILE *fp = fopen(fullpath, "rb");
    fread(buf, 1, size, fp); // Baca isi file
}
```
Implementasi filesystem virtual:

x_getattr: Cek atribut file

x_readdir: List isi folder

x_read: Baca konten file

Fungsi Main
```c
int main(int argc, char *argv[]) {
    // 1. Download zip jika belum ada
    if (access(ZIP_FILE, F_OK) {
        snprintf(cmd, "curl -L -o %s 'https://drive.google.com/uc?...", ZIP_FILE, ZIP_ID);
        run_shell(cmd);
    }

    // 2. Unzip dan hapus file zip
    run_shell("mkdir -p sampel_anomali");
    run_shell("unzip -o -q anomali.zip -d sampel_anomali");
    run_shell("rm -f anomali.zip");

    // 3. Konversi file
    convert_hex_files(EXTRACT_DIR, IMAGE_DIR);

    // 4. Jalankan FUSE
    return fuse_main(argc, argv, &ops, NULL);
}
```
Alur utama:

Download file zip

Ekstrak dan bersihkan

Proses konversi

Mount filesystem


# Kompilasi
```
gcc hexed.c -o hexed -lfuse3
```
# Jalankan (butuh sudo)
mkdir mount
```
./hexed -f mount
```
# Akses gambar
```
ls mount/  # File akan muncul di sini
```

# Unmount
```
fusermount -u mount
```
Catatan Penting
Wajib install fuse3 dan libfuse3-dev

File zip otomatis terhapus setelah diunzip

Hasil gambar bisa diakses via folder mount atau image
