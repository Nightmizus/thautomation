#include "stdafx.h"
#include "th06_player.h"
#include "config/th_config.h"
#include "hook/th_di8_hook.h"

namespace
{
	constexpr uintptr_t TH06_PLAYER_POS = 0x006CAA68;
	constexpr uintptr_t TH06_BULLET_MANAGER = 0x005A5FF8;
	constexpr uintptr_t TH06_ENEMY_MANAGER = 0x004B79C8;
	constexpr uintptr_t TH06_ITEM_MANAGER = 0x0069E268;

	constexpr int TH06_BULLET_COUNT = 640;
	constexpr int TH06_LASER_COUNT = 64;
	constexpr int TH06_ENEMY_COUNT = 257;
	constexpr int TH06_ITEM_COUNT = 513;

	constexpr uintptr_t TH06_BULLETS_OFFSET = 0x5600;
	constexpr uintptr_t TH06_BULLET_SIZE = 0x5C4;
	constexpr uintptr_t TH06_BULLET_POS = 0x560;
	constexpr uintptr_t TH06_BULLET_VEL = 0x56C;
	constexpr uintptr_t TH06_BULLET_HITBOX = 0x550;
	constexpr uintptr_t TH06_BULLET_STATE = 0x5BE;

	constexpr uintptr_t TH06_LASERS_OFFSET = 0xEC000;
	constexpr uintptr_t TH06_LASER_SIZE = 0x270;
	constexpr uintptr_t TH06_LASER_POS = 0x220;
	constexpr uintptr_t TH06_LASER_ANGLE = 0x22C;
	constexpr uintptr_t TH06_LASER_END_OFFSET = 0x234;
	constexpr uintptr_t TH06_LASER_WIDTH = 0x23C;
	constexpr uintptr_t TH06_LASER_SPEED = 0x240;
	constexpr uintptr_t TH06_LASER_IN_USE = 0x258;

	constexpr uintptr_t TH06_ENEMIES_OFFSET = 0xED0;
	constexpr uintptr_t TH06_ENEMY_SIZE = 0xEC8;
	constexpr uintptr_t TH06_ENEMY_POS = 0xC6C;
	constexpr uintptr_t TH06_ENEMY_HITBOX = 0xC78;
	constexpr uintptr_t TH06_ENEMY_VEL = 0xC84;
	constexpr uintptr_t TH06_ENEMY_LIFE = 0xCE4;

	constexpr uintptr_t TH06_ITEM_SIZE = 0x144;
	constexpr uintptr_t TH06_ITEM_POS = 0x110;
	constexpr uintptr_t TH06_ITEM_TYPE = 0x140;
	constexpr uintptr_t TH06_ITEM_IN_USE = 0x141;
	constexpr int TH06_ITEM_POINT_BULLET = 6;

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

void th06_player::onInit()
{
	th_player::onInit();
}

void th06_player::onTick()
{
	th_player::onTick();
}

void th06_player::onBeginTick()
{
	th_player::onBeginTick();
	this->doBulletPoll();
	this->doEnemyPoll();
	this->doPowerupPoll();
	this->doLaserPoll();
}

void th06_player::onAfterTick()
{
	th_player::onAfterTick();
}

void th06_player::draw(IDirect3DDevice9* d3dDev)
{
	th_player::draw(d3dDev);
}

void th06_player::handleInput(const BYTE diKeys[256], const BYTE press[256])
{
	th_player::handleInput(diKeys, press);
}

void th06_player::onEnableChanged(bool enable)
{
	th_player::onEnableChanged(enable);
}

player th06_player::getPlayerEntity()
{
	vec2 center;
	if (!readVec2(TH06_PLAYER_POS, center) || !isInPlayfield(center))
		return player{ aabb() };

	vec2 sz(2.5f, 2.5f);
	aabb a{
		center - sz / 2,
		vec2(),
		sz,
	};
	return player{ a };
}

void th06_player::doBulletPoll()
{
	for (int i = 0; i < TH06_BULLET_COUNT; ++i)
	{
		uintptr_t bulletAddr = TH06_BULLET_MANAGER + TH06_BULLETS_OFFSET + i * TH06_BULLET_SIZE;
		uint16_t state;
		if (!readValue(bulletAddr + TH06_BULLET_STATE, state))
			continue;
		if (state != 1)
			continue;

		vec2 center;
		vec2 velocity;
		vec2 sz;
		if (!readVec2(bulletAddr + TH06_BULLET_POS, center) ||
			!readVec2(bulletAddr + TH06_BULLET_VEL, velocity) ||
			!readVec2(bulletAddr + TH06_BULLET_HITBOX, sz) ||
			!isInPlayfield(center) ||
			sz.x <= 0 || sz.y <= 0 || sz.x > 64 || sz.y > 64)
			continue;

		aabb a{
			center - sz / 2,
			velocity,
			sz,
		};
		bullets.push_back(bullet{ a });
	}
}

void th06_player::doEnemyPoll()
{
	for (int i = 0; i < TH06_ENEMY_COUNT; ++i)
	{
		uintptr_t enemyAddr = TH06_ENEMY_MANAGER + TH06_ENEMIES_OFFSET + i * TH06_ENEMY_SIZE;
		int32_t life;
		if (!readValue(enemyAddr + TH06_ENEMY_LIFE, life) || life <= 0)
			continue;

		vec2 center;
		vec2 sz;
		vec2 velocity;
		if (!readVec2(enemyAddr + TH06_ENEMY_POS, center) ||
			!readVec2(enemyAddr + TH06_ENEMY_HITBOX, sz) ||
			!readVec2(enemyAddr + TH06_ENEMY_VEL, velocity) ||
			!isInPlayfield(center) ||
			sz.x <= 0 || sz.y <= 0 || sz.x > 128 || sz.y > 128)
			continue;

		aabb a{
			center - sz / 2,
			velocity,
			sz,
		};
		enemies.push_back(enemy{ a });
	}
}

void th06_player::doPowerupPoll()
{
	for (int i = 0; i < TH06_ITEM_COUNT; ++i)
	{
		uintptr_t itemAddr = TH06_ITEM_MANAGER + i * TH06_ITEM_SIZE;
		uint8_t inUse;
		if (!readValue(itemAddr + TH06_ITEM_IN_USE, inUse) || !inUse)
			continue;

		int8_t itemType;
		if (!readValue(itemAddr + TH06_ITEM_TYPE, itemType))
			continue;
		if (itemType < 0)
			continue;

		vec2 center;
		if (!readVec2(itemAddr + TH06_ITEM_POS, center) || !isInPlayfield(center))
			continue;

		vec2 sz(6, 6);
		aabb a{
			center - sz / 2,
			vec2(),
			sz,
		};
		powerups.push_back(powerup{ a, itemType == TH06_ITEM_POINT_BULLET ? 1 : 0 });
	}
}

void th06_player::doLaserPoll()
{
	for (int i = 0; i < TH06_LASER_COUNT; ++i)
	{
		uintptr_t laserAddr = TH06_BULLET_MANAGER + TH06_LASERS_OFFSET + i * TH06_LASER_SIZE;
		int32_t inUse;
		if (!readValue(laserAddr + TH06_LASER_IN_USE, inUse) || !inUse)
			continue;

		float length;
		float width;
		float angle;
		float speed;
		vec2 center;
		if (!readValue(laserAddr + TH06_LASER_END_OFFSET, length) ||
			!readValue(laserAddr + TH06_LASER_WIDTH, width) ||
			!readValue(laserAddr + TH06_LASER_ANGLE, angle) ||
			!readValue(laserAddr + TH06_LASER_SPEED, speed) ||
			!readVec2(laserAddr + TH06_LASER_POS, center))
			continue;
		if (!std::isfinite(length) || !std::isfinite(width) ||
			!std::isfinite(angle) || !std::isfinite(speed) ||
			length <= 0 || width <= 0 || length > 1024 || width > 256 ||
			!isInPlayfield(center))
			continue;

		obb a{
			center,
			length,
			width / 2.f,
			angle,
			vec2(
				speed * cos(angle),
				speed * sin(angle))
		};
		lasers.push_back(laser{ a });
	}
}
