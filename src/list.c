#include <stdio.h>

#include "imapfilter.h"
#include "list.h"


/*
 * Add a new element at the end of the list.
 */
list *
list_append(list *lst, void *data)
{
	list *l, *nl;

	nl = (list *)xmalloc(sizeof(list));
	nl->data = data;
	nl->prev = nl->next = NULL;

	if (lst != NULL) {
		for (l = lst; l->next != NULL; l = l->next);
		l->next = nl;
		nl->prev = l;

		return lst;
	} else {
		return nl;
	}
}


/*
 * Remove an element from the list.
 */
list *
list_remove(list *lst, void *data)
{
	list *l;

	if (!lst)
		return NULL;

	l = lst;
	while (l != NULL) {
		if (l->data != data)
			l = l->next;
		else {
			if (l->prev)
				l->prev->next = l->next;
			if (l->next)
				l->next->prev = l->prev;
			if (lst == l)
				lst = lst->next;

			xfree(l);

			break;
		}
	}


	return lst;
}

