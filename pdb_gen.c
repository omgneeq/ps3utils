/*
 * pdb_gen.h -- PS3 .pdb file format generator
 *
 * Copyright (C) Youness Alaoui (KaKaRoTo)
 *
 * This software is distributed under the terms of the GNU General Public
 * License ("GPL") version 3, as published by the Free Software Foundation.
 *
 */


#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>


#define PDB_HEADER		0x00000000

#define UNKNOWN_HEADER1		0x00000064
#define UNKNOWN_HEADER2		0x00000065
#define UNKNOWN_HEADER3		0x00000066
#define UNKNOWN_HEADER4		0x0000006B
#define UNKNOWN_HEADER5		0x00000068
#define UNKNOWN_HEADER6		0x0000006C

#define HEADER5_MAGIC_VALUE	0x80023E13


#define CURRENT_LENGTH		0x000000D0
#define TOTAL_LENGTH 		0x000000CE
#define PKG_DATE		0x000000CC
#define IMAGE_PATH		0x0000006A
#define TITLE			0x00000069
#define DOWNLOAD_URL		0x000000CA
#define FILENAME		0x000000CB
#define CONTENT_ID		0x000000D9
#define UNKNOWN1		0x000000DA
#define UNKNOWN2		0x000000CD
#define LOG_URL			0x000000EB
#define UNKNOWN3		0x000000EC


typedef struct {
  uint32_t magic;
  uint32_t type;
  uint32_t pkg_info_offset;
  uint32_t unknown;
  uint32_t header_size;
  uint32_t item_count;
  uint64_t pkg_size;
  uint64_t data_offset;
  uint64_t data_size;
  char content_id[30];
  uint8_t QA_digest[10];
  uint8_t unknown_digest[10];
} PkgHeader;


static int am_big_endian(void)
{
  long one= 1;
  return !(*((char *)(&one)));
}


static uint32_t cpu_to_be32 (uint32_t cpu)
{
  unsigned int i;
  uint32_t result;

  if (am_big_endian ())
    return cpu;

  for (i = 0; i < sizeof(uint32_t); i++)
    ((char *)&result)[i] = ((char *)&cpu)[sizeof(uint32_t) - i - 1];

  return result;
}

static void
write_kllv (FILE *f, uint32_t key, uint32_t len, uint8_t *value)
{
  uint32_t key_be = cpu_to_be32 (key);
  uint32_t len_be = cpu_to_be32 (len);

  fwrite (&key_be, sizeof(uint32_t), 1, f);
  fwrite (&len_be, sizeof(uint32_t), 1, f);
  fwrite (&len_be, sizeof(uint32_t), 1, f);
  fwrite (value, 1, len, f);
}


static void
write_pdb (FILE *f, char *out_path, char *pkg_path, char *title,
    PkgHeader *pkg_header, uint32_t header5) {
  uint32_t pdb_header = cpu_to_be32 (PDB_HEADER);
  uint32_t header1 = cpu_to_be32 (0);
  uint32_t header2 = cpu_to_be32 (0);
  uint8_t header3 = 0;
  uint32_t header4 = cpu_to_be32 (3);
  uint32_t header6 = cpu_to_be32 (0);
  uint64_t pkg_size = pkg_header->pkg_size; /* already in BE */
  char pkg_date[] = "Thu, 02 Sep 2010 17:28:10 GMT";
  char icon_file[1024];
  char content_title[1024];
  char download_url[1024];
  uint8_t unknown1 = 1;
  uint8_t unknown2 = 0;
  char log_url[] = "http://google.com";
  uint8_t unknown3 = 0;
  int i;

  fwrite (&pdb_header, sizeof(pdb_header), 1, f);

  i = 0;
  memset (download_url, 0, sizeof(download_url));
  strcpy (download_url, "http://zeus.dl.playstation.net/cdn/");
  while (pkg_header->content_id[i] != '-')
    download_url[strlen (download_url)] = pkg_header->content_id[i++];
  download_url[strlen (download_url)] = '/';
  i++;
  while (pkg_header->content_id[i] != '-')
    download_url[strlen (download_url)] = pkg_header->content_id[i++];
  download_url[strlen (download_url)] = '/';
  strcat (download_url, pkg_path);
  strcat (download_url, "?product=0084&country=us");
  i++;

  if (title == NULL)
    strcpy (content_title, pkg_header->content_id + i);
  else
    strcpy (content_title, title);

  sprintf (icon_file, "/dev_hdd0/vsh/task/%s/ICON_FILE", out_path);

  write_kllv (f, UNKNOWN_HEADER1, sizeof(uint32_t), (uint8_t *) &header1);
  write_kllv (f, UNKNOWN_HEADER2, sizeof(uint32_t), (uint8_t *) &header2);
  write_kllv (f, UNKNOWN_HEADER3, sizeof(uint8_t), &header3);
  write_kllv (f, UNKNOWN_HEADER4, sizeof(uint32_t), (uint8_t *) &header4);
  write_kllv (f, UNKNOWN_HEADER5, sizeof(uint32_t), (uint8_t *) &header5);
  write_kllv (f, UNKNOWN_HEADER6, sizeof(uint32_t), (uint8_t *) &header6);

  write_kllv (f, CURRENT_LENGTH, sizeof(uint64_t), (uint8_t *) &pkg_size);
  write_kllv (f, TOTAL_LENGTH, sizeof(uint64_t), (uint8_t *) &pkg_size);
  write_kllv (f, PKG_DATE, strlen (pkg_date) + 1, (uint8_t *) pkg_date);
  write_kllv (f, IMAGE_PATH, strlen (icon_file) + 1, (uint8_t *) icon_file);
  write_kllv (f, TITLE, strlen (content_title) + 1, (uint8_t *) content_title);
  write_kllv (f, DOWNLOAD_URL, strlen (download_url) + 1, (uint8_t *) download_url);
  write_kllv (f, FILENAME, strlen (pkg_path) + 1, (uint8_t *) pkg_path);
  write_kllv (f, CONTENT_ID, strlen (pkg_header->content_id) + 1,
      (uint8_t *) pkg_header->content_id);
  write_kllv (f, UNKNOWN1, sizeof(uint8_t), (uint8_t *) &unknown1);
  write_kllv (f, UNKNOWN2, sizeof(uint8_t), (uint8_t *) &unknown2);
  write_kllv (f, LOG_URL, strlen (log_url) + 1, (uint8_t *) log_url);
  write_kllv (f, UNKNOWN3, sizeof(uint8_t), (uint8_t *) &unknown3);
}

int main (int argc, char *argv[])
{
  FILE *pkg = NULL;
  FILE *out1 = NULL;
  FILE *out2 = NULL;
  PkgHeader pkg_header;
  char path[9];
  int ret = -1;
  int i;

  if (argc < 2 || argc > 3) {
    printf ("Usage: %s file.pkg ?title?", argv[0]);
    return -1;
  }


  pkg = fopen (argv[1], "r");
  if (pkg == NULL) {
    perror ("Failed to open .pkg file : ");
    return -2;
  }
  if (fread (&pkg_header, sizeof(pkg_header), 1, pkg) < 1) {
    perror ("Couldn't read pkg data : ");
    return -3;
  }
  fclose (pkg);


  for (i = 2; i < 20 && ret == -1; i++) {
    sprintf (path, "%.8X", i);
    ret = mkdir (path, 0777);
    if (ret != 0 && errno != EEXIST) {
      perror ("Error creating directory : ");
      return -4;
    }
  }

  if (ret == -1) {
    perror ("Couldn't created directory in 20 attemps : ");
    return -5;
  }

  if (chdir (path) != 0) {
    perror ("Couldn't change directory : ");
    return -6;
  }

  out1 = fopen ("f0.pdb", "w");
  if (out1 == NULL) {
    perror ("Couldn't create f0.pdb : ");
    return -7;
  }
  fclose (out1);

  out1 = fopen ("d0.pdb", "w");
  if (out1 == NULL) {
    perror ("Couldn't create d0.pdb : ");
    return -8;
  }
  out2 = fopen ("d1.pdb", "w");
  if (out2 == NULL) {
    perror ("Couldn't create d1.pdb : ");
    fclose (out1);
    return -9;
  }

  write_pdb (out1, path, argv[1], argc >= 3 ? argv[2]: NULL,
      &pkg_header, cpu_to_be32 (HEADER5_MAGIC_VALUE));
  write_pdb (out2, path, argv[1], argc >= 3 ? argv[2]: NULL,
      &pkg_header, cpu_to_be32 (0));

  fclose (out1);
  fclose (out2);

  printf ("Created pkg directory %s\n", path);

  return 0;
}
