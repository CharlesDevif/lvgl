/**
 * MIT License
 *
 * -----------------------------------------------------------------------------
 * Copyright (c) 2008-24 Think Silicon Single Member PC
 * -----------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next paragraph)
 * shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/**
 * @file lv_nema_gfx_path.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "../../core/lv_refr.h"

#if LV_USE_NEMA_GFX
#if LV_USE_NEMA_VG

#include "lv_nema_gfx_path.h"

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

static int data_point = 0;
static int seg_point  = 0;

lv_nema_gfx_path_t * lv_nema_gfx_path_create(void)
{
    LV_PROFILER_DRAW_BEGIN;
    lv_nema_gfx_path_t * nema_gfx_path = lv_malloc_zeroed(sizeof(lv_nema_gfx_path_t));
    LV_ASSERT_MALLOC(nema_gfx_path);
    nema_gfx_path->seg = NULL;
    nema_gfx_path->data = NULL;
    nema_gfx_path->seg_size = 0;
    nema_gfx_path->data_size = 0;
    nema_gfx_path->seg_cap = 0;
    nema_gfx_path->data_cap = 0;
    nema_gfx_path->failed = false;
    data_point = 0;
    seg_point  = 0;
    LV_PROFILER_DRAW_END;
    return nema_gfx_path;
}

/* Grow the buffers. The sizes LVGL announces at LV_EVENT_CREATE are zero --
 * lv_freetype_outline.c fills them in afterwards -- so nothing can be sized
 * up front. No LV_ASSERT_MALLOC here: a failure has to degrade, not halt. */
static bool path_reserve(lv_nema_gfx_path_t * path, uint32_t data, uint32_t seg)
{
    if(data > path->data_cap) {
        uint32_t cap = path->data_cap ? path->data_cap * 2 : 64;
        if(cap < data) cap = data;
        float * p = (float *) lv_realloc(path->data, cap * sizeof(float));
        if(p == NULL) return false;
        path->data = p;
        path->data_cap = cap;
    }
    if(seg > path->seg_cap) {
        uint32_t cap = path->seg_cap ? path->seg_cap * 2 : 32;
        if(cap < seg) cap = seg;
        uint8_t * p = (uint8_t *) lv_realloc(path->seg, cap * sizeof(uint8_t));
        if(p == NULL) return false;
        path->seg = p;
        path->seg_cap = cap;
    }
    return true;
}

/* Room for `n` more coordinates and one more command, or the path gives up.
 * Dropping just the point would leave commands and coordinates out of step,
 * and NemaVG does not reject a malformed path -- it never raises its
 * interrupt, and nema_wait_irq_cl() waits for ever. */
static bool path_room(lv_nema_gfx_path_t * path, uint32_t n)
{
    if(path->failed) return false;
    if(path_reserve(path, (uint32_t)data_point + n, (uint32_t)seg_point + 1)) return true;
    path->failed = true;
    return false;
}

void lv_nema_gfx_path_alloc(lv_nema_gfx_path_t * nema_gfx_path)
{
    LV_PROFILER_DRAW_BEGIN;
    nema_gfx_path->path = nema_vg_path_create();
    nema_gfx_path->paint = nema_vg_paint_create();
    /* Sizes are still zero here; treat whatever is claimed as a hint. */
    (void)path_reserve(nema_gfx_path, nema_gfx_path->data_size, nema_gfx_path->seg_size);
    LV_PROFILER_DRAW_END;
}

void lv_nema_gfx_path_destroy(lv_nema_gfx_path_t * nema_gfx_path)
{
    LV_PROFILER_DRAW_BEGIN;
    LV_ASSERT_NULL(nema_gfx_path);

    if(nema_gfx_path->path != NULL) {
        nema_vg_path_destroy(nema_gfx_path->path);
        nema_gfx_path->path = NULL;
    }

    if(nema_gfx_path->paint != NULL) {
        nema_vg_paint_destroy(nema_gfx_path->paint);
        nema_gfx_path->paint = NULL;
    }

    if(nema_gfx_path->data != NULL) {
        lv_free(nema_gfx_path->data);
        nema_gfx_path->data = NULL;
    }
    if(nema_gfx_path->seg != NULL) {
        lv_free(nema_gfx_path->seg);
        nema_gfx_path->seg = NULL;
    }
    lv_free(nema_gfx_path);
    LV_PROFILER_DRAW_END;
}

void lv_nema_gfx_path_move_to(lv_nema_gfx_path_t * path, float x, float y)
{
    LV_ASSERT_NULL(path);
    if(!path_room(path, 2)) return;
    path->seg[seg_point++] = NEMA_VG_PRIM_MOVE;
    path->data[data_point++] = x;
    path->data[data_point++] = y;
}

void lv_nema_gfx_path_line_to(lv_nema_gfx_path_t * path, float x, float y)
{
    LV_ASSERT_NULL(path);
    if(!path_room(path, 2)) return;
    path->seg[seg_point++] = NEMA_VG_PRIM_LINE;
    path->data[data_point++] = x;
    path->data[data_point++] = y;

}

void lv_nema_gfx_path_quad_to(lv_nema_gfx_path_t * path, float cx, float cy, float x, float y)
{
    LV_ASSERT_NULL(path);
    if(!path_room(path, 4)) return;
    path->seg[seg_point++] = NEMA_VG_PRIM_BEZIER_QUAD;
    path->data[data_point++] = cx;
    path->data[data_point++] = cy;
    path->data[data_point++] = x;
    path->data[data_point++] = y;
}

void lv_nema_gfx_path_cubic_to(lv_nema_gfx_path_t * path, float cx1, float cy1, float cx2, float cy2, float x, float y)
{
    LV_ASSERT_NULL(path);
    if(!path_room(path, 6)) return;
    path->seg[seg_point++] = NEMA_VG_PRIM_BEZIER_CUBIC;
    path->data[data_point++] = cx1;
    path->data[data_point++] = cy1;
    path->data[data_point++] = cx2;
    path->data[data_point++] = cy2;
    path->data[data_point++] = x;
    path->data[data_point++] = y;
}

void lv_nema_gfx_path_end(lv_nema_gfx_path_t * path)
{
    /* data_size and seg_size are what nema_vg_path_set_shape() is given as
     * counts, so they must hold what was written, not what was reserved. */
    if(path != NULL) {
        path->data_size = path->failed ? 0u : (uint32_t)data_point;
        path->seg_size  = path->failed ? 0u : (uint32_t)seg_point;
    }
    seg_point = 0;
    data_point = 0;
}

#endif  /*LV_USE_NEMA_VG*/
#endif  /*LV_USE_NEMA_GFX*/
