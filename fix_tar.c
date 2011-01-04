/*
 * fix_tar.c -- TAR fixer for PS3 packages
 *
 * Copyright (C) Youness Alaoui (KaKaRoTo)
 *
 * This software is distributed under the terms of the GNU General Public
 * License ("GPL") version 3, as published by the Free Software Foundation.
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char filename[100];
  char filemode[8];
  char owner_id[8];
  char group_id[8];
  char filesize[12];
  char atime[12];
  char checksum[8];
  char file_type;
  char link[100];
  char ustar[6];
  char ustar_version[2];
  char owner[32];
  char group[32];
  char device_major[8];
  char device_minor[8];
  char filename_prefix[155];
} TARHeader;

int main (int argc, char *argv[])
{
  FILE *fd = NULL;
  TARHeader block;
  size_t pos = 0;

  fprintf (stderr, "TAR Fixer for PS3 packages\n");
  fprintf (stderr, "By KaKaRoTo\n\n");

  if (argc != 2) {
    fprintf (stderr, "Usage: %s <file.tar>", argv[0]);
    exit (-1);
  }

  fd = fopen (argv[1], "rb+");

  if (fd == NULL) {
    perror ("Error opening input file");
    exit (-2);
  }

  do {
    size_t size = 0;
    unsigned int checksum = 0;
    unsigned int i;

    fseek (fd, pos, SEEK_SET);
    if (fread (&block, sizeof(TARHeader), 1, fd) != 1)
      break;

    // Found end of file block
    if (block.filename[0] == 0)
      break;

    printf ("Fixing file : %s\n", block.filename);
    printf ("\tOwner/group: %s(%s):%s(%s)\n", block.owner, block.owner_id, block.group, block.group_id);

    sscanf (block.filesize, "%o", &size);

    strncpy (block.owner_id, "0001752", 7);
    strncpy (block.group_id, "0001274", 7);
    strncpy (block.owner, "pup_tool", 32);
    strncpy (block.group, "psnes", 32);
    strncpy (block.ustar, "ustar  ", 7);
    memset (block.device_major, 0, 8);
    memset (block.device_minor, 0, 8);

    // Rebuild checksum
    memset (block.checksum, ' ', 8);
    for (i = 0; i < sizeof(TARHeader); i++)
      checksum += ((unsigned char *) &block)[i];

    snprintf (block.checksum, 8, "0%o", checksum);

    fseek (fd, pos, SEEK_SET);
    if (fwrite (&block, sizeof(TARHeader), 1, fd) != 1)
      break;

    pos += 512;
    pos += size;
    // padding to 512 block boundary
    if (size % 512 != 0)
      pos += 512 - (size % 512);

  } while (!feof (fd) && block.filename[0] != 0);

  return 0;
}
