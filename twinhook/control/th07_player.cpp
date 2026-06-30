#include "stdafx.h"
#include "th07_player.h"
#include "util/cdraw.h"
#include "gfx/di8_input_overlay.h"
#include "hook/th_di8_hook.h"
#include "directx/IDI8ADevice_Wrapper.h"
#include "config/th_config.h"
#include "hook/th_d3d9_hook.h"
#include "gfx/th_info_overlay.h"

void th07_player::onInit()
{
	th_player::onInit();
}

void th07_player::onTick()
{
	th_player::onTick();
}

void th07_player::onBeginTick()
{
	th_player::onBeginTick();
}

void th07_player::onAfterTick()
{
	th_player::onAfterTick();
}

void th07_player::handleInput(const BYTE diKeys[256], const BYTE press[256])
{
	th_player::handleInput(diKeys, press);
}

void th07_player::onEnableChanged(bool enable)
{
	th_player::onEnableChanged(enable);
}

player th07_player::getPlayerEntity()
{
	PBYTE PlayerPtrAddr = (PBYTE)this->gs_ptr.plyr_pos;

	vec2 sz(5, 5);
	aabb a{
		vec2(*(float*)PlayerPtrAddr - th_param.GAME_X_OFFSET,
			 *(float*)(PlayerPtrAddr + 4) - th_param.GAME_Y_OFFSET) - sz / 2,
		vec2(),
		sz,
	};
	return player{ a };
}

void th07_player::draw(IDirect3DDevice9* d3dDev)
{
	th_player::draw(d3dDev);
}

