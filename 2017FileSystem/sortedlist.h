#include <stdlib.h>
#include <stdint.h>
/* this sorted list orders nodes from lowest offset to highest
	using a singly linked list implementation */

typedef struct sorted_list sorted_list;
typedef struct node node;

// my sorted linked list 
struct sorted_list {
	node *head;
	int size;
};

/* node element of list, which is used to help track and 
perform calculations and changes to directory_table. Each node
stores information about a record from the directory_table */
struct node {
	node* next;
	char* name;
	uint32_t offset;
	uint32_t length;
	uint32_t index;
};

// creates list with head node
sorted_list* list_init();

/* adds node n to correct position based on ordering
   special attention needed for when head needs to be replaced
*/
node* node_add(sorted_list *list, char *name, uint32_t offset, uint32_t length, uint32_t index);

// removes node from list
void node_delete(sorted_list *list, node* n);

// returns the node pointed to by n
node* node_next(node* n);

/* returns the lowest offset position which can fit length bytes without
 exceeding the offset of the next node */
uint32_t free_pos(sorted_list *list, uint32_t length); //finds a space with free offset in list big enough to store length bytes;

/* returns the node who's name value matches name */
node* get_node(sorted_list *list, char *name);

/* frees all nodes from list, then frees the list */
void list_free(sorted_list *list);


