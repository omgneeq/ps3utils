/*
 * pup.c -- PS3 PUP update file extractor/creator
 *
 * Copyright (C) Youness Alaoui (KaKaRoTo)
 *
 * This software is distributed under the terms of the GNU General Public
 * License ("GPL") version 3, as published by the Free Software Foundation.
 *
 */


#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

#define VERSION "0.1"

#define PUP_MAGIC 0x5343455546000000  /* "SCEUF\0\0\0" */

typedef struct {
  uint64_t magic;
  uint64_t package_version;
  uint64_t image_version;
  uint64_t file_count;
  uint64_t header_length;
  uint64_t data_length;
} PUPHeader;

typedef struct {
  uint64_t entry_id;
  uint64_t data_offset;
  uint64_t data_length;
  uint8_t padding[8];
} PUPFileEntry;

typedef struct {
  uint64_t entry_id;
  uint8_t hash[20];
  uint8_t padding[4];
} PUPHashEntry;

typedef struct
{
  uint8_t hash[20];
  uint8_t padding[12];
} PUPFooter;

#define ntohll(x) (((uint64_t) ntohl (x) << 32) | (uint64_t) ntohl (x >> 32) )

static void usage (const char*program)
{
  fprintf (stderr, "Usage:\n\t%s [e|c] <file> <directory>\n\n", program);
  exit (-1);
}

static const char *id_to_filename (uint64_t entry_id)
{
  switch (entry_id) {
    case 0x100:
      return "version.txt";
    case 0x101:
      return "license.txt";
    case 0x102:
      return "promo_flags.txt";
    case 0x103:
      return "update_flags.txt";
    case 0x104:
      return "patch_build.txt";
    case 0x200:
      return "ps3swu.self";
    case 0x201:
      return "vsh.tar";
    case 0x202:
      return "dots.txt";
    case 0x203:
      return "patch_data.pkg";
    case 0x300:
      return "update_files.tar";
    default:
      return NULL;
  }
}

static void extract (const char *file, const char *dest)
{
  FILE *fd = NULL;
  FILE *out = NULL;
  int read;
  int written;
  int i;
  PUPHeader header;
  PUPFooter footer;
  PUPFileEntry *files = NULL;
  PUPHashEntry *hashes = NULL;
  char filename[PATH_MAX+1];
  char buffer[1024];

  fd = fopen (file, "rb");

  if (fd == NULL) {
    perror ("Opening input file");
    goto error;
  }

  read = fread (&header, sizeof(PUPHeader), 1, fd);

  if (read < 1) {
    perror ("Couldn't read header");
    goto error;
  }

  header.magic = ntohll(header.magic);
  header.package_version = ntohll(header.package_version);
  header.image_version = ntohll(header.image_version);
  header.file_count = ntohll(header.file_count);
  header.header_length = ntohll(header.header_length);
  header.data_length = ntohll(header.data_length);

  if (header.magic != PUP_MAGIC) {
    fprintf (stderr, "Magic number is not the same 0x%X%X\n",
        (uint32_t) (header.magic >> 32), (uint32_t) header.magic);
    goto error;
  }

  files = malloc (header.file_count * sizeof(PUPFileEntry));
  hashes = malloc (header.file_count * sizeof(PUPHashEntry));

  read = fread (files, sizeof(PUPFileEntry), header.file_count, fd);

  if (read < header.file_count) {
    perror ("Couldn't read file entries");
    goto error;
  }

  read = fread (hashes, sizeof(PUPHashEntry), header.file_count, fd);

  if (read < header.file_count) {
    perror ("Couldn't read hash entries");
    goto error;
  }

  read = fread (&footer, sizeof(PUPFooter), 1, fd);

  if (read < 1) {
    perror ("Couldn't read footer");
    goto error;
  }

  printf ("PUP file found\nPackage version: %llu\nImage version: %llu\n"
      "File count: %llu\nHeader length: %llu\nData length: %llu\n",
      header.package_version, header.image_version,
      header.file_count, header.header_length, header.data_length);

  if (mkdir (dest, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
    perror ("Couldn't create output directory");
    goto error;
  }

  for (i = 0; i < header.file_count; i++) {
    const char *file = NULL;
    int len;

    files[i].entry_id = ntohll (files[i].entry_id);
    files[i].data_offset = ntohll (files[i].data_offset);
    files[i].data_length = ntohll (files[i].data_length);
    hashes[i].entry_id = ntohll (hashes[i].entry_id);
    file = id_to_filename (files[i].entry_id);

    printf ("\tFile %d\n\tEntry id: 0x%X\n\tFilename : %s\n\t"
        "Data offset: 0x%X\n\tData length: %llu\n",
        (uint32_t) hashes[i].entry_id,  (uint32_t) files[i].entry_id,
        file ? file : "Unknown entry id",
        (uint32_t) files[i].data_offset, files[i].data_length);

    if (file == NULL) {
      printf ("*** Unknown entry id, file skipped ****\n\n");
      continue;
    }
    sprintf (filename, "%s/%s", dest, file);

    printf ("Opening file %s\n", filename);
    out = fopen (filename, "wb");
    if (out == NULL) {
      perror ("Could not open output file");
      goto error;
    }
    fseek (fd, files[i].data_offset, SEEK_SET);
    do {
      len = files[i].data_length;
      if (len > sizeof(buffer))
        len = sizeof(buffer);
      read = fread (buffer, 1, len, fd);

      if (read < len) {
        perror ("Couldn't read all the data");
        goto error;
      }

      written = fwrite (buffer, 1, len, out);
      if (written < len) {
        perror ("Couldn't write all the data");
        goto error;
      }
      files[i].data_length -= len;
    } while (files[i].data_length > 0);

    fclose (out);
    out = NULL;
  }

  fclose (fd);
  free (files);
  free (hashes);

  return;

 error:
  if (fd)
    fclose (fd);
  if (out)
    fclose (out);
  if (files)
    free (files);
  if (hashes)
    free (hashes);

  exit (-2);
}

static void create (const char *directory, const char *dest)
{
  fprintf (stderr, "Not implemented yet");
}


int main (int argc, char *argv[])
{
  struct stat stat_buf;

  fprintf (stderr, "PUP Creator/Extractor %s\nBy KaKaRoTo\n\n", VERSION);

  if (argc != 4)
    usage (argv[0]);

  if (argv[1][1] != '\0')
    usage (argv[0]);

  switch (argv[1][0]) {
    case 'e':
      if (stat (argv[2], &stat_buf) != 0) {
        fprintf (stderr, "Input file does not exist\n");
        exit (-1);
      }
      if (stat (argv[3], &stat_buf) == 0) {
        fprintf (stderr, "Destination directory must not exist\n");
        exit (-1);
      }
      extract (argv[2], argv[3]);
      break;
    case 'c':
      if (stat (argv[2], &stat_buf) == 0) {
        fprintf (stderr, "Output file must not exist\n");
        exit (-1);
      }
      if (stat (argv[3], &stat_buf) != 0) {
        fprintf (stderr, "Input directory does not exist\n");
        exit (-1);
      }
      create (argv[3], argv[2]);
      break;
    default:
      usage (argv[0]);
  }

  return 0;
}
