#pragma once

#include "TrillInput.h"
#include "TriggerOut.h"

class FillBar {
public:
	void configure(int releaseFrames);
	void setBar(const BarSample& b);
	void process(float subPhase, TriggerOut& out, int* ledFlash);

	bool active() const { return mActive; }
	int hitsPerStep() const { return mHits; }

private:
	bool mActive = false;
	bool mKickDrop = false;
	int mHits = 1;
	int mLastIdx = -1;
	float mLastPhase = 0.f;
	float mIntensity = 0.f;

};
