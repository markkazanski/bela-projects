#pragma once

#include "TrillControls.h"
#include <Bela.h>

class PatternPosition {
public:
	void update(const TrillPadState& pad, float potX, float potY, BelaContext* context);

	float x() const { return mOutX; }
	float y() const { return mOutY; }
	bool squareIsSource() const { return mSourceSquare; }

private:
	bool mSourceSquare = false;
	bool mPadWasTouching = false;
	float mTargetX = 0.5f;
	float mTargetY = 0.5f;
	float mOutX = 0.5f;
	float mOutY = 0.5f;
	float mPotRefX = 0.f;
	float mPotRefY = 0.f;

	void applySlew(float potX, float potY, BelaContext* context);
};
