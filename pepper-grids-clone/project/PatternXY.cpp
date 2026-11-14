#include "PatternXY.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr float kTakeover = 0.06f;
constexpr float kSlewMs = 120.f;
} // namespace

void PatternXY::clearLatch() { mSquareSource = false; }

void PatternXY::update(const PadSample& pad, float knobX, float knobY, BelaContext* ctx) {
	knobX = std::max(0.f, std::min(1.f, knobX));
	knobY = std::max(0.f, std::min(1.f, knobY));

	if (!pad.present) {
		mSquareSource = false;
		mWasTouching = false;
		mTargetX = knobX;
		mTargetY = knobY;
		mOutX = knobX;
		mOutY = knobY;
		return;
	}

	const bool touchDown = pad.touching && !mWasTouching;
	mWasTouching = pad.touching;
	if (touchDown) {
		mSquareSource = true;
		mPotRefX = knobX;
		mPotRefY = knobY;
	}

	if (mSquareSource) {
		const float dx = std::fabs(knobX - mPotRefX);
		const float dy = std::fabs(knobY - mPotRefY);
		if (dx > kTakeover || dy > kTakeover)
			mSquareSource = false;
	}

	if (mSquareSource) {
		if (pad.touching) {
			mTargetX = pad.x;
			mTargetY = pad.y;
		}
		slew(ctx);
	} else {
		mTargetX = knobX;
		mTargetY = knobY;
		slew(ctx);
	}
}

void PatternXY::slew(BelaContext* ctx) {
	if (kSlewMs <= 0.f) {
		mOutX = mTargetX;
		mOutY = mTargetY;
		return;
	}
	const float block = (ctx->analogFrames > 0 && ctx->analogSampleRate > 0.f)
		? (float)ctx->analogFrames / ctx->analogSampleRate
		: 0.001f;
	const float tau = kSlewMs * 0.001f;
	const float k = 1.f - std::exp(-block / std::max(tau, 1e-6f));
	mOutX += (mTargetX - mOutX) * k;
	mOutY += (mTargetY - mOutY) * k;
}
