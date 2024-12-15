# DXIIDED

## Goal

DXIIDED aims to create a downgrade layer that allows DirectX 12 games to run on systems that only support DirectX 11. This project is an ambitious attempt to bridge the gap between newer games and older hardware.

## How It Works

- Implements a `d3d12.dll` that intercepts DirectX 12 API calls
- Downgrades these calls into equivalent DirectX 11 operations
- Attempts to maintain a balance between compatibility and performance

## Target Users

- Gamers with DirectX 11-only hardware hoping to run DirectX 12 games
- Windows systems with older GPUs seeking extended gaming possibilities

## Main Challenges

### Compatibility

- Accurate downgrading of D3D12 features to D3D11 equivalents
- Addressing significant feature gaps between D3D11 and D3D12
- Ensuring broad game compatibility and stability

### Performance

- Minimizing translation overhead
- Optimizing resource management for older hardware
- Implementing efficient command list translation

## Current Status

DXIIDED is in experimental stages. While we're working hard to make this a reality, please be aware that this project might not come to fruition. The technical challenges are significant, and success is not guaranteed.

## Building

1. Install MinGW-w64 and Windows headers
2. Run `make` in the project root

## Contributing

Interested in contributing? We welcome your expertise, but please understand the experimental nature of this project. Your efforts might help push boundaries, but there's no guarantee of a fully functional end product.

## Disclaimer

This software is highly experimental. Use at your own risk. We are not responsible for any hardware damage, data loss, or disappointment resulting from attempts to use this downgrade layer.

Remember, DXIIDED is an ambitious experiment - it might revolutionize your gaming experience, prove that DirectX 12 is indeed "DED", or it might not work at all.
