#include "stdafx.h"
#include "th09_player.h"
#include "config/th_config.h"

namespace
{
	constexpr uintptr_t TH09_P1_PLAYER_PTR = 0x004A7D94;
	constexpr uintptr_t TH09_P1_BULLET_MANAGER_PTR = 0x004A7D98;

	constexpr uintptr_t TH09_PLAYER_POS = 0x1B88;

	constexpr uintptr_t TH09_BULLETS_OFFSET = 0x1A900;
	constexpr int TH09_BULLET_COUNT = 0x218;
	constexpr uintptr_t TH09_BULLET_SIZE = 0x10C4;
	constexpr uintptr_t TH09_BULLET_HITBOX = 0x0D38;
	constexpr uintptr_t TH09_BULLET_POS = 0x0D4C;
	constexpr uintptr_t TH09_BULLET_VEL = 0x0D58;
	constexpr uintptr_t TH09_BULLET_STATE = 0x0DBE;

	bool canRead(uintptr_t addr, size_t size)
	{
		uintptr_t end = addr + size;
		for (uintptr_t cur = addr; cur < end;)
		{
			MEMORY_BASIC_INFORMATION mbi{};
			if (!VirtualQuery((void*)cur, &mbi, sizeof(mbi)) ||
				mbi.State != MEM_COMMIT ||
				(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
				return false;

			uintptr_t regionEnd = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
			if (regionEnd <= cur)
				return false;
			cur = regionEnd;
		}
		return true;
	}

	template <typename T>
	bool readValue(uintptr_t addr, T& value)
	{
		if (!canRead(addr, sizeof(T)))
			return false;
		value = *(T*)addr;
		return true;
	}

	bool readVec2(uintptr_t addr, vec2& value)
	{
		float x;
		float y;
		if (!readValue(addr, x) || !readValue(addr + sizeof(float), y))
			return false;
		if (!std::isfinite(x) || !std::isfinite(y))
			return false;
		value = vec2(x, y);
		return true;
	}

	bool isInPlayfield(const vec2& p)
	{
		return p.x >= -64 && p.x <= th_param.GAME_WIDTH + 64 &&
			p.y >= -64 && p.y <= th_param.GAME_HEIGHT + 64;
	}
}

void th09_player::onInit()
{
	th_player::onInit();
}

void th09_player::onTick()
{
	th_player::onTick();
}

void th09_player::onBeginTick()
{
	th_player::onBeginTick();
	this->doBulletPoll();
}

void th09_player::onAfterTick()
{
	th_player::onAfterTick();
}

void th09_player::draw(IDirect3DDevice9* d3dDev)
{
	th_player::draw(d3dDev);
}

void th09_player::handleInput(const BYTE diKeys[256], const BYTE press[256])
{
	th_player::handleInput(diKeys, press);
}

void th09_player::onEnableChanged(bool enable)
{
	th_player::onEnableChanged(enable);
}

player th09_player::getPlayerEntity()
{
	uintptr_t playerAddr;
	if (!readValue(TH09_P1_PLAYER_PTR, playerAddr))
		return player{ aabb() };
	if (!playerAddr)
		return player{ aabb() };

	vec2 center;
	if (!readVec2(playerAddr + TH09_PLAYER_POS, center) || !isInPlayfield(center))
		return player{ aabb() };

	vec2 sz(5, 5);
	aabb a{
		center - sz / 2,
		vec2(),
		sz,
	};
	return player{ a };
}

void th09_player::doBulletPoll()
{
	uintptr_t bulletManager;
	if (!readValue(TH09_P1_BULLET_MANAGER_PTR, bulletManager))
		return;
	if (!bulletManager)
		return;

	for (int i = 0; i < TH09_BULLET_COUNT; ++i)
	{
		uintptr_t bulletAddr = bulletManager + TH09_BULLETS_OFFSET + i * TH09_BULLET_SIZE;
		uint16_t state;
		if (!readValue(bulletAddr + TH09_BULLET_STATE, state))
			continue;
		if (state == 0 || state == 6)
			continue;

		vec2 center;
		if (!readVec2(bulletAddr + TH09_BULLET_POS, center) || !isInPlayfield(center))
			continue;

		vec2 sz;
		if (!readVec2(bulletAddr + TH09_BULLET_HITBOX, sz))
			continue;
		if (sz.x <= 0 || sz.y <= 0 || sz.x > 64 || sz.y > 64)
			sz = vec2(6, 6);

		vec2 velocity;
		if (!readVec2(bulletAddr + TH09_BULLET_VEL, velocity))
			velocity = vec2();

		aabb a{
			center - sz / 2,
			velocity,
			sz,
		};
		bullets.push_back(bullet{ a });
	}
}
