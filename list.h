/*
 * list.h
 *
 * Copyright (C) 2020 Ivaylo Dimitrov <ivo.g.dimitrov.75@gmail.com>
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef LIST_H
#define LIST_H

typedef struct _slist slist;

struct _slist
{
  slist *next;
  void *data;
};

typedef int (*slist_find_fn_t)(const void *data, const void *user_data);

__attribute__((warn_unused_result)) slist *
slist_append(slist *list, void *data);

slist *
slist_find(slist *list, slist_find_fn_t find_fn, void *user_data);

int
slist_size(const slist *list);

__attribute__((warn_unused_result)) slist *
slist_remove(slist *list, void *data, void (*free_func)(void *));

#define slist_for_each(head, pos) \
  for (pos = (head); pos; pos = pos->next)

#endif // LIST_H

