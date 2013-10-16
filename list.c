#include "list.h"

void list_append(dir_record_t r, struct list* l)
{
	l->array[l->count] = r;
	l->count++;
}

void list_clear(struct list *l)
{
	l->count = 0;
}