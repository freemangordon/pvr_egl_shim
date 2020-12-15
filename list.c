/*
 * list.c
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

#include <stdlib.h>

#include "list.h"

static slist *
slist_last(slist *list)
{
  while (list && list->next)
    list = list->next;

  return list;
}

slist *
slist_append(slist *list, void *data)
{
  slist *new_list = malloc(sizeof(slist));

  new_list->data = data;
  new_list->next = NULL;

  if (list)
    slist_last(list)->next = new_list;
  else
    list = new_list;

  return list;
}

slist *
slist_find(slist *list, slist_find_fn_t find_fn, void *user_data)
{
  while (list && !find_fn(list->data, user_data))
    list = list->next;

  return list;
}

int
slist_size(const slist *list)
{
  int i = 0;

  while (list)
  {
    i++;
    list = list->next;
  }

  return i;
}

slist *
slist_remove(slist *list, void *data, void (*free_func)(void *))
{
  slist *prev = NULL;
  slist *l = list;

  while (l && l->data != data)
  {
    prev = l;
    l = l->next;
  }

  if (!l)
    return list;

  if (prev)
    prev->next = l->next;
  else
    list = l->next;

  if (free_func)
    free_func(l->data);

  free(l);

  return list;
}
