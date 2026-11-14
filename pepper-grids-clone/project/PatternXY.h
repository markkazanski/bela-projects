#pragma once

#include <Bela.h>

struct PadSample {
	bool present = false;
	bool touching = false;
	float x = 0.5f;
	float y = 0.5f;
};

class PatternXY {
public:
	void clearLatch();
	void update(const PadSample& pad, float knobX, float knobY, BelaContext* ctx);

	float x() const { return mOutX; }
	float y() const { return mOutY; }
	bool squareActive() const { return mSquareSource; }

	uint8_t mapX() const { return (uint8_t)(mOutX * 255.f + 0.5f); }
	uint8_t mapY() const { return (uint8_t)(mOutY * 255.f + 0.5f); }

private:
	bool mSquareSource = false;
	bool mWasTouching = false;
	float mPotRefX = 0.f;
	float mPotRefY = 0.f;
	float mTargetX = 0.5f;
	float mTargetY = 0.5f;
	float mOutX = 0.5f;
	float mOutY = 0.5f;

	void slew(BelaContext* ctx);
};
