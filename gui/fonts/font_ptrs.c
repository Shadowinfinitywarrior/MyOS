// Font initialization - includes all baked font headers and provides external symbols
// This file must be compiled separately to get external linkage for the font data

#include "ui.h"
#include "mono.h"
#include "ubold.h"
#include "blocks.h"

// UI Font
const uint16_t *ui_gw_ptr = UI_gw;
const uint16_t *ui_gh_ptr = UI_gh;
const int8_t *ui_xo_ptr = UI_xo;
const int8_t *ui_yo_ptr = UI_yo;
const uint16_t *ui_xa_ptr = UI_xa;
const uint32_t *ui_bo_ptr = UI_bo;
const uint8_t *ui_bitmap_ptr = UI_bitmap;
const uint32_t ui_bitmap_size = sizeof(UI_bitmap);

// Mono Font
const uint16_t *mono_gw_ptr = MONO_gw;
const uint16_t *mono_gh_ptr = MONO_gh;
const int8_t *mono_xo_ptr = MONO_xo;
const int8_t *mono_yo_ptr = MONO_yo;
const uint16_t *mono_xa_ptr = MONO_xa;
const uint32_t *mono_bo_ptr = MONO_bo;
const uint8_t *mono_bitmap_ptr = MONO_bitmap;
const uint32_t mono_bitmap_size = sizeof(MONO_bitmap);

// Bold Font
const uint16_t *ubold_gw_ptr = UBOLD_gw;
const uint16_t *ubold_gh_ptr = UBOLD_gh;
const int8_t *ubold_xo_ptr = UBOLD_xo;
const int8_t *ubold_yo_ptr = UBOLD_yo;
const uint16_t *ubold_xa_ptr = UBOLD_xa;
const uint32_t *ubold_bo_ptr = UBOLD_bo;
const uint8_t *ubold_bitmap_ptr = UBOLD_bitmap;
const uint32_t ubold_bitmap_size = sizeof(UBOLD_bitmap);

// Blocks Font
const uint16_t *blocks_gw_ptr = BLOCKS_gw;
const uint16_t *blocks_gh_ptr = BLOCKS_gh;
const int8_t *blocks_xo_ptr = BLOCKS_xo;
const int8_t *blocks_yo_ptr = BLOCKS_yo;
const uint16_t *blocks_xa_ptr = BLOCKS_xa;
const uint32_t *blocks_bo_ptr = BLOCKS_bo;
const uint8_t *blocks_bitmap_ptr = BLOCKS_bitmap;
const uint32_t blocks_bitmap_size = sizeof(BLOCKS_bitmap);