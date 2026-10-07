// Font symbol exports for Rust GUI
// This file re-exports the static font data from the baked font headers

#include "fonts/ui.h"
#include "fonts/mono.h"
#include "fonts/ubold.h"
#include "fonts/blocks.h"

// Re-export font data with external linkage
const uint16_t UI_gw_export[] = UI_gw;
const uint16_t UI_gh_export[] = UI_gh;
const int8_t UI_xo_export[] = UI_xo;
const int8_t UI_yo_export[] = UI_yo;
const uint16_t UI_xa_export[] = UI_xa;
const uint32_t UI_bo_export[] = UI_bo;
const uint8_t UI_bitmap_export[] = UI_bitmap;

const uint16_t MONO_gw_export[] = MONO_gw;
const uint16_t MONO_gh_export[] = MONO_gh;
const int8_t MONO_xo_export[] = MONO_xo;
const int8_t MONO_yo_export[] = MONO_yo;
const uint16_t MONO_xa_export[] = MONO_xa;
const uint32_t MONO_bo_export[] = MONO_bo;
const uint8_t MONO_bitmap_export[] = MONO_bitmap;

const uint16_t UBOLD_gw_export[] = UBOLD_gw;
const uint16_t UBOLD_gh_export[] = UBOLD_gh;
const int8_t UBOLD_xo_export[] = UBOLD_xo;
const int8_t UBOLD_yo_export[] = UBOLD_yo;
const uint16_t UBOLD_xa_export[] = UBOLD_xa;
const uint32_t UBOLD_bo_export[] = UBOLD_bo;
const uint8_t UBOLD_bitmap_export[] = UBOLD_bitmap;

const uint16_t BLOCKS_gw_export[] = BLOCKS_gw;
const uint16_t BLOCKS_gh_export[] = BLOCKS_gh;
const int8_t BLOCKS_xo_export[] = BLOCKS_xo;
const int8_t BLOCKS_yo_export[] = BLOCKS_yo;
const uint16_t BLOCKS_xa_export[] = BLOCKS_xa;
const uint32_t BLOCKS_bo_export[] = BLOCKS_bo;
const uint8_t BLOCKS_bitmap_export[] = BLOCKS_bitmap;

// Sizes
const uint32_t UI_bitmap_size = sizeof(UI_bitmap);
const uint32_t MONO_bitmap_size = sizeof(MONO_bitmap);
const uint32_t UBOLD_bitmap_size = sizeof(UBOLD_bitmap);
const uint32_t BLOCKS_bitmap_size = sizeof(BLOCKS_bitmap);