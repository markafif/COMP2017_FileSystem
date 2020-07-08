/* Do not change! */
#define FUSE_USE_VERSION 29
#define _FILE_OFFSET_BITS 64
/******************/

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fuse.h>
#include <errno.h>

#include "myfilesystem.h"

typedef struct file_info file_info;

struct file_info {
	uint64_t fd;
	uint64_t open;
};

file_info *files = NULL;

char * file_data_file_name = NULL;
char * directory_table_file_name = NULL;
char * hash_data_file_name = NULL;


int numfiles = 1;

uint64_t counter = 0;


int myfuse_getattr(const char * name, struct stat * result) {
    // MODIFY THIS FUNCTION
    memset(result, 0, sizeof(struct stat));
	
/* USYD CODE CITATION ACKNOWLEDGEMENT
 * I declare that the following four lines of code (33-36) have been taken from the
 * a website entitled: Writing a Simple Filesystem Using FUSE in C, written by  Mohammed Q. Hussain
 * on May 21, 2016. 
 * 
 * Writing a Simple Filesystem Using FUSE in C:
 * http://www.maastaar.net/fuse/linux/filesystem/c/2016/05/21/writing-a-simple-filesystem-using-fuse/?fbclid=IwAR3AlQ-fmYwMuZSlCPcFKxAcljB8IYmnF61MPg5cPiESaiZxmoCEcLUlxS0
 */ 
	result->st_uid = getuid();
	result->st_gid = getgid();
	result->st_atime = time( NULL );
	result->st_mtime = time( NULL );
    
	if (strcmp(name, "/") == 0) {
        result->st_mode = S_IFDIR;
    } else {
        result->st_mode = S_IFREG;
    }
    return 0;
}

int myfuse_readdir(const char * name, void * buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info * fi) {

/* USYD CODE CITATION ACKNOWLEDGEMENT
 * I declare that the following four lines of code (58-59) have been taken from the
 * a website entitled: Writing a Simple Filesystem Using FUSE in C, written by  Mohammed Q. Hussain
 * on May 21, 2016. 
 * 
 * Writing a Simple Filesystem Using FUSE in C:
 * http://www.maastaar.net/fuse/linux/filesystem/c/2016/05/21/writing-a-simple-filesystem-using-fuse/?fbclid=IwAR3AlQ-fmYwMuZSlCPcFKxAcljB8IYmnF61MPg5cPiESaiZxmoCEcLUlxS0
 */ 
	
	filler( buf, ".", NULL, 0 ); // Current Directory
	filler( buf, "..", NULL, 0 ); // Parent Directory
	
	file_system *sys = (file_system *) fuse_get_context()->private_data;
	
    if (strcmp(name, "/") == 0) {
        // filler(buf, "existing_file", NULL, 0);
		if (sys == NULL || sys->list == NULL) {
			return 0;
		}
		node *t = sys->list->head;
		while(t!=NULL) {
			filler(buf, t->name, NULL, 0);
			t = t->next;
		}
		
    }
    return 0;
}

int myfuse_unlink(const char *filename) {
	
	int res = delete_file((char*)filename, fuse_get_context()->private_data);
	
	if (res == 1) {
		return ENOENT;
	}

	return res;
}

int myfuse_rename(const char * oldname, const char *newname) {
	
	int res = rename_file((char*)oldname, (char*)newname, fuse_get_context()->private_data);
	
	if (res == 1) {
		return ENOENT;
	}
	
	return res;
}


int myfuse_truncate(const char *filename, off_t newsize) {
	
	int res = resize_file((char*)filename, newsize, fuse_get_context()->private_data);
	
	if (res == 1) {
		return ENOENT;
	}
	
	if (res==2) {
		return ENOMEM;
	}

	return 0;
}
    

int myfuse_open(const char *filename, struct fuse_file_info *fi) {
	// check if file has been opened before
	file_system *sys = (file_system *) fuse_get_context()->private_data;
	if (sys == NULL || sys->list == NULL) {
		return 0;
	}
	node *t = get_node(sys->list,(char*)filename);
	if (t==NULL) {
		return ENOENT;
	}
	return 0;
}



int myfuse_read(const char *filename, char *buf, size_t count, off_t offset, struct fuse_file_info * fi) {

// 	if (fi->fh < 0) {
// 		return EBADF;
// 	}
	file_system *sys = (file_system *) fuse_get_context()->private_data;
	if (sys == NULL || sys->list == NULL) {
		return ENOENT;
	}
	node *t = get_node(sys->list,(char*)filename);
	if (t==NULL) {
		return ENOENT;
	}
	
	size_t length = file_size((char*) filename, fuse_get_context()->private_data);
	if (length==0) {
		return EBADF;
	}
	
	if (offset>length) {
		return ENOMEM;
	}
	
	if (offset+count>length) {
		size_t excess = (offset+count)-(length);
		count = count - excess;
	}

	int res = read_file((char*)filename, (size_t) offset, count, buf, fuse_get_context()->private_data);
	
	if (res == 1) {
		return ENOENT;
	}
	
	if (res == 0) {
		res = count;
	}
	if (res == 3) {
		return EIO;
	}
	return res;
}


int myfuse_write(const char *filename, const char *buf, size_t count, off_t offset, struct fuse_file_info *fi) {
	
	// if (fi->fh < 0) {
	// 	return EBADF;
	// }
	
	file_system *sys = (file_system *) fuse_get_context()->private_data;
	if (sys == NULL || sys->list == NULL) {
		return ENOENT;
	}
	node *t = get_node(sys->list,(char*)filename);
	if (t==NULL) {
		return ENOENT;
	}
	
	size_t length = file_size((char*)filename,fuse_get_context()->private_data);
	size_t num_read = count;
	if (count+offset > length) {
		if (sys->data_used+(count+offset-length) > sys->max_data) {
			num_read = (sys->data_used)+(count+offset-length)-(sys->max_data);
		}
	}
	
	if (num_read==0) {
		return ENOMEM;
	}
	
	int res = write_file((char*)filename,offset,count,(void*)buf,fuse_get_context()->private_data);
	
	if (res == 1) {
		return ENOENT;
	}
	
	if (res==3) {
		return ENOMEM;
	}
	
	if (res==0) {
		res = count;
	}

	return res;
}

int myfuse_release(const char *filename, struct fuse_file_info *fi) {
	file_system *sys = (file_system *) fuse_get_context()->private_data;
	if (sys == NULL || sys->list == NULL) {
		return ENOENT;
	}
	node *t = get_node(sys->list,(char*)filename);
	if (t==NULL) {
		return ENOENT;
	}
	return 0;

}

void * myfuse_init(struct fuse_conn_info *ci) {
	void *helper = init_fs(file_data_file_name,directory_table_file_name,hash_data_file_name,1);
    counter = 10;
	return helper;
}
   

void myfuse_destroy(void *helper) {
	close_fs(helper);
	free(files);
    return;
}


int myfuse_create(const char *filename, mode_t mode, struct fuse_file_info *fi) {

	int res = create_file((char*)filename,0,fuse_get_context()->private_data);
	
	if (res==2) {
		return ENOMEM;
	}
	
	return res;
}

struct fuse_operations operations = {
    .getattr = myfuse_getattr,
    .readdir = myfuse_readdir,
    .unlink = myfuse_unlink,
    .rename = myfuse_rename,
    .truncate = myfuse_truncate,
    .open = myfuse_open,
    .read = myfuse_read,
    .write = myfuse_write,
    .release = myfuse_release,
    .init = myfuse_init,
    .destroy = myfuse_destroy,
    .create = myfuse_create
};

int main(int argc, char * argv[]) {
    // MODIFY (OPTIONAL)
    if (argc >= 5) {
        if (strcmp(argv[argc-4], "--files") == 0) {
            file_data_file_name = argv[argc-3];
            directory_table_file_name = argv[argc-2];
            hash_data_file_name = argv[argc-1];
            argc -= 4;
        }
    }
    // After this point, you have access to file_data_file_name, directory_table_file_name and hash_data_file_name
    int ret = fuse_main(argc, argv, &operations, NULL);
    return ret;
}
