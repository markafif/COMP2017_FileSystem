#ifndef MYFILESYSTEM_H
#define MYFILESYSTEM_H
#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>

#include "sortedlist.h"

typedef struct file_system file_system;
typedef struct record record;

/* this struct is used to make it easier to
read in records from the directory table */
struct record {
	char name[64];
	uint32_t offset;
	uint32_t length;
};

// this struct acts as my helper function
struct file_system {
  int filedata;
  int dirtable; 
  int hashmap; // file_data, directory_table, hash_data
  char *fdata;
  char *dir;
  char *hash;
  // file *files;
  uint8_t *hash_data;
  sorted_list *list;
  size_t max_records;
  size_t max_data;
  size_t data_used;
  size_t num_blocks;
  size_t hash_size;
  int n_processors;
};

void * init_fs(char * f1, char * f2, char * f3, int n_processors);

void close_fs(void * helper);

int create_file(char * filename, size_t length, void * helper);

int resize_file(char * filename, size_t length, void * helper);

void repack(void * helper);

int delete_file(char * filename, void * helper);

int rename_file(char * oldname, char * newname, void * helper);

int read_file(char * filename, size_t offset, size_t count, void * buf, void * helper);

int write_file(char * filename, size_t offset, size_t count, void * buf, void * helper);

ssize_t file_size(char * filename, void * helper);

void fletcher(uint8_t * buf, size_t length, uint8_t * output);

void compute_hash_tree(void * helper);

void compute_hash_block(size_t block_offset, void * helper);


#endif
