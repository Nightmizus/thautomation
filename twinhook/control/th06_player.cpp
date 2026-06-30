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

	vec2 readVec2(uintptr_t addr)
	{
		return vec2(*(float*)addr, *(float*)(addr + sizeof(float)));
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
	vec2 sz(2.5f, 2.5f);
	aabb a{
		readVec2(TH06_PLAYER_POS) - sz / 2,
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
		uint16_t state = *(uint16_t*)(bulletAddr + TH06_BULLET_STATE);
		if (state != 1)
			continue;

		vec2 center = readVec2(bulletAddr + TH06_BULLET_POS);
		vec2 velocity = readVec2(bulletAddr + TH06_BULLET_VEL);
		vec2 sz = readVec2(bulletAddr + TH06_BULLET_HITBOX);
		if (sz.x <= 0 || sz.y <= 0)
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
		if (*(int32_t*)(enemyAddr + TH06_ENEMY_LIFE) <= 0)
			continue;

		vec2 center = readVec2(enemyAddr + TH06_ENEMY_POS);
		vec2 sz = readVec2(enemyAddr + TH06_ENEMY_HITBOX);
		if (center.x < -64 || center.x > th_param.GAME_WIDTH + 64 ||
			center.y < -64 || center.y > th_param.GAME_HEIGHT + 64 ||
			sz.x <= 0 || sz.y <= 0)
			continue;

		aabb a{
			center - sz / 2,
			readVec2(enemyAddr + TH06_ENEMY_VEL),
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
		if (!*(uint8_t*)(itemAddr + TH06_ITEM_IN_USE))
			continue;

		int8_t itemType = *(int8_t*)(itemAddr + TH06_ITEM_TYPE);
		if (itemType < 0)
			continue;

		vec2 sz(6, 6);
		aabb a{
			readVec2(itemAddr + TH06_ITEM_POS) - sz / 2,
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
		if (!*(int32_t*)(laserAddr + TH06_LASER_IN_USE))
			continue;

		float length = *(float*)(laserAddr + TH06_LASER_END_OFFSET);
		float width = *(float*)(laserAddr + TH06_LASER_WIDTH);
		if (length <= 0 || width <= 0)
			continue;

		obb a{
			readVec2(laserAddr + TH06_LASER_POS),
			length,
			width / 2.f,
			*(float*)(laserAddr + TH06_LASER_ANGLE),
			vec2(
				*(float*)(laserAddr + TH06_LASER_SPEED) * cos(*(float*)(laserAddr + TH06_LASER_ANGLE)),
				*(float*)(laserAddr + TH06_LASER_SPEED) * sin(*(float*)(laserAddr + TH06_LASER_ANGLE)))
		};
		lasers.push_back(laser{ a });
	}
}
