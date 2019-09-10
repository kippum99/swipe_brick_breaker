#include "list.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>


typedef struct list {
    size_t max_size;
    size_t length;
    FreeFunc free_elem;
    void **arr;
} List;


List *list_init(size_t initial_size, FreeFunc free_elem) {
    assert(initial_size != 0);

    List *l = malloc(sizeof(List));
    assert(l != NULL);

    void **arr = malloc(initial_size * sizeof(void *));
    assert(arr != NULL);

    l->max_size = initial_size;
    l->length = 0;
    l->free_elem = free_elem;
    l->arr = arr;
    return l;
}

void list_free(List *list) {
    if (list->free_elem != NULL) {
        for (size_t i = 0; i < list->length; i++) {
            list->free_elem(list->arr[i]);
        }
    }
    free(list->arr);
    free(list);
}

size_t list_size(List *list) {
    return list->length;
}

void *list_get(List *list, size_t index) {
    assert(index < list->length);
    return list->arr[index];
}

void list_set(List *list, size_t index, void *value) {
    assert(index < list->length);
    list->free_elem(list->arr[index]);
    list->arr[index] = value;
}

void *list_remove(List *list, size_t index) {
    assert(list->length > 0);
    assert(index < list->length);
    void *elem = list->arr[index];

    for (size_t i = index + 1; i < list->length; i++) {
        list->arr[i-1] = list->arr[i];
    }

    list->length--;

    return elem;
}

void list_add(List *lst, void *value) {
    lst->length += 1;
    if (lst->length > lst->max_size) {
        lst->max_size *= 2;
        lst->arr = realloc(lst->arr, lst->max_size * sizeof(void *));
        assert(lst->arr != NULL);
    }
    lst->arr[lst->length - 1] = value;
}
