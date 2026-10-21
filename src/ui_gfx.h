#pragma once
#include "imgui/imgui.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11RenderTargetView;
struct IDXGISwapChain;

bool UIGfx_Init(ID3D11Device* device, ID3D11DeviceContext* ctx);
void UIGfx_Shutdown();

void UIGfx_Process(IDXGISwapChain* swapChain);

bool UIGfx_BlurReady();
void UIGfx_DrawGlass(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding, float alpha);
void UIGfx_DrawBlur(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding, float alpha);
void UIGfx_DrawShadow(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding, float alpha);

ImDrawList* UIGfx_GlowList();
void        UIGfx_DrawGlowLayer(ImDrawList* dl);

void UIGfx_DrawTestScene(ID3D11RenderTargetView* rtv, float width, float height, float time);
