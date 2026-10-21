#include "ui_gfx.h"
#include "ui_theme.h"

#include "imgui/imgui_internal.h"
#include "imgui/backends/imgui_impl_dx11.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <cmath>
#include <cstring>

#pragma comment(lib, "d3dcompiler.lib")

static ID3D11Device*        s_Dev = nullptr;
static ID3D11DeviceContext* s_Ctx = nullptr;
static bool                 s_Ok  = false;
static bool                 s_Requested = false;

static ID3D11VertexShader*    s_VS        = nullptr;
static ID3D11PixelShader*     s_PSDown    = nullptr;
static ID3D11PixelShader*     s_PSBlur    = nullptr;
static ID3D11PixelShader*     s_PSScene   = nullptr;
static ID3D11PixelShader*     s_PSDownA   = nullptr;
static ID3D11PixelShader*     s_PSBlurA   = nullptr;
static ID3D11PixelShader*     s_PSUnpre   = nullptr;
static ID3D11SamplerState*    s_Sampler   = nullptr;
static ID3D11RasterizerState* s_Raster    = nullptr;
static ID3D11Buffer*          s_CB        = nullptr;

static ID3D11ShaderResourceView* s_ShadowSrv = nullptr;

struct Target { ID3D11Texture2D* tex = nullptr; ID3D11RenderTargetView* rtv = nullptr; ID3D11ShaderResourceView* srv = nullptr; UINT w = 0, h = 0; };
static ID3D11Texture2D*          s_Full    = nullptr;
static ID3D11ShaderResourceView* s_FullSrv = nullptr;
static Target s_Half, s_QA, s_QB;
static UINT        s_W = 0, s_H = 0;
static DXGI_FORMAT s_Fmt = DXGI_FORMAT_UNKNOWN;
static bool        s_HaveResult = false;

static IUnknown* s_Graveyard[32];
static int       s_GraveCount = 0;

template <typename T> static void Bury(T*& p) {
    if (!p) return;
    if (s_GraveCount < 32) s_Graveyard[s_GraveCount++] = p; else p->Release();
    p = nullptr;
}
static void EmptyGraveyard() { for (int i = 0; i < s_GraveCount; i++) s_Graveyard[i]->Release(); s_GraveCount = 0; }
template <typename T> static void SafeRelease(T*& p) { if (p) { p->Release(); p = nullptr; } }

static const char* kShaderSrc = R"(
cbuffer CB : register(b0) { float2 texel; float2 dir; float4 extra; };
Texture2D    tex : register(t0);
SamplerState smp : register(s0);
struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

VSOut VS(uint id : SV_VertexID) {
    VSOut o;
    o.uv  = float2((id << 1) & 2, id & 2);
    o.pos = float4(o.uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return o;
}

float4 PSDown(VSOut i) : SV_Target {
    float2 o = texel * 0.5;
    float4 c = tex.Sample(smp, i.uv + float2(-o.x, -o.y)) + tex.Sample(smp, i.uv + float2(o.x, -o.y))
             + tex.Sample(smp, i.uv + float2(-o.x,  o.y)) + tex.Sample(smp, i.uv + float2(o.x,  o.y));
    return float4(c.rgb * 0.25, 1);
}

float4 PSBlur(VSOut i) : SV_Target {
    float2 o1 = dir * texel * 1.3846153846;
    float2 o2 = dir * texel * 3.2307692308;
    float3 c = tex.Sample(smp, i.uv).rgb * 0.2270270270;
    c += (tex.Sample(smp, i.uv + o1).rgb + tex.Sample(smp, i.uv - o1).rgb) * 0.3162162162;
    c += (tex.Sample(smp, i.uv + o2).rgb + tex.Sample(smp, i.uv - o2).rgb) * 0.0702702703;
    return float4(c, 1);
}

float4 PSDownA(VSOut i) : SV_Target {
    float2 o = texel * 0.5;
    return (tex.Sample(smp, i.uv + float2(-o.x, -o.y)) + tex.Sample(smp, i.uv + float2(o.x, -o.y))
          + tex.Sample(smp, i.uv + float2(-o.x,  o.y)) + tex.Sample(smp, i.uv + float2(o.x,  o.y))) * 0.25;
}

float4 PSBlurA(VSOut i) : SV_Target {
    float2 o1 = dir * texel * 1.3846153846;
    float2 o2 = dir * texel * 3.2307692308;
    float4 c = tex.Sample(smp, i.uv) * 0.2270270270;
    c += (tex.Sample(smp, i.uv + o1) + tex.Sample(smp, i.uv - o1)) * 0.3162162162;
    c += (tex.Sample(smp, i.uv + o2) + tex.Sample(smp, i.uv - o2)) * 0.0702702703;
    return c;
}

float4 PSUnpremul(VSOut i) : SV_Target {
    float4 c = tex.Sample(smp, i.uv);
    float3 rgb = c.a > 0.002 ? c.rgb / c.a : float3(0, 0, 0);
    return float4(rgb, saturate(c.a * 1.8));
}

float3 blob(float2 uv, float2 c, float r, float3 col) {
    float d = length((uv - c) * float2(1.6, 1.0));
    return col * exp(-d * d / (r * r));
}

float4 PSScene(VSOut i) : SV_Target {
    float2 uv = i.uv;
    float  t  = extra.x * 0.25;
    float3 col = lerp(float3(0.035, 0.040, 0.070), float3(0.010, 0.012, 0.025), uv.y);
    col += blob(uv, float2(0.22 + 0.06 * sin(t * 0.9), 0.30 + 0.05 * cos(t * 0.7)), 0.30, float3(0.95, 0.42, 0.18));
    col += blob(uv, float2(0.78 + 0.05 * cos(t * 0.6), 0.25 + 0.06 * sin(t * 0.8)), 0.28, float3(0.30, 0.25, 0.95));
    col += blob(uv, float2(0.55 + 0.07 * sin(t * 0.5), 0.80 + 0.04 * cos(t * 1.1)), 0.34, float3(0.10, 0.70, 0.75));
    col += blob(uv, float2(0.10 + 0.04 * cos(t * 1.3), 0.85), 0.22, float3(0.85, 0.20, 0.55));
    float2 g = abs(frac(uv * float2(32.0, 18.0)) - 0.5);
    col += 0.018 * step(0.47, max(g.x, g.y));
    float vig = 1.0 - 0.45 * dot(uv - 0.5, uv - 0.5);
    return float4(col * vig, 1);
}
)";

static bool Compile(const char* entry, const char* profile, ID3DBlob** out) {
    ID3DBlob* err = nullptr;
    const HRESULT hr = D3DCompile(kShaderSrc, strlen(kShaderSrc), "ui_gfx", nullptr, nullptr, entry, profile,
                                  D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, out, &err);
    if (err) err->Release();
    return SUCCEEDED(hr);
}

static bool CreatePS(const char* entry, ID3D11PixelShader** ps) {
    ID3DBlob* b = nullptr;
    if (!Compile(entry, "ps_4_0", &b)) return false;
    const HRESULT hr = s_Dev->CreatePixelShader(b->GetBufferPointer(), b->GetBufferSize(), nullptr, ps);
    b->Release();
    return SUCCEEDED(hr);
}

static bool CreateShadowTexture() {
    const int S = 96, lo = 32, hi = 64;
    static unsigned char px[S * S * 4];
    const float sigma = 11.0f;
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++) {
            const float fx = x + 0.5f, fy = y + 0.5f;
            const float dx = fx < lo ? lo - fx : (fx > hi ? fx - hi : 0.0f);
            const float dy = fy < lo ? lo - fy : (fy > hi ? fy - hi : 0.0f);
            const float d2 = dx * dx + dy * dy;
            const float a  = expf(-d2 / (2.0f * sigma * sigma));
            unsigned char* p = &px[(y * S + x) * 4];
            p[0] = p[1] = p[2] = 255;
            p[3] = (unsigned char)(a * 255.0f + 0.5f);
        }
    D3D11_TEXTURE2D_DESC d = {};
    d.Width = d.Height = S; d.MipLevels = d.ArraySize = 1; d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    d.SampleDesc.Count = 1; d.Usage = D3D11_USAGE_IMMUTABLE; d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init = { px, S * 4, 0 };
    ID3D11Texture2D* tex = nullptr;
    if (FAILED(s_Dev->CreateTexture2D(&d, &init, &tex))) return false;
    const HRESULT hr = s_Dev->CreateShaderResourceView(tex, nullptr, &s_ShadowSrv);
    tex->Release();
    return SUCCEEDED(hr);
}

bool UIGfx_Init(ID3D11Device* device, ID3D11DeviceContext* ctx) {
    s_Dev = device; s_Ctx = ctx;
    if (!s_Dev || !s_Ctx) return false;

    ID3DBlob* vs = nullptr;
    if (!Compile("VS", "vs_4_0", &vs)) return false;
    const HRESULT hr = s_Dev->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &s_VS);
    vs->Release();
    if (FAILED(hr)) return false;
    if (!CreatePS("PSDown", &s_PSDown) || !CreatePS("PSBlur", &s_PSBlur) || !CreatePS("PSScene", &s_PSScene)) return false;
    if (!CreatePS("PSDownA", &s_PSDownA) || !CreatePS("PSBlurA", &s_PSBlurA) || !CreatePS("PSUnpremul", &s_PSUnpre)) return false;

    D3D11_SAMPLER_DESC sd = {};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(s_Dev->CreateSamplerState(&sd, &s_Sampler))) return false;

    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE;
    if (FAILED(s_Dev->CreateRasterizerState(&rd, &s_Raster))) return false;

    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = 32; bd.Usage = D3D11_USAGE_DYNAMIC; bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER; bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(s_Dev->CreateBuffer(&bd, nullptr, &s_CB))) return false;

    CreateShadowTexture();

    s_Ok = true;
    Theme::GlassAvailable = true;
    Theme::GlassOn = Theme::GlassWanted;
    ApplyTheme();
    return true;
}

static void ReleaseTargets() {
    Bury(s_Full); Bury(s_FullSrv);
    Target* targets[3] = { &s_Half, &s_QA, &s_QB };
    for (Target* t : targets) { Bury(t->tex); Bury(t->rtv); Bury(t->srv); t->w = t->h = 0; }
    s_HaveResult = false;
}

static void ReleaseGlowTargets();

void UIGfx_Shutdown() {
    ReleaseTargets();
    ReleaseGlowTargets();
    EmptyGraveyard();
    SafeRelease(s_VS); SafeRelease(s_PSDown); SafeRelease(s_PSBlur); SafeRelease(s_PSScene);
    SafeRelease(s_PSDownA); SafeRelease(s_PSBlurA); SafeRelease(s_PSUnpre);
    SafeRelease(s_Sampler); SafeRelease(s_Raster); SafeRelease(s_CB); SafeRelease(s_ShadowSrv);
    s_Ok = false;
    Theme::GlassAvailable = false;
    Theme::GlassOn = false;
}

static DXGI_FORMAT ViewFormat(DXGI_FORMAT f) {
    switch (f) {
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:    return DXGI_FORMAT_R8G8B8A8_UNORM;
        case DXGI_FORMAT_B8G8R8A8_TYPELESS:    return DXGI_FORMAT_B8G8R8A8_UNORM;
        case DXGI_FORMAT_R10G10B10A2_TYPELESS: return DXGI_FORMAT_R10G10B10A2_UNORM;
        case DXGI_FORMAT_R16G16B16A16_TYPELESS:return DXGI_FORMAT_R16G16B16A16_FLOAT;
        default:                               return f;
    }
}

static bool MakeTarget(Target& t, UINT w, UINT h) {
    D3D11_TEXTURE2D_DESC d = {};
    d.Width = w ? w : 1; d.Height = h ? h : 1; d.MipLevels = d.ArraySize = 1;
    d.Format = DXGI_FORMAT_R8G8B8A8_UNORM; d.SampleDesc.Count = 1; d.Usage = D3D11_USAGE_DEFAULT;
    d.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(s_Dev->CreateTexture2D(&d, nullptr, &t.tex))) return false;
    if (FAILED(s_Dev->CreateRenderTargetView(t.tex, nullptr, &t.rtv))) return false;
    if (FAILED(s_Dev->CreateShaderResourceView(t.tex, nullptr, &t.srv))) return false;
    t.w = d.Width; t.h = d.Height;
    return true;
}

static const DWORD kTargetRetryMs = 2000;
static DWORD       s_TargetsFailTick = 0;
static DWORD       s_GlowFailTick    = 0;

static bool RetryBlocked(DWORD failTick) {
    return failTick && GetTickCount() - failTick < kTargetRetryMs;
}

static bool CreateTargets(const D3D11_TEXTURE2D_DESC& bb);

static bool EnsureTargets(const D3D11_TEXTURE2D_DESC& bb) {
    if (s_Full && s_QB.srv && bb.Width == s_W && bb.Height == s_H && bb.Format == s_Fmt) return true;
    if (RetryBlocked(s_TargetsFailTick)) return false;
    ReleaseTargets();
    if (CreateTargets(bb)) { s_TargetsFailTick = 0; return true; }
    ReleaseTargets();
    s_W = s_H = 0;
    s_TargetsFailTick = GetTickCount() | 1;
    return false;
}

static bool CreateTargets(const D3D11_TEXTURE2D_DESC& bb) {
    s_W = bb.Width; s_H = bb.Height; s_Fmt = bb.Format;

    D3D11_TEXTURE2D_DESC d = {};
    d.Width = bb.Width; d.Height = bb.Height; d.MipLevels = d.ArraySize = 1;
    d.Format = bb.Format; d.SampleDesc.Count = 1; d.Usage = D3D11_USAGE_DEFAULT;
    d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(s_Dev->CreateTexture2D(&d, nullptr, &s_Full))) return false;
    D3D11_SHADER_RESOURCE_VIEW_DESC sv = {};
    sv.Format = ViewFormat(bb.Format); sv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; sv.Texture2D.MipLevels = 1;
    if (FAILED(s_Dev->CreateShaderResourceView(s_Full, &sv, &s_FullSrv))) return false;

    return MakeTarget(s_Half, bb.Width / 2, bb.Height / 2) &&
           MakeTarget(s_QA,   bb.Width / 4, bb.Height / 4) &&
           MakeTarget(s_QB,   bb.Width / 4, bb.Height / 4);
}

struct StateBackup {
    UINT                        vpCount;
    D3D11_VIEWPORT              vp[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    ID3D11RasterizerState*      rs;
    ID3D11BlendState*           bs;
    FLOAT                       bf[4];
    UINT                        sm;
    ID3D11DepthStencilState*    ds;
    UINT                        sref;
    ID3D11ShaderResourceView*   srv;
    ID3D11SamplerState*         smp;
    ID3D11PixelShader*          ps;
    ID3D11VertexShader*         vs;
    ID3D11GeometryShader*       gs;
    ID3D11Buffer*               vsCB;
    ID3D11Buffer*               psCB;
    D3D11_PRIMITIVE_TOPOLOGY    topo;
    ID3D11InputLayout*          il;
    ID3D11RenderTargetView*     rtv[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
    ID3D11DepthStencilView*     dsv;
};

static void Backup(StateBackup& b) {
    memset(&b, 0, sizeof(b));
    b.vpCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    s_Ctx->RSGetViewports(&b.vpCount, b.vp);
    s_Ctx->RSGetState(&b.rs);
    s_Ctx->OMGetBlendState(&b.bs, b.bf, &b.sm);
    s_Ctx->OMGetDepthStencilState(&b.ds, &b.sref);
    s_Ctx->PSGetShaderResources(0, 1, &b.srv);
    s_Ctx->PSGetSamplers(0, 1, &b.smp);
    s_Ctx->PSGetShader(&b.ps, nullptr, nullptr);
    s_Ctx->VSGetShader(&b.vs, nullptr, nullptr);
    s_Ctx->GSGetShader(&b.gs, nullptr, nullptr);
    s_Ctx->VSGetConstantBuffers(0, 1, &b.vsCB);
    s_Ctx->PSGetConstantBuffers(0, 1, &b.psCB);
    s_Ctx->IAGetPrimitiveTopology(&b.topo);
    s_Ctx->IAGetInputLayout(&b.il);
    s_Ctx->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, b.rtv, &b.dsv);
}

static void Restore(StateBackup& b) {
    s_Ctx->RSSetViewports(b.vpCount, b.vp);
    s_Ctx->RSSetState(b.rs);                       SafeRelease(b.rs);
    s_Ctx->OMSetBlendState(b.bs, b.bf, b.sm);      SafeRelease(b.bs);
    s_Ctx->OMSetDepthStencilState(b.ds, b.sref);   SafeRelease(b.ds);
    s_Ctx->PSSetShaderResources(0, 1, &b.srv);     SafeRelease(b.srv);
    s_Ctx->PSSetSamplers(0, 1, &b.smp);            SafeRelease(b.smp);
    s_Ctx->PSSetShader(b.ps, nullptr, 0);          SafeRelease(b.ps);
    s_Ctx->VSSetShader(b.vs, nullptr, 0);          SafeRelease(b.vs);
    s_Ctx->GSSetShader(b.gs, nullptr, 0);          SafeRelease(b.gs);
    s_Ctx->VSSetConstantBuffers(0, 1, &b.vsCB);    SafeRelease(b.vsCB);
    s_Ctx->PSSetConstantBuffers(0, 1, &b.psCB);    SafeRelease(b.psCB);
    s_Ctx->IASetPrimitiveTopology(b.topo);
    s_Ctx->IASetInputLayout(b.il);                 SafeRelease(b.il);
    s_Ctx->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, b.rtv, b.dsv);
    for (auto*& r : b.rtv) SafeRelease(r);
    SafeRelease(b.dsv);
}

static void SetCB(float tx, float ty, float dx, float dy, float e0 = 0.0f) {
    D3D11_MAPPED_SUBRESOURCE m;
    if (FAILED(s_Ctx->Map(s_CB, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) return;
    float* f = (float*)m.pData;
    f[0] = tx; f[1] = ty; f[2] = dx; f[3] = dy; f[4] = e0; f[5] = f[6] = f[7] = 0.0f;
    s_Ctx->Unmap(s_CB, 0);
}

static void CommonState() {
    s_Ctx->IASetInputLayout(nullptr);
    s_Ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    s_Ctx->VSSetShader(s_VS, nullptr, 0);
    s_Ctx->GSSetShader(nullptr, nullptr, 0);
    s_Ctx->RSSetState(s_Raster);
    s_Ctx->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    s_Ctx->OMSetDepthStencilState(nullptr, 0);
    s_Ctx->PSSetSamplers(0, 1, &s_Sampler);
    s_Ctx->VSSetConstantBuffers(0, 1, &s_CB);
    s_Ctx->PSSetConstantBuffers(0, 1, &s_CB);
}

static void Pass(ID3D11ShaderResourceView* src, UINT srcW, UINT srcH, const Target& dst, ID3D11PixelShader* ps, float dx, float dy) {
    ID3D11ShaderResourceView* nullSrv = nullptr;
    s_Ctx->PSSetShaderResources(0, 1, &nullSrv);
    s_Ctx->OMSetRenderTargets(1, &dst.rtv, nullptr);
    D3D11_VIEWPORT vp = { 0, 0, (float)dst.w, (float)dst.h, 0, 1 };
    s_Ctx->RSSetViewports(1, &vp);
    SetCB(1.0f / srcW, 1.0f / srcH, dx, dy);
    s_Ctx->PSSetShader(ps, nullptr, 0);
    s_Ctx->PSSetShaderResources(0, 1, &src);
    s_Ctx->Draw(3, 0);
}

static ImDrawList* s_GlowList  = nullptr;
static int         s_GlowFrame = -1;
static Target      s_GFull, s_GHalf, s_GA, s_GB, s_GOut;

static void ReleaseGlowTargets() {
    Target* targets[5] = { &s_GFull, &s_GHalf, &s_GA, &s_GB, &s_GOut };
    for (Target* t : targets) { Bury(t->tex); Bury(t->rtv); Bury(t->srv); t->w = t->h = 0; }
}

static bool EnsureGlowTargets(UINT w, UINT h) {
    if (s_GFull.tex && s_GOut.srv && s_GFull.w == w && s_GFull.h == h) return true;
    if (RetryBlocked(s_GlowFailTick)) return false;
    ReleaseGlowTargets();
    if (MakeTarget(s_GFull, w, h) && MakeTarget(s_GHalf, w / 2, h / 2) &&
        MakeTarget(s_GA, w / 4, h / 4) && MakeTarget(s_GB, w / 4, h / 4) && MakeTarget(s_GOut, w / 4, h / 4)) {
        s_GlowFailTick = 0;
        return true;
    }
    ReleaseGlowTargets();
    s_GlowFailTick = GetTickCount() | 1;
    return false;
}

ImDrawList* UIGfx_GlowList() {
    if (!s_Ok) return nullptr;
    if (!s_GlowList) s_GlowList = IM_NEW(ImDrawList)(ImGui::GetDrawListSharedData());
    const int f = ImGui::GetFrameCount();
    if (f != s_GlowFrame) {
        s_GlowFrame = f;
        s_GlowList->_ResetForNewFrame();
        s_GlowList->PushTextureID(ImGui::GetIO().Fonts->TexID);
        s_GlowList->PushClipRectFullScreen();
    }
    return s_GlowList;
}

void UIGfx_DrawGlowLayer(ImDrawList* dl) {
    if (!s_Ok || !s_GOut.srv) { UIGfx_GlowList(); return; }
    dl->AddImage((ImTextureID)s_GOut.srv, ImVec2(0, 0), ImGui::GetIO().DisplaySize);
}

static void RenderGlow(UINT w, UINT h) {
    ImDrawList* gl = s_GlowList;
    gl->_PopUnusedDrawCmd();
    if (gl->VtxBuffer.Size == 0) {
        if (s_GOut.rtv) { const float z[4] = { 0, 0, 0, 0 }; s_Ctx->ClearRenderTargetView(s_GOut.rtv, z); }
        return;
    }
    if (!EnsureGlowTargets(w, h)) return;

    StateBackup b;
    Backup(b);
    const float clear[4] = { 0, 0, 0, 0 };
    s_Ctx->ClearRenderTargetView(s_GFull.rtv, clear);
    s_Ctx->OMSetRenderTargets(1, &s_GFull.rtv, nullptr);

    ImDrawData dd;
    dd.Valid            = true;
    dd.DisplayPos       = ImVec2(0, 0);
    dd.DisplaySize      = ImVec2((float)w, (float)h);
    dd.FramebufferScale = ImVec2(1, 1);
    dd.AddDrawList(gl);
    ImGui_ImplDX11_RenderDrawData(&dd);

    CommonState();
    Pass(s_GFull.srv, s_GFull.w, s_GFull.h, s_GHalf, s_PSDownA, 0, 0);
    Pass(s_GHalf.srv, s_GHalf.w, s_GHalf.h, s_GA, s_PSDownA, 0, 0);
    Pass(s_GA.srv, s_GA.w, s_GA.h, s_GB, s_PSBlurA, 1.0f, 0);
    Pass(s_GB.srv, s_GB.w, s_GB.h, s_GA, s_PSBlurA, 0, 1.0f);
    Pass(s_GA.srv, s_GA.w, s_GA.h, s_GB, s_PSBlurA, 2.0f, 0);
    Pass(s_GB.srv, s_GB.w, s_GB.h, s_GA, s_PSBlurA, 0, 2.0f);
    Pass(s_GA.srv, s_GA.w, s_GA.h, s_GOut, s_PSUnpre, 0, 0);
    ID3D11ShaderResourceView* nullSrv = nullptr;
    s_Ctx->PSSetShaderResources(0, 1, &nullSrv);
    Restore(b);
}

void UIGfx_Process(IDXGISwapChain* sc) {
    EmptyGraveyard();
    const bool wanted = s_Requested;
    s_Requested = false;
    const bool glow = s_GlowList && s_GlowFrame == ImGui::GetFrameCount();
    if (!s_Ok || !sc || (!wanted && !glow)) return;

    ID3D11Texture2D* bb = nullptr;
    if (FAILED(sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&bb)) || !bb) return;
    D3D11_TEXTURE2D_DESC desc;
    bb->GetDesc(&desc);

    if (wanted) {
        if (!EnsureTargets(desc)) { bb->Release(); ReleaseTargets(); return; }
        if (desc.SampleDesc.Count > 1) s_Ctx->ResolveSubresource(s_Full, 0, bb, 0, ViewFormat(desc.Format));
        else                           s_Ctx->CopyResource(s_Full, bb);
    }
    bb->Release();

    if (wanted) {
        StateBackup b;
        Backup(b);
        CommonState();
        Pass(s_FullSrv, s_W, s_H, s_Half, s_PSDown, 0, 0);
        Pass(s_Half.srv, s_Half.w, s_Half.h, s_QA, s_PSDown, 0, 0);
        for (int i = 0; i < 2; i++) {
            const float spread = 1.0f + i * 1.5f;
            Pass(s_QA.srv, s_QA.w, s_QA.h, s_QB, s_PSBlur, spread, 0);
            Pass(s_QB.srv, s_QB.w, s_QB.h, s_QA, s_PSBlur, 0, spread);
        }
        ID3D11ShaderResourceView* nullSrv = nullptr;
        s_Ctx->PSSetShaderResources(0, 1, &nullSrv);
        Restore(b);
        s_HaveResult = true;
    }

    if (glow) RenderGlow(desc.Width, desc.Height);
}

bool UIGfx_BlurReady() { return s_Ok; }

void UIGfx_DrawGlass(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding, float alpha) {
    if (!Theme::GlassOn) return;
    UIGfx_DrawBlur(dl, a, b, rounding, alpha);
}

void UIGfx_DrawBlur(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding, float alpha) {
    if (!s_Ok) return;
    s_Requested = true;
    if (!s_HaveResult && !s_QA.srv) return;
    const ImVec2 ds = ImGui::GetIO().DisplaySize;
    if (ds.x <= 0 || ds.y <= 0) return;
    dl->AddImageRounded((ImTextureID)s_QA.srv, a, b, ImVec2(a.x / ds.x, a.y / ds.y), ImVec2(b.x / ds.x, b.y / ds.y),
                        IM_COL32(255, 255, 255, (int)(255 * alpha)), rounding);
}

void UIGfx_DrawShadow(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding, float alpha) {
    if (!s_ShadowSrv) {
        for (int i = 0; i < 12; i++) {
            const float spread = 2.0f + i * 3.0f, k = 0.08f * (1.0f - i / 12.0f) * alpha;
            dl->AddRectFilled(ImVec2(a.x - spread, a.y - spread + 12), ImVec2(b.x + spread, b.y + spread + 12),
                              IM_COL32(0, 0, 0, (int)(255 * k)), rounding + spread);
        }
        return;
    }
    const float E = 44.0f, off = 14.0f;
    const ImU32 col = IM_COL32(0, 0, 0, (int)(255 * 0.62f * alpha));
    const float xs[4] = { a.x - E, a.x, b.x, b.x + E };
    const float ys[4] = { a.y - E + off, a.y + off, b.y + off, b.y + E + off };
    const float us[4] = { 0.0f, 1.0f / 3.0f, 2.0f / 3.0f, 1.0f };
    for (int j = 0; j < 3; j++)
        for (int i = 0; i < 3; i++)
            dl->AddImage((ImTextureID)s_ShadowSrv, ImVec2(xs[i], ys[j]), ImVec2(xs[i + 1], ys[j + 1]),
                         ImVec2(us[i], us[j]), ImVec2(us[i + 1], us[j + 1]), col);
}

void UIGfx_DrawTestScene(ID3D11RenderTargetView* rtv, float w, float h, float time) {
    if (!s_Ok || !rtv) return;
    StateBackup b;
    Backup(b);
    CommonState();
    s_Ctx->OMSetRenderTargets(1, &rtv, nullptr);
    D3D11_VIEWPORT vp = { 0, 0, w, h, 0, 1 };
    s_Ctx->RSSetViewports(1, &vp);
    SetCB(1.0f / w, 1.0f / h, 0, 0, time);
    s_Ctx->PSSetShader(s_PSScene, nullptr, 0);
    s_Ctx->Draw(3, 0);
    Restore(b);
}
