#include "TrillInput.h"
#include "PatternXY.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <libraries/Trill/Trill.h>
#include <unistd.h>

namespace Cfg {
constexpr uint8_t kBarAddr = 0x20;
constexpr uint8_t kSquareAddr = 0x28;
constexpr float kBarTouchMin = 0.10f;
constexpr float kBarSizeMin = 0.15f;
constexpr float kBarSizeMax = 1.50f;
constexpr float kKickDropOn = 0.70f;
constexpr float kKickDropOff = 0.50f;
constexpr float kBarFollowMaxDist = 0.25f;
constexpr float kSquareSizeMin = 0.15f;
constexpr float kSquareSizeMax = 1.50f;
constexpr float kSquareSplitMax = 0.15f;
constexpr float kSquareFollowMaxDist = 0.25f;
constexpr float kSquareXMin = 0.13f;
constexpr float kSquareXMax = 0.91f;
constexpr float kSquareYMin = 0.08f;
constexpr float kSquareYMax = 0.90f;
constexpr unsigned kRetryIntervalUs = 500000;
constexpr unsigned kRetryBackoffUs = 2000000;
constexpr unsigned kRecoverBackoffAfter = 3;
constexpr unsigned kSetupAttempts = 5;
constexpr unsigned kSetupRetryUs = 300000;
constexpr bool kSquareInvertX = false;
constexpr bool kSquareInvertY = true;
constexpr bool kSquareSwapXY = false;
constexpr unsigned kSquarePollUs = 12000;
constexpr unsigned kBarPollUs = 10000;
constexpr unsigned kBarFillOnsetUs = 25000;
constexpr unsigned kBarFillOnsetPolls =
	(kBarFillOnsetUs + kBarPollUs - 1) / kBarPollUs;
constexpr float kBarPinchMinSep = 0.12f;
constexpr float kBarPinchRange = 0.35f;
constexpr bool kBarLedInvert = false;
} // namespace

enum BarMode { kBarIdle = 0, kBarFill = 1, kBarDensity = 2 };

static std::atomic<float> sDbgBarPos{0.f};
static std::atomic<float> sDbgBarSize{0.f};
static std::atomic<uint8_t> sDbgBarNt{0};
static std::atomic<uint8_t> sDbgSqN{0};
static std::atomic<uint8_t> sDbgSqNh{0};
static std::atomic<float> sDbgSqXRaw{0.f};
static std::atomic<float> sDbgSqYRaw{0.f};
static std::atomic<float> sDbgSqSize{0.f};
static std::atomic<uint8_t> sDbgSqMode{0}; // 0 none 1 full 2 edgeX 3 edgeY
static std::atomic<uint8_t> sDbgBarFollow{0};

static Trill sBar;
static Trill sSquare;
static std::atomic<bool> sBarEverOk{false};
static std::atomic<bool> sSquareEverOk{false};
static std::atomic<uint32_t> sSquareXY{0x80008000u};
static std::atomic<uint8_t> sSquareTouch{0};
// bits: 0 active, 1-3 zone, 4-11 intensity U8, 12 kickDrop, 13 densityMode, 14-21 macro U8
static std::atomic<uint32_t> sBarState{0};
static std::atomic<uint32_t> sBarTouches{0};
static int sRateZone = 2;
static bool sKickDrop = false;
static float sLastIntensity = 0.f;
static int sReleaseFrames = 0;
static int sReleaseMax = 660;
static bool sBarLink = false;
static bool sSquareLink = false;
static int sBarRetryUs = 0;
static int sSquareRetryUs = 0;
static int sBarRecoverFails = 0;
static int sSquareRecoverFails = 0;

static unsigned retryIntervalUs(int failCount) {
	if (failCount >= (int)Cfg::kRecoverBackoffAfter)
		return Cfg::kRetryBackoffUs;
	return Cfg::kRetryIntervalUs;
}
static float sLastSqXRaw = 0.f;
static float sLastSqYRaw = 0.f;
static bool sSqWasTouching = false;
static float sLastBarPos = 0.5f;
static bool sBarWasTouching = false;
static BarMode sBarMode = kBarIdle;
static float sPinchRef = 0.2f;
static float sLastPinchE = 0.f;
static int sFillOnsetPolls = 0;

static float mapAxis(float raw, float edgeMin, float edgeMax) {
	const float span = edgeMax - edgeMin;
	if (span <= 0.f)
		return 0.5f;
	float v = (raw - edgeMin) / span;
	return std::max(0.f, std::min(1.f, v));
}

static void mapSquare(float xRaw, float yRaw, float& outX, float& outY) {
	float x = mapAxis(xRaw, Cfg::kSquareXMin, Cfg::kSquareXMax);
	float y = mapAxis(yRaw, Cfg::kSquareYMin, Cfg::kSquareYMax);
	if (Cfg::kSquareInvertX)
		x = 1.f - x;
	if (Cfg::kSquareInvertY)
		y = 1.f - y;
	if (Cfg::kSquareSwapXY) {
		const float t = x;
		x = y;
		y = t;
	}
	outX = x;
	outY = y;
}

static float normSize(float s) {
	const float span = Cfg::kBarSizeMax - Cfg::kBarSizeMin;
	if (span <= 0.f)
		return 0.f;
	return std::max(0.f, std::min(1.f, (s - Cfg::kBarSizeMin) / span));
}

static int zoneFromPos(float pos, int prev) {
	pos = std::max(0.f, std::min(1.f, pos));
	const float w = 1.f / 6.f;
	prev = std::max(0, std::min(5, prev));
	const float lo = prev * w;
	const float hi = (prev + 1) * w;
	const float h = 0.04f;
	if (pos < lo - h && prev > 0)
		return prev - 1;
	if (pos > hi + h && prev < 5)
		return prev + 1;
	return prev;
}

static bool squareSizeOk(float sz) {
	return sz >= Cfg::kSquareSizeMin && sz <= Cfg::kSquareSizeMax;
}

static void squareStoreDbg(float xR, float yR, float sz) {
	sDbgSqXRaw.store(xR, std::memory_order_relaxed);
	sDbgSqYRaw.store(yR, std::memory_order_relaxed);
	sDbgSqSize.store(sz, std::memory_order_relaxed);
}

static int squarePickByNearest(unsigned count, float lastRef,
	bool (*locFn)(unsigned, float&), bool (*szFn)(unsigned, float&)) {
	if (count == 0)
		return -1;
	int pick = 0;
	float bestSz = -1.f;
	for (unsigned i = 0; i < count; ++i) {
		float sz = 0.f;
		if (!szFn(i, sz))
			continue;
		if (sz > bestSz) {
			bestSz = sz;
			pick = (int)i;
		}
	}
	if (sSqWasTouching) {
		float bestDist = Cfg::kSquareFollowMaxDist + 1.f;
		int nearIdx = -1;
		for (unsigned i = 0; i < count; ++i) {
			float loc = 0.f;
			if (!locFn(i, loc))
				continue;
			const float d = std::fabs(loc - lastRef);
			if (d < bestDist) {
				bestDist = d;
				nearIdx = (int)i;
			}
		}
		if (nearIdx >= 0 && bestDist <= Cfg::kSquareFollowMaxDist)
			return nearIdx;
	}
	return pick;
}

static bool squareVLoc(unsigned i, float& out) {
	if (i >= sSquare.getNumTouches())
		return false;
	out = sSquare.touchLocation(i);
	return true;
}

static bool squareVSz(unsigned i, float& out) {
	if (i >= sSquare.getNumTouches())
		return false;
	out = sSquare.touchSize((uint8_t)i);
	return true;
}

static bool squareHLoc(unsigned i, float& out) {
	if (i >= sSquare.getNumHorizontalTouches())
		return false;
	out = sSquare.touchHorizontalLocation(i);
	return true;
}

static bool squareHSz(unsigned i, float& out) {
	if (i >= sSquare.getNumHorizontalTouches())
		return false;
	out = sSquare.touchHorizontalSize((uint8_t)i);
	return true;
}

static void squareBlendVertical(unsigned n, int pick, float& yOut, float& szOut) {
	yOut = sSquare.touchLocation((unsigned)pick);
	szOut = sSquare.touchSize((uint8_t)pick);
	float sum = yOut;
	unsigned cnt = 1;
	for (unsigned i = 0; i < n; ++i) {
		if ((int)i == pick)
			continue;
		const float loc = sSquare.touchLocation(i);
		if (std::fabs(loc - yOut) <= Cfg::kSquareSplitMax) {
			sum += loc;
			cnt++;
			szOut = std::max(szOut, sSquare.touchSize((uint8_t)i));
		}
	}
	yOut = sum / (float)cnt;
}

static void squareBlendHorizontal(unsigned nh, int pick, float& xOut, float& szOut) {
	xOut = sSquare.touchHorizontalLocation((unsigned)pick);
	szOut = sSquare.touchHorizontalSize((uint8_t)pick);
	float sum = xOut;
	unsigned cnt = 1;
	for (unsigned i = 0; i < nh; ++i) {
		if ((int)i == pick)
			continue;
		const float loc = sSquare.touchHorizontalLocation(i);
		if (std::fabs(loc - xOut) <= Cfg::kSquareSplitMax) {
			sum += loc;
			cnt++;
			szOut = std::max(szOut, sSquare.touchHorizontalSize((uint8_t)i));
		}
	}
	xOut = sum / (float)cnt;
}

static void squarePickFull(unsigned n, unsigned nh, float& xRaw, float& yRaw,
	float& ySz, float& xSz) {
	const int vPick = squarePickByNearest(n, sLastSqYRaw, squareVLoc, squareVSz);
	const int hPick =
		squarePickByNearest(nh, sLastSqXRaw, squareHLoc, squareHSz);
	if (vPick >= 0)
		squareBlendVertical(n, vPick, yRaw, ySz);
	if (hPick >= 0)
		squareBlendHorizontal(nh, hPick, xRaw, xSz);
}

// outMode: 0 none 1 full 2 edgeX 3 edgeY
static bool squareParse(unsigned n, unsigned nh, float& xRaw, float& yRaw, float& sz,
	uint8_t& outMode) {
	outMode = 0;
	xRaw = 0.f;
	yRaw = 0.f;
	float ySz = 0.f;
	float xSz = 0.f;

	if (n >= 1 && nh >= 1) {
		squarePickFull(n, nh, xRaw, yRaw, ySz, xSz);
		if (!squareSizeOk(ySz) || !squareSizeOk(xSz))
			return false;
		sz = std::max(ySz, xSz);
		outMode = 1;
		return true;
	}
	if (n >= 1 && nh == 0 && sSqWasTouching) {
		const int vPick = squarePickByNearest(n, sLastSqYRaw, squareVLoc, squareVSz);
		if (vPick < 0)
			return false;
		squareBlendVertical(n, vPick, yRaw, ySz);
		if (!squareSizeOk(ySz))
			return false;
		xRaw = sLastSqXRaw;
		sz = ySz;
		outMode = 3;
		return true;
	}
	if (n == 0 && nh >= 1 && sSqWasTouching) {
		const int hPick =
			squarePickByNearest(nh, sLastSqXRaw, squareHLoc, squareHSz);
		if (hPick < 0)
			return false;
		squareBlendHorizontal(nh, hPick, xRaw, xSz);
		if (!squareSizeOk(xSz))
			return false;
		yRaw = sLastSqYRaw;
		sz = xSz;
		outMode = 2;
		return true;
	}
	return false;
}

static float barPosForLed(float p) {
	if (Cfg::kBarLedInvert)
		p = 1.f - p;
	return std::max(0.f, std::min(1.f, p));
}

static void publishBarTouches(int count, float p0, float p1) {
	count = std::max(0, std::min(2, count));
	p0 = barPosForLed(p0);
	p1 = barPosForLed(p1);
	const uint32_t v = (uint32_t)count
		| ((uint32_t)(int)(p0 * 255.f + 0.5f) << 8)
		| ((uint32_t)(int)(p1 * 255.f + 0.5f) << 16);
	sBarTouches.store(v, std::memory_order_release);
}

static void publishBar(bool active, int zone, float intensity, bool kickDrop,
	bool densityMode, float densityMacro) {
	const int macroU8 =
		(int)(std::max(-1.f, std::min(1.f, densityMacro)) * 127.f + 128.f + 0.5f);
	uint32_t v = (active ? 1u : 0u)
		| ((uint32_t)zone << 1)
		| ((uint32_t)(int)(intensity * 255.f) << 4)
		| (kickDrop ? (1u << 12) : 0u)
		| (densityMode ? (1u << 13) : 0u)
		| ((uint32_t)(macroU8 & 0xFF) << 14);
	sBarState.store(v, std::memory_order_release);
}

static void releaseBarPublished() {
	sKickDrop = false;
	sReleaseFrames = 0;
	sBarWasTouching = false;
	sBarMode = kBarIdle;
	sFillOnsetPolls = 0;
	sLastPinchE = 0.f;
	publishBar(false, sRateZone, 0.f, false, false, 0.f);
	sDbgBarNt.store(0, std::memory_order_relaxed);
	sDbgBarPos.store(0.f, std::memory_order_relaxed);
	sDbgBarSize.store(0.f, std::memory_order_relaxed);
	sDbgBarFollow.store(0, std::memory_order_relaxed);
	publishBarTouches(0, 0.f, 0.f);
}

static bool setupWithRetry(Trill& t, Trill::Device dev, uint8_t addr) {
	for (unsigned i = 0; i < Cfg::kSetupAttempts; ++i) {
		if (i > 0) {
			t.closeI2C();
			usleep(Cfg::kSetupRetryUs);
		}
		if (t.setup(1, dev, addr) == 0)
			return true;
	}
	return false;
}

static void releaseSquarePublished() {
	sSquareTouch.store(0, std::memory_order_release);
	sSqWasTouching = false;
	sDbgSqN.store(0, std::memory_order_relaxed);
	sDbgSqNh.store(0, std::memory_order_relaxed);
	sDbgSqXRaw.store(0.f, std::memory_order_relaxed);
	sDbgSqYRaw.store(0.f, std::memory_order_relaxed);
	sDbgSqSize.store(0.f, std::memory_order_relaxed);
	sDbgSqMode.store(0, std::memory_order_relaxed);
}

static void tryRecoverBar() {
	sBarRetryUs += (int)Cfg::kBarPollUs;
	if (sBarRetryUs < (int)retryIntervalUs(sBarRecoverFails))
		return;
	sBarRetryUs = 0;
	sBar.closeI2C();
	if (sBar.setup(1, Trill::BAR, Cfg::kBarAddr) == 0) {
		sBarLink = true;
		sBarEverOk.store(true, std::memory_order_release);
		sBarRecoverFails = 0;
		sBar.printDetails();
		rt_printf("Trill Bar recovered\n");
	} else {
		sBarRecoverFails++;
	}
}

static void tryRecoverSquare() {
	sSquareRetryUs += (int)Cfg::kSquarePollUs;
	if (sSquareRetryUs < (int)retryIntervalUs(sSquareRecoverFails))
		return;
	sSquareRetryUs = 0;
	sSquare.closeI2C();
	if (sSquare.setup(1, Trill::SQUARE, Cfg::kSquareAddr) == 0) {
		sSquareLink = true;
		sSquareEverOk.store(true, std::memory_order_release);
		sSquareRecoverFails = 0;
		sSquare.printDetails();
		rt_printf("Trill Square recovered\n");
	} else {
		sSquareRecoverFails++;
	}
}

static void pollSquare() {
	if (!sSquareLink) {
		tryRecoverSquare();
		return;
	}
	if (sSquare.readI2C() != 0) {
		if (sSquareLink) {
			rt_printf("Trill Square read error, retrying\n");
			sSquareLink = false;
			releaseSquarePublished();
		}
		return;
	}
	const unsigned n = sSquare.getNumTouches();
	const unsigned nh = sSquare.getNumHorizontalTouches();
	sDbgSqN.store((uint8_t)n, std::memory_order_relaxed);
	sDbgSqNh.store((uint8_t)nh, std::memory_order_relaxed);
	uint8_t touch = 0;
	float xRaw = 0.f, yRaw = 0.f, sz = 0.f;
	uint8_t mode = 0;
	if (squareParse(n, nh, xRaw, yRaw, sz, mode)) {
		touch = 1;
		float x = 0.f, y = 0.f;
		mapSquare(xRaw, yRaw, x, y);
		const uint32_t packed = ((uint32_t)(x * 65535.f) << 16)
			| (uint32_t)(y * 65535.f);
		sSquareXY.store(packed, std::memory_order_release);
		sLastSqXRaw = xRaw;
		sLastSqYRaw = yRaw;
		sDbgSqMode.store(mode, std::memory_order_relaxed);
		squareStoreDbg(xRaw, yRaw, sz);
	} else {
		sDbgSqMode.store(0, std::memory_order_relaxed);
		if (n >= 1 && nh >= 1) {
			float ySz = 0.f, xSz = 0.f;
			squarePickFull(n, nh, xRaw, yRaw, ySz, xSz);
			squareStoreDbg(xRaw, yRaw, std::max(ySz, xSz));
		} else if (n >= 1 && nh == 0) {
			const int vPick = squarePickByNearest(n, sLastSqYRaw, squareVLoc, squareVSz);
			if (vPick >= 0) {
				float ySz = 0.f;
				squareBlendVertical(n, vPick, yRaw, ySz);
				squareStoreDbg(sLastSqXRaw, yRaw, ySz);
			}
		} else if (n == 0 && nh >= 1) {
			const int hPick =
				squarePickByNearest(nh, sLastSqXRaw, squareHLoc, squareHSz);
			if (hPick >= 0) {
				float xSz = 0.f;
				squareBlendHorizontal(nh, hPick, xRaw, xSz);
				squareStoreDbg(xRaw, sLastSqYRaw, xSz);
			}
		} else {
			squareStoreDbg(0.f, 0.f, 0.f);
		}
	}
	sSqWasTouching = (touch != 0);
	sSquareTouch.store(touch, std::memory_order_release);
}

static int barTopTwoTouches(unsigned nt, float& p0, float& p1, float* pinchDist) {
	int i0 = -1;
	int i1 = -1;
	float s0 = -1.f;
	float s1 = -1.f;
	for (unsigned i = 0; i < nt; ++i) {
		const float sz = sBar.touchSize((uint8_t)i);
		if (sz < Cfg::kBarTouchMin)
			continue;
		if (sz > s0) {
			s1 = s0;
			i1 = i0;
			s0 = sz;
			i0 = (int)i;
		} else if (sz > s1) {
			s1 = sz;
			i1 = (int)i;
		}
	}
	if (i0 < 0)
		return 0;
	p0 = sBar.touchLocation((unsigned)i0);
	if (i1 < 0)
		return 1;
	p1 = sBar.touchLocation((unsigned)i1);
	if (pinchDist)
		*pinchDist = std::fabs(p0 - p1);
	return 2;
}

static void pollBar(bool& touching, float& intensity) {
	touching = false;
	intensity = 0.f;
	if (!sBarLink) {
		tryRecoverBar();
		return;
	}
	if (sBar.readI2C() != 0) {
		if (sBarLink) {
			rt_printf("Trill Bar read error, retrying\n");
			sBarLink = false;
			releaseBarPublished();
		}
		return;
	}
	const unsigned nt = sBar.getNumTouches();
	sDbgBarNt.store((uint8_t)nt, std::memory_order_relaxed);
	if (nt == 0) {
		sFillOnsetPolls = 0;
		if (sBarMode == kBarDensity) {
			sBarMode = kBarIdle;
			sLastPinchE = 0.f;
			publishBar(false, sRateZone, 0.f, false, false, 0.f);
		}
		sBarWasTouching = false;
		sDbgBarPos.store(0.f, std::memory_order_relaxed);
		sDbgBarSize.store(0.f, std::memory_order_relaxed);
		sDbgBarFollow.store(0, std::memory_order_relaxed);
		publishBarTouches(0, 0.f, 0.f);
		return;
	}

	float topP0 = 0.f;
	float topP1 = 0.f;
	float pinchDist = 0.f;
	const int nValid = barTopTwoTouches(nt, topP0, topP1, &pinchDist);
	if (nValid >= 2) {
		publishBarTouches(2, topP0, topP1);
		if (pinchDist >= Cfg::kBarPinchMinSep) {
			if (sBarMode != kBarDensity) {
				sBarMode = kBarDensity;
				sPinchRef = pinchDist;
				sLastPinchE = 0.f;
				sReleaseFrames = 0;
				sFillOnsetPolls = 0;
			}
			float e = (pinchDist - sPinchRef) / Cfg::kBarPinchRange;
			e = std::max(-1.f, std::min(1.f, e));
			sLastPinchE = e;
			publishBar(false, sRateZone, 0.f, false, true, e);
			return;
		}
		if (sBarMode == kBarDensity) {
			publishBar(false, sRateZone, 0.f, false, true, sLastPinchE);
			return;
		}
	}

	if (sBarMode == kBarDensity) {
		if (nValid >= 2)
			publishBarTouches(2, topP0, topP1);
		else if (nValid == 1)
			publishBarTouches(1, topP0, 0.f);
		else
			publishBarTouches(0, 0.f, 0.f);
		publishBar(false, sRateZone, 0.f, false, true, sLastPinchE);
		return;
	}

	int pick = -1;
	if (sBarWasTouching && sBarMode == kBarFill) {
		float bestDist = Cfg::kBarFollowMaxDist + 1.f;
		for (unsigned i = 0; i < nt; ++i) {
			const float p = sBar.touchLocation(i);
			const float d = std::fabs(p - sLastBarPos);
			if (d < bestDist) {
				bestDist = d;
				pick = (int)i;
			}
		}
		if (pick < 0 || bestDist > Cfg::kBarFollowMaxDist)
			pick = -1;
	}
	if (pick < 0) {
		float best = -1.f;
		for (unsigned i = 0; i < nt; ++i) {
			const float sz = sBar.touchSize((uint8_t)i);
			if (sz > best) {
				best = sz;
				pick = (int)i;
			}
		}
	}
	if (pick < 0) {
		publishBarTouches(0, 0.f, 0.f);
		return;
	}
	const float pos = sBar.touchLocation((unsigned)pick);
	const float size = sBar.touchSize((uint8_t)pick);
	sDbgBarFollow.store((uint8_t)pick, std::memory_order_relaxed);
	if (size < Cfg::kBarTouchMin) {
		sBarWasTouching = false;
		sFillOnsetPolls = 0;
		sDbgBarPos.store(0.f, std::memory_order_relaxed);
		sDbgBarSize.store(0.f, std::memory_order_relaxed);
		publishBarTouches(0, 0.f, 0.f);
		return;
	}

	if (nValid < 2)
		publishBarTouches(1, pos, 0.f);

	if (sBarMode == kBarIdle) {
		sFillOnsetPolls++;
		if (sFillOnsetPolls < (int)Cfg::kBarFillOnsetPolls)
			return;
		sBarMode = kBarFill;
	}

	touching = true;
	sLastBarPos = pos;
	sBarWasTouching = true;
	sRateZone = zoneFromPos(pos, sRateZone);
	intensity = normSize(size);
	sDbgBarPos.store(pos, std::memory_order_relaxed);
	sDbgBarSize.store(size, std::memory_order_relaxed);
	sLastIntensity = intensity;
	sReleaseFrames = sReleaseMax;
	if (sKickDrop) {
		if (intensity < Cfg::kKickDropOff)
			sKickDrop = false;
	} else if (intensity >= Cfg::kKickDropOn) {
		sKickDrop = true;
	}
}

static void squareAuxLoop(void*) {
	while (!Bela_stopRequested()) {
		pollSquare();
#if GRIDS_TRILL_DEBUG
		static unsigned dbgSq;
		if (++dbgSq >= 40) {
			dbgSq = 0;
			static const char* kSqMode[] = {"none", "full", "edgeX", "edgeY"};
			const uint8_t sm = sDbgSqMode.load(std::memory_order_relaxed);
			const char* modeStr = sm < 4 ? kSqMode[sm] : "?";
			const uint32_t sq = sSquareXY.load(std::memory_order_acquire);
			const float mx = (sq >> 16) / 65535.f;
			const float my = (sq & 0xFFFFu) / 65535.f;
			rt_printf("Sq n=%u nh=%u xR=%.3f yR=%.3f sz=%.3f "
				"map=%.3f,%.3f touch=%u mode=%s link=%d\n",
				(unsigned)sDbgSqN.load(), (unsigned)sDbgSqNh.load(),
				sDbgSqXRaw.load(), sDbgSqYRaw.load(), sDbgSqSize.load(),
				mx, my, (unsigned)sSquareTouch.load(), modeStr,
				sSquareLink ? 1 : 0);
		}
#endif
		usleep(Cfg::kSquarePollUs);
	}
}

static void barAuxLoop(void*) {
	while (!Bela_stopRequested()) {
		bool touching = false;
		float intensity = 0.f;
		pollBar(touching, intensity);

		if (sBarMode == kBarDensity) {
			// fill/mute state published in pollBar
		} else if (touching) {
			publishBar(true, sRateZone, intensity, sKickDrop, false, 0.f);
		} else if (sBarLink
			|| sBarEverOk.load(std::memory_order_acquire)) {
			if (sReleaseFrames > 0) {
				sReleaseFrames -= (int)(22050.f * Cfg::kBarPollUs / 1e6f + 0.5f);
				if (sReleaseFrames < 0)
					sReleaseFrames = 0;
				publishBar(true, sRateZone, sLastIntensity, sKickDrop, false, 0.f);
			} else {
				sKickDrop = false;
				sBarMode = kBarIdle;
				publishBar(false, sRateZone, 0.f, false, false, 0.f);
			}
		}

#if GRIDS_TRILL_DEBUG
		static unsigned dbgBar;
		if (++dbgBar >= 40) {
			dbgBar = 0;
			const uint32_t b = sBarState.load(std::memory_order_acquire);
			const int zone = (int)((b >> 1) & 7u);
			const float norm = ((b >> 4) & 0xFFu) / 255.f;
			const int macroU8 = (int)((b >> 14) & 0xFFu);
			const float macro = ((float)macroU8 - 128.f) / 127.f;
			rt_printf("Bar nt=%u fi=%u pos=%.3f size=%.3f norm=%.3f zone=%d "
				"kickDrop=%d dens=%d macro=%.2f link=%d\n",
				(unsigned)sDbgBarNt.load(), (unsigned)sDbgBarFollow.load(),
				sDbgBarPos.load(), sDbgBarSize.load(), norm, zone,
				(b & (1u << 12)) ? 1 : 0, (b & (1u << 13)) ? 1 : 0, macro,
				sBarLink ? 1 : 0);
		}
#endif
		usleep(Cfg::kBarPollUs);
	}
}

bool TrillInput::setup() {
	sBarEverOk.store(false, std::memory_order_release);
	sSquareEverOk.store(false, std::memory_order_release);

	const bool barUp =
		setupWithRetry(sBar, Trill::BAR, Cfg::kBarAddr);
	if (barUp) {
		sBarEverOk.store(true, std::memory_order_release);
		sBar.printDetails();
	} else
		fprintf(stderr, "pepper-grids-clone: Trill Bar missing\n");

	const bool sqUp =
		setupWithRetry(sSquare, Trill::SQUARE, Cfg::kSquareAddr);
	if (sqUp) {
		sSquareEverOk.store(true, std::memory_order_release);
		sSquare.printDetails();
	} else
		fprintf(stderr, "pepper-grids-clone: Trill Square missing\n");

	sBarLink = barUp;
	sSquareLink = sqUp;
	Bela_runAuxiliaryTask(squareAuxLoop);
	Bela_runAuxiliaryTask(barAuxLoop);
	return true;
}

void TrillInput::publishToPattern(PadSample& out) const {
	out.present = sSquareEverOk.load(std::memory_order_acquire);
	out.touching = sSquareTouch.load(std::memory_order_acquire) != 0;
	const uint32_t u = sSquareXY.load(std::memory_order_acquire);
	out.x = (u >> 16) / 65535.f;
	out.y = (u & 0xFFFFu) / 65535.f;
}

BarSample TrillInput::bar() const {
	const uint32_t v = sBarState.load(std::memory_order_acquire);
	BarSample b;
	b.touching = (v & 1u) != 0;
	const int zone = (int)((v >> 1) & 7u);
	static const int kHits[] = {1, 2, 3, 4, 6, 8};
	const int z = std::max(0, std::min(5, zone));
	b.hitsPerStep = kHits[z];
	b.position = (z + 0.5f) / 6.f;
	b.size = ((v >> 4) & 0xFFu) / 255.f;
	b.kickDrop = (v & (1u << 12)) != 0;
	b.densityMode = (v & (1u << 13)) != 0;
	const int macroU8 = (int)((v >> 14) & 0xFFu);
	b.densityMacro = ((float)macroU8 - 128.f) / 127.f;
	return b;
}

BarTouches TrillInput::barTouches() const {
	const uint32_t v = sBarTouches.load(std::memory_order_acquire);
	BarTouches t;
	t.count = (int)(v & 3u);
	t.pos[0] = ((v >> 8) & 0xFFu) / 255.f;
	t.pos[1] = ((v >> 16) & 0xFFu) / 255.f;
	return t;
}

bool TrillInput::anyTouch() const {
	return fillActive()
		|| (sSquareEverOk.load(std::memory_order_acquire)
			&& sSquareTouch.load() != 0);
}
