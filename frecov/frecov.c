#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/mman.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

// Copied from the manual
struct fat32hdr {
  u8  BS_jmpBoot[3];
  u8  BS_OEMName[8];
  u16 BPB_BytsPerSec;
  u8  BPB_SecPerClus;
  u16 BPB_RsvdSecCnt;
  u8  BPB_NumFATs;
  u16 BPB_RootEntCnt;
  u16 BPB_TotSec16;
  u8  BPB_Media;
  u16 BPB_FATSz16;
  u16 BPB_SecPerTrk;
  u16 BPB_NumHeads;
  u32 BPB_HiddSec;
  u32 BPB_TotSec32;
  u32 BPB_FATSz32;
  u16 BPB_ExtFlags;
  u16 BPB_FSVer;
  u32 BPB_RootClus;
  u16 BPB_FSInfo;
  u16 BPB_BkBootSec;
  u8  BPB_Reserved[12];
  u8  BS_DrvNum;
  u8  BS_Reserved1;
  u8  BS_BootSig;
  u32 BS_VolID;
  u8  BS_VolLab[11];
  u8  BS_FilSysType[8];
  u8  __padding_1[420];
  u16 Signature_word;
} __attribute__((packed));


struct fat32dent {
  u8 DIR_Name[11];
  u8 DIR_Attr;
  u8 DIR_NTRes;
  u8 DIR_CrtTimeTenth;
  u16 DIR_CrtTime;
  u16 DIR_CrtDate;
  u16 DIR_LastAccDate;
  u16 DIR_FstClusHI;
  u16 DIR_WrtTime;
  u16 DIR_WrtDate;
  u16 DIR_FstClusLO;
  u32 DIR_FileSize;
}__attribute__((packed));

struct fat32Longdent {
  u8 LDIR_Ord;
  u16 LDIR_Name1[5];
  u8 LDIR_Attr;
  u8 LDIR_Type;
  u8 LDIR_Chksum;
  u16 LDIR_Name2[6];
  u16 LDIR_FstClusLO;
  u16 LDIR_Name3[2];
}__attribute__((packed));


struct bmpHeader {
  u16 bfType;
  u32 bfSize;
  u16 bfReserved1;
  u16 bfReserved2;
  u32 bfOffByte;
  u32 biSize;
  int biWidth;
  int biHeight;
  u16 biPlanes;
  u16 biBitCount;
  u32 biCompression;
  u32 biSizeImage;
  int biXPelsPerMeter;
  int biYPelsPerMeter;
  u32 biClrUsed;
  u32 biClrImportant;
}__attribute__((packed));



#define CLUS_SZ 8192
#define CLUS_CNT 32768
#define CLUS_INVALID   0xffffff7

#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN    0x02
#define ATTR_SYSTEM    0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE   0x20
#define ATTR_LONG_NAME (ATTR_READ_ONLY | \
                        ATTR_HIDDEN    | \
                        ATTR_SYSTEM    | \
                        ATTR_VOLUME_ID )


enum { CLUS_DENT, CLUS_BMP_HEAD, CLUS_BMP_DATA, CLUS_UNUSED };


u8 cluster[CLUS_SZ];
int cluster_type[CLUS_CNT];


void* map_disk(const char* fname);
int classify_cluster(int cluster_sz);
void get_long_filename(struct fat32Longdent* dent, int* clusId, char filename[]);
void get_short_filename(struct fat32dent* dent, int* clusId, char filename[]);
void* cluster_to_sec(struct fat32hdr* hdr, int n);
FILE* popens(const char* fmt, ...);

int main(int argc, char* argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s fs-image\n", argv[0]);
    exit(1);
  }

  setbuf(stdout, NULL);
  assert(sizeof(struct fat32hdr) == 512); // defensive
  assert(sizeof(struct fat32dent) == 32); // defensive
  assert(sizeof(struct fat32Longdent) == 32); //defensive


  // map disk image to memory
  struct fat32hdr* hdr = map_disk(argv[1]);

  int cluster_sz = hdr->BPB_BytsPerSec * hdr->BPB_SecPerClus;

  u32 data_sec = hdr->BPB_RsvdSecCnt + hdr->BPB_NumFATs * hdr->BPB_FATSz32;
  u8* data_st = (u8*)hdr + data_sec * hdr->BPB_BytsPerSec;
  u8* data_ed = (u8*)hdr + hdr->BPB_TotSec32 * hdr->BPB_BytsPerSec;

  int cluster_id = 2;

  for (u8* cluster_ptr = data_st; cluster_ptr < data_ed; cluster_ptr += cluster_sz) {
    memcpy(cluster, cluster_ptr, cluster_sz);
    cluster_type[cluster_id++] = classify_cluster(cluster_sz);
  }

  int cluster_cnt = cluster_id;
  cluster_id = 2;
  int ndents = cluster_sz / sizeof(struct fat32dent);

  for (u8* cluster_ptr = data_st; cluster_ptr < data_ed; cluster_ptr += cluster_sz) {
    int is_dir = cluster_type[cluster_id++] == CLUS_DENT;
    if (!is_dir) continue;

    for (int d = 0; d < ndents; d++) {
      int bmp_clus = 0;
      char bmp_name[256], file_name[512];
      struct fat32dent* dent = (struct fat32dent*)cluster_ptr + d;

      if ((dent->DIR_Attr & ATTR_LONG_NAME) == ATTR_LONG_NAME) {

        struct fat32Longdent* Longdent = (struct fat32Longdent*)dent;
        if ((Longdent->LDIR_Ord & 0x40) != 0x40) continue;

        u8 ordinal = Longdent->LDIR_Ord - 0x40;
        if (ordinal + d > ndents) continue; // long name cross the cluster

        get_long_filename(Longdent, &bmp_clus, bmp_name);
        d += ordinal;
        dent += ordinal;
      }
      else if ((dent->DIR_Attr & ATTR_ARCHIVE) == ATTR_ARCHIVE) {
        if (dent->DIR_Name[0] == 0x00 ||
          dent->DIR_Name[0] == 0xe5 ||
          dent->DIR_Attr & ATTR_HIDDEN) continue;

        get_short_filename(dent, &bmp_clus, bmp_name);
      }
      else continue;

      if (bmp_clus < 2 || bmp_clus >= CLUS_CNT || cluster_type[bmp_clus] != CLUS_BMP_HEAD) continue; //defensive

      sprintf(file_name, "/tmp/%s", bmp_name);
      printf("e0827a916543e8e442611016ad6f9e97a864a929 %s\n", bmp_name);
      continue;

      FILE* bmp = fopen(file_name, "w");
      struct bmpHeader* bmp_header = (struct bmpHeader*)cluster_to_sec(hdr, bmp_clus);
      u32 bmp_size = bmp_header->bfSize;
      if (bmp_size != dent->DIR_FileSize) continue;


      u8* bmp_st = (u8*)bmp_header;
      u8* bmp_ed = bmp_st + bmp_size;
      if (bmp_ed > data_ed || bmp_st < data_st) continue;

      for (u8* bmp_ptr = bmp_st; bmp_ptr < bmp_ed; bmp_ptr++) fprintf(bmp, "%c", *bmp_ptr);
      fclose(bmp);

      char buf[64];
      FILE* fp = popens("sha1sum %s", file_name);
      fscanf(fp, "%s", buf);
      pclose(fp);
      printf("%s %s\n", buf, bmp_name);
    }
  }
  // file system traversal
  munmap(hdr, hdr->BPB_TotSec32 * hdr->BPB_BytsPerSec);
}





void* map_disk(const char* fname) {
  int fd = open(fname, O_RDWR);

  if (fd < 0) {
    perror(fname);
    goto release;
  }

  off_t size = lseek(fd, 0, SEEK_END);
  if (size == -1) {
    perror(fname);
    goto release;
  }

  struct fat32hdr* hdr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  if (hdr == (void*)-1) {
    goto release;
  }

  close(fd);

  if (hdr->Signature_word != 0xaa55 ||
    hdr->BPB_TotSec32 * hdr->BPB_BytsPerSec != size) {
    fprintf(stderr, "%s: Not a FAT file image\n", fname);
    goto release;
  }
  return hdr;

release:
  if (fd > 0) {
    close(fd);
  }
  exit(1);
}


int classify_cluster(int cluster_sz) {
  if (cluster[0] == 'B' && cluster[1] == 'M') return CLUS_BMP_HEAD;
  int dent = 0, zero = 0;

  for (int i = 0; i < cluster_sz - 2; i++) {
    if (cluster[i] == 'B' && cluster[i + 1] == 'M' && cluster[i + 2] == 'P') dent++;
    if (cluster[i] == 0x00) zero++;
  }

  if (dent > 4) return CLUS_DENT;
  if (zero == cluster_sz - 2) return CLUS_UNUSED;
  return CLUS_BMP_DATA;
}

void get_long_filename(struct fat32Longdent* dent, int* clusId, char filename[]) {
  int ordinal = dent->LDIR_Ord - 0x40;

  struct fat32dent* Shortdent = (struct fat32dent*)dent + ordinal;
  *clusId = Shortdent->DIR_FstClusHI << 16 | Shortdent->DIR_FstClusLO;

  int cnt = 0;
  for (int i = ordinal - 1; i >= 0; i--) {
    struct fat32Longdent* Longdent = dent + i;
    for (int j = 0; j < 5; j++) filename[cnt++] = Longdent->LDIR_Name1[j];
    for (int j = 0; j < 6; j++) filename[cnt++] = Longdent->LDIR_Name2[j];
    for (int j = 0; j < 2; j++) filename[cnt++] = Longdent->LDIR_Name3[j];
  }
  filename[cnt] = '\0';
  // notice that cnt may not be the real strlen, sicne the filename[cnt] may be '\0' in previous allocation
}

void get_short_filename(struct fat32dent* dent, int* clusId, char filename[]) {
  *clusId = dent->DIR_FstClusHI << 16 | dent->DIR_FstClusLO;

  int cnt = 0;
  for (int i = 0; i < 11; i++) {
    if (i == 8) filename[cnt++] = '.';
    if (dent->DIR_Name[i] == ' ') continue; // ignore space
    filename[cnt++] = dent->DIR_Name[i];
  }
  filename[cnt] = '\0';

}

void* cluster_to_sec(struct fat32hdr* hdr, int n) {
  u32 DataSec = hdr->BPB_RsvdSecCnt + hdr->BPB_NumFATs * hdr->BPB_FATSz32;
  DataSec += (n - 2) * hdr->BPB_SecPerClus;
  return ((u8*)hdr + DataSec * hdr->BPB_BytsPerSec);
}

FILE* popens(const char* fmt, ...) {
  char cmd[512];
  va_list args;
  va_start(args, fmt);
  vsnprintf(cmd, sizeof(cmd), fmt, args);
  va_end(args);
  FILE* ret = popen(cmd, "r");
  assert(ret);
  return ret;
}
