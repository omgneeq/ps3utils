#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>


int main (int argc, char *argv[])
{
  FILE *in = NULL;
  char *buf;
  int i;
  int ret;

  if (argc != 2) {
    printf ("Usage : %s dump.bin\n", argv[0]);
    return -1;
  }

  in = fopen (argv[1], "rb");
  if (in == NULL) {
    perror ("Could not open input file ");
    return -1;
  }
  buf = malloc (8*1024*1024);
  ret = fread(buf, 1, 8*1024*1024, in);
  printf ("Read %d bytes\n", ret);
  for (i = 0; i < ret; i+=8) {
    uint64_t *syscall_table = (uint64_t *)(buf + i);
    uint64_t sc0 = syscall_table[0];

    if (syscall_table[1] != sc0 &&
        syscall_table[2] != sc0 &&
        syscall_table[3] != sc0 &&
        syscall_table[14] != sc0 &&
        syscall_table[15] == sc0 &&
        syscall_table[16] == sc0 &&
        syscall_table[17] == sc0 &&
        syscall_table[18] != sc0 &&
        syscall_table[19] != sc0 &&
        syscall_table[20] == sc0 &&
        syscall_table[21] != sc0 &&
        syscall_table[31] != sc0 &&
        syscall_table[32] == sc0 &&
        syscall_table[33] == sc0 &&
        syscall_table[41] != sc0 &&
        syscall_table[42] == sc0 &&
        syscall_table[43] != sc0) {
      printf ("Syscall table found at 0x%X\n", i);
    }
  }
  fclose (in);
  free (buf);

  return 0;
}
