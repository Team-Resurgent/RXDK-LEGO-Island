// Direct3D 8 on the original Xbox. This file cannot include any miniwin headers.
//
// The fixed-function pipeline as the OpenGL 1.1 backend uses it: lights in eye
// space (the view transform is the identity and each draw's world transform is
// its model-view matrix), the material's ambient and diffuse from the
// appearance colour, white specular when the appearance is shiny.

#include "actual.h"

#include "structs.h"

#include <cstring>
// xgraphics.h needs the types xtl.h declares, as with the XDK.
#include <xtl.h>

#include <xgraphics.h>

static D3DDevice* g_device;
static int g_width;
static int g_height;
static bool g_inScene;
static bool g_dirty;

static const DWORD kMeshFVF = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1;

struct XB8_ScreenVertex {
	float x, y, z, rhw;
	D3DCOLOR color;
	float u, v;
};
static const DWORD kScreenFVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;

static void StartScene()
{
	if (!g_inScene) {
		IDirect3DDevice8_BeginScene(g_device);
		g_inScene = true;
	}
	g_dirty = true;
}

static D3DCOLOR ToD3DColor(float r, float g, float b, float a)
{
	auto c = [](float v) { return (DWORD) (SDL_clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
	return D3DCOLOR_ARGB(c(a), c(r), c(g), c(b));
}

bool XB8_Initialize(void* device)
{
	// The SDL video driver owns the device (the console has one); its back
	// buffer is the display mode, with a 24-bit depth buffer.
	g_device = static_cast<D3DDevice*>(device);
	if (!g_device) {
		return false;
	}
	IDirect3DDevice8_SetRenderState(g_device, D3DRS_CULLMODE, D3DCULL_CCW);
	return true;
}

void XB8_Shutdown()
{
	if (g_device && g_inScene) {
		IDirect3DDevice8_EndScene(g_device);
	}
	g_inScene = false;
	g_device = nullptr;
}

D3DTexture* XB8_UploadTexture(SDL_Surface* surface)
{
	D3DTexture* texture = nullptr;
	if (FAILED(IDirect3DDevice8_CreateTexture(
			g_device,
			surface->w,
			surface->h,
			1,
			0,
			D3DFMT_A8R8G8B8,
			D3DPOOL_MANAGED,
			&texture
		))) {
		return nullptr;
	}

	// A8R8G8B8 textures are swizzled on the Xbox.
	D3DLOCKED_RECT locked;
	if (FAILED(IDirect3DTexture8_LockRect(texture, 0, &locked, nullptr, 0))) {
		IDirect3DTexture8_Release(texture);
		return nullptr;
	}
	XGSwizzleRect(surface->pixels, surface->pitch, nullptr, locked.pBits, surface->w, surface->h, nullptr, 4);
	IDirect3DTexture8_UnlockRect(texture, 0);
	return texture;
}

D3DTexture* XB8_CreateLinearTexture(int width, int height)
{
	D3DTexture* texture = nullptr;
	if (FAILED(IDirect3DDevice8_CreateTexture(
			g_device,
			width,
			height,
			1,
			0,
			D3DFMT_LIN_A8R8G8B8,
			D3DPOOL_MANAGED,
			&texture
		))) {
		return nullptr;
	}
	return texture;
}

void* XB8_LockTexture(D3DTexture* texture, int* pitch)
{
	D3DLOCKED_RECT locked;
	if (FAILED(IDirect3DTexture8_LockRect(texture, 0, &locked, nullptr, 0))) {
		return nullptr;
	}
	*pitch = locked.Pitch;
	return locked.pBits;
}

void XB8_UnlockTexture(D3DTexture* texture)
{
	IDirect3DTexture8_UnlockRect(texture, 0);
}

void XB8_ReleaseTexture(D3DTexture* texture)
{
	if (texture) {
		IDirect3DTexture8_Release(texture);
	}
}

bool XB8_UploadMesh(
	const void* vertices,
	uint32_t vertexCount,
	const uint16_t* indices,
	uint32_t indexCount,
	XB8MeshCacheEntry& cache
)
{
	cache.vbo = nullptr;
	cache.ibo = nullptr;
	cache.vertexCount = vertexCount;
	cache.indexCount = indexCount;
	if (vertexCount == 0 || indexCount == 0) {
		return true;
	}

	const UINT vertexSize = vertexCount * 32; // D3DRMVERTEX: position, normal, uv
	const UINT indexSize = indexCount * sizeof(uint16_t);
	if (FAILED(IDirect3DDevice8_CreateVertexBuffer(g_device, vertexSize, 0, kMeshFVF, D3DPOOL_MANAGED, &cache.vbo))) {
		cache.vbo = nullptr;
		return false;
	}
	BYTE* data;
	IDirect3DVertexBuffer8_Lock(cache.vbo, 0, vertexSize, &data, 0);
	std::memcpy(data, vertices, vertexSize);
	IDirect3DVertexBuffer8_Unlock(cache.vbo);

	if (FAILED(
			IDirect3DDevice8_CreateIndexBuffer(g_device, indexSize, 0, D3DFMT_INDEX16, D3DPOOL_MANAGED, &cache.ibo)
		)) {
		IDirect3DVertexBuffer8_Release(cache.vbo);
		cache.vbo = nullptr;
		cache.ibo = nullptr;
		return false;
	}
	IDirect3DIndexBuffer8_Lock(cache.ibo, 0, indexSize, &data, 0);
	std::memcpy(data, indices, indexSize);
	IDirect3DIndexBuffer8_Unlock(cache.ibo);
	return true;
}

void XB8_ReleaseMesh(XB8MeshCacheEntry& cache)
{
	if (cache.vbo) {
		IDirect3DVertexBuffer8_Release(cache.vbo);
		cache.vbo = nullptr;
	}
	if (cache.ibo) {
		IDirect3DIndexBuffer8_Release(cache.ibo);
		cache.ibo = nullptr;
	}
}

static void SetMatrix(D3DTRANSFORMSTATETYPE state, const Matrix4x4* m)
{
	// miniwin's matrices are Direct3D's row-vector, row-major layout.
	IDirect3DDevice8_SetTransform(g_device, state, reinterpret_cast<const D3DMATRIX*>(m));
}

void XB8_BeginFrame(const Matrix4x4* projection, const XB8_BridgeSceneLight* lights, size_t lightCount)
{
	StartScene();

	IDirect3DDevice8_SetRenderState(g_device, D3DRS_ALPHABLENDENABLE, FALSE);
	IDirect3DDevice8_SetRenderState(g_device, D3DRS_ZENABLE, D3DZB_TRUE);
	IDirect3DDevice8_SetRenderState(g_device, D3DRS_ZWRITEENABLE, TRUE);
	IDirect3DDevice8_SetRenderState(g_device, D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
	IDirect3DDevice8_SetRenderState(g_device, D3DRS_LIGHTING, TRUE);
	IDirect3DDevice8_SetRenderState(g_device, D3DRS_LOCALVIEWER, TRUE);
	IDirect3DDevice8_SetRenderState(g_device, D3DRS_NORMALIZENORMALS, TRUE);
	IDirect3DDevice8_SetRenderState(g_device, D3DRS_CULLMODE, D3DCULL_CCW);

	SetMatrix(D3DTS_PROJECTION, projection);
	D3DMATRIX identity = {};
	identity._11 = identity._22 = identity._33 = identity._44 = 1.0f;
	IDirect3DDevice8_SetTransform(g_device, D3DTS_VIEW, &identity);

	// Lights are in eye space, as is the world transform of every draw.
	float ambientR = 0, ambientG = 0, ambientB = 0;
	DWORD index = 0;
	for (size_t i = 0; i < lightCount && index < 8; ++i) {
		const XB8_BridgeSceneLight& l = lights[i];
		const FColor& c = l.color;
		if (l.positional == 0.f && l.directional == 0.f) {
			ambientR += c.r;
			ambientG += c.g;
			ambientB += c.b;
			continue;
		}
		D3DLIGHT8 light = {};
		light.Diffuse = {c.r, c.g, c.b, c.a};
		if (l.directional == 1.f) {
			light.Type = D3DLIGHT_DIRECTIONAL;
			light.Direction = {l.direction.x, l.direction.y, l.direction.z};
			light.Specular = light.Diffuse;
		}
		else {
			light.Type = D3DLIGHT_POINT;
			light.Position = {l.position.x, l.position.y, l.position.z};
			light.Range = 1.0e6f;
			light.Attenuation0 = 1.0f;
		}
		IDirect3DDevice8_SetLight(g_device, index, &light);
		IDirect3DDevice8_LightEnable(g_device, index, TRUE);
		++index;
	}
	for (; index < 8; ++index) {
		IDirect3DDevice8_LightEnable(g_device, index, FALSE);
	}
	IDirect3DDevice8_SetRenderState(g_device, D3DRS_AMBIENT, ToD3DColor(ambientR, ambientG, ambientB, 1.0f));

	IDirect3DDevice8_SetVertexShader(g_device, kMeshFVF);
}

void XB8_EnableTransparency()
{
	IDirect3DDevice8_SetRenderState(g_device, D3DRS_ALPHABLENDENABLE, TRUE);
	IDirect3DDevice8_SetRenderState(g_device, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	IDirect3DDevice8_SetRenderState(g_device, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	IDirect3DDevice8_SetRenderState(g_device, D3DRS_ZWRITEENABLE, FALSE);
}

static void SetTextureStage(D3DTexture* texture, bool clamp)
{
	IDirect3DDevice8_SetTexture(g_device, 0, texture);
	if (texture) {
		IDirect3DDevice8_SetTextureStageState(g_device, 0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		IDirect3DDevice8_SetTextureStageState(g_device, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		IDirect3DDevice8_SetTextureStageState(g_device, 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		IDirect3DDevice8_SetTextureStageState(g_device, 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		IDirect3DDevice8_SetTextureStageState(g_device, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		IDirect3DDevice8_SetTextureStageState(g_device, 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
		const DWORD address = clamp ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP;
		IDirect3DDevice8_SetTextureStageState(g_device, 0, D3DTSS_ADDRESSU, address);
		IDirect3DDevice8_SetTextureStageState(g_device, 0, D3DTSS_ADDRESSV, address);
		IDirect3DDevice8_SetTextureStageState(g_device, 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
		IDirect3DDevice8_SetTextureStageState(g_device, 0, D3DTSS_MAGFILTER, clamp ? D3DTEXF_POINT : D3DTEXF_LINEAR);
		IDirect3DDevice8_SetTextureStageState(g_device, 0, D3DTSS_MIPFILTER, D3DTEXF_NONE);
	}
	else {
		IDirect3DDevice8_SetTextureStageState(g_device, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
		IDirect3DDevice8_SetTextureStageState(g_device, 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
		IDirect3DDevice8_SetTextureStageState(g_device, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
		IDirect3DDevice8_SetTextureStageState(g_device, 0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
	}
	IDirect3DDevice8_SetTextureStageState(g_device, 1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	IDirect3DDevice8_SetTextureStageState(g_device, 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
}

void XB8_SubmitDraw(
	const XB8MeshCacheEntry& mesh,
	const Matrix4x4* modelViewMatrix,
	const Appearance& appearance,
	D3DTexture* texture
)
{
	if (!mesh.vbo || !mesh.ibo) {
		return;
	}
	StartScene();

	SetMatrix(D3DTS_WORLD, modelViewMatrix);

	D3DMATERIAL8 material = {};
	material.Diffuse = {
		appearance.color.r / 255.0f,
		appearance.color.g / 255.0f,
		appearance.color.b / 255.0f,
		appearance.color.a / 255.0f
	};
	material.Ambient = material.Diffuse;
	if (appearance.shininess != 0.0f) {
		material.Specular = {1.0f, 1.0f, 1.0f, 1.0f};
		material.Power = appearance.shininess;
		IDirect3DDevice8_SetRenderState(g_device, D3DRS_SPECULARENABLE, TRUE);
	}
	else {
		IDirect3DDevice8_SetRenderState(g_device, D3DRS_SPECULARENABLE, FALSE);
	}
	IDirect3DDevice8_SetMaterial(g_device, &material);
	IDirect3DDevice8_SetRenderState(g_device, D3DRS_SHADEMODE, mesh.flat ? D3DSHADE_FLAT : D3DSHADE_GOURAUD);

	SetTextureStage(texture, false);

	IDirect3DDevice8_SetVertexShader(g_device, kMeshFVF);
	IDirect3DDevice8_SetStreamSource(g_device, 0, mesh.vbo, 32);
	IDirect3DDevice8_SetIndices(g_device, mesh.ibo, 0);
	IDirect3DDevice8_DrawIndexedPrimitive(g_device, D3DPT_TRIANGLELIST, 0, mesh.vertexCount, 0, mesh.indexCount / 3);
}

void XB8_Resize(int width, int height)
{
	g_width = width;
	g_height = height;
	D3DVIEWPORT8 viewport = {0, 0, (DWORD) width, (DWORD) height, 0.0f, 1.0f};
	IDirect3DDevice8_SetViewport(g_device, &viewport);
}

void XB8_Clear(float r, float g, float b)
{
	StartScene();
	IDirect3DDevice8_SetRenderState(g_device, D3DRS_ZENABLE, D3DZB_TRUE);
	IDirect3DDevice8_SetRenderState(g_device, D3DRS_ZWRITEENABLE, TRUE);
	IDirect3DDevice8_Clear(
		g_device,
		0,
		nullptr,
		D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
		ToD3DColor(r, g, b, 1.0f),
		1.0f,
		0
	);
}

void XB8_Flip()
{
	if (!g_dirty) {
		return;
	}
	if (g_inScene) {
		IDirect3DDevice8_EndScene(g_device);
		g_inScene = false;
	}
	HRESULT hr = IDirect3DDevice8_Present(g_device, nullptr, nullptr, nullptr, nullptr);
	if (FAILED(hr)) {
		SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Present failed: 0x%08lx", (unsigned long) hr);
	}
	g_dirty = false;
}

void XB8_Draw2DImage(
	D3DTexture* texture,
	float u1,
	float v1,
	float u2,
	float v2,
	float x1,
	float y1,
	float x2,
	float y2,
	const FColor& color
)
{
	StartScene();

	IDirect3DDevice8_SetRenderState(g_device, D3DRS_ZENABLE, D3DZB_FALSE);
	IDirect3DDevice8_SetRenderState(g_device, D3DRS_ZWRITEENABLE, FALSE);
	IDirect3DDevice8_SetRenderState(g_device, D3DRS_LIGHTING, FALSE);
	IDirect3DDevice8_SetRenderState(g_device, D3DRS_SHADEMODE, D3DSHADE_FLAT);
	IDirect3DDevice8_SetRenderState(g_device, D3DRS_ALPHABLENDENABLE, TRUE);
	IDirect3DDevice8_SetRenderState(g_device, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	IDirect3DDevice8_SetRenderState(g_device, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	IDirect3DDevice8_SetRenderState(g_device, D3DRS_CULLMODE, D3DCULL_NONE);

	SetTextureStage(texture, true);

	// Pixel centres are at integer coordinates in Direct3D 8.
	x1 -= 0.5f;
	y1 -= 0.5f;
	x2 -= 0.5f;
	y2 -= 0.5f;
	const D3DCOLOR c = ToD3DColor(color.r, color.g, color.b, color.a);
	XB8_ScreenVertex quad[4] = {
		{x1, y1, 0.0f, 1.0f, c, u1, v1},
		{x2, y1, 0.0f, 1.0f, c, u2, v1},
		{x2, y2, 0.0f, 1.0f, c, u2, v2},
		{x1, y2, 0.0f, 1.0f, c, u1, v2},
	};
	IDirect3DDevice8_SetVertexShader(g_device, kScreenFVF);
	IDirect3DDevice8_DrawPrimitiveUP(g_device, D3DPT_TRIANGLEFAN, 2, quad, sizeof(XB8_ScreenVertex));

	IDirect3DDevice8_SetRenderState(g_device, D3DRS_CULLMODE, D3DCULL_CCW);
}

bool XB8_Download(SDL_Surface* target)
{
	D3DSurface* backBuffer = nullptr;
	if (FAILED(IDirect3DDevice8_GetBackBuffer(g_device, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer))) {
		return false;
	}
	// Wait for the GPU to finish drawing into it.
	IDirect3DDevice8_BlockUntilIdle(g_device);
	D3DSURFACE_DESC desc;
	IDirect3DSurface8_GetDesc(backBuffer, &desc);
	D3DLOCKED_RECT locked;
	if (FAILED(IDirect3DSurface8_LockRect(backBuffer, &locked, nullptr, D3DLOCK_READONLY))) {
		IDirect3DSurface8_Release(backBuffer);
		return false;
	}
	const int w = SDL_min((int) desc.Width, target->w);
	const int h = SDL_min((int) desc.Height, target->h);
	for (int y = 0; y < h; ++y) {
		std::memcpy(
			static_cast<uint8_t*>(target->pixels) + y * target->pitch,
			static_cast<uint8_t*>(locked.pBits) + y * locked.Pitch,
			w * 4
		);
	}
	IDirect3DSurface8_UnlockRect(backBuffer);
	IDirect3DSurface8_Release(backBuffer);
	return true;
}

void XB8_SetDither(bool dither)
{
	IDirect3DDevice8_SetRenderState(g_device, D3DRS_DITHERENABLE, dither ? TRUE : FALSE);
}
