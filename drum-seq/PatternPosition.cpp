#include "PatternPosition.h"
#include <algorithm>
#include <cmath>

void PatternPosition::update(const TrillPadState& pad, float potX, float potY,
	BelaContext* context) {
	potX = std::max(0.f, std::min(1.f, potX));
	potY = std::max(0.f, std::min(1.f, potY));

	if (!pad.present) {
		mSourceSquare = false;
		mPadWasTouching = false;
		mTargetX = potX;
		mTargetY = potY;
		mOutX = potX;
		mOutY = potY;
		return;
	}

	const bool touchDown = pad.touching && !mPadWasTouching;
	mPadWasTouching = pad.touching;

	if (touchDown) {
		mSourceSquare = true;
		mPotRefX = potX;
		mPotRefY = potY;
	}

	if (mSourceSquare) {
		const float dx = std::fabs(potX - mPotRefX);
		const float dy = std::fabs(potY - mPotRefY);
		if (dx > TrillCfg::kPotTakeoverDelta || dy > TrillCfg::kPotTakeoverDelta) {
			mSourceSquare = false;
		}
	}

	if (mSourceSquare) {
		if (pad.touching) {
			mTargetX = pad.x;
			mTargetY = pad.y;
		}
		applySlew(potX, potY, context);
	} else {
		mTargetX = potX;
		mTargetY = potY;
		mOutX = potX;
		mOutY = potY;
	}
}

void PatternPosition::applySlew(float /*potX*/, float /*potY*/, BelaContext* context) {
	if (TrillCfg::kSquareSlewMs <= 0.f) {
		mOutX = mTargetX;
		mOutY = mTargetY;
		return;
	}
	const float blockSec = (context->analogFrames > 0 && context->analogSampleRate > 0.f)
		? (float)context->analogFrames / context->analogSampleRate
		: 0.001f;
	const float tau = TrillCfg::kSquareSlewMs * 0.001f;
	const float k = 1.f - std::exp(-blockSec / std::max(tau, 1e-6f));
	mOutX += (mTargetX - mOutX) * k;
	mOutY += (mTargetY - mOutY) * k;
}
