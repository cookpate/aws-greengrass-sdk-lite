#ifndef GGL_OBJECT_ITER_H
#define GGL_OBJECT_ITER_H

#include <ggl/object.h>
#include <stdint.h>

typedef enum {
    LEVEL_DEFAULT,
    LEVEL_LIST,
    LEVEL_MAP,
} IterLevelState;

typedef struct {
    GglObject *obj[GGL_MAX_OBJECT_DEPTH];
    uint8_t state[GGL_MAX_OBJECT_DEPTH];
    uint8_t elem_index[GGL_MAX_OBJECT_DEPTH];
    uint8_t index;
} IterLevels;

#endif
