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
#include <arpa/inet.h>
#include <string.h>

#include "sha1.h"

#define VERSION "0.2"

#define PUP_MAGIC (uint64_t) 0x5343455546000000  /* "SCEUF\0\0\0" */

static const uint8_t hmac_pup_key[] = {
  0xf4, 0x91, 0xad, 0x94, 0xc6, 0x81, 0x10, 0x96,
  0x91, 0x5f, 0xd5, 0xd2, 0x44, 0x81, 0xae, 0xdc,
  0xed, 0xed, 0xbe, 0x6b, 0xe5, 0x13, 0x72, 0x4d,
  0xd8, 0xf7, 0xb6, 0x91, 0xe8, 0x8a, 0x38, 0xf4,
  0xb5, 0x16, 0x2b, 0xfb, 0xec, 0xbe, 0x3a, 0x62,
  0x18, 0x5d, 0xd7, 0xc9, 0x4d, 0xa2, 0x22, 0x5a,
  0xda, 0x3f, 0xbf, 0xce, 0x55, 0x5b, 0x9e, 0xa9,
  0x64, 0x98, 0x29, 0xeb, 0x30, 0xce, 0x83, 0x66
};

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

static void usage (const char *program)
{
  fprintf (stderr, "Usage:\n\t%s <command> <options>\n\n"
      "Commands/Options:\n"
      "\ti <filename.pup>:\tInformation about the PUP file\n"
      "\tx <filename.pup> <output directory>:\tExtract PUP file\n"
      "\tc <input directory> <filename.pup>:\tCreate PUP file\n\n", program);
  exit (-1);
}

typedef struct {
  uint64_t id;
  const char *filename;
} PUPEntryID;

static const PUPEntryID entries[] = {
  {0x100, "version.txt"},
  {0x101, "license.xml"},
  {0x102, "promo_flags.txt"},
  {0x103, "update_flags.txt"},
  {0x104, "patch_build.txt"},
  {0x200, "ps3swu.self"},
  {0x201, "vsh.tar"},
  {0x202, "dots.txt"},
  {0x203, "patch_data.pkg"},
  {0x300, "update_files.tar"},
  {0, NULL}
};

static const char *id_to_filename (uint64_t entry_id)
{
  const PUPEntryID *entry = entries;

  while (entry->id) {
    if (entry->id == entry_id)
      return entry->filename;
    entry++;
  }
  return NULL;
}

static uint64_t filename_to_id (const char * entry_filename)
{
  const PUPEntryID *entry = entries;

  while (entry->filename) {
    if (strcmp(entry->filename, entry_filename) == 0)
      return entry->id;
    entry++;
  }
  return 0;
}

static void print_hash (const char *message, uint8_t hash[20])
{
  int i;

  printf ("%s : ", message);
  for (i = 0; i < 20; i++) {
    printf ("%.2X", hash[i]);
  }
  printf ("\n");
}

static void print_header_info (PUPHeader *header, PUPFooter *footer)
{
  printf ("PUP file information\n"
      "Package version: %llu\n"
      "Image version: %llu\n"
      "File count: %llu\n"
      "Header length: %llu\n"
      "Data length: %llu\n",
      header->package_version, header->image_version,
      header->file_count, header->header_length, header->data_length);

  print_hash ("PUP file hash", footer->hash);
}

static void print_file_info ( PUPFileEntry *file, PUPHashEntry *hash)
{
  const char *filename = NULL;

  filename = id_to_filename (file->entry_id);

  printf ("\tFile %d\n"
      "\tEntry id: 0x%X\n"
      "\tFilename : %s\n"
      "\tData offset: 0x%X\n"
      "\tData length: %llu\n",
      (uint32_t) hash->entry_id,  (uint32_t) file->entry_id,
      filename ? filename : "Unknown entry id",
      (uint32_t) file->data_offset, file->data_length);

  print_hash ("File hash", hash->hash);
}

static int read_header (FILE *fd, PUPHeader *header,
    PUPFileEntry **files, PUPHashEntry **hashes, PUPFooter *footer)
{

  PUPHeader orig_header;
  HMAC_CTX context;
  uint8_t hash[SHA1_MAC_LEN];
  int read;
  unsigned int i;

  *files = NULL;
  *hashes = NULL;

  read = fread (&orig_header, sizeof(PUPHeader), 1, fd);

  if (read < 1) {
    perror ("Couldn't read header");
    goto error;
  }

  header->magic = ntohll(orig_header.magic);
  header->package_version = ntohll(orig_header.package_version);
  header->image_version = ntohll(orig_header.image_version);
  header->file_count = ntohll(orig_header.file_count);
  header->header_length = ntohll(orig_header.header_length);
  header->data_length = ntohll(orig_header.data_length);

  if (header->magic != PUP_MAGIC) {
    fprintf (stderr, "Magic number is not the same 0x%X%X\n",
        (uint32_t) (header->magic >> 32), (uint32_t) header->magic);
    goto error;
  }

  *files = malloc (header->file_count * sizeof(PUPFileEntry));
  *hashes = malloc (header->file_count * sizeof(PUPHashEntry));

  read = fread (*files, sizeof(PUPFileEntry), header->file_count, fd);

  if (read < 0 || (uint) read < header->file_count) {
    perror ("Couldn't read file entries");
    goto error;
  }

  read = fread (*hashes, sizeof(PUPHashEntry), header->file_count, fd);

  if (read < 0 || (uint) read < header->file_count) {
    perror ("Couldn't read hash entries");
    goto error;
  }

  read = fread (footer, sizeof(PUPFooter), 1, fd);

  if (read < 1) {
    perror ("Couldn't read footer");
    goto error;
  }

  HMACInit (&context, hmac_pup_key, sizeof(hmac_pup_key));
  HMACUpdate (&context, &orig_header, sizeof(PUPHeader));
  HMACUpdate (&context, *files, header->file_count * sizeof(PUPFileEntry));
  HMACUpdate (&context, *hashes, header->file_count * sizeof(PUPHashEntry));
  HMACFinal (hash, &context);

  if (memcmp (hash, footer->hash, SHA1_MAC_LEN) != 0) {
    fprintf (stderr, "PUP file is corrupted, wrong header hash\n\n");
    print_hash ("Header hash", hash);
    print_hash ("Expected hash", footer->hash);
    goto error;
  }

  for (i = 0; i < header->file_count; i++) {
    (*files)[i].entry_id = ntohll ((*files)[i].entry_id);
    (*files)[i].data_offset = ntohll ((*files)[i].data_offset);
    (*files)[i].data_length = ntohll ((*files)[i].data_length);
    (*hashes)[i].entry_id = ntohll ((*hashes)[i].entry_id);
  }

  return 1;

 error:
  if (*files)
    free (*files);
  *files = NULL;
  if (*hashes)
    free (*hashes);
  *hashes = NULL;

  return 0;
}

static void info (const char *file)
{
  FILE *fd = NULL;
  int i;
  PUPHeader header;
  PUPFooter footer;
  PUPFileEntry *files = NULL;
  PUPHashEntry *hashes = NULL;

  fd = fopen (file, "rb");

  if (fd == NULL) {
    perror ("Error opening input file");
    exit (-2);
  }

  if (read_header (fd, &header, &files, &hashes, &footer) == 0)
    goto error;

  print_header_info (&header, &footer);

  for (i = 0; (uint) i < header.file_count; i++)
    print_file_info (&files[i], &hashes[i]);

  fclose (fd);
  free (files);
  free (hashes);
  return;

 error:
  fclose (fd);
  exit (-2);
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
  HMAC_CTX context;
  uint8_t hash[SHA1_MAC_LEN];
  struct stat stat_buf;

  if (stat (dest, &stat_buf) == 0) {
    fprintf (stderr, "Destination directory must not exist\n");
    goto error;
  }

  fd = fopen (file, "rb");

  if (fd == NULL) {
    perror ("Error opening input file");
    goto error;
  }

  if (read_header (fd, &header, &files, &hashes, &footer) == 0)
    goto error;

  print_header_info (&header, &footer);

  if (mkdir (dest, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
    perror ("Couldn't create output directory");
    goto error;
  }

  for (i = 0; (uint) i < header.file_count; i++) {
    const char *file = NULL;
    unsigned int len;

    print_file_info (&files[i], &hashes[i]);

    file = id_to_filename (files[i].entry_id);
    if (file == NULL) {
      printf ("*** Unknown entry id, file skipped ****\n\n");
      continue;
    }
    sprintf (filename, "%s/%s", dest, file);

    printf ("Writing file %s\n", filename);
    out = fopen (filename, "wb");
    if (out == NULL) {
      perror ("Could not open output file");
      goto error;
    }
    fseek (fd, files[i].data_offset, SEEK_SET);
    HMACInit (&context, hmac_pup_key, sizeof(hmac_pup_key));

    do {
      len = files[i].data_length;
      if (len > sizeof(buffer))
        len = sizeof(buffer);
      read = fread (buffer, 1, len, fd);

      if (read < 0 || (uint) read < len) {
        perror ("Couldn't read all the data");
        goto error;
      }

      HMACUpdate (&context, buffer, len);

      written = fwrite (buffer, 1, len, out);
      if (written < 0 || (uint) written < len) {
        perror ("Couldn't write all the data");
        goto error;
      }
      files[i].data_length -= len;
    } while (files[i].data_length > 0);

    HMACFinal (hash, &context);
    fclose (out);
    out = NULL;

    if (memcmp (hash, hashes[i].hash, SHA1_MAC_LEN) != 0) {
      fprintf (stderr, "PUP file is corrupted, wrong file hash\n\n");
      print_hash ("File hash", hash);
      print_hash ("Expected hash", hashes[i].hash);
      goto error;
    }

  }

  if (out)
    fclose (out);
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

  fprintf (stderr, "PUP Creator/Extractor %s\nBy KaKaRoTo\n\n", VERSION);

  if (argc < 2)
    usage (argv[0]);

  if (argv[1][1] != '\0')
    usage (argv[0]);

  switch (argv[1][0]) {
    case 'i':
      if (argc != 3)
        usage (argv[0]);

      info (argv[2]);

      break;
    case 'e':
    case 'x':
      if (argc != 4)
        usage (argv[0]);
      extract (argv[2], argv[3]);
      break;
    case 'c':
      if (argc != 4)
        usage (argv[0]);
      create (argv[2], argv[3]);
      break;
    default:
      usage (argv[0]);
  }

  return 0;
}
