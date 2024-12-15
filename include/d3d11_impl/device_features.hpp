#pragma once
#include <windows.h>
#include <d3d12.h>  // For D3D12 feature enums

namespace dxiided {

// Internal feature enums that mirror D3D12 but are independent
// These are used to query device capabilities in a D3D12-like manner
enum DXII_FEATURE {
    DXII_FEATURE_D3D12_OPTIONS = 0,   // Basic D3D12 feature options
    DXII_FEATURE_SHADER_MODEL = 7,    // Supported shader model
    DXII_FEATURE_OPTIONS1 = 8,        // Additional D3D12 options
    DXII_FEATURE_SHADER_CACHE = 19,   // Shader caching support
};

// Must match D3D12_FEATURE_DATA_D3D12_OPTIONS byte-for-byte
struct DXII_FEATURE_DATA_D3D12_OPTIONS {
    BOOL DoublePrecisionFloatShaderOps;   // Support for double-precision float operations in shaders
    BOOL OutputMergerLogicOp;             // Support for logic operations in output merger
    D3D12_SHADER_MIN_PRECISION_SUPPORT MinPrecisionSupport;             // Support for minimum precision in shaders
    D3D12_TILED_RESOURCES_TIER TiledResourcesTier;              // Level of support for tiled resources
    D3D12_RESOURCE_BINDING_TIER ResourceBindingTier;             // Resource binding tier
    BOOL PSSpecifiedStencilRefSupported;  // Support for pixel shader specified stencil ref
    BOOL TypedUAVLoadAdditionalFormats;   // Support for additional formats in typed UAV loads
    BOOL ROVsSupported;                   // Support for Rasterizer Ordered Views (ROVs)
    D3D12_CONSERVATIVE_RASTERIZATION_TIER ConservativeRasterizationTier;   // Level of support for conservative rasterization
    UINT64 MaxGPUVirtualAddressBitsPerResource; // Maximum GPU virtual address bits per resource
    BOOL StandardSwizzle64KBSupported;    // Support for 64KB standard swizzle
    D3D12_CROSS_NODE_SHARING_TIER CrossNodeSharingTier;            // Level of support for cross-node sharing
    BOOL CrossAdapterRowMajorTextureSupported; // Support for cross-adapter row-major textures
    BOOL VPAndRTArrayIndexFromAnyShaderFeedingRasterizerSupportedWithoutGSEmulation; // Support for VP and RT array index from any shader feeding rasterizer without GS emulation
    D3D12_RESOURCE_HEAP_TIER ResourceHeapTier;                // Resource heap tier
};

// Mirrors D3D12_FEATURE_DATA_SHADER_CACHE
// Contains information about shader caching support
struct DXII_FEATURE_DATA_SHADER_CACHE {
    UINT SupportFlags;  // Flags indicating level of shader cache support
};

// Mirrors D3D12_FEATURE_DATA_D3D12_OPTIONS1
// Contains information about additional D3D12 feature support
struct DXII_FEATURE_DATA_OPTIONS1 {
    BOOL WaveOps;                   // Support for wave operations
    UINT WaveLaneCountMin;          // Minimum number of lanes in a wave
    UINT WaveLaneCountMax;          // Maximum number of lanes in a wave
    UINT TotalLaneCount;            // Total number of lanes
    BOOL ExpandedComputeResourceStates; // Support for expanded compute resource states
    BOOL Int64ShaderOps;            // Support for 64-bit integer operations in shaders
};

// Mirrors D3D12_FEATURE_DATA_SHADER_MODEL
// Contains information about the highest supported shader model
struct DXII_FEATURE_DATA_SHADER_MODEL {
    UINT HighestShaderModel;  // Highest supported shader model
};

} // namespace dxiided
