#include "ggl/object_compare.h"
#include <float.h>
#include <ggl/buffer.h>
#include <ggl/log.h>
#include <ggl/map.h>
#include <ggl/object.h>
#include <ggl/object_iter.h>
#include <inttypes.h>
#include <math.h>

/// Prints info about all parents of the current subobject.
static void print_state(const IterLevels state[static 1]) {
    for (uint8_t i = state->index; i > 0; --i) {
        switch (state->state[i - 1]) {
        case LEVEL_LIST: {
            GGL_LOGE("In list (idx = %d).", (int) state->elem_index[i]);
        } break;
        case LEVEL_MAP: {
            GglMap map = ggl_obj_into_map(*state->obj[i]);
            GglBuffer key = ggl_kv_key(map.pairs[state->elem_index[i]]);
            GGL_LOGE("In map (key = \"%.*s\").", (int) key.len, key.data);
        } break;
        default:
            return;
        }
    }
}

static bool bool_eq(bool lhs, bool rhs) {
    if (lhs != rhs) {
        GGL_LOGE(
            "Bool comparison failed (%s != %s).",
            lhs ? " true" : "false",
            rhs ? " true" : "false"
        );
        return false;
    }
    return true;
}

static bool int_eq(int64_t lhs, int64_t rhs) {
    if (lhs != rhs) {
        GGL_LOGE(
            "Int comparison failed (%" PRIi64 " != %" PRIi64 ").", lhs, rhs
        );
        return false;
    }
    return true;
}

static bool float_eq(double lhs, double rhs) {
    if (isnan(lhs) != isnan(rhs)) {
        GGL_LOGE("NaN comparison failed (%g != %g)", lhs, rhs);
        return false;
    }
    double error = fabs(lhs - rhs);
    if (error <= DBL_EPSILON) {
        return true;
    }
    if (error <= (DBL_EPSILON * fmin(fabs(lhs), fabs(rhs)))) {
        return true;
    }
    GGL_LOGE("Float comparison failed (%g != %g).", lhs, rhs);
    return false;
}

static bool buf_eq(GglBuffer lhs, GglBuffer rhs) {
    if (!ggl_buffer_eq(lhs, rhs)) {
        GGL_LOGE(
            "Buffer comparison failed (\"%.*s\" != \"%.*s\").",
            (int) lhs.len,
            lhs.data,
            (int) rhs.len,
            rhs.data
        );
        return false;
    }
    return true;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
bool ggl_obj_eq(GglObject lhs, GglObject rhs) {
    IterLevels lhs_state;
    IterLevels rhs_state;

    lhs_state.index = 0;
    lhs_state.obj[0] = &lhs;
    lhs_state.state[0] = LEVEL_DEFAULT;
    lhs_state.elem_index[0] = 0;

    rhs_state.index = 0;
    rhs_state.obj[0] = &rhs;
    // lhs state array is used for both iter levels
    rhs_state.elem_index[0] = 0;

    uint8_t subobjects = 0;

    while (true) {
        GglObject *lhs_obj = lhs_state.obj[lhs_state.index];
        GglObject *rhs_obj = rhs_state.obj[rhs_state.index];

        IterLevelState cur_state = lhs_state.state[lhs_state.index];
        uint8_t lhs_index = lhs_state.elem_index[lhs_state.index];
        uint8_t rhs_index = rhs_state.elem_index[rhs_state.index];

        switch (cur_state) {
        case LEVEL_DEFAULT: {
            if (ggl_obj_type(*lhs_obj) != ggl_obj_type(*rhs_obj)) {
                print_state(&lhs_state);
                return false;
            }

            switch (ggl_obj_type(*lhs_obj)) {
            case GGL_TYPE_NULL:
                break;
            case GGL_TYPE_BOOLEAN:
                if (!bool_eq(
                        ggl_obj_into_bool(*lhs_obj), ggl_obj_into_bool(*rhs_obj)
                    )) {
                    print_state(&lhs_state);
                    return false;
                }
                break;
            case GGL_TYPE_I64:
                if (!int_eq(
                        ggl_obj_into_i64(*lhs_obj), ggl_obj_into_i64(*rhs_obj)
                    )) {
                    print_state(&lhs_state);
                    return false;
                }
                break;
            case GGL_TYPE_F64:
                if (!float_eq(
                        ggl_obj_into_f64(*lhs_obj), ggl_obj_into_f64(*rhs_obj)
                    )) {
                    return false;
                }
                break;
            case GGL_TYPE_BUF:
                if (!buf_eq(
                        ggl_obj_into_buf(*lhs_obj), ggl_obj_into_buf(*rhs_obj)
                    )) {
                    print_state(&lhs_state);
                    return false;
                }
                break;
            case GGL_TYPE_LIST: {
                GglList lhs_list = ggl_obj_into_list(*lhs_obj);
                if (lhs_list.len > GGL_MAX_OBJECT_SUBOBJECTS - subobjects) {
                    GGL_LOGE("Visited object's subobjects exceeds maximum.");
                    print_state(&lhs_state);
                    return false;
                }
                subobjects += (uint8_t) lhs_list.len;
                GglList rhs_list = ggl_obj_into_list(*rhs_obj);
                if (lhs_list.len != rhs_list.len) {
                    GGL_LOGE("List length mismatch.");
                    print_state(&lhs_state);
                    return false;
                }

                lhs_state.state[lhs_state.index] = LEVEL_LIST;
                continue;
            }
            case GGL_TYPE_MAP: {
                GglMap lhs_map = ggl_obj_into_map(*lhs_obj);
                if (lhs_map.len
                    > (GGL_MAX_OBJECT_SUBOBJECTS - subobjects) / 2) {
                    GGL_LOGE("Visited object's subobjects exceeds maximum.");
                    print_state(&lhs_state);
                    return false;
                }
                subobjects += (uint8_t) (lhs_map.len * 2);

                GglMap rhs_map = ggl_obj_into_map(*rhs_obj);
                if (lhs_map.len != rhs_map.len) {
                    GGL_LOGE("Map length mismatch.");
                    print_state(&lhs_state);
                    return false;
                }

                lhs_state.state[lhs_state.index] = LEVEL_MAP;
                continue;
            }
            }
        } break;
        case LEVEL_LIST: {
            GglList lhs_list = ggl_obj_into_list(*lhs_obj);
            if (lhs_index == lhs_list.len) {
                break;
            }
            GglList rhs_list = ggl_obj_into_list(*rhs_obj);

            if (lhs_state.index >= GGL_MAX_OBJECT_DEPTH - 1) {
                GGL_LOGE("Visited object's depth exceeds maximum.");
                print_state(&lhs_state);
                return false;
            }
            lhs_state.index += 1;
            rhs_state.index += 1;

            lhs_state.obj[lhs_state.index] = &lhs_list.items[lhs_index];
            lhs_state.state[lhs_state.index] = LEVEL_DEFAULT;
            lhs_state.elem_index[lhs_state.index] = 0;

            lhs_state.elem_index[lhs_state.index - 1] += 1;

            rhs_state.obj[rhs_state.index] = &rhs_list.items[rhs_index];
            rhs_state.elem_index[rhs_state.index] = 0;

            rhs_state.elem_index[rhs_state.index - 1] += 1;
            continue;
        }
        case LEVEL_MAP: {
            GglMap lhs_map = ggl_obj_into_map(*lhs_obj);
            if (lhs_index == lhs_map.len) {
                break;
            }
            GglMap rhs_map = ggl_obj_into_map(*rhs_obj);

            GglKV *kv = &lhs_map.pairs[lhs_index];

            // GglMap is unordered, so each rhs key must be found
            GglObject *rhs_val = NULL;
            GglBuffer key = ggl_kv_key(*kv);
            if (!ggl_map_get(rhs_map, key, &rhs_val)) {
                GGL_LOGE("Map key %.*s not found.", (int) key.len, key.data);
                print_state(&lhs_state);
                return false;
            }

            if (lhs_state.index >= GGL_MAX_OBJECT_DEPTH - 1) {
                GGL_LOGE("Visited object's depth exceeds maximum.");
                print_state(&lhs_state);
                return false;
            }
            lhs_state.index += 1;
            rhs_state.index += 1;

            lhs_state.obj[lhs_state.index] = ggl_kv_val(kv);
            lhs_state.state[lhs_state.index] = LEVEL_DEFAULT;
            lhs_state.elem_index[lhs_state.index] = 0;

            lhs_state.elem_index[lhs_state.index - 1] += 1;

            rhs_state.obj[rhs_state.index] = rhs_val;
            rhs_state.elem_index[rhs_state.index] = 0;

            rhs_state.elem_index[rhs_state.index - 1]
                = (uint8_t) (kv - rhs_map.pairs);

            continue;
        }
        }

        if (lhs_state.index == 0) {
            break;
        }

        lhs_state.index -= 1;
        rhs_state.index -= 1;
    }

    return true;
}
