#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>

#include "myfilesystem.h"
#define LARGE (4294967295)
#define HASH_BLOCK (16)
#define DATA_BLOCK (256)
#include "myfilesystem.h"

pthread_mutex_t mylock = PTHREAD_MUTEX_INITIALIZER;

// used as a helper function for resize_file
static int resize_file_helper(char * filename, size_t length, void * helper);
// used as a helper function for repack_helper
static void repack_helper(void * helper);
// used as a helper function for write_file
static int write_file_helper(char * filename, size_t offset, size_t count, void * buf, void * helper);
// used as a helper function for compute hash block
static void compute_hash_block_helper(size_t block_offset, void * helper);
// used as a helper function for compute hash tree
static void compute_hash_tree_helper(void * helper);
// used to update hash subtree, from start_offset to end_offset
static void update_hash(size_t start_offset, size_t end_offset, void * helper);
// verifies that hash file is correct (for read_file)
static int hash_verify(void *helper, size_t start_offset, size_t end_offset);

void * init_fs(char * f1, char * f2, char * f3, int n_processors) {
	if (f1==NULL || f2 == NULL || f3 == NULL) {
		return NULL;
	}
    file_system *sys = malloc(sizeof(file_system)); // helper struct of type file_system
    if (sys!=NULL) {
		// initialize sorted linked list to assist with file tracking
		sys->list = list_init();
        sys->list->head = NULL;
        sys->n_processors = n_processors;
		// open files f1, f2, f3
		sys->filedata = open(f1,O_RDWR);
		sys->dirtable = open(f2,O_RDWR);
		sys->hashmap = open(f3,O_RDWR);
		
		// get hash data size and map to disk using mmap
		size_t sz = lseek(sys->hashmap,0,SEEK_END);
		lseek(sys->hashmap,0,SEEK_SET);
		sys->hash_size = sz/HASH_BLOCK;
        sys->hash = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_SHARED, sys->hashmap, 0);
		// get file size and map to disk using mmap
		sz = lseek(sys->filedata,0,SEEK_END);
		lseek(sys->filedata,0,SEEK_SET);
		sys->max_data = sz;
		sys->num_blocks = sz/DATA_BLOCK;
		sys->data_used = 0;
		sys->fdata = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_SHARED, sys->filedata, 0);
		// get directory table size and map to disk using mmap
	    sz = lseek(sys->dirtable,0L,SEEK_END);
	    sys->max_records = sz/72;
		lseek(sys->dirtable,0,SEEK_SET);
		sys->dir = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_SHARED, sys->dirtable, 0);
		// check if mapping failed
		if (sys->dir == MAP_FAILED || sys->fdata == MAP_FAILED || sys->hash == MAP_FAILED) {
			return NULL;
		}
		/* go through current contents of directory table and add 
		preexisting elements to my linked list structure  which 
		stores: filename, offset, length and index in directory table*/
		uint32_t off = 0;
		uint32_t len = 0;
		size_t index;
		for (size_t i = 0; i < sys->max_records; i++) {
			index = 72*i;
			if (*(sys->dir+index)!=0) {
				char *name = malloc(64);
				memcpy((name),(sys->dir+index),64);
				index += 64;
				memcpy(&off,(sys->dir+index),4);
				index +=4;
				memcpy(&len,(sys->dir+index),4);
				sys->data_used = len+(sys->data_used);
				node *t = node_add(sys->list,name,off,len,(uint32_t)i);
				t->name = name;
			}
		}
        return sys;
    }
    return NULL;
}

void close_fs(void * helper) {
	if (helper == NULL) {
		return;
	}
 	file_system* sys = (file_system*) helper;
	// close files
	close(sys->filedata);
	close(sys->dirtable);
	close(sys->hashmap);
	// unmap memory regions
	msync(sys->fdata,sys->max_data,MS_ASYNC);
	munmap(sys->fdata, sys->max_data);
	msync(sys->dir,(sys->max_records*72),MS_ASYNC);
	munmap(sys->dir, (sys->max_records*72));
	msync(sys->hash, (sys->hash_size*HASH_BLOCK),MS_ASYNC);
	munmap(sys->hash, (sys->hash_size*HASH_BLOCK));
	// free linked list
 	list_free(sys->list);
	// free helper
	free(sys);
	pthread_mutex_destroy(&mylock);
	return;
}

int create_file(char * filename, size_t length, void * helper) {
	pthread_mutex_lock(&mylock); // synchronization
	if (filename == NULL || helper == NULL || length < 0) {
		pthread_mutex_unlock(&mylock);
		return 1; // parameter checking
	}
	file_system* sys = (file_system*) helper;
	if (((sys->data_used)+length) > (sys->max_data)) { // check if file_data has space
		pthread_mutex_unlock(&mylock);
		return 2;
	}
	// truncate string to 64 bytes
	char *fname = malloc(64);
	strncpy(fname,filename,64);
	fname[63] = '\0';
	// check if linked list is initialized
	if (sys->list == NULL) {
		pthread_mutex_unlock(&mylock);
		return 1;
	}
	// check if filename already exists
	node *t = get_node(sys->list,fname);
	if (t!=NULL) {
		free(fname);
		pthread_mutex_unlock(&mylock);
   		return 1; // filename already exists
	}
	// determine first available offset
	size_t offset = free_pos(sys->list,length);
	// check if we need to try to repack
	if ((offset + length) > sys->max_data) {
		repack_helper(helper);
		offset = free_pos(sys->list,length);
		// check if repack helped
		if ((offset + length) > sys->max_data) {
			free(fname);
			pthread_mutex_unlock(&mylock);
			return 2; // no space at all
		}
	}
 	// create buffer of 0 bytes to write to file data
 	node *n = node_add(sys->list,fname,offset,length,0);
	n->name = fname;
 	uint8_t buff[length];
 	for (size_t i = 0; i<length;i++) {
 	  	buff[i] = 0;
 	}
	// these offsets indicate which hash blocks need to be updated
	size_t start_offset = offset/256;
	size_t end_offset = (offset+length)/256;
	size_t index;
	// find first available slot in directory table to store new file info 
	for (size_t i = 0; i < sys->max_records; i++) {
		index = 72*i;
		if (*(sys->dir+index)==0) {
			n->index = i;
			memcpy((sys->dir+index),fname,64);
			index += 64;
			memcpy((sys->dir+index),&offset,4);
			index +=4;
			memcpy((sys->dir+index),&length,4);
			sys->data_used+=length;
			msync(sys->dir,(sys->max_records*72),MS_ASYNC);
			// write buff to filedata
			memcpy((sys->fdata+offset),buff,length);
			msync(sys->fdata,sys->max_data,MS_ASYNC);
			// update hash data
			update_hash(start_offset,end_offset,helper);	
			msync(sys->hash, (sys->hash_size*HASH_BLOCK),MS_ASYNC);
			pthread_mutex_unlock(&mylock);
			return 0;
		}
	}
	// no directory table spaces available if we reach here;
	node_delete(sys->list,n);
	pthread_mutex_unlock(&mylock);
    return 2; 
}

int resize_file(char * filename, size_t length, void * helper) {
	pthread_mutex_lock(&mylock); // synchronization
	if (filename == NULL || length < 0 || helper == NULL) {
		pthread_mutex_unlock(&mylock);
		return 1; // parameter checking
	}
	int res = resize_file_helper(filename,length,helper);
	pthread_mutex_unlock(&mylock);
	return res;
}
// resize_file helper
static int resize_file_helper(char * filename, size_t length, void * helper) {
	file_system* sys = (file_system*) helper;
	size_t start_offset;
	size_t end_offset;
	int diff;
	if (sys->list == NULL) {
		return 1;
	}
	// check if file exists
	node *t = get_node(sys->list,filename);
	if (t==NULL) {
		return 1; // file does not exist
	}

	if (((sys->data_used)-(t->length)+length) > sys->max_data) {
		return 2; // no space in file_data
	} 
	size_t index;
	if (t->length > length) { // new size is smaller
		diff = t->length - length;
		t->length = length;
		sys->data_used = (sys->data_used - diff);
		index = ((72*t->index)+68);
		memcpy((sys->dir+index),&length,4);
		msync(sys->dir,(sys->max_records*72),MS_ASYNC);
		return 0;
	} 
    if (t->length < length) { // new size is larger
		if (t->next != NULL) {
			if ((t->next->offset) >= (t->offset + length)) {
				diff = length - t->length;
				char buff[diff];
				for (size_t i = 0; i<diff; i++) {
					buff[i] = 0;
				}
				index = t->offset + t->length;
				start_offset = (t->offset + t->length)/256;
				end_offset = (t->offset + t->length + diff)/256;
				memcpy(sys->fdata+index,buff,diff);
				t->length = length;
				sys->data_used = (sys->data_used + diff);
				index = (72*t->index)+68;
				memcpy((sys->dir+index),&length,4);
				update_hash(start_offset,end_offset,helper);
				msync(sys->hash, (sys->hash_size*HASH_BLOCK),MS_ASYNC);
				msync(sys->dir,(sys->max_records*72),MS_ASYNC);
				msync(sys->fdata,sys->max_data,MS_ASYNC);
				return 0;
			}
		}
		if (t->next == NULL) {
			if (sys->max_data >= (t->offset + length)) {
				diff = length - t->length;
				char buff[diff];
				for (size_t i = 0; i<diff; i++) {
					buff[i] = 0;
				}
				index = (t->offset + t->length);
				start_offset = (t->offset+t->length)/256;
				end_offset = (t->offset+t->length+diff)/256;
				memcpy(sys->fdata+index,buff,diff);
				t->length = length;
				sys->data_used = (sys->data_used + diff);
				index = (72*t->index)+68;
				memcpy((sys->dir+index),&length,4);
				
				update_hash(start_offset,end_offset,helper);
				
				msync(sys->hash, (sys->hash_size*HASH_BLOCK),MS_ASYNC);
				msync(sys->dir,(sys->max_records*72),MS_ASYNC);
				msync(sys->fdata,sys->max_data,MS_ASYNC);
				return 0;
			}
		}
		/* now we are at the point where we have to repack!
		 delete the node from list, repack, and add the node back 
		 in with new offset, length (because repack is based on list) */
		uint32_t new_index = t->index;
		size_t len = t->length;
		char data[len];
		char *fname = malloc(64);
		strncpy(fname,filename,64);
		fname[63] = '\0';
		memcpy(data,(sys->fdata+t->offset),len);
		node_delete(sys->list,t); // delete entry from node list
		repack_helper((void *)sys); // now repacked, add to end
		node *p = sys->list->head;
		while(p->next!=NULL) {
			p = node_next(p); // loop through to end
		}
		size_t offset = p->offset;
		size_t size = p->length;
		size_t new_offset = offset+size;
		start_offset = new_offset/256;
		end_offset = (new_offset+length)/256;
		node_add(sys->list,fname,new_offset,length,new_index);
		diff = (int)length - (int)len;
		char *buff = calloc(1,(int)diff);
		memcpy((sys->fdata+new_offset),data,len);
		memcpy((sys->fdata+new_offset+len),buff,diff);
		free(buff);
		index = ((72*new_index)+64);
		memcpy((sys->dir+index),&(new_offset),4);
		memcpy((sys->dir+index+4),&(length),4);
		sys->data_used = (sys->data_used + diff);
		
		update_hash(start_offset,end_offset,helper);
					
		msync(sys->hash,(sys->hash_size*HASH_BLOCK),MS_ASYNC);
		msync(sys->dir,(sys->max_records*72),MS_ASYNC);
		msync(sys->fdata,sys->max_data,MS_ASYNC);
	}
   	return 0;
}

// calls repack helper
void repack(void * helper) {
	pthread_mutex_lock(&mylock); // synchronization
	if (helper == NULL) {
		pthread_mutex_unlock(&mylock);
		return;
	}
	repack_helper(helper);
	pthread_mutex_unlock(&mylock);
	return;
}

static void repack_helper(void * helper) {
	file_system* sys = (file_system*) helper;
	uint32_t offset = 0;
	node *t = sys->list->head;
	size_t index;
	/* repack moves file to the minimum offset position using the 
	sorted linked list to ensure files are moved in order of least 
	offset to greatest offset so as to not incorrectly modify file data*/
	while (t!=NULL) {
		if (t==sys->list->head) {
			// change file_data to match new offset
			char buff[t->length];
			memcpy(buff,(sys->fdata + t->offset),t->length);
			t->offset = 0;
			offset = t->offset;
			memcpy((sys->fdata+t->offset),buff,t->length);
			// change offset in directory table
			index = (72*(t->index))+64;
			memcpy((sys->dir+index),&offset,4);
		}
		if (t->next != NULL) {
			char buff[t->next->length];
			memcpy(buff,(sys->fdata + t->next->offset),t->next->length);
			t->next->offset = (t->offset) + (t->length);
			offset = t->next->offset;
			memcpy((sys->fdata + t->next->offset),buff,t->next->length);
			// change offset in directory table
			index = 72*(t->next->index)+64;
			memcpy((sys->dir+index),&offset,4);
		}
		t = node_next(t);
	}
	compute_hash_tree_helper(helper); // update hash
	// flush changes
	msync(sys->hash, (sys->hash_size*HASH_BLOCK),MS_ASYNC);
	msync(sys->dir,(sys->max_records*72),MS_ASYNC);
	msync(sys->fdata,sys->max_data,MS_ASYNC);
	return;
}

int delete_file(char * filename, void * helper) {
	pthread_mutex_lock(&mylock);// synchronization
	if (filename == NULL || helper == NULL) {
		pthread_mutex_unlock(&mylock);
		return 1;
	}
	file_system* sys = (file_system*) helper;
	if (sys->list == NULL) {
		pthread_mutex_unlock(&mylock);
		return 1;
	}
	
	node *t = get_node(sys->list,filename);
	if (t==NULL) {
		pthread_mutex_unlock(&mylock);
		return 1; // file does not exist
	}

	char buff[72];
	for (size_t i = 0; i<72; i++) {
		buff[i] = 0;
	}
	// wipe entry from directory table
	memcpy((sys->dir+(72*t->index)),buff,72);
	node_delete(sys->list,t); // remove file from linked list
	msync(sys->dir,(sys->max_records*72),MS_ASYNC);
	msync(sys->fdata,sys->max_data,MS_ASYNC);
	pthread_mutex_unlock(&mylock);
    return 0;
}

int rename_file(char * oldname, char * newname, void * helper) {
	pthread_mutex_lock(&mylock);
	if (oldname == NULL || newname == NULL || helper == NULL) {
		pthread_mutex_unlock(&mylock);
		return 1;
	}
	file_system* sys = (file_system*) helper;
	if (sys->list == NULL) {
		pthread_mutex_unlock(&mylock);
		return 1;
	}
	
	node *t = get_node(sys->list,newname);
	if (t!=NULL) {
		pthread_mutex_unlock(&mylock);
		return 1; // filename already in use
	}
	t = get_node(sys->list,oldname);
	if (t==NULL) {
		pthread_mutex_unlock(&mylock);
		return 1; // file does not exist
	}
	// truncate filename
	char *fname = malloc(64);
	strncpy(fname,newname,64);
	fname[63] = '\0';

	free(t->name); // free oldname as it is on the heap
	t->name = fname;
	memcpy((sys->dir+72*(t->index)),fname,64);
	msync(sys->dir,(sys->max_records*72),MS_ASYNC);
	pthread_mutex_unlock(&mylock);
    return 0;
}

int read_file(char * filename, size_t offset, size_t count, void * buf, void * helper) {
	pthread_mutex_lock(&mylock);
	if (filename == NULL || buf == NULL || helper == NULL) {
		pthread_mutex_unlock(&mylock);
		return 1;
	}
	file_system* sys = (file_system*) helper;
	if (sys->list == NULL) {
		pthread_mutex_unlock(&mylock);
		return 1;
	}	

	node *t = get_node(sys->list, filename);
	if (t == NULL) {
		pthread_mutex_unlock(&mylock);
		return 1; // file does not exist
	}
	// trying to read outside (out of bounds) of the file size
	if ((offset+count) > t->length) {
		pthread_mutex_unlock(&mylock);
		return 2;
	}
	
	size_t start_offset = (t->offset+offset)/256;
	size_t end_offset = (t->offset+offset+count)/256;
	// verify contents of hash data 
	if (hash_verify(helper,start_offset,end_offset)!=0) { 
		pthread_mutex_unlock(&mylock);
		return 3; // verification has failed
	}
	memcpy(buf,(sys->fdata+offset+t->offset),count);
	pthread_mutex_unlock(&mylock);
	return 0;
}
// calls write_file_helper
int write_file(char * filename, size_t offset, size_t count, void * buf, void * helper) {
	pthread_mutex_lock(&mylock);
	if (filename == NULL || buf == NULL || helper == NULL) {
		pthread_mutex_unlock(&mylock);
		return 1;
	}
	int res = write_file_helper(filename,offset,count,buf,helper);
	pthread_mutex_unlock(&mylock);
	return res;
}

int write_file_helper(char * filename, size_t offset, size_t count, void * buf, void * helper) {
	file_system* sys = (file_system*) helper;
	if (sys->list == NULL) {
		return 1;
	}	
	
	node *t = get_node(sys->list, filename);
	if (t == NULL) {
		pthread_mutex_unlock(&mylock);
		return 1; // file does not exist
	}
	// check if write is trying to increase filesize when there is not enough free space
	if (((sys->data_used)+offset+count) > (sys->max_data+t->length)) {
		return 3;
	}
	// offset exceeds file length
	if (offset > t->length) {
		return 2;
	}
	size_t start_offset;
	size_t end_offset;
	// write_file is not trying to increase filesize
	if (t->length >= offset+count) {
		start_offset = (t->offset+offset)/256;
		end_offset = (t->offset+offset+count)/256;
		memcpy((sys->fdata+(t->offset+(offset))),buf,count);
		
		update_hash(start_offset,end_offset,helper);
		
		msync(sys->hash, (sys->hash_size*HASH_BLOCK),MS_ASYNC);
		msync(sys->dir,(sys->max_records*72),MS_ASYNC);
		msync(sys->fdata,sys->max_data,MS_ASYNC);
		return 0;
	} 
	// use of resize is required as write_file is trying to increase filesize
	int res = resize_file_helper(filename, (count+offset), helper);
	if (res == 2 || res == 1) {
		return res;
	}
	t = get_node(sys->list,filename);
	memcpy((sys->fdata+(t->offset+(offset))),buf,count);
	end_offset = ((t->offset+t->length))/256;
	start_offset = (t->offset)/256;
	// update hash and flush changes
	update_hash(start_offset,end_offset,helper);	
	msync(sys->hash, (sys->hash_size*HASH_BLOCK),MS_ASYNC);
	msync(sys->dir,(sys->max_records*72),MS_ASYNC);
	msync(sys->fdata,sys->max_data,MS_ASYNC);
	return 0;
}


ssize_t file_size(char * filename, void * helper) {
	pthread_mutex_lock(&mylock);
	if (filename == NULL || helper == NULL) {
		pthread_mutex_unlock(&mylock);
		return -1;
	}
    file_system* sys = (file_system*) helper;
	if (sys->list == NULL) {
		pthread_mutex_unlock(&mylock);
		return -1;
	}

	node *t = get_node(sys->list, filename);
	if (t==NULL) {
		pthread_mutex_unlock(&mylock);
		return -1; // file does not exist
	} 

	pthread_mutex_unlock(&mylock);
	return t->length;
}

// compute fletcher hash;
void fletcher(uint8_t * buf, size_t length, uint8_t * output) {
	size_t nloops = length/4;
    size_t rem = length%4;
	// if length is not a multiple of 4 pad with 0's
	if (rem!=0) {
		nloops++; 
		size_t padding = 4-rem;
		buf = realloc(buf,(length+padding));
		for (size_t i = length; i < length+padding; i++) {
			buf[i] = 0;
		}
	}
	uint32_t *data = (uint32_t *) buf;

	uint64_t a = 0;
	uint64_t b = 0;
	uint64_t c = 0;
	uint64_t d = 0;

	for (size_t i = 0; i < nloops; i++) {
		a = (a + data[i]) % LARGE;
		b = (b + a) % LARGE;
		c = (c + b) % LARGE;
		d = (d + c) % LARGE;
	}

	
	uint32_t *x = (uint32_t*) &a;
	uint32_t *y = (uint32_t*) &b;
	uint32_t *z = (uint32_t*) &c;
	uint32_t *w = (uint32_t*) &d;
	
	memcpy((output),x,4);
	memcpy((output+4),y,4);
	memcpy((output+8),z,4);
	memcpy((output+12),w,4);
	
	return;
}


void compute_hash_tree(void * helper) {
	pthread_mutex_lock(&mylock);
	if (helper == NULL) {
		pthread_mutex_unlock(&mylock);
		return;
	}
	compute_hash_tree_helper(helper);
	pthread_mutex_unlock(&mylock);
	return;
}

/* this function computes the hashes by moving through moving through 
   each level of the binary tree and computing the hash,
   beginning with the leaf nodes and ending at the root */
static void compute_hash_tree_helper(void * helper) {
	file_system* sys = (file_system*) helper;
	size_t nhash = sys->hash_size;
	size_t nblocks = sys->num_blocks;
	size_t leaf = nhash - nblocks; // 15-8=7
	size_t index;
	uint8_t *out1 = malloc(HASH_BLOCK);
	uint8_t *concat = malloc(HASH_BLOCK*2);
	uint8_t buff[256];
	size_t i = 0;
	if (sys->hash_size == 1) {
		memcpy(buff,sys->fdata,256);
		fletcher(buff,256,out1);
		memcpy(sys->hash,out1,16);
		free(out1);
		free(concat);
		msync(sys->hash, (sys->hash_size*HASH_BLOCK),MS_ASYNC);
		return;
	} 
	for (index = nhash - nblocks; index < nhash; index++) {
		memcpy(buff,(sys->fdata+DATA_BLOCK*i),DATA_BLOCK);
		fletcher(buff,DATA_BLOCK,out1);
		for (size_t x = 0; x < HASH_BLOCK; x++) {
			sys->hash[(index*HASH_BLOCK)+x] = out1[x];
		}
		i++;
	}
	size_t n = nblocks/2; // ex. 8/2=4
	int cont = 0;
	index = 0;
	while (cont==0) {
		if (n==1) {
			cont = 1;
		}
		leaf = leaf - n; // 7-4 = 3
		for (size_t j = 0; j < n; j++) { // 4 iterations
			index = leaf + j; //  
			memcpy(concat, (sys->hash + 16*(2*index+1)),16);
			memcpy((concat+16),(sys->hash + 16*(2*index+2)),16);
			fletcher(concat,32,out1);
			memcpy((sys->hash +16*index),out1,16);
		}
		n = n/2;
	}
	free(out1);
	free(concat);
	msync(sys->hash, (sys->hash_size*HASH_BLOCK),MS_ASYNC);
   	return;
}

void compute_hash_block(size_t block_offset, void * helper) {
	pthread_mutex_lock(&mylock);
	if (helper == NULL) {
		pthread_mutex_unlock(&mylock);
		return;
	}
	compute_hash_block_helper(block_offset, helper);
	pthread_mutex_unlock(&mylock);
	return;
}
/* this function computes a subtree of the hash tree at block offset beginning by
   recomputing the hash of the specified block, and obtaining its pair-block's
   (each leaf block has a pair block used to calculate the parent) hash, and 
   moving up the subtree, recalculating the new hashes at each level until the root*/
void compute_hash_block_helper(size_t block_offset, void * helper) {
	file_system* sys = (file_system*) helper;
	size_t nblocks = sys->num_blocks;
	size_t index = block_offset + (sys->hash_size - sys->num_blocks);
	size_t index2 = 0;
	uint8_t block[256];
	memcpy(block,(sys->fdata+(DATA_BLOCK*block_offset)),DATA_BLOCK);
	uint8_t *out1 = malloc(HASH_BLOCK);
	uint8_t *out2 = malloc(HASH_BLOCK);
	uint8_t *concat = malloc(HASH_BLOCK*2);
	fletcher(block,DATA_BLOCK,out1);
	memcpy((sys->hash+HASH_BLOCK*index),out1,HASH_BLOCK);
	if (sys->hash_size == 1) {
		free(out1);
		free(out2);
		free(concat);
		msync(sys->hash, (sys->hash_size*HASH_BLOCK),MS_ASYNC);
		return;
	} 
	size_t parent;
	if (index%2 == 0) { // even, need index prev
		parent = (index-2)/2;
		index2 = index - 1;
		memcpy(out2,(sys->hash+HASH_BLOCK*index2),HASH_BLOCK);
		memcpy(concat, out2,16); // concat in reverse order
		memcpy((concat+16),out1,16);
		fletcher(concat,32,out1);
	} else {
		parent = (index-1)/2;
		index2 = index + 1;
		memcpy(out2,(sys->hash+HASH_BLOCK*index2),HASH_BLOCK);
		memcpy(concat, out1,16);
		memcpy((concat+16),out2,16);
		fletcher(concat,32,out1);
	}
	memcpy((sys->hash +16*parent),out1,16);
	// now loop from parent back to root, updating as you go along
	index = parent;
	size_t n = nblocks/2; // ex. 8/2=4
	int cont = 0;
	while (cont==0) {
		if (n==2) {
			cont = 1;
		}
		// from previous iteration, we know parent hash is in out1
		if (index%2 == 0) { //even
			parent = (index-2)/2;
			index2 = index - 1;
			memcpy(out2,(sys->hash+HASH_BLOCK*index2),HASH_BLOCK);
			memcpy(concat, out2,16); // concat in reverse order
			memcpy((concat+16),out1,16);
			fletcher(concat,32,out1);
		} else {
			parent = (index-1)/2;
			index2 = index + 1;
			memcpy(out2,(sys->hash+HASH_BLOCK*index2),HASH_BLOCK);
			memcpy(concat, out1,16);
			memcpy((concat+16),out2,16);
			fletcher(concat,32,out1);
		}
		memcpy((sys->hash+16*parent),out1,16);
		index = parent;
		n = n/2;
	}
	
	free(out1);
	free(out2);
	free(concat);
	msync(sys->hash, (sys->hash_size*HASH_BLOCK),MS_ASYNC);
	return;
}

/* this helper function takes a start offset and end offset for the data blocks in file data
   and computes the hash block for each offset from start to end (inclusive). It is 
   used to update hash data in functions create_file, resize_file and write_file */
void update_hash(size_t start_offset, size_t end_offset, void * helper) {
	file_system* sys = (file_system*) helper;
	if (start_offset == end_offset && end_offset != (sys->max_data)/256) {
		compute_hash_block_helper(start_offset,helper);
	 // ensure we don't exceed block offset bounds
	} else if (end_offset == (sys->max_data)/256) {
		for (size_t i=start_offset; i < end_offset; i++) {
			compute_hash_block_helper(i,helper);
		}
	} else {
		for (size_t i=start_offset; i <= end_offset; i++) {
			compute_hash_block_helper(i,helper);
		}
	}
	return;
}
/* this function verifies the hash data at a particular offset. Used in read_file */
int hash_verify(void *helper, size_t start_offset, size_t end_offset) {
	int res = 0;
	file_system* sys = (file_system*) helper;
	char *cmp = malloc((sys->hash_size*16));
	memcpy(cmp,sys->hash,(sys->hash_size*16));
	update_hash(start_offset,end_offset,helper);
	res = memcmp(cmp,sys->hash,(sys->hash_size*16));
	if (res!=0) { // if not same
		memcpy(sys->hash,cmp,(sys->hash_size*16)); // put everything back
	}
	free(cmp);
	return res;
}



// list file

// initialize list
sorted_list* list_init() {
 sorted_list *list = malloc(sizeof(sorted_list));
 list->size = 0;
 list->head = NULL;
 return list;
}

// add a node to list
node* node_add(sorted_list *list, char *name, uint32_t offset, uint32_t length,uint32_t index) {
 node *t = list->head;
 node *n = malloc(sizeof(node));
 n->offset = offset;
 n->length = length;
 n->name = name;
 n->index = index;
 if (t==NULL) {
   list->head = n;
   n->next = NULL;
   (list->size)++;
   return n;
 }
 if (t->offset >= n->offset) {
   n->next = t; // n is new head node
   list->head = n;
 } else {
   while(t->next!=NULL) { // search through list
     if (t->next->offset >= n->offset) {
       n->next = t->next;
       t->next = n;
       break;
     } else {
       t = t->next;
     }
   }
   if (t->next == NULL) {
     t->next = n;
     n->next = NULL;
   }
 }
 (list->size)++;
 return n;
}

// remove a node from list
void node_delete(sorted_list *list, node* n) {
 node *t = list->head;
 if (n == t) { // n is the head node;
   list->head = n->next;
   free(n->name);
   free(n);
   (list->size)--;
   return;
 }
   while (t->next != n) {
       t = t->next;
   }
   t->next = n->next;
   free(n->name);
   free(n);
 (list->size)--;
 return;
}
// find the first (least) offset available to store a new file of size length
// used to improve speed in create_file and repack, as linked list is ordered
uint32_t free_pos(sorted_list *list, uint32_t length) {
  node *t = list->head;
  uint32_t offset = 0;
   if (list->head==NULL) {
     return 0;
   }
   while(t!=NULL) {
	  if (t==list->head) {
		  if (t->offset >= length) {
			  return 0;
		  }
	  } 
      if (t->next == NULL) {
       offset = (t->offset) + (t->length);
       return offset;
     } else if ((t->offset)+(t->length)+ length < (t->next->offset)) {
	   offset = (t->offset)+(t->length);
       return offset;
     }
     t = node_next(t);
   }
   return offset;
}
// return next node
node* node_next(node* n) {
 return n->next;
}
// return node with matching name
node* get_node(sorted_list *list, char *name) {
	node *t = list->head;
	while (t!=NULL) {
		if (strncmp(name,t->name,64)==0) {
			return t;
		}
		t = node_next(t);
	}
	return NULL;
}
// free list and all of its nodes
void list_free(sorted_list *list) {
 list->size = 0;
 if (list->head!=NULL) {
   node *t = list->head;
   while (t!=NULL) {
     node *n = t->next;
	 free(t->name);
     free(t);
     t = n;
   }
 }
 list->head = NULL;
 if (list!=NULL) {
   free(list);
 }
 list=NULL;
 return;
}
