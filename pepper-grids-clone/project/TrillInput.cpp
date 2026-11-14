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
constexpr bool kSquareInvertX = false;
constexpr bool kSquareInvertY = true;
constexpr bool kSquareSwapXY = false;
constexpr unsigned kPollUs = 5000;
} // namespace

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
static bool sBarOk = false;
static bool sSquareOk = false;
static std::atomic<uint32_t> sSquareXY{0x80008000u};
static std::atomic<uint8_t> sSquareTouch{0};
static std::atomic<uint32_t> sBarState{0}; // bits: active, kickDrop, zone[0..5], intensity U8
static int sRateZone = 2;
static bool sKickDrop = false;
static float sLastIntensity = 0.f;
static int sReleaseFrames = 0;
static int sReleaseMax = 660;
static bool sBarLink = false;
static bool sSquareLink = false;
static int sBarRetryUs = 0;
static int sSquareRetryUs = 0;
static float sLastSqXRaw = 0.f;
static float sLastSqYRaw = 0.f;
static bool sSqWasTouching = false;
static float sLastBarPos = 0.5f;
static bool sBarWasTouching = false;

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

static void publishBar(bool active, int zone, float intensity, bool kickDrop) {
	uint32_t v = (active ? 1u : 0u)
		| ((uint32_t)zone << 1)
		| ((uint32_t)(int)(intensity * 255.f) << 4)
		| (kickDrop ? (1u << 12) : 0u);
	sBarState.store(v, std::memory_order_release);
}

static void releaseBarPublished() {
	sKickDrop = false;
	sReleaseFrames = 0;
	sBarWasTouching = false;
	publishBar(false, sRateZone, 0.f, false);
	sDbgBarNt.store(0, std::memory_order_relaxed);
	sDbgBarPos.store(0.f, std::memory_order_relaxed);
	sDbgBarSize.store(0.f, std::memory_order_relaxed);
	sDbgBarFollow.store(0, std::memory_order_relaxed);
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
	sBarRetryUs += (int)Cfg::kPollUs;
	if (sBarRetryUs < (int)Cfg::kRetryIntervalUs)
		return;
	sBarRetryUs = 0;
	if (sBar.setup(1, Trill::BAR, Cfg::kBarAddr) == 0) {
		sBarLink = true;
		rt_printf("Trill Bar recovered\n");
	}
}

static void tryRecoverSquare() {
	sSquareRetryUs += (int)Cfg::kPollUs;
	if (sSquareRetryUs < (int)Cfg::kRetryIntervalUs)
		return;
	sSquareRetryUs = 0;
	if (sSquare.setup(1, Trill::SQUARE, Cfg::kSquareAddr) == 0) {
		sSquareLink = true;
		rt_printf("Trill Square recovered\n");
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
		sBarWasTouching = false;
		sDbgBarPos.store(0.f, std::memory_order_relaxed);
		sDbgBarSize.store(0.f, std::memory_order_relaxed);
		sDbgBarFollow.store(0, std::memory_order_relaxed);
		return;
	}
	int pick = -1;
	if (sBarWasTouching) {
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
	const float pos = sBar.touchLocation((unsigned)pick);
	const float size = sBar.touchSize((uint8_t)pick);
	sDbgBarFollow.store((uint8_t)pick, std::memory_order_relaxed);
	if (size < Cfg::kBarTouchMin) {
		sBarWasTouching = false;
		sDbgBarPos.store(0.f, std::memory_order_relaxed);
		sDbgBarSize.store(0.f, std::memory_order_relaxed);
		return;
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

static void trillAuxLoop(void*) {
	while (!Bela_stopRequested()) {
		if (sSquareOk)
			pollSquare();

		bool touching = false;
		float intensity = 0.f;
		if (sBarOk)
			pollBar(touching, intensity);

		if (touching) {
			publishBar(true, sRateZone, intensity, sKickDrop);
		} else if (sBarLink || sBarOk) {
			if (sReleaseFrames > 0) {
				sReleaseFrames -= (int)(22050.f * Cfg::kPollUs / 1e6f + 0.5f);
				if (sReleaseFrames < 0)
					sReleaseFrames = 0;
				publishBar(true, sRateZone, sLastIntensity, sKickDrop);
			} else {
				sKickDrop = false;
				publishBar(false, sRateZone, 0.f, false);
			}
		}

#if GRIDS_TRILL_DEBUG
		static unsigned dbg;
		if (++dbg >= 40) {
			dbg = 0;
			const uint32_t b = sBarState.load(std::memory_order_acquire);
			const int zone = (int)((b >> 1) & 7u);
			const float norm = ((b >> 4) & 0xFFu) / 255.f;
			const uint32_t sq = sSquareXY.load(std::memory_order_acquire);
			const float mx = (sq >> 16) / 65535.f;
			const float my = (sq & 0xFFFFu) / 65535.f;
			static const char* kSqMode[] = {"none", "full", "edgeX", "edgeY"};
			const uint8_t sm = sDbgSqMode.load(std::memory_order_relaxed);
			const char* modeStr = sm < 4 ? kSqMode[sm] : "?";
			rt_printf("Bar nt=%u fi=%u pos=%.3f size=%.3f norm=%.3f zone=%d "
				"kickDrop=%d | Sq n=%u nh=%u xR=%.3f yR=%.3f sz=%.3f "
				"map=%.3f,%.3f touch=%u mode=%s\n",
				(unsigned)sDbgBarNt.load(), (unsigned)sDbgBarFollow.load(),
				sDbgBarPos.load(), sDbgBarSize.load(), norm, zone,
				(b & (1u << 12)) ? 1 : 0,
				(unsigned)sDbgSqN.load(), (unsigned)sDbgSqNh.load(),
				sDbgSqXRaw.load(), sDbgSqYRaw.load(), sDbgSqSize.load(),
				mx, my, (unsigned)sSquareTouch.load(), modeStr);
		}
#endif
		usleep(Cfg::kPollUs);
	}
}

bool TrillInput::setup() {
	sBarOk = (sBar.setup(1, Trill::BAR, Cfg::kBarAddr) == 0);
	if (sBarOk)
		sBar.printDetails();
	else
		fprintf(stderr, "pepper-grids-clone: Trill Bar missing\n");

	sSquareOk = (sSquare.setup(1, Trill::SQUARE, Cfg::kSquareAddr) == 0);
	if (sSquareOk)
		sSquare.printDetails();
	else
		fprintf(stderr, "pepper-grids-clone: Trill Square missing\n");

	mBarOk = sBarOk;
	mSquareOk = sSquareOk;
	sBarLink = sBarOk;
	sSquareLink = sSquareOk;
	if (mBarOk || mSquareOk)
		Bela_runAuxiliaryTask(trillAuxLoop);
	return true;
}

void TrillInput::publishToPattern(PadSample& out) const {
	out.present = mSquareOk;
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
	return b;
}

bool TrillInput::anyTouch() const {
	return fillActive() || (mSquareOk && sSquareTouch.load() != 0);
}
