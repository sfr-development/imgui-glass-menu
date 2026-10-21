#pragma once

enum class Sfx { Click, ToggleOn, ToggleOff, Open, Close, Hover, Count_ };

void UISound_Play(Sfx s);
void UISound_NewFrame();
bool UISound_PlayedThisFrame();
