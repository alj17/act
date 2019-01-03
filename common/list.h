/*************************************************************************
 *
 *  Copyright (c) 2009 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#ifndef __LIST_H__
#define __LIST_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _listDATA_ list_t;
typedef struct _list_ listitem_t;

struct _list_ {
  const void *data;
  struct _list_ *next;
};

struct _listDATA_ {
  listitem_t *hd, *tl;
};

list_t *list_new (void);
void list_append (list_t *, const void *);
int list_length (list_t *);
const void *list_delete_tail (list_t *);
void list_concat (list_t *main, list_t *x);

/* in-place reversal */
void list_reverse (list_t *);

void list_free (list_t *);
void list_cleanup (void);

list_t *list_map (list_t *l, void *(*f)(const void *));
/*
  The functions take the cookie as the first argument
*/
list_t *list_map_cookie (list_t *l, void *cookie, void * (*f)(void *, const void *));
void list_apply (list_t *l, void *cookie, void (*f)(void *, const void *));

#define list_isempty(l)  (!((l)->hd))
#define list_first(l)    ((l)->hd)
#define list_tail(l)  ((l)->tl)
#define list_next(l)     ((l)->next)
#define list_value(l)    ((l)->data)

typedef void *(*LISTMAPFN)(const void *);
typedef void *(*LISTMAPFNCOOKIE)(void *, const void *);

void stack_push (list_t *l, const void *item);
const void *stack_pop (list_t *l);
#define stack_isempty(l) list_isempty(l)

#ifdef __cplusplus
}
#endif

#endif /* __LIST_H__ */
