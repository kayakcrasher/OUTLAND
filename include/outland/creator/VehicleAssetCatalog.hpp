#pragma once
#ifdef OUTLAND_DEV_TOOLS
#include "outland/creator/CreatorAssetRegistry.hpp"
namespace outland::creator {
inline void register_vehicle_assets(CreatorAssetRegistry& registry) {
(void)registry.add({.id="vehicle_hatchback",.name="Classic hatchback",.category=CreatorAssetCategory::Vehicle,.model_path="assets/verda/vehicles/runtime/hatchback_preview.glb",.thumbnail_path="",.footprint={1.64F,4.2F,1.58F},.placement={},.default_scale=1,.tags={"vehicles","vehicle_definition:hatchback"}});
}
}
#endif
