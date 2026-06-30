#pragma once
#include "th_player.h"

class th09_player : public th_player
{
public:
	th09_player() : th_player(gs_addr{ (uint8_t*)0x004A7D94, (uint8_t*)0x004A7EC4 }) {}
	~th09_player() = default;

	void onInit() override;
	void onTick() override;
	void onBeginTick() override;
	void onAfterTick() override;
	void draw(IDirect3DDevice9* d3dDev) override;

	void handleInput(const BYTE diKeys[256], const BYTE press[256]) override;
	void onEnableChanged(bool enable) override;

private:
	void doBulletPoll();

	player getPlayerEntity() override;
};
