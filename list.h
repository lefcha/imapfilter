#ifndef LIST_H
#define LIST_H


typedef struct list {
	void *data;
	struct list *next, *prev;
} list;


/*	list.h		*/
list *list_append(list *lst, void *data);
list *list_remove(list *lst, void *data);


#endif				/* LIST_H */
