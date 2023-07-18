// #include <stdio.h>
// #include <stdint.h>
// #include <stdlib.h>
// #include <string.h>
// #include <assert.h>
// #include <fcntl.h>
// #include <unistd.h>
// #include <sys/mman.h>

// typedef uint8_t u8;
// typedef uint16_t u16;
// typedef uint32_t u32;

// // Copied from the manual
// struct fat32hdr {
//   u8  BS_jmpBoot[3];
//   u8  BS_OEMName[8];
//   u16 BPB_BytsPerSec;
//   u8  BPB_SecPerClus;
//   u16 BPB_RsvdSecCnt;
//   u8  BPB_NumFATs;
//   u16 BPB_RootEntCnt;
//   u16 BPB_TotSec16;
//   u8  BPB_Media;
//   u16 BPB_FATSz16;
//   u16 BPB_SecPerTrk;
//   u16 BPB_NumHeads;
//   u32 BPB_HiddSec;
//   u32 BPB_TotSec32;
//   u32 BPB_FATSz32;
//   u16 BPB_ExtFlags;
//   u16 BPB_FSVer;
//   u32 BPB_RootClus;
//   u16 BPB_FSInfo;
//   u16 BPB_BkBootSec;
//   u8  BPB_Reserved[12];
//   u8  BS_DrvNum;
//   u8  BS_Reserved1;
//   u8  BS_BootSig;
//   u32 BS_VolID;
//   u8  BS_VolLab[11];
//   u8  BS_FilSysType[8];
//   u8  __padding_1[420];
//   u16 Signature_word;
// } __attribute__((packed));


// void* map_disk(const char* fname);

// int main(int argc, char* argv[]) {
//   if (argc < 2) {
//     fprintf(stderr, "Usage: %s fs-image\n", argv[0]);
//     exit(1);
//   }

//   setbuf(stdout, NULL);

//   assert(sizeof(struct fat32hdr) == 512); // defensive

//   // map disk image to memory
//   struct fat32hdr* hdr = map_disk(argv[1]);
//   size_t cluster_sz = hdr->BPB_BytsPerSec * hdr->BPB_SecPerClus;
//   u32 data_sec = hdr->BPB_RsvdSecCnt + hdr->BPB_NumFATs * hdr->BPB_FATSz32;
//   u8* data_st = (u8*)hdr + data_sec * hdr->BPB_BytsPerSec;
//   u8* data_ed = (u8*)hdr + hdr->BPB_TotSec32 * hdr->BPB_BytsPerSec;

//   for (u8* cluster_st = data_st; cluster_st < data_ed; cluster_st += cluster_sz) {
//     if (*(u16*)cluster_st == 0x424d) printf("Found a BMP header!\n");
//     // else printf("this cluster not found!\n");
//   }

//   // TODO: frecov

//   // file system traversal
//   munmap(hdr, hdr->BPB_TotSec32 * hdr->BPB_BytsPerSec);
// }





// void* map_disk(const char* fname) {
//   int fd = open(fname, O_RDWR);

//   if (fd < 0) {
//     perror(fname);
//     goto release;
//   }

//   off_t size = lseek(fd, 0, SEEK_END);
//   if (size == -1) {
//     perror(fname);
//     goto release;
//   }

//   struct fat32hdr* hdr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
//   if (hdr == (void*)-1) {
//     goto release;
//   }

//   close(fd);

//   if (hdr->Signature_word != 0xaa55 ||
//     hdr->BPB_TotSec32 * hdr->BPB_BytsPerSec != size) {
//     fprintf(stderr, "%s: Not a FAT file image\n", fname);
//     goto release;
//   }
//   return hdr;

// release:
//   if (fd > 0) {
//     close(fd);
//   }
//   exit(1);
// }



#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
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


struct fat32hdr* hdr;


void* map_disk(const char* fname);
void dfs(u32 clusId, int depth, int is_dir);

int main(int argc, char* argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s fs-image\n", argv[0]);
    exit(1);
  }

  setbuf(stdout, NULL);

  assert(sizeof(struct fat32hdr) == 512); // defensive

  // map disk image to memory
  hdr = map_disk(argv[1]);

  // file system traversal
  dfs(hdr->BPB_RootClus, 0, 1);

  munmap(hdr, hdr->BPB_TotSec32 * hdr->BPB_BytsPerSec);

}

void* map_disk(const char* fname) {
  int fd = open(fname, O_RDWR);
  if (fd < 0) {
    goto release;
  }

  off_t size = lseek(fd, 0, SEEK_END);
  if (size == -1) {
    goto release;
  }

  struct fat32hdr* hdr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  if (hdr == (void*)-1) {
    goto release;
  }

  close(fd);

  assert(hdr->Signature_word == 0xaa55); // this is an MBR
  assert(hdr->BPB_TotSec32 * hdr->BPB_BytsPerSec == size);

  return hdr;


release:
  perror("map disk");
  if (fd > 0) {
    close(fd);
  }
  exit(1);
}


u32 next_cluster(int n) {
  // RTFM: Sec 4.1
  u32* fat = (u32*)((u8*)hdr + hdr->BPB_RsvdSecCnt * hdr->BPB_BytsPerSec);
  return fat[n];
}


void* cluster_to_sec(int n) {
  // RTFM: Sec 3.5 and 4 (TRICKY)
  // Don't copy code. Write your own.
  u32 DataSec = hdr->BPB_RsvdSecCnt + hdr->BPB_NumFATs * hdr->BPB_FATSz32;
  DataSec += (n - 2) * hdr->BPB_SecPerClus;
  return ((char*)hdr) + DataSec * hdr->BPB_BytsPerSec;
}

#define CLUS_INVALID 0xfffff7

void dfs(u32 clusId, int depth, int is_dir) {
  // RTFM: Sec 6
  for (; clusId < CLUS_INVALID; clusId = next_cluster(clusId)) {
    printf("#%d \n", clusId);
  }
}