#include "actual.h"
#include "d3drmrenderer_xboxd3d8.h"
#include "ddraw_impl.h"
#include "ddsurface_impl.h"
#include "mathutils.h"
#include "meshutils.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cstring>
#include <vector>

static_assert(sizeof(Matrix4x4) == sizeof(D3DRMMATRIX4D), "Matrix4x4 is wrong size");
static_assert(sizeof(XB8_BridgeVector) == sizeof(D3DVECTOR), "XB8_BridgeVector is wrong size");
static_assert(sizeof(XB8_BridgeSceneLight) == sizeof(SceneLight), "XB8_BridgeSceneLight is wrong size");

Direct3DRMRenderer* XboxD3D8Renderer::Create(DWORD width, DWORD height)
{
	if (!DDWindow) {
		SDL_Log("No window handler");
		return nullptr;
	}
	void* device =
		SDL_GetPointerProperty(SDL_GetWindowProperties(DDWindow), SDL_PROP_WINDOW_XBOXOG_D3DDEVICE_POINTER, nullptr);
	if (!XB8_Initialize(device)) {
		SDL_Log("The window has no Direct3D 8 device");
		return nullptr;
	}
	return new XboxD3D8Renderer(width, height);
}

XboxD3D8Renderer::XboxD3D8Renderer(DWORD width, DWORD height)
{
	m_width = width;
	m_height = height;
	m_virtualWidth = width;
	m_virtualHeight = height;
	m_renderedImage = SDL_CreateSurface(m_width, m_height, SDL_PIXELFORMAT_XRGB8888);
	XB8_Resize(m_width, m_height);
}

XboxD3D8Renderer::~XboxD3D8Renderer()
{
	for (auto& tex : m_textures) {
		XB8_ReleaseTexture(tex.d3dTexture);
	}
	for (auto& mesh : m_meshs) {
		XB8_ReleaseMesh(mesh);
	}
	SDL_DestroySurface(m_renderedImage);
	XB8_Shutdown();
}

void XboxD3D8Renderer::PushLights(const SceneLight* lightsArray, size_t count)
{
	if (count > 8) {
		SDL_Log("Unsupported number of lights (%d)", static_cast<int>(count));
		count = 8;
	}
	m_lights.assign(lightsArray, lightsArray + count);
}

void XboxD3D8Renderer::SetFrustumPlanes(const Plane* frustumPlanes)
{
}

void XboxD3D8Renderer::SetProjection(const D3DRMMATRIX4D& projection, D3DVALUE front, D3DVALUE back)
{
	memcpy(&m_projection, projection, sizeof(D3DRMMATRIX4D));
}

void XboxD3D8Renderer::AddTextureDestroyCallback(Uint32 id, IDirect3DRMTexture* texture)
{
	RegisterDestroyCallback(texture, id, [](Direct3DRMRenderer* renderer, Uint32 textureId) {
		auto& cache = static_cast<XboxD3D8Renderer*>(renderer)->m_textures[textureId];
		XB8_ReleaseTexture(cache.d3dTexture);
		cache.d3dTexture = nullptr;
		cache.texture = nullptr;
	});
}

static int NextPowerOfTwo(int v)
{
	int power = 1;
	while (power < v) {
		power <<= 1;
	}
	return power;
}

// Xbox textures (swizzled) have power-of-two sides: a source of another size is
// scaled to the next one up, and texture coordinates stay relative to the source.
static D3DTexture* UploadTexture(SDL_Surface* src)
{
	SDL_Surface* working = src;
	if (src->format != SDL_PIXELFORMAT_ARGB8888) {
		working = SDL_ConvertSurface(src, SDL_PIXELFORMAT_ARGB8888);
		if (!working) {
			SDL_Log("SDL_ConvertSurface failed: %s", SDL_GetError());
			return nullptr;
		}
	}

	const int newW = std::min(NextPowerOfTwo(working->w), 4096);
	const int newH = std::min(NextPowerOfTwo(working->h), 4096);
	if (newW != working->w || newH != working->h) {
		SDL_Surface* resized = SDL_CreateSurface(newW, newH, SDL_PIXELFORMAT_ARGB8888);
		if (!resized) {
			SDL_Log("SDL_CreateSurface (resize) failed: %s", SDL_GetError());
			if (working != src) {
				SDL_DestroySurface(working);
			}
			return nullptr;
		}
		SDL_BlitSurfaceScaled(working, nullptr, resized, nullptr, SDL_SCALEMODE_NEAREST);
		if (working != src) {
			SDL_DestroySurface(working);
		}
		working = resized;
	}

	D3DTexture* texture = XB8_UploadTexture(working);
	if (working != src) {
		SDL_DestroySurface(working);
	}
	return texture;
}

Uint32 XboxD3D8Renderer::GetTextureId(IDirect3DRMTexture* iTexture, bool isUI, float scaleX, float scaleY)
{
	auto texture = static_cast<Direct3DRMTextureImpl*>(iTexture);
	auto surface = static_cast<DirectDrawSurfaceImpl*>(texture->m_surface);

	for (Uint32 i = 0; i < m_textures.size(); ++i) {
		auto& tex = m_textures[i];
		if (tex.texture == texture) {
			if (tex.version != texture->m_version) {
				XB8_ReleaseTexture(tex.d3dTexture);
				tex.d3dTexture = UploadTexture(surface->m_surface);
				tex.version = texture->m_version;
				tex.width = static_cast<float>(surface->m_surface->w);
				tex.height = static_cast<float>(surface->m_surface->h);
			}
			return tex.d3dTexture ? i : NO_TEXTURE_ID;
		}
	}

	D3DTexture* newTexture = UploadTexture(surface->m_surface);
	if (!newTexture) {
		return NO_TEXTURE_ID;
	}
	XB8TextureCacheEntry entry{
		texture,
		texture->m_version,
		newTexture,
		static_cast<float>(surface->m_surface->w),
		static_cast<float>(surface->m_surface->h)
	};

	for (Uint32 i = 0; i < m_textures.size(); ++i) {
		if (!m_textures[i].texture) {
			m_textures[i] = entry;
			AddTextureDestroyCallback(i, texture);
			return i;
		}
	}

	m_textures.push_back(entry);
	AddTextureDestroyCallback((Uint32) (m_textures.size() - 1), texture);
	return (Uint32) (m_textures.size() - 1);
}

static XB8MeshCacheEntry UploadMesh(const MeshGroup& meshGroup)
{
	XB8MeshCacheEntry cache{&meshGroup, meshGroup.version};
	cache.flat = meshGroup.quality == D3DRMRENDER_FLAT || meshGroup.quality == D3DRMRENDER_UNLITFLAT;

	std::vector<D3DRMVERTEX> vertices;
	std::vector<uint16_t> indices;
	if (cache.flat) {
		FlattenSurfaces(
			meshGroup.vertices.data(),
			meshGroup.vertices.size(),
			meshGroup.indices.data(),
			meshGroup.indices.size(),
			meshGroup.texture != nullptr,
			vertices,
			indices
		);
	}
	else {
		vertices = meshGroup.vertices;
		indices.resize(meshGroup.indices.size());
		std::transform(meshGroup.indices.begin(), meshGroup.indices.end(), indices.begin(), [](DWORD index) {
			return static_cast<uint16_t>(index);
		});
	}

	if (!XB8_UploadMesh(vertices.data(), vertices.size(), indices.data(), indices.size(), cache)) {
		SDL_Log("Failed to upload a mesh (%d vertices)", static_cast<int>(vertices.size()));
	}
	return cache;
}

void XboxD3D8Renderer::AddMeshDestroyCallback(Uint32 id, IDirect3DRMMesh* mesh)
{
	RegisterDestroyCallback(mesh, id, [](Direct3DRMRenderer* renderer, Uint32 meshId) {
		auto& cache = static_cast<XboxD3D8Renderer*>(renderer)->m_meshs[meshId];
		cache.meshGroup = nullptr;
		XB8_ReleaseMesh(cache);
	});
}

Uint32 XboxD3D8Renderer::GetMeshId(IDirect3DRMMesh* mesh, const MeshGroup* meshGroup)
{
	for (Uint32 i = 0; i < m_meshs.size(); ++i) {
		auto& cache = m_meshs[i];
		if (cache.meshGroup == meshGroup) {
			if (cache.version != meshGroup->version) {
				XB8_ReleaseMesh(cache);
				cache = UploadMesh(*meshGroup);
			}
			return i;
		}
	}

	auto newCache = UploadMesh(*meshGroup);

	for (Uint32 i = 0; i < m_meshs.size(); ++i) {
		if (!m_meshs[i].meshGroup) {
			m_meshs[i] = newCache;
			AddMeshDestroyCallback(i, mesh);
			return i;
		}
	}

	m_meshs.push_back(newCache);
	AddMeshDestroyCallback((Uint32) (m_meshs.size() - 1), mesh);
	return (Uint32) (m_meshs.size() - 1);
}

HRESULT XboxD3D8Renderer::BeginFrame()
{
	XB8_BeginFrame(
		reinterpret_cast<const Matrix4x4*>(&m_projection),
		reinterpret_cast<const XB8_BridgeSceneLight*>(m_lights.data()),
		m_lights.size()
	);
	return DD_OK;
}

void XboxD3D8Renderer::EnableTransparency()
{
	XB8_EnableTransparency();
}

void XboxD3D8Renderer::SubmitDraw(
	DWORD meshId,
	const D3DRMMATRIX4D& modelViewMatrix,
	const D3DRMMATRIX4D& worldMatrix,
	const D3DRMMATRIX4D& viewMatrix,
	const Matrix3x3& normalMatrix,
	const Appearance& appearance
)
{
	D3DTexture* texture = nullptr;
	if (appearance.textureId != NO_TEXTURE_ID) {
		texture = m_textures[appearance.textureId].d3dTexture;
	}
	XB8_SubmitDraw(m_meshs[meshId], reinterpret_cast<const Matrix4x4*>(&modelViewMatrix), appearance, texture);
}

HRESULT XboxD3D8Renderer::FinalizeFrame()
{
	return DD_OK;
}

void XboxD3D8Renderer::Resize(int width, int height, const ViewportTransform& viewportTransform)
{
	m_width = width;
	m_height = height;
	m_viewportTransform = viewportTransform;
	SDL_DestroySurface(m_renderedImage);
	m_renderedImage = SDL_CreateSurface(m_width, m_height, SDL_PIXELFORMAT_XRGB8888);
	XB8_Resize(width, height);
}

void XboxD3D8Renderer::Clear(float r, float g, float b)
{
	XB8_Clear(r, g, b);
}

void XboxD3D8Renderer::Flip()
{
	XB8_Flip();
}

void XboxD3D8Renderer::Draw2DImage(Uint32 textureId, const SDL_Rect& srcRect, const SDL_Rect& dstRect, FColor color)
{
	// Virtual (game) coordinates to the back buffer, through the viewport transform.
	const float scale = m_viewportTransform.scale;
	auto toScreenX = [&](float x) { return x * scale + m_viewportTransform.offsetX; };
	auto toScreenY = [&](float y) { return y * scale + m_viewportTransform.offsetY; };

	D3DTexture* texture = nullptr;
	float u1 = 0, v1 = 0, u2 = 0, v2 = 0;
	if (textureId != NO_TEXTURE_ID) {
		const auto& tex = m_textures[textureId];
		texture = tex.d3dTexture;
		u1 = srcRect.x / tex.width;
		v1 = srcRect.y / tex.height;
		u2 = (srcRect.x + srcRect.w) / tex.width;
		v2 = (srcRect.y + srcRect.h) / tex.height;
	}

	XB8_Draw2DImage(
		texture,
		u1,
		v1,
		u2,
		v2,
		toScreenX(static_cast<float>(dstRect.x)),
		toScreenY(static_cast<float>(dstRect.y)),
		toScreenX(static_cast<float>(dstRect.x + dstRect.w)),
		toScreenY(static_cast<float>(dstRect.y + dstRect.h)),
		color
	);
}

void XboxD3D8Renderer::SetDither(bool dither)
{
	XB8_SetDither(dither);
}

void XboxD3D8Renderer::Download(SDL_Surface* target)
{
	if (!XB8_Download(m_renderedImage)) {
		SDL_Log("Failed to read the back buffer");
		return;
	}

	SDL_Rect srcRect = {
		static_cast<int>(m_viewportTransform.offsetX),
		static_cast<int>(m_viewportTransform.offsetY),
		static_cast<int>(target->w * m_viewportTransform.scale),
		static_cast<int>(target->h * m_viewportTransform.scale),
	};
	SDL_BlitSurfaceScaled(m_renderedImage, &srcRect, target, nullptr, SDL_SCALEMODE_NEAREST);
}
