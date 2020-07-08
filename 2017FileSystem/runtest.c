#include <stdio.h>
#include <string.h>

#define TEST(x) test(x, #x)
#include "myfilesystem.h"

/* You are free to modify any part of this file. The only requirement is that when it is run, all your tests are automatically executed */


// this function is used to reset files a[1-3] to original state stored in files f[1-3] 
void reset_file(char *f1, char *f2, char *f3, char *a1, char *a2, char *a3, int s1, int s2, int s3) {
	char *fbuff = malloc(s1);
	char *dbuff = malloc(s2);
	char *hbuff = malloc(s3);
	
	FILE *ff = fopen(f1,"rb+");
	FILE *af = fopen(a1,"rb+");
	fseek(ff,0,SEEK_SET);
	fseek(af,0,SEEK_SET);
	fread(fbuff,s1,1,ff);
	fwrite(fbuff,s1,1,af);
	fclose(ff);
	fclose(af);
	
	FILE *fd = fopen(f2,"rb+");
	FILE *ad = fopen(a2,"rb+");
	fseek(fd,0,SEEK_SET);
	fseek(ad,0,SEEK_SET);
	fread(dbuff,s2,1,fd);
	fwrite(dbuff,s2,1,ad);
	fclose(fd);
	fclose(ad);
	
	FILE *fh = fopen(f3,"rb+");
	FILE *ah = fopen(a3,"rb+");
	fseek(fh,0,SEEK_SET);
	fseek(ah,0,SEEK_SET);
	fread(hbuff,s3,1,fh);
	fwrite(hbuff,s3,1,ah);
	fclose(fh);
	fclose(ah);
	
	free(fbuff);
	free(dbuff);
	free(hbuff);
		
	return;
}

void make_hash(char *f1, char* f3, size_t hash_size, size_t num_blocks) {
	FILE *fdata = fopen(f1,"rb");
	uint8_t *hash_data = malloc(hash_size*16);
	rewind(fdata);
	size_t nhash = hash_size;
	size_t nblocks = num_blocks;
	size_t leaf = nhash - nblocks; // 15-8=7
	size_t index;
	uint8_t *out1 = malloc(16);
	uint8_t *concat = malloc(16*2);
	uint8_t buff[256];
	for (index = nhash - nblocks; index < nhash; index++) {
		fread(buff,256,1,fdata);
		fletcher(buff,256,out1);
		for (size_t x = 0; x < 256; x++) {
			memcpy(hash_data+(index*16),out1,16);
		}
	}
	fclose(fdata);
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
			memcpy(concat, (hash_data + 16*(2*index+1)),16);
			memcpy((concat+16),(hash_data + 16*(2*index+2)),16);
			fletcher(concat,32,out1);
			memcpy((hash_data +16*index),out1,16);
		}
		n = n/2;
	}
	FILE *hash = fopen(f3,"rb+");
	fwrite(hash_data,(hash_size*16),1,hash);
	fclose(hash);
	free(hash_data);
	free(out1);
	free(concat);
   	return;
}


/* USYD CODE CITATION ACKNOWLEDGEMENT
 * I declare that the following function was
 * written by Comp2017 Tutor Sam Arch
 *
 * TAKEN FROM ED POST BY SAM ARCH
 * LINK: https://edstem.org/courses/3191/discussion/136361?answer=313737
*/
void make_empty(char* filename, int filesize){
	FILE* test_binary_file = fopen(filename,"wb+");
	char* zero_bytes = calloc(filesize,1);
	fwrite(zero_bytes, filesize, 1, test_binary_file);
	free(zero_bytes);
	fseek(test_binary_file, 0, SEEK_END);
	//printf("Size of %s is: %lu\n", filename, ftell(test_binary_file));
	fclose(test_binary_file);
}


int hash_tester(void *helper) {
	int res = 0;
	file_system* sys = (file_system*) helper;
	char *cmp = malloc((sys->hash_size*16));
	memcpy(cmp,sys->hash,(sys->hash_size*16));
	compute_hash_tree(helper);
	res = memcmp(cmp,sys->hash,(sys->hash_size*16));
	if (res!=0) { // if not same
		memcpy(sys->hash,cmp,(sys->hash_size*16)); // put everything back
	}
	free(cmp);
	return res;
}

int create_file_LARGE() {
	int res = 0;
	void * helper = init_fs("file-after.bin", "dir-after.bin", "hash-after.bin", 1);
    res = create_file("birthday\n",9000,helper);
	res = hash_tester(helper);
	if (res!=0) {
		close_fs(helper);
		return -5;
	}
	close_fs(helper);

	return 0;
}

int create_file1() {
	int res = 0;
	int count = 0;
	void * helper = init_fs("file-after.bin", "dir-after.bin", "hash-after.bin", 1);
	FILE *dir = fopen("dir-after.bin","rb");
	FILE *prev = fopen("dir.bin","rb");

	res = create_file("mark\0",26,helper);
	
	char buf[5];
	uint32_t off;
	uint32_t len;

	fseek(dir,0,SEEK_SET);
	fread(buf,5,1,dir);
	fseek(dir,64,SEEK_SET);
	fread(&off,4,1,dir);
	fread(&len,4,1,dir);
	fclose(dir);
	
	if (strncmp(buf,"mark\0",4)!=0 || off != 0 || len != 26 || res != 0) {
		count++;
	}
	
	fseek(prev,0,SEEK_SET);
	fread(buf,5,1,prev);
	fseek(prev,64,SEEK_SET);
	fread(&off,4,1,prev);
	fread(&len,4,1,prev);
	fclose(prev);
	
	if (buf[0] != '\0' || off != 0 || len != 0) {
		count++; // previous contents modified
	}
	
	res = hash_tester(helper);
	if (res!=0) {
		fclose(dir);
		close_fs(helper);
		return -5;
	}
	
	close_fs(helper);
	return count;
}

int create_file_NO_DIR_SPACE() {
	int res = 0;
	void * helper = init_fs("file-after.bin", "dir-after.bin", "hash-after.bin", 1);
	FILE *dir = fopen("dir-after.bin","rb");

	create_file("maximum\0",5,helper);
	create_file("mark\0",26,helper);

	res = create_file("bundle\0",6,helper);
	fclose(dir);
	close_fs(helper);
	if (res!=2) {
		return res;
	}
	return 0;
}

int delete_file1() {
	void * helper = init_fs("file-after.bin", "dir-after.bin", "hash-after.bin", 1);
    int res = 0;
	int count = 0;
	FILE *dir = fopen("dir-after.bin","rb");
	res = delete_file("happy\0",helper);
	if (res != 0) {
		count++;
	}
	res = delete_file("burger\0",helper);
	if (res != 0) {
		count++;
	}

	char name0, name3;
	char name1[64], name2[64];
	
	fread(&name0,1,1,dir);
	
	fseek(dir,72,SEEK_SET);
	fread(&name1,64,1,dir);
	
	fseek(dir,144,SEEK_SET);
	fread(&name2,64,1,dir);
	
	fseek(dir,216,SEEK_SET);
	fread(&name3,1,1,dir);
	
	if (name0!='\0' || name3!='\0') {
		fclose(dir);
		close_fs(helper);
		return -2;
	}
	if (name1[0] != '\0' || name2[0] != '\0') {
		fclose(dir);
		close_fs(helper);
		return -2;
	}
	fclose(dir);
	close_fs(helper);
	return count;
}



int delete_file_NOT_EXIST() {
	void * helper = init_fs("file.bin", "dir.bin", "hash.bin", 1);
	int res = delete_file("rain\0",helper);
	if (res!=1) {
		res = -1;
	} else {
		res = 0;
	}
	close_fs(helper);
	return res;
}

int test_sorted_list_initialize() {
	void *helper = init_fs("fdata_after.bin","directorytable_after.bin","hashtable_after.bin",1);
	file_system *sys = (file_system*) helper;
	int count = 0;
	node *t = sys->list->head;

	if (strncmp(t->name,"this is a file\0",64)||t->offset!=0||t->length!=67||t->index!=0) {
		count++;
	}
	t = t->next;
	if (strncmp(t->name,"birthday\0",64)||t->offset!=560||t->length!=212||t->index!=1) {
		count++;
	}
	t = t->next;
	if (strncmp(t->name,"cheeseburgerdeluxe\0",64)||t->offset!=1000||t->length!=30||t->index!=5) {
		count++;
	}
	t = t->next;
	if (strncmp(t->name,"file_the_third_also_known_as_brian\0",64)||t->offset!=1920||t->length!=3000||t->index!=3) {
		count++;
	}

	close_fs(helper);
	return count;
}

int test_create_2() {
	int res;
	void *helper = init_fs("fdata_after.bin","directorytable_after.bin","hashtable_after.bin",1);
	// file_system *sys = (file_system*) helper;
	res = create_file("laugh",3000,helper);
	
	if (res!=0) {
		printf("failed to create file\n"); // helps to debug
		close_fs(helper);
		return res;
	}
	
	record *rec_prev = malloc(sizeof(record));
	record *rec_after = malloc(sizeof(record));
	
	FILE *dir_before = fopen("directorytable_before.bin","rb");
	fseek(dir_before,72*2,SEEK_SET);
	fread(rec_prev,sizeof(record),1,dir_before);
	
	FILE *dir_after = fopen("directorytable_after.bin","rb");
	fseek(dir_after,72*2,SEEK_SET);
	fread(rec_after,sizeof(record),1,dir_after);
	
	fclose(dir_before);
	fclose(dir_after);
	
	int count = 5;
	if (strncmp(rec_after->name,"laugh",64)!=0 && strncmp(rec_prev->name,"\0",64)!=0) {
		count++;
	}
	if (rec_after->offset != 4920 || rec_prev->offset != 0) {
		count++;
	}
	if (rec_after->length != 3000 || rec_prev->length != 0) {
		count++;
	}
	if (count!=5) { // count returns 6,7,8, we know it failed here
		free(rec_prev);
		free(rec_after);
		close_fs(helper);
		return count;
	}
	
	free(rec_prev);
	free(rec_after);
	char c;
	// check previous data
	FILE *data_prev = fopen("fdata_before.bin","rb");
	fseek(data_prev,4920,SEEK_SET);
	for (size_t i = 0; i < 3000; i++) {
		fread(&c,1,1,data_prev);
		if (c!='p') {
			fclose(data_prev);
			close_fs(helper);
			return 9;
		}
	}
	fclose(data_prev);
	// check new file data to ensure correct update
	FILE *data_after = fopen("fdata_after.bin","rb");
	fseek(data_after,4920,SEEK_SET);
	for (size_t i = 0; i < 3000; i++) {
		fread(&c,1,1,data_after);
		if (c!=0) {
			close_fs(helper);
			fclose(data_after);
			return 10;
		}
	}
	
	res = hash_tester(helper);
	if (res!=0) {
		close_fs(helper);
		return -5;
	}
	
	
	close_fs(helper);
	return 0;
}

int test_get_filesize() {
	int count = 0;
	int res = 0;
	void *helper = init_fs("fdata_after.bin","directorytable_after.bin","hashtable_after.bin",1);
	res = file_size("this is a file",helper);
	if (res!= 67) {
		close_fs(helper);
		return 1;
	}
	res = file_size("cheeseburgerdeluxe",helper);
	if (res!= 30) {
		close_fs(helper);
		return 2;
	}
	res = file_size("birthday",helper);
	if (res!= 212) {
		close_fs(helper);
		return 3;
	}
	res = file_size("djhr",helper);
	if (res!=-1) {
		close_fs(helper);
		return 4;
	}
	
	close_fs(helper);
	return count;
}

int test_no_mem() {
	void *helper = init_fs("fdata_after.bin","directorytable_after.bin","hashtable_after.bin",1);
	
	int res = 0;
	char *buf = calloc(1,8000);
	res = write_file("birthday",0,8000,(void*)calloc,helper); // no memory for resize
	if (res!=3) {
		free(buf);
		close_fs(helper);
		return 1;
	}
	res = write_file("this is a file",100,1,(void*)buf,helper); // offset greater than length
	if (res!=2) {
		free(buf);
		close_fs(helper);
		return 2;
	}
	res = resize_file("this is a file",7900,helper);
	if (res!=2) {
		free(buf);
		close_fs(helper);
		return 3;
	}

	res = resize_file("cheeseburgerdeluxe",4914,helper);
	if (res!=2) {
		free(buf);
		close_fs(helper);
		return 4;
	}
	
	free(buf);
	close_fs(helper);
	return 0;
}

int test_write_1() {
	void *helper = init_fs("fdata_after.bin","directorytable_after.bin","hashtable_after.bin",1);
    char * buf = malloc(20);
	for (int i=0; i<20; i++) {
		buf[i]='$';
	}
	int res = write_file("this is a file",0,20,buf,helper);
	if (res!=0) {
		free(buf);
		close_fs(helper);
		printf("write failed\n");
		return res;
	}
	char *out = malloc(20);
	char *expected = malloc(20);
	for (int i=0; i<20; i++) {
		expected[i] = '9';
	}
	FILE *fdb = fopen("fdata_before.bin","rb");
	fread(out,1,20,fdb);
	res = memcmp(out,expected,20);
	fclose(fdb);
	if (res!=0) {
		printf("data prev corrupted\n");
		free(expected);
		free(out);
		free(buf);
		close_fs(helper);
		return res;
	}
	
	
	FILE *fd = fopen("fdata_after.bin","rb");
	fread(out,1,20,fd);
	res = memcmp(buf,out,20);
	fclose(fd);
	free(out);
	free(buf);
	free(expected);
	
	res = hash_tester(helper);
	if (res!=0) {
		close_fs(helper);
		return -5;
	}
	
	close_fs(helper);
    return res;
}

int test_write_big_filesize() {
	void *helper = init_fs("fdata_after.bin","directorytable_after.bin","hashtable_after.bin",1);
	FILE *fbefore = fopen("fdata_before.bin","rb");
	char * cont = malloc(150);
	fread(cont,150,1,fbefore);
	for (int i = 0; i < 150; i++) {
		if (cont[i]!='9') {
			free(cont);
			fclose(fbefore);
			close_fs(helper);
			return 4; // file contents illegally modified
		}
	}
	fclose(fbefore);
	uint32_t length;
	uint32_t offset;
	FILE *dirbefore = fopen("directorytable_before.bin","rb");
	fseek(dirbefore,64,SEEK_SET);
	fread(&offset,4,1,dirbefore);
	fread(&length,4,1,dirbefore);
	if (length!=67 || offset !=0) {
		close_fs(helper);
		free(cont);
		fclose(dirbefore);
		return 4;
	}
	fclose(dirbefore);
	free(cont);
	char * buf = malloc(150);
	for (int i=0; i<150; i++) {
		buf[i]='$';
	}
	int res = write_file("this is a file",10,150,buf,helper);
	free(buf);
	FILE *dir = fopen("directorytable_after.bin","rb");
	fseek(dir,64,SEEK_SET);
	fread(&offset,4,1,dir);
	fread(&length,4,1,dir);
	if (length!=160 || offset !=0) {
		close_fs(helper);
		fclose(dir);
		return 4;
	}
	fclose(dir);
	char *info = malloc(160);
	FILE *fdata = fopen("fdata_after.bin","rb");
	fread(info,160,1,fdata);
	for (int i = 0; i < 10; i++) {
		if (info[i]!='9') {
			free(info);
			fclose(fdata);
			close_fs(helper);
			return 5;
		}
	}
	for (int i = 10; i < 160; i++) {
		if (info[i] != '$') {
			free(info);
			fclose(fdata);
			close_fs(helper);
			return 6;
		}
	}
	free(info);
	fclose(fdata);

	
	res = hash_tester(helper);
	if (res!=0) {
		close_fs(helper);
		return -5;
	}
	
	close_fs(helper);
	return res;
}

int test_write_resize() {
	void *helper = init_fs("fdata_after.bin","directorytable_after.bin","hashtable_after.bin",1);
	FILE *fb = fopen("fdata_before.bin","rb");
	char *bef = malloc(8192);
	fread(bef,8192,1,fb);
	fclose(fb);
	FILE *fa = fopen("fdata_after.bin","rb");
	char *aft = malloc(8192);
	fread(aft,8192,1,fa);
	fclose(fa);
	int res = memcmp(bef,aft,8192);
	free(bef);
	free(aft);
	if (res!=0) {
		close_fs(helper);
		return res;
	}
	char *buf = calloc(2000,1);
	res = write_file("cheeseburgerdeluxe",0,2000,buf,helper);
	free(buf);
	if (res!=0) {
		close_fs(helper);
		return res;
	}
	// Now check that dir and file data have been correctly repacked
	FILE *dir = fopen("directorytable_after.bin","rb");
	char name[64];
	uint32_t offset;
	uint32_t length;
	
	fseek(dir,0,SEEK_SET);
	fread(name,1,64,dir);
	fread(&offset,4,1,dir);
	fread(&length,4,1,dir);
	if (offset!=0 || length !=67 || strcmp(name,"this is a file")!=0) {
		close_fs(helper);
		return 4;
	}
	fseek(dir,72,SEEK_SET);
	fread(name,1,64,dir);
	fread(&offset,4,1,dir);
	fread(&length,4,1,dir);
	if (offset!=67 || length !=212 || strcmp(name,"birthday")!=0) {
		close_fs(helper);
		return 4;
	}
	fseek(dir,216,SEEK_SET);
	fread(name,1,64,dir);
	fread(&offset,4,1,dir);
	fread(&length,4,1,dir);
	if (offset!=279 || length !=3000 || strncmp(name,"file_the_third_also_known_as_brian",64)!=0) {
		close_fs(helper);
		return 4;
	}
	fseek(dir,360,SEEK_SET);
	fread(name,1,64,dir);
	fread(&offset,4,1,dir);
	fread(&length,4,1,dir);
	if (offset!=3279 || length !=2000 || strncmp(name,"cheeseburgerdeluxe",64)!=0) {
		close_fs(helper);
		return 4;
	}
	// directory table repacked correctly.
	fclose(dir);
	FILE *fdata = fopen("fdata_after.bin","rb");
	char *cont1 = malloc(67);
	char *cont2 = malloc(212);
	char *cont3 = malloc(3000);
	char *cont4 = malloc(2000);
	fseek(fdata,0,SEEK_SET);
	fread(cont1,1,67,fdata);
	for (int i = 0; i < 67; i++) {
		if (cont1[i]!='9') {
			res = 5;
		}
	}
	fread(cont2,1,212,fdata);
	for (int i = 0; i <212; i++) {
		if (cont2[i] != '9') {
			res = 5;
		}
	}
	fread(cont3,1,3000,fdata);
	for (int i = 0; i < 2980; i++) {
		if (cont3[i] != '5') {
			res = 5;
		}
	}
	for (int i = 2980; i <3000; i++) {
		if (cont3[i]!='p') {
			res = 5;
		}
	}
	fread(cont4,1,2000,fdata);
	for (int i = 0; i < 2000; i++) {
		if (cont4[i] != '\0') {
			res = 5;
		}
	}
	
	free(cont1);
	free(cont2);
	free(cont3);
	free(cont4);
	
	res = hash_tester(helper);
	if (res!=0) {
		close_fs(helper);
		return -5;
	}
	
	
	close_fs(helper);
	return res;
}

int test_repack_create_delete() {
	void *helper = init_fs("fdata_after.bin","directorytable_after.bin","hashtable_after.bin",1);
	FILE *fb = fopen("fdata_before.bin","rb");
	char *bef = malloc(8192);
	fread(bef,8192,1,fb);
	fclose(fb);
	FILE *fa = fopen("fdata_after.bin","rb");
	char *aft = malloc(8192);
	fread(aft,8192,1,fa);
	fclose(fa);
	int res = memcmp(bef,aft,8192);
	free(bef);
	free(aft);
	if (res!=0) {
		close_fs(helper);
		return res;
	}
	
	FILE *db = fopen("directorytable_before.bin","rb");
	char* dircontbef = malloc(720);
	fread(dircontbef,720,1,db);
	fclose(db);
	FILE *da = fopen("directorytable_after.bin","rb");
	char* dircontaft = malloc(720);
	fread(dircontaft,720,1,da);
	fclose(da);
	res = memcmp(dircontbef,dircontaft,720);
	free(dircontbef);
	free(dircontaft);
	if (res!=0) {
		close_fs(helper);
		return res;
	}
	
	res = delete_file("cheeseburgerdeluxe",helper);
	res = create_file("cool",500,helper);
	res = create_file("napoleon",300,helper);
	if (res!=0) {
		close_fs(helper);
		return res;
	}
	FILE *dir = fopen("directorytable_after.bin","rb");
	char name[64];
	uint32_t offset;
	uint32_t length;
	fseek(dir,0,SEEK_SET);
	fread(name,1,64,dir);
	fread(&offset,4,1,dir);
	fread(&length,4,1,dir);
	if (offset!=0 || length!=67 || strcmp(name,"this is a file")!=0) {
		res = 4;
	}
	fread(name,1,64,dir);
	fread(&offset,4,1,dir);
	fread(&length,4,1,dir);	
	if (offset!=560 || length!=212 || strcmp(name,"birthday")!=0) {
		res = 4;
	}
	fread(name,1,64,dir);
	fread(&offset,4,1,dir);
	fread(&length,4,1,dir);	
	if (offset!=772 || length!=500 || strcmp(name,"cool")!=0) {
		res = 4;
	}
	fread(name,1,64,dir);
	fread(&offset,4,1,dir);
	fread(&length,4,1,dir);	
	if (offset!=1920 || length!=3000 || strncmp(name,"file_the_third_also_known_as_brian",64)!=0) {
		res = 4;
	}
	fread(name,1,64,dir);
	fread(&offset,4,1,dir);
	fread(&length,4,1,dir);	
	if (offset!=67 || length!=300 || strcmp(name,"napoleon")!=0) {
		res = 4;
	}
	fread(name,1,64,dir);
	fread(&offset,4,1,dir);
	fread(&length,4,1,dir);	
	if (offset!=0 || length!=0 || name[0]!='\0') {
		res = 5; // file has been deleted
	}
	fclose(dir);
	repack(helper);
	FILE *dir2 = fopen("directorytable_after.bin","rb");
	// 0 67, napolean 67 367, 367 579, cool 579 1079, 1079 4079

	fseek(dir2,0,SEEK_SET);
	fread(name,1,64,dir2);
	fread(&offset,4,1,dir2);
	fread(&length,4,1,dir2);
	if (offset!=0 || length!=67 || strcmp(name,"this is a file")!=0) {
		res = 6;
	}
	fread(name,1,64,dir2);
	fread(&offset,4,1,dir2);
	fread(&length,4,1,dir2);
	if (offset!=367 || length!=212 || strcmp(name,"birthday")!=0) {
		res = 6;
	}
	fread(name,1,64,dir2);
	fread(&offset,4,1,dir2);
	fread(&length,4,1,dir2);
	if (offset!=579 || length!=500 || strcmp(name,"cool")!=0) {
		res = 6;
	}
	fread(name,1,64,dir2);
	fread(&offset,4,1,dir2);
	fread(&length,4,1,dir2);
	if (offset!=1079 || length!=3000 || strncmp(name,"file_the_third_also_known_as_brian",64)!=0) {
		res = 6;
	}
	fread(name,1,64,dir2);
	fread(&offset,4,1,dir2);
	fread(&length,4,1,dir2);
	if (offset!=67 || length!=300 || strcmp(name,"napoleon")!=0) {
		res = 6;
	}
	fclose(dir2);
	FILE *fdata = fopen("fdata_after.bin","rb");
	char *cont1 = malloc(67);
	char *cont2 = malloc(300);
	char *cont3 = malloc(212);
	char *cont4 = malloc(500);
	char *cont5 = malloc(3000);
	fseek(fdata,0,SEEK_SET);
	fread(cont1,1,67,fdata);
	for (int i = 0; i < 67; i++) {
		if (cont1[i]!='9') {
			res = 7;
		}
	}
	fread(cont2,1,300,fdata);
	for (int i = 0; i < 300; i++) {
		if (cont2[i]!= 0) {
			res = 7;
		}
	}
	fread(cont3,1,212,fdata);
	for (int i = 0; i < 212; i++) {
		if (cont3[i]!= '9') {
			res = 7;
		}
	}
	fread(cont4,1,500,fdata);
	for (int i = 0; i < 500; i++) {
		if (cont4[i]!= 0) {
			res = 7;
		}
	}
	fread(cont5,1,3000,fdata);
	for (int i = 0; i < 2980; i++) {
		if (cont5[i]!= '5') {
			res = 7;
		}
	}
	for (int i = 2980; i < 3000; i++) {
		if (cont5[i]!= 'p') {
			res = 7;
		}
	}
	
	free(cont1);
	free(cont2);
	free(cont3);
	free(cont4);
	free(cont5);
	
	res = hash_tester(helper);
	if (res!=0) {
		close_fs(helper);
		return -5;
	}
	
	close_fs(helper);
	return res;
}

int test_read_excess() {
	void *helper = init_fs("fdata_after.bin","directorytable_after.bin","hashtable_after.bin",1);
	char *buf = malloc(1000);
	read_file("birthday",0,200,buf,helper);
	for (int i = 0; i < 200; i++) {
		if (buf[i]!='9') {
			free(buf);
			close_fs(helper);
			return 5;
		}
	}
	int count = 0;
	int res = read_file("birthday",212,0,buf,helper);
	if (res!=0) {
		count++;
	}
	res = read_file("birthday",900,50,buf,helper);
	if (res!=2) {
		count++;
	}
	res = read_file("birthday",0,500,buf,helper);
	if (res!=2) {
		count++;
	}	
	
	free(buf);
	close_fs(helper);
	return count;
}

int read_verify_fail() {
	FILE *hash = fopen("hashtable_after.bin","rb+");
	fseek(hash,600,SEEK_SET);
	uint32_t *cont = malloc(sizeof(uint32_t)*10);
	fwrite(cont,4,10,hash);
	fseek(hash,2500,SEEK_SET);
	fwrite(cont,4,10,hash);
	fclose(hash);
	free(cont);
	void *helper = init_fs("fdata_after.bin","directorytable_after.bin","hashtable_after.bin",1);
	char *buf = malloc(sizeof(char)*100);
	int res = read_file("birthday",0,100,buf,helper);
	int count = 0;
	if (res!=3) {
		count++;
	}
	res = read_file("file_the_third_also_known_as_brian",2500,100,buf,helper);
	if (res!=3) {
		count++;
	}
	
	free(buf);
	close_fs(helper);
	return count;
}

int test_resize_small_repack() {
	void *helper = init_fs("fdata_after.bin","directorytable_after.bin","hashtable_after.bin",1);
	FILE *fb = fopen("fdata_before.bin","rb");
	char *bef = malloc(8192);
	fread(bef,8192,1,fb);
	fclose(fb);
	FILE *fa = fopen("fdata_after.bin","rb");
	char *aft = malloc(8192);
	fread(aft,8192,1,fa);
	fclose(fa);
	int res = memcmp(bef,aft,8192);
	free(bef);
	free(aft);
	if (res!=0) {
		close_fs(helper);
		return res;
	}
	
	FILE *db = fopen("directorytable_before.bin","rb");
	char* dircontbef = malloc(720);
	fread(dircontbef,720,1,db);
	fclose(db);
	FILE *da = fopen("directorytable_after.bin","rb");
	char* dircontaft = malloc(720);
	fread(dircontaft,720,1,da);
	fclose(da);
	res = memcmp(dircontbef,dircontaft,720);
	free(dircontbef);
	free(dircontaft);
	if (res!=0) {
		close_fs(helper);
		return res;
	}
	res = resize_file("file_the_third_also_known_as_brian",250,helper);
	if (res!=0) {
		close_fs(helper);
		return 3;
	}
	res = resize_file("this is a file",10,helper);
	if (res!=0) {
		close_fs(helper);
		return 3;
	}
	repack(helper);
	
	FILE *dir = fopen("directorytable_after.bin","rb");
	char name[64];
	uint32_t offset;
	uint32_t length;
	
	fseek(dir,0,SEEK_SET);
	fread(name,1,64,dir);
	fread(&offset,4,1,dir);
	fread(&length,4,1,dir);
	if (strcmp(name,"this is a file")!=0 || offset!=0 || length!=10) {
		fclose(dir);
		close_fs(helper);
		return 4;
	}
	fread(name,1,64,dir);
	fread(&offset,4,1,dir);
	fread(&length,4,1,dir);
	if (strcmp(name,"birthday")!=0 || offset != 10 || length!=212) {
		fclose(dir);
		close_fs(helper);
		return 4;
	}
	fseek(dir,216,SEEK_SET);
	fread(name,1,64,dir);
	fread(&offset,4,1,dir);
	fread(&length,4,1,dir);
	if (strcmp(name,"file_the_third_also_known_as_brian")!=0 || offset != 252 || length!= 250) {
		fclose(dir);
		close_fs(helper);
		return 4;
	}
	fseek(dir,360,SEEK_SET);
	fread(name,1,64,dir);
	fread(&offset,4,1,dir);
	fread(&length,4,1,dir);
	if (strcmp(name,"cheeseburgerdeluxe")!=0 || offset != 222 || length!= 30) {
		fclose(dir);
		close_fs(helper);
		return 4;
	}
	fclose(dir);
	
	FILE *fdata = fopen("fdata_after.bin","rb");
	fseek(fdata,0,SEEK_SET);
	char *cont1 = malloc(10);
	char *cont2 = malloc(212);
	char *cont3 = malloc(30);
	char *cont4 = malloc(250);
	fread(cont1,1,10,fdata);
	for (int i = 0; i < 10; i++) {
		if (cont1[i]!='9') {
			res = 5;
		}
	}
	fread(cont2,1,212,fdata);
	for (int i = 0; i < 212; i++) {
		if (cont2[i]!='9') {
			res = 5;
		}
	}
	fread(cont3,1,30,fdata);
	for (int i = 0; i < 30; i++) {
		if (cont3[i]!='5') {
			res = 5;
		}
	}	
	fread(cont4,1,250,fdata);
	for (int i = 0; i < 250; i++) {
		if (cont4[i]!='5') {
			res = 5;
		}
	}	
	free(cont1);
	free(cont2);
	free(cont3);
	free(cont4);
	
	res = hash_tester(helper);
	if (res!=0) {
		close_fs(helper);
		return -5;
	}
	
	close_fs(helper);
	return 0;
}

int test_rename() {
	void *helper = init_fs("fdata_after.bin","directorytable_after.bin","hashtable_after.bin",1);	
	int res = rename_file("this is a file", "A",helper);
	if (res!=0) {
		close_fs(helper);
		return 4;
	}
	res = rename_file("cheeseburgerdeluxe","B",helper);
	if (res!=0) {
		close_fs(helper);
		return 4;
	}
	res = rename_file("file_the_third_also_known_as_brian","C",helper);
	if (res!=0) {
		close_fs(helper);
		return 4;
	}
	char *fname = "It is always better to be nice than to be mean. This is a really long name for a file.";
	res = rename_file("birthday",fname,helper);
	if (res!=0) {
		close_fs(helper);
		return 4;
	}
		FILE *dir = fopen("directorytable_after.bin","rb");
	char name[64];
	uint32_t offset;
	uint32_t length;
	
	fseek(dir,0,SEEK_SET);
	fread(name,1,64,dir);
	fread(&offset,4,1,dir);
	fread(&length,4,1,dir);
	if (strcmp(name,"A")!=0 || offset!=0 || length!=67) {
		fclose(dir);
		close_fs(helper);
		return 5;
	}
	fread(name,1,64,dir);
	fread(&offset,4,1,dir);
	fread(&length,4,1,dir);
	if (strncmp(name,"It is always better to be nice than to be mean. This is a reall",64)!=0 || offset != 560 || length!=212 || name[63]!= 0) {
		fclose(dir);
		close_fs(helper);
		return 4;
	}
	fseek(dir,216,SEEK_SET);
	fread(name,1,64,dir);
	fread(&offset,4,1,dir);
	fread(&length,4,1,dir);
	if (strcmp(name,"C")!=0 || offset != 1920 || length!= 3000) {
		fclose(dir);
		close_fs(helper);
		return 4;
	}
	fseek(dir,360,SEEK_SET);
	fread(name,1,64,dir);
	fread(&offset,4,1,dir);
	fread(&length,4,1,dir);
	if (strcmp(name,"B")!=0 || offset != 1000 || length!= 30) {
		fclose(dir);
		close_fs(helper);
		return 4;
	}
	fclose(dir);

	
	close_fs(helper);
	return 0;
	
}

int test_file_not_exist() {
	void *helper = init_fs("fdata_after.bin","directorytable_after.bin","hashtable_after.bin",1);
	int count = 6;
	int res = delete_file("huwsew",helper);
	if (res==1) {
		count--;
	}
	res = resize_file("huwsew",1,helper);
	if (res==1) {
		count--;
	}
	res = rename_file("hsuwxc","thanks",helper);
	if (res==1) {
		count--;
	}
	char *stuff = malloc(5);
	res = read_file("khajit",5,5,(void*)stuff,helper);
	if (res==1) {
		count--;
	}
	res = write_file("khajit",5,5,(void*)stuff,helper);
	if (res==1) {
		count--;
	}
	res = file_size("napping is fun",helper);
	if (res==-1) {
		count--;
	}
	free(stuff);
	close_fs(helper);
	return count;
}

int test_rename_already_exists() {
	void *helper = init_fs("fdata_after.bin","directorytable_after.bin","hashtable_after.bin",1);
	int count = 2;
	int res = rename_file("this is a file","cheeseburgerdeluxe",helper);
	if (res==1) {
		count--;
	}
	res = rename_file("cheeseburgerdeluxe","birthday",helper);
	if (res==1) {
		count--;
	}
	
	close_fs(helper);
	return count;
}


int no_operation() {
    void * helper = init_fs("file.bin", "dir.bin", "hash.bin", 1); // Remember you need to provide your own test files and also check their contents as part of testing
    close_fs(helper);
    return 0;
}
/****************************/

/* Helper function */
void test(int (*test_function) (), char * function_name) {
    int ret = test_function();
    if (ret == 0) {
        printf("Passed %s\n", function_name);
    } else {
        printf("Failed %s returned %d\n", function_name, ret);
    }
}
/************************/

int main(int argc, char * argv[]) {
    size_t offset = 0;
	size_t length = 0;
    // You can use the TEST macro as TEST(x) to run a test function named "x"
	
	make_empty("file.bin",1024);
	make_empty("dir.bin",288);
	make_empty("hash.bin",112);
	make_empty("file-after.bin",1024);
	make_empty("dir-after.bin",288);
	make_empty("hash-after.bin",112);
	FILE* fdata = fopen("file.bin","rb+");
	FILE* dir = fopen("dir.bin","rb+");
	char buf[150];
	for (int i=0; i<150; i++) {
		buf[i] = 'x';
	}
	fseek(fdata,200,SEEK_SET);
	fwrite(buf,150,1,fdata);
	
	fseek(dir,72,SEEK_SET);
	fwrite("happy\0",sizeof("happy\0"),1,dir);
	fseek(dir,136,SEEK_SET);
	offset = 200;
	length = 150;
	fwrite(&offset,4,1,dir);
	fwrite(&length,4,1,dir);
	
	fwrite("burger\0",sizeof("burger\0"),1,dir);
	offset = 56;
	length = 90;
	fseek(dir,208,SEEK_SET);
	fwrite(&offset,4,1,dir);
	fwrite(&length,4,1,dir);

	char *dircontents = malloc(288);
	FILE *dirafter = fopen("dir-after.bin","rb+");
	fseek(dir,0,SEEK_SET);
	fread(dircontents,1,288,dir);
	fwrite(dircontents,1,288,dirafter);
	
	free(dircontents);
	fclose(dir);
	fclose(dirafter);
	
	char *fdcontents = malloc(1024);
	FILE *fdafter = fopen("file-after.bin","rb+");
	fseek(fdata,0,SEEK_SET);
	fread(fdcontents,1,288,fdata);
	fwrite(fdcontents,1,288,fdafter);;
	
	free(fdcontents);
	fclose(fdafter);
	fclose(fdata);
	
	make_hash("file.bin","hash.bin",7,4);
	make_hash("file-after.bin","hash-after.bin",7,4);
	
	// FIRST SERIES OF TESTS START

	TEST(no_operation);
	
	TEST(create_file_LARGE);
	reset_file("file.bin","dir.bin","hash.bin","file-after.bin","dir-after.bin","hash-after.bin",1024,288,112);
	TEST(create_file1);
	reset_file("file.bin","dir.bin","hash.bin","file-after.bin","dir-after.bin","hash-after.bin",1024,288,112);
	TEST(create_file_NO_DIR_SPACE);
	reset_file("file.bin","dir.bin","hash.bin","file-after.bin","dir-after.bin","hash-after.bin",1024,288,112);

	TEST(delete_file1);
	TEST(delete_file_NOT_EXIST);
	
	// FIRST SERIES OF TESTS END
	
	// CREATE FILE FOR SUBSEQUENT TESTS
	
    // before
	make_empty("fdata_before.bin",8192); // 32 blocks
	make_empty("directorytable_before.bin",720); // 10 entries
	make_empty("hashtable_before.bin",1008); // 63 hashes
	
	// after
	make_empty("fdata_after.bin",8192); // 32 blocks
	make_empty("directorytable_after.bin",720); // 10 entries
	make_empty("hashtable_after.bin",1008); // 63 hashes
	
	FILE* fd = fopen("fdata_before.bin","rb+");
	int i = 0;
	char ch = '9';
	for (i=0; i<900;i++) {
		fwrite(&ch,1,sizeof(char),fd);
	}
	ch = '5';
	for (i=900; i<4900;i++) {
		fwrite(&ch,1,sizeof(char),fd);
	}
	ch = 'p';
	for (i=4900; i<7950;i++) {
		fwrite(&ch,1,sizeof(char),fd);
	}
	ch = '@';
	for (i=7950; i<8192;i++) {
		fwrite(&ch,1,sizeof(char),fd);
	}
	
	FILE *fd_after = fopen("fdata_after.bin","rb+");
	
	char *buffer = malloc(8192);
	fseek(fd,0,SEEK_SET);
	fread(buffer,1,8192,fd); // read from before file
	fwrite(buffer,1,8192,fd_after); // write into after file
	// keeps before file available for comparison
	fclose(fd_after);
	fclose(fd);
	
	FILE *fdir = fopen("directorytable_before.bin","rb+");
	char *fname = calloc(64,1);

	char *name0 = "this is a file\0";
	strncpy(fname,name0,64);
	fname[63] = '\0';
	offset = 0;
	length = 67;
	
	fwrite(fname,1,64,fdir);
	fwrite(&offset,4,1,fdir);
	fwrite(&length,4,1,fdir);
	
	char *name1 = "birthday\0"; 
	strncpy(fname,name1,64);
	fname[63] = '\0';
	offset = 560;
	length = 212;
	
	fwrite(fname,1,64,fdir);
	fwrite(&offset,4,1,fdir);
	fwrite(&length,4,1,fdir);
	
	fseek(fdir,72,SEEK_CUR);
	
	char *name3 = "file_the_third_also_known_as_brian\0"; 
	strncpy(fname,name3,64);
	fname[63] = '\0';
	offset = 1920;
	length = 3000;
	
	fwrite(fname,1,64,fdir);
	fwrite(&offset,4,1,fdir);
	fwrite(&length,4,1,fdir);
	
	fseek(fdir,72,SEEK_CUR);
	
	char *name5 = "cheeseburgerdeluxe\0"; 
	strncpy(fname,name5,64);
	fname[63] = '\0';
	offset = 1000;
	length = 30;
	
	fwrite(fname,1,64,fdir);
	fwrite(&offset,4,1,fdir);
	fwrite(&length,4,1,fdir);
	
	free(fname);
	
	buffer = realloc(buffer,720);
	FILE *dir_after = fopen("directorytable_after.bin","rb+");
	fseek(fdir,0,SEEK_SET);
	fread((char*)buffer,720,1,fdir); // read from before file
	fwrite((char*)buffer,720,1,dir_after); // write into after file
	
	fclose(fdir);
	fclose(dir_after);
	
	make_hash("fdata_before.bin","hashtable_before.bin",63,32);
	make_hash("fdata_after.bin","hashtable_after.bin",63,32);
	
	// SECOND SERIES OF TESTS START
	
	TEST(test_sorted_list_initialize);
	reset_file("fdata_before.bin","directorytable_before.bin","hashtable_before.bin","fdata_after.bin","directorytable_after.bin","hashtable_after.bin",8192,720,1008);
	TEST(test_create_2);
	reset_file("fdata_before.bin","directorytable_before.bin","hashtable_before.bin","fdata_after.bin","directorytable_after.bin","hashtable_after.bin",8192,720,1008);
	TEST(test_get_filesize);
	TEST(test_no_mem);
	TEST(test_write_1);
	reset_file("fdata_before.bin","directorytable_before.bin","hashtable_before.bin","fdata_after.bin","directorytable_after.bin","hashtable_after.bin",8192,720,1008);
	TEST(test_write_big_filesize);
	reset_file("fdata_before.bin","directorytable_before.bin","hashtable_before.bin","fdata_after.bin","directorytable_after.bin","hashtable_after.bin",8192,720,1008);
	TEST(test_write_resize);
	reset_file("fdata_before.bin","directorytable_before.bin","hashtable_before.bin","fdata_after.bin","directorytable_after.bin","hashtable_after.bin",8192,720,1008);
	TEST(test_repack_create_delete);
	reset_file("fdata_before.bin","directorytable_before.bin","hashtable_before.bin","fdata_after.bin","directorytable_after.bin","hashtable_after.bin",8192,720,1008);
	TEST(test_read_excess);
	reset_file("fdata_before.bin","directorytable_before.bin","hashtable_before.bin","fdata_after.bin","directorytable_after.bin","hashtable_after.bin",8192,720,1008);
	TEST(read_verify_fail);
	reset_file("fdata_before.bin","directorytable_before.bin","hashtable_before.bin","fdata_after.bin","directorytable_after.bin","hashtable_after.bin",8192,720,1008);
	TEST(test_resize_small_repack);
	reset_file("fdata_before.bin","directorytable_before.bin","hashtable_before.bin","fdata_after.bin","directorytable_after.bin","hashtable_after.bin",8192,720,1008);
	TEST(test_rename);
	reset_file("fdata_before.bin","directorytable_before.bin","hashtable_before.bin","fdata_after.bin","directorytable_after.bin","hashtable_after.bin",8192,720,1008);
	TEST(test_file_not_exist);
	TEST(test_rename_already_exists);
	free(buffer);
    return 0;
}
