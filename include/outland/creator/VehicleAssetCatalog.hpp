#pragma once
#ifdef OUTLAND_DEV_TOOLS
#include "outland/creator/CreatorAssetRegistry.hpp"
namespace outland::creator {
inline void register_vehicle_assets(CreatorAssetRegistry& registry) {
(void)registry.add({.id="vehicle_hatchback",.name="Classic hatchback",.category=CreatorAssetCategory::Vehicle,.model_path="assets/verda/vehicles/runtime/hatchback_preview.glb",.thumbnail_path="",.footprint={1.64F,4.2F,1.58F},.placement={},.default_scale=1,.tags={"vehicles","vehicle_definition:hatchback"}});
(void)registry.add({.id="vehicle_bus",.name="Verda city bus",.category=CreatorAssetCategory::Vehicle,.model_path="assets/verda/vehicles/runtime/bus_body.glb",.thumbnail_path="",.footprint={2.55F,12.28F,3.42F},.placement={},.default_scale=1,.tags={"vehicles","vehicle_definition:bus"}});
}
}
#endif
