/*
 * RSFS - Really Simple File System
 *
 * Copyright © 2010,2012,2019 Gustavo Maciel Dias Vieira
 * Copyright © 2010 Rodrigo Rocco Barbieri
 *
 * This file is part of RSFS.
 *
 * RSFS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define CLUSTERSIZE 4096
#define FATCLUSTERS 65536
#define DIRENTRIES 128

unsigned short fat[FATCLUSTERS];

typedef struct {
  char used;
  char name[25];
  unsigned short first_block;
  int size;
} dir_entry;

dir_entry dir[DIRENTRIES];

int formated = 0;

int fs_init() { // faz o "inverso" de fs_format
  // lê o disco e salva fat e dir
  for(int i = 0; i < 32; i++) {
    if(!bl_read(i, ((char *) fat) + i * CLUSTERSIZE))
      return 0;
  }
  if(!bl_read(32, (char *) dir))
    return 0;

  // verificação se está formatado
  formated = 1;
  for(int i = 0; i < FATCLUSTERS; i++) {
    if (i < 32 && fat[i] != 3) 
      formated = 0;
    else if(i == 32 && fat[i] != 4)
      formated = 0;
    else if (i > 32 && (fat[i] >= 3 && fat[i] <= 32))
      formated = 0;
  }
  
  return 1;
}

int fs_format() { 
  // escreve fat e dir
  for(int i = 0; i < FATCLUSTERS; i++) {
    if(i < 32) {
      fat[i] = 3;
    }
    else if(i == 32) {
      fat[i] = 4;
    }
    else {
      fat[i] = 1;
    }
  }
  for(int i = 0; i < DIRENTRIES; i++) {
    dir[i].used = 0;
  }

  // escreve no disco
  for(int i = 0; i < 32; i++) {
    if(!bl_write(i, ((char *) fat) + i * CLUSTERSIZE))
      return 0;
  }
  if(!bl_write(32, (char *) dir))
    return 0;

  formated = 1;

  return 1;
}

int fs_free() { 
  if (!formated) {
    printf("Sistema de Arquivos não formatado!\n");
    return 0;
  }

  int clusters_free = 0;
  for(int i = 33; i < FATCLUSTERS; i++) {
    if (fat[i] == 1) 
      clusters_free++;
  }
  
  return clusters_free * CLUSTERSIZE;
}

int fs_list(char *buffer, int size) { 
  if (!formated) {
    printf("Sistema de Arquivos não formatado!\n");
    return 0;
  }

  return 1;
}

int fs_create(char* file_name) { 
  if (!formated) {
    printf("Sistema de Arquivos não formatado!\n");
    return 0;
  }

  // nome não pode ser maior ou igual a 25
  if (strlen(file_name) >= 25) {
    printf("Nome de arquivo maior que 25 caracteres.\n");
    return 0;
  }

  int first_dir = -1;
  for(int i = 0; i < DIRENTRIES; i++){
    if(dir[i].used == 0 && first_dir == -1) {
      first_dir = i;
    }
    // erro se o nome já existe
    if(dir[i].used && strcmp(file_name, dir[i].name) == 0){
      printf("Nome de arquivo duplicado.\n");
      return 0;
    }

  }
  // erro se não dá para ter mais arquivos
  if(first_dir == -1){
    printf("Limite de arquivos alcançado.\n");
    return 0;
  }

  // busca primeiro bloco livre na fat
  int first_fat = -1;
  for(int i = 33; i < FATCLUSTERS; i++){
    if(fat[i] == 1){
      first_fat = i;
      break;
    }
  }
  if(first_fat == - 1){
    printf("Nenhum bloco livre.\n");
    return 0;
  }

  // preenche fat e dir
  fat[first_fat] = 2;

  strcpy(dir[first_dir].name, file_name);
  dir[first_dir].used = 1;
  dir[first_dir].first_block = first_fat;
  dir[first_dir].size = 0;

  // escreve no disco
  for(int i = 0; i < 32; i++) {
    if(!bl_write(i, ((char *) fat) + i * CLUSTERSIZE))
      return 0;
  }
  
  if(!bl_write(32, (char *) dir))
    return 0;

  return 1;
}

int fs_remove(char *file_name) { 
  if (!formated) {
    printf("Sistema de Arquivos não formatado!\n");
    return 0;
  }

  //erro se arquivo não existe
  int pos_dir = -1;
  for(int i = 0; i < DIRENTRIES; i++){
    if(dir[i].used && strcmp(file_name, dir[i].name) == 0){
      pos_dir = i;
      break;
    }
  }

  if(pos_dir == -1){
    printf("Arquivo não existe.\n");
    return 0;
  }

  // atualiza dir e fat
  dir[pos_dir].used = 0;

  int cur = dir[pos_dir].first_block;
  while(fat[cur] != 2) {
      int next = fat[cur];
      fat[cur] = 1;
      cur = next;
  }
  fat[cur] = 1;

  // escreve no disco
  for(int i = 0; i < 32; i++) {
    if(!bl_write(i, ((char *) fat) + i * CLUSTERSIZE))
      return 0;
  }
  
  if(!bl_write(32, (char *) dir))
    return 0;

  return 1;
}

int fs_open(char *file_name, int mode) {
  printf("Função não implementada: fs_open\n");
  return -1;
}

int fs_close(int file)  {
  printf("Função não implementada: fs_close\n");
  return 0;
}

int fs_write(char *buffer, int size, int file) {
  printf("Função não implementada: fs_write\n");
  return -1;
}

int fs_read(char *buffer, int size, int file) {
  printf("Função não implementada: fs_read\n");
  return -1;
}

