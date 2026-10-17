#pragma once
#ifdef OUTLAND_DEV_TOOLS
#include "outland/creator/CreatorAssetRegistry.hpp"
namespace outland::creator {
inline void register_vehicle_assets(CreatorAssetRegistry& registry) {
(void)registry.add({.id="vehicle_hatchback",.name="Classic hatchback",.category=CreatorAssetCategory::Vehicle,.model_path="assets/verda/vehicles/runtime/hatchback_preview.glb",.thumbnail_path="",.footprint={1.64F,4.2F,1.58F},.placement={},.default_scale=1,.tags={"vehicles","vehicle_definition:hatchback"}});
(void)registry.add({.id="vehicle_bus",.name="Verda city bus",.category=CreatorAssetCategory::Vehicle,.model_path="assets/verda/vehicles/runtime/bus_body.glb",.thumbnail_path="",.footprint={2.55F,12.28F,3.42F},.placement={},.default_scale=1,.tags={"vehicles","vehicle_definition:bus"}});
(void)registry.add({.id="vehicle_truck",.name="Verda pickup truck",.category=CreatorAssetCategory::Vehicle,.model_path="assets/verda/vehicles/runtime/truck_body.glb",.thumbnail_path="",.footprint={1.95F,5.51F,1.96F},.placement={},.default_scale=1,.tags={"vehicles","vehicle_definition:truck"}});
(void)registry.add({.id="vehicle_police",.name="Policio patrol car",.category=CreatorAssetCategory::Vehicle,.model_path="assets/verda/vehicles/runtime/police_body.glb",.thumbnail_path="",.footprint={1.84F,4.8F,1.52F},.placement={},.default_scale=1,.tags={"vehicles","vehicle_definition:police"}});
(void)registry.add({.id="vehicle_interceptor",.name="Policio Interceptor",.category=CreatorAssetCategory::Vehicle,.model_path="assets/verda/vehicles/runtime/interceptor_body.glb",.thumbnail_path="",.footprint={1.9F,5.0F,1.44F},.placement={},.default_scale=1,.tags={"vehicles","vehicle_definition:interceptor"}});
(void)registry.add({.id="vehicle_ambulance",.name="Hospitalo ambulance",.category=CreatorAssetCategory::Vehicle,.model_path="assets/verda/vehicles/runtime/ambulance_body.glb",.thumbnail_path="",.footprint={2.2F,6.1F,2.66F},.placement={},.default_scale=1,.tags={"vehicles","vehicle_definition:ambulance"}});
(void)registry.add({.id="vehicle_sxs",.name="Vakero side-by-side",.category=CreatorAssetCategory::Vehicle,.model_path="assets/verda/vehicles/runtime/sxs_body.glb",.thumbnail_path="",.footprint={1.68F,3.12F,1.88F},.placement={},.default_scale=1,.tags={"vehicles","vehicle_definition:sxs"}});
(void)registry.add({.id="vehicle_semi",.name="Verda big rig tractor",.category=CreatorAssetCategory::Vehicle,.model_path="assets/verda/vehicles/runtime/semi_body.glb",.thumbnail_path="",.footprint={2.5F,7.41F,4.32F},.placement={},.default_scale=1,.tags={"vehicles","vehicle_definition:semi"}});
(void)registry.add({.id="vehicle_green_hand_gt",.name="Verda Mano GT",.category=CreatorAssetCategory::Vehicle,.model_path="assets/verda/vehicles/runtime/green_hand_gt_body.glb",.thumbnail_path="",.footprint={1.94F,4.6F,1.22F},.placement={},.default_scale=1,.tags={"vehicles","vehicle_definition:green_hand_gt"}});
}
}
#endif
