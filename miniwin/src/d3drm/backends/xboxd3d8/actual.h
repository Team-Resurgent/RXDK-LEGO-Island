#pragma once

// The bridge between the miniwin renderer (renderer.cpp) and the original Xbox's
// Direct3D 8 (actual.cpp): miniwin's Windows-style headers and the Xbox SDK's
// cannot be included in the same translation unit.

#include "structs.h"

#include <SDL3/SDL.h>
#include <stdint.h>

typedef float Matrix4x4[4][4];

struct XB8_BridgeVector {
	float x, y, z;
};

struct XB8_BridgeSceneLight {
	FColor color;
	XB8_BridgeVector position;
	float positional;
	XB8_BridgeVector direction;
	float directional;
};

struct IDirect3DRMTexture;
struct MeshGroup;
struct D3DTexture;
struct D3DVertexBuffer;
struct D3DIndexBuffer;

struct XB8TextureCacheEntry {
	IDirect3DRMTexture* texture;
	uint32_t version;
	D3DTexture* d3dTexture;
	// The source image's size: texture coordinates are relative to it, whatever
	// size the (power-of-two) texture has.
	float width;
	float height;
	// A 2D image in a linear texture of the image's own size (see
	// XB8_CreateLinearTexture); its texture coordinates are in texels.
	bool linear;
};

struct XB8MeshCacheEntry {
	const MeshGroup* meshGroup;
	uint32_t version;
	bool flat;

	D3DVertexBuffer* vbo;
	uint32_t vertexCount;
	D3DIndexBuffer* ibo;
	uint32_t indexCount;
};

bool XB8_Initialize(void* device);
void XB8_Shutdown();
// Uploads an ARGB8888 surface whose sides are powers of two.
D3DTexture* XB8_UploadTexture(SDL_Surface* argb8888);
void XB8_ReleaseTexture(D3DTexture* texture);
// A linear (unswizzled) A8R8G8B8 texture for a 2D image: any size, written
// directly with no scaling or swizzling, addressed in texels and clamped.
D3DTexture* XB8_CreateLinearTexture(int width, int height);
// Level 0 for writing; waits for the GPU if it is still reading the texture.
void* XB8_LockTexture(D3DTexture* texture, int* pitch);
void XB8_UnlockTexture(D3DTexture* texture);
bool XB8_UploadMesh(
	const void* vertices,
	uint32_t vertexCount,
	const uint16_t* indices,
	uint32_t indexCount,
	XB8MeshCacheEntry& cache
);
void XB8_ReleaseMesh(XB8MeshCacheEntry& cache);
void XB8_BeginFrame(const Matrix4x4* projection, const XB8_BridgeSceneLight* lights, size_t lightCount);
void XB8_EnableTransparency();
void XB8_SubmitDraw(
	const XB8MeshCacheEntry& mesh,
	const Matrix4x4* modelViewMatrix,
	const Appearance& appearance,
	D3DTexture* texture
);
void XB8_Resize(int width, int height);
void XB8_Clear(float r, float g, float b);
void XB8_Flip();
// dst* are screen (back buffer) coordinates; texture may be null for a plain fill.
void XB8_Draw2DImage(
	D3DTexture* texture,
	float u1,
	float v1,
	float u2,
	float v2,
	float dstX1,
	float dstY1,
	float dstX2,
	float dstY2,
	const FColor& color
);
// Copies the back buffer into an XRGB8888 surface of the same size.
bool XB8_Download(SDL_Surface* target);
void XB8_SetDither(bool dither);
