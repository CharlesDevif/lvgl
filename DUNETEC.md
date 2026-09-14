# Branch `dunetec/9.5.0`

LVGL **v9.5.0** plus the fixes the Dunetec project isolated on an
STM32U5G9J-DK2 (NeoChrom GPU2D / NemaVG) and an STM32F769I-DISCO.

The base is the `v9.5.0` tag, not `master`: the project pins a release, and a
fix is judged against the version you ship. `amont` stays configured to track
lvgl/lvgl.

```
git remote -v
  origin  https://github.com/CharlesDevif/lvgl.git   (this fork)
  amont   https://github.com/lvgl/lvgl.git           (upstream)
```

## What the branch adds

| # | Fix | Upstream status |
|---|-----|-----------------|
| 1 | `nema_gfx`: set the fill rule before a gradient background | **PR #10688** open |
| 2 | `nema_gfx`: resolve percentage units in radial gradients | to submit |
| 3 | `nema_gfx`: draw conical gradients instead of black | to submit |
| 4 | `nema_gfx`: clamp the stop count with `LV_MIN`, not `LV_MAX` | to submit |
| 5 | `draw`: wait for the GPU even without an OS | to submit |
| 6 | `draw`: do not freeze when a layer buffer cannot be allocated | ported from `amont/master` |

### 1 — Fill rule

`lv_draw_nema_gfx_vector.c` leaves `NEMA_VG_STROKE` set after every path that
has a stroke, and never restores it. `lv_draw_nema_gfx_fill.c` never sets the
rule at all. A stroke-only vector icon therefore left the GPU in stroke mode,
and the next gradient background came out outlined instead of filled. Global
state that reads as local.

### 2 — Percentage units in radial gradients

The linear branch resolves its coordinates with `lv_pct_to_px()`, and
`lv_draw_sw_grad_radial_setup()` does the same for the radial ones. The radial
branch of the GPU driver used them raw. `LV_GRAD_CENTER` expands to
`LV_PCT(50)`, the integer 536870962, so the documented way of centring a
radial gradient was exactly the one that broke it. The radius was also taken
as the horizontal component of the extent vector rather than its length.

### 3 — Conical gradients

There was no branch for `LV_GRAD_DIR_CONICAL` at all: the paint object kept
the state left by `nema_vg_paint_clear()` and the background was drawn black,
with no warning anywhere. NemaVG exposes `NEMA_VG_PAINT_GRAD_CONICAL`, so the
centre can be honoured. The API takes no angle range, so a gradient defined
over part of a turn is rendered over the whole one. That is written down in
the code.

### 4 — Stop count

`lv_nemagfx_grad_set()` sized its loop with `LV_MAX(stops_count,
LV_GRADIENT_MAX_STOPS)` while the arrays hold `LV_GRADIENT_MAX_STOPS` entries.
Past that limit the loop writes beyond the end of two stack arrays. Below it,
the loop still reads the uninitialised tail of the stop array and hands it to
the GPU. `lv_draw_nema_gfx_fill.c` clamps the same value with `LV_MIN`.

The function has no caller inside the tree, so the overflow is not reachable
today, but the symbol is exported.

### 5 — Waiting for the GPU without an OS

The whole body of `lv_draw_wait_for_finish()` sat behind `#if LV_USE_OS`. That
is correct for the software draw unit, which draws inside its dispatch
callback and has nothing outstanding. It is not correct for an accelerator:
`lv_draw_nema_gfx` registers a `wait_for_finish_cb` that submits the command
list and waits on the GPU, and that callback was simply never called without
an RTOS.

Observed consequence: the flush callback arms the LTDC buffer swap while the
NeoChrom may still be writing into the buffer the controller is about to scan.
The race is short — the vertical blanking usually covers it — which is exactly
what makes it unpleasant to chase.

Draw units that register no callback are unaffected; the software renderer
registers none.

### 6 — Freeze when a layer buffer cannot be allocated

When `lv_draw_layer_alloc_buf()` returns NULL the draw units returned
`LV_DRAW_UNIT_IDLE` and left the task in its previous state. Nothing ever
moved that task forward: never dispatched again with a buffer, never removed.
The layer therefore never reported itself complete, and neither did the frame.

With an OS, the caller blocks on the draw semaphore forever — process alive,
0% CPU, nothing rendered. Without one, the refresh loop spins on the same
layer. Either way the UI is frozen, and the only visible cause is that a layer
happened to be larger than `LV_MEM_SIZE` allows: an object with a transform or
a partial opacity is enough.

The fix adds `LV_DRAW_TASK_STATE_FAILED`, sets it wherever a layer allocation
fails, and removes failed tasks alongside finished ones with an error logged.
The frame then completes with that task missing — visibly degraded, therefore
recoverable — instead of not completing at all.

Measured: a 300 × 300 transformed object with `LV_MEM_SIZE` at 48 kB hung the
test program for 2 min 49 s at 0% CPU; after the fix it returns immediately,
logging the failed task.

Ported from `amont/master`, which carries the same change. Absent from the
v9.5.0 release this branch is based on.

## Measured on hardware

STM32U5G9J-DK2, Dunetec demo, after the six fixes:

```
GPU screen (perspective cube)     60.6 fps   0 FIFO underrun   no fault
7 enter/leave round trips         60.3 fps   command list recycled
```

## What is still open

`lv_draw_image.c`, "child layer" branch: `layer->_clip_area` is overwritten
with the image area and never restored. Everything drawn afterwards in the
same layer should end up clipped to it. **Unconfirmed**: the bench built to
demonstrate it never reaches the rendering stage, the layer exceeding
`LV_MEM_SIZE` (which is what led to fix 6). To revisit with a smaller layer.

Upstream issue #9778 — SVG icons said to vanish under a parent's
`transform_scale` — **does not reproduce** here. Ink measured in pixels on an
identical path:

```
no parent                    1412        parent transform_scale 200    320
parent, no transform         1412        parent transform_scale 128    144
parent transform_scale 256   1412
```

The icon shrinks, it does not vanish. No speculative fix is carried here for a
defect that cannot be observed.

## Sending a fix upstream

```
git checkout -b fix/<topic> amont/master
git cherry-pick <commit from this branch>
```

Commit messages are written in English and ready for a pull request.
