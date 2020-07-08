My implementation makes use of a sorted linked list which I have created.
The header file "sortedlist.h" lists the struct definitions and functions of 
this data structure. Its functions are implemented at the end of myfilesystem.c

runtest.c contains and uses twelve files. Each file has a file pair, 
for example: dir.bin and dir-after.bin. Each file has a preserved before state, and 
an after state which are tested against each other to ensure correct output

FILES:
				Set 1
before     		-----     		after

file.bin					file-after.bin
dir.bin						dir-after.bin
hash.bin					hash-after.bin

				Set 2
				-----
fdata_before.bin			fdata_after.bin
directorytable_before.bin	directorytable_after.bin
hashtable_before.bin		hashtable_after.bin

For any test, a return value of -5 indicates that the hash table is incorrect.

TESTS:

0. no_operation: tests init_fs and close_fs

1. create_file_LARGE: tests create file

2. create_file1: tests create_file, returns number of directory table elements that are incorrect

3. create_file_NO_DIR_SPACE: tests create_file, returns 0 if file is not added due to no space

4. delete_file1: tests delete_file, return value indicates number of files unsuccessfully deleted

5. delete_file_NOT_EXIST: tests delete_file, returns 0 if the file (which should not exist)is not found
 
6. test_sorted_list_initialize: tests sortedlist, ensures that init_fs adds the files to my list correctly

7. test_create_2: more complex test of create_file

8. test_get_filesize: tests file_size

9. test_no_mem: tests write_file, resize_file when trying to make a file when there is no available space

10. test_write_1: tests write_file

11. test_write_big_filesize: tests write_file and resize

12. test_write_resize: tests write_file, resize_file and repack

13. test_repack_create_delete: tests create_file, delete_file and repack

14. read_verify_fail: tests read_file and cases where read verification fails

15. test_resize_small_repack: tests resize_file and repack

16. test_rename: tests rename_file

17. test_file_not_exist: tests resize_file, write_file, read_file, rename_file and delete_file
in cases where file does not exist. Returns the number of above functions tested which did not 
return the correct result value.

18. test_rename_already_exists: tests rename_file, including cases where new-name already exists
 
 
 
 
 
 
 
