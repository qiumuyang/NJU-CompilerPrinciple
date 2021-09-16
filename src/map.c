#include "map.h"

map_t *get(root_t *root, const char *str) {
    rb_node_t *node = root->rb_node;
    while (node) {
        map_t *data = container_of(node, map_t, node);

        // compare between the key with the keys in map
        int cmp = strcmp(str, data->key);
        if (cmp < 0) {
            node = node->rb_left;
        } else if (cmp > 0) {
            node = node->rb_right;
        } else {
            return data;
        }
    }
    return NULL;
}

int put(root_t *root, const char *key, void *val) {
    map_t *data = (map_t *)malloc(sizeof(map_t));
    data->key = (char *)malloc((strlen(key) + 1) * sizeof(char));
    strcpy(data->key, key);
    data->val = val;

    rb_node_t **new_node = &(root->rb_node), *parent = NULL;
    while (*new_node) {
        map_t *this_node = container_of(*new_node, map_t, node);
        int result = strcmp(key, this_node->key);
        parent = *new_node;

        if (result < 0) {
            new_node = &((*new_node)->rb_left);
        } else if (result > 0) {
            new_node = &((*new_node)->rb_right);
        } else {
            this_node->val = val;
            free(data);
            return 0;
        }
    }

    rb_link_node(&data->node, parent, new_node);
    rb_insert_color(&data->node, root);

    return 1;
}

map_t *map_first(root_t *tree) {
    rb_node_t *node = rb_first(tree);
    return (rb_entry(node, map_t, node));
}

map_t *map_next(rb_node_t *node) {
    rb_node_t *next = rb_next(node);
    return rb_entry(next, map_t, node);
}

void freeMap(root_t *tree, void (*delval)(void *)) {
    // pass in a function pointer to free [value] of node
    for (map_t *nodeFree = map_first(tree); nodeFree;
         nodeFree = map_first(tree)) {
        if (nodeFree) {
            rb_erase(&nodeFree->node, tree);
            if (nodeFree) {
                // free single node
                if (nodeFree->key) {
                    free(nodeFree->key);
                    if (delval != NULL) {
                        delval(nodeFree->val);
                    }
                    nodeFree->key = NULL;
                    nodeFree->val = NULL;
                }
                free(nodeFree);
                nodeFree = NULL;
            }
        }
    }
}