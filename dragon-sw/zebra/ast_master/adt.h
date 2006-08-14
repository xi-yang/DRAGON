#include <stdio.h>
#include <stdlib.h>

struct adtlist {
  int count;
  struct adtlistnode* head;
  struct adtlistnode* tail;
};

struct adtlistnode {
  void* data;
  struct adtlistnode* next;
};

/* function prototypes for adtlist operation */
int adtlist_getcount(struct adtlist*);
void adtlist_add(struct adtlist*, void*);
void adtlist_free(struct adtlist*);
