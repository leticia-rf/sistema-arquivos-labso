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
int total_clusters;

typedef struct {
  char used;
  char name[25];
  unsigned short first_block;
  int size;
} dir_entry;

typedef struct {
  int cur_block;
  int offset;
  int mode;
  char open;
  char buffer[CLUSTERSIZE];
} dir_info;

dir_entry dir[DIRENTRIES];
dir_info info[DIRENTRIES];

int formated = 0;

int fs_init() { // faz o "inverso" de fs_format
  total_clusters = bl_size();

  // lê o disco e salva fat e dir
  for(int i = 0; i < 32; i++) {
    if(!bl_read(i, ((char *) fat) + i * CLUSTERSIZE))
      return 0;
  }
  if(!bl_read(32, (char *) dir))
    return 0;

  // verificação se está formatado
  formated = 1;
  for(int i = 0; i < total_clusters; i++) {
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
  for(int i = 0; i < total_clusters; i++) {
    if(i < 32) fat[i] = 3;
    else if(i == 32) fat[i] = 4;
    else fat[i] = 1;
  }
  
  for(int i = 0; i < DIRENTRIES; i++) {
    dir[i].used = 0;
    info[i].open = 0;
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
  for(int i = 33; i < total_clusters; i++) {
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

  int offset = 0, wrt;

  for(int i = 0; i < DIRENTRIES; ++i){
    if(!dir[i].used) continue;

    wrt = snprintf(buffer + offset, size - offset, "%s\t\t%d\n", dir[i].name, dir[i].size);
    
    if (wrt < 0 || wrt >= size - offset) break;    
    offset += wrt;
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
  // erro se não tem espaco para mais arquivos
  if(first_dir == -1){
    printf("Limite de arquivos alcançado.\n");
    return 0;
  }

  // busca primeiro bloco livre na fat
  int first_fat = -1;
  for(int i = 33; i < total_clusters; i++){
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

  info[first_dir].cur_block = first_fat;

  // escreve no disco
  for(int i = 0; i < 32; i++) {
    if(!bl_write(i, ((char *) fat) + i * CLUSTERSIZE))
      return 0;
  }
  
  if(!bl_write(32, (char *) dir))
    return 0;

  return 1;
}

int fs_create_pos(char* file_name) { 
  if (!formated) {
    printf("Sistema de Arquivos não formatado!\n");
    return -1;
  }

  // nome não pode ser maior ou igual a 25
  if (strlen(file_name) >= 25) {
    printf("Nome de arquivo maior que 25 caracteres.\n");
    return -1;
  }

  int first_dir = -1;
  for(int i = 0; i < DIRENTRIES; i++){
    if(dir[i].used == 0 && first_dir == -1) {
      first_dir = i;
    }

    if(dir[i].used && strcmp(file_name, dir[i].name) == 0){
      printf("Nome de arquivo duplicado.\n");
      return -1;
    }
  }

  // erro se não tem espaco para mais arquivos
  if(first_dir == -1){
    printf("Limite de arquivos alcançado.\n");
    return -1;
  }

  // busca primeiro bloco livre na fat
  int first_fat = -1;

  for(int i = 33; i < total_clusters; i++){
    if(fat[i] == 1){
      first_fat = i;
      break;
    }
  }
  if(first_fat == - 1){
    printf("Nenhum bloco livre.\n");
    return -1;
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
      return -1;
  }
  
  if(!bl_write(32, (char *) dir))
    return -1;

  return first_dir;
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

  // atualiza fat e dir
  int cur = dir[pos_dir].first_block;

  while(fat[cur] != 2) {
      int next = fat[cur];
      fat[cur] = 1;
      cur = next;
  }
  fat[cur] = 1;

  dir[pos_dir].used = 0;

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
  if (!formated) {
    printf("Sistema de Arquivos não formatado!\n");
    return -1;
  }

  int pos_dir = -1;

  for(int i = 0; i < DIRENTRIES; i++){
    if(dir[i].used && strcmp(file_name, dir[i].name) == 0){
      pos_dir = i;
      break;
    }
  }
  
  if (pos_dir == -1 && mode == FS_R){
    printf("Arquivo não existe.\n");
    return -1;
  }

  // se arquivo aberto para escrita já existe, remove e cria de novo
  if (mode == FS_W) {
    if(pos_dir != -1) fs_remove(file_name);
    pos_dir = fs_create_pos(file_name);
    if(pos_dir == -1) return -1;
  }
  
  info[pos_dir].mode = mode;
  info[pos_dir].open = 1;
  info[pos_dir].cur_block = dir[pos_dir].first_block;
  info[pos_dir].offset = 0;

  return pos_dir;
}

int fs_close(int file)  {
  if (!formated) {
    printf("Sistema de Arquivos não formatado!\n");
    return 0;
  }  

  if(file < 0 || file >= DIRENTRIES){
    printf("Identificador inválido!\n");
    return 0;
  }
  
  if(!dir[file].used || !info[file].open){
    printf("Não existe arquivo aberto com esse identificador!\n");
    return 0;
  }

  info[file].open = 0;

  // escreve no disco
  for(int i = 0; i < 32; i++) {
    if(!bl_write(i, ((char *) fat) + i * CLUSTERSIZE))
      return 0;
  }
  if(!bl_write(32, (char *) dir))
    return 0;

  return 1;
}

int fs_write(char *buffer, int size, int file) {
  if (!formated) {
    printf("Sistema de Arquivos não formatado!\n");
    return -1;
  }
  
  if(size < 0){
    printf("Tamanho inválido!\n");
    return -1;
  }

  if(file < 0 || file >= DIRENTRIES){
    printf("Identificador inválido!\n");
    return -1;
  }
  
  if(!dir[file].used || !info[file].open || info[file].mode == FS_R){
    printf("Arquivo não aberto para escrita!\n");
    return -1;
  }

  
  int bytes_lidos = 0;

  // percorre os blocos da fat
  if (info[file].offset == CLUSTERSIZE) {
    info[file].offset = 0;
  }
  int prev = info[file].cur_block;
  int cur;
  
  //Marcando o primeiro bloco como livre para funcionar na primeira interação do loop
  fat[prev] = 1;
  while(bytes_lidos < size){
    for(int i = prev ; i <= total_clusters; i++){
      if(i == total_clusters){
        printf("Armazenamento cheio\n");
        return -1;
      }
      if (fat[i] == 1) {
        cur = i;
        break;
      }
    }

    int disponiveis = CLUSTERSIZE - info[file].offset;
    int faltam = size - bytes_lidos;
    int copiar = faltam < disponiveis ? faltam : disponiveis;

    memcpy(info[file].buffer + info[file].offset, buffer + bytes_lidos, copiar);
    if(!bl_write(cur, info[file].buffer))
      return -1;

    fat[prev] = cur;
    fat[cur] = 2;
    prev = cur;

    bytes_lidos += copiar;
    info[file].offset += copiar;
    if (info[file].offset == CLUSTERSIZE) {
      info[file].offset = 0;
      info[file].cur_block = cur;
    }
  }

  dir[file].size += bytes_lidos;

  return bytes_lidos;
}

int fs_read(char *buffer, int size, int file) {
  if (!formated) {
    printf("Sistema de Arquivos não formatado!\n");
    return 0;
  }  

  if(size < 0){
    printf("Tamanho inválido!\n");
    return -1;
  }

  if(file < 0 || file >= DIRENTRIES){
    printf("Identificador inválido!\n");
    return -1;
  }
  
  if(!dir[file].used || !info[file].open || info[file].mode == FS_W){
    printf("Arquivo não aberto para leitura!\n");
    return -1;
  }

  if(info[file].cur_block == -1){
    printf("Arquivo terminou!\n");
    return 0;
  }

  int bytes_lidos = 0;
  int cur = info[file].cur_block;

  // enquanto nao estiver no ultimo bloco e ainda tem bytes para ler, faz o encadeamento na fat
  while (bytes_lidos < size) {
    int total_bloco = CLUSTERSIZE;
    if((fat[cur] == 2  && dir[file].size % CLUSTERSIZE != 0) || dir[file].size == 0){
      total_bloco = dir[file].size % CLUSTERSIZE ;
    }
      
    int disponiveis = total_bloco - info[file].offset;
    int faltam = size - bytes_lidos;
    int copiar = faltam < disponiveis ? faltam : disponiveis; 

    bl_read(cur, info[file].buffer);
    memcpy(buffer + bytes_lidos, info[file].buffer + info[file].offset, copiar);

    bytes_lidos += copiar;
    info[file].offset += copiar;

    // se chega no final do bloco, atualiza o offset o cur_block
    if (info[file].offset == total_bloco) {
      info[file].offset = 0;
      cur = fat[cur];
      info[file].cur_block = cur;
      //Cheguei no último último bloco, então atualizo o cur_block para -1 (terminei de ler o arquivo)
      if(cur == 2){
        info[file].cur_block = -1;
        //Caso o buffer seja maior que o arquivo, terminamos o loop
        break;
      }
    }

  }

  return bytes_lidos;
}