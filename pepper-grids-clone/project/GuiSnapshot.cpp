#include "GuiSnapshot.h"
#include "GridsEngine.h"
#include "Patterns.h"
#include <atomic>
#include <cstring>

namespace {
constexpr int kAccentThreshold = 192;
constexpr int kFmVoice = 3;

std::atomic<uint8_t> sMapX{128};
std::atomic<uint8_t> sMapY{128};
std::atomic<uint8_t> sDensity[4];
std::atomic<uint8_t> sPert[4];
std::atomic<int> sClockStep{0};
std::atomic<int> sAudibleStep{0};
std::atomic<int> sFreezeStart{0};
std::atomic<uint32_t> sFlags{0};
std::atomic<float> sBpm{120.f};
std::atomic<uint8_t> sSwing{0};
std::atomic<uint8_t> sChaos{0};
std::atomic<uint32_t> sSeq{0};

static float levelWithPert(uint8_t raw, uint8_t pert) {
	if (raw == 0)
		return 0.f;
	uint8_t level = raw;
	if (level < 255 - pert)
		level = (uint8_t)(level + pert);
	else
		level = 255;
	return (float)level;
}

static void packBits(const bool* bits, int count, float out[4]) {
	uint32_t w[4] = {};
	for (int i = 0; i < count; ++i) {
		if (bits[i])
			w[i >> 5] |= (1u << (i & 31));
	}
	static_assert(sizeof(float) == sizeof(uint32_t), "");
	for (int i = 0; i < 4; ++i)
		std::memcpy(&out[i], &w[i], sizeof(float));
}
} // namespace

void GuiSnapshot::publish(uint8_t mapX, uint8_t mapY, const uint8_t density[4],
	const uint8_t perturb[4], int clockStep, int audibleStep, int freezeStart,
	bool running, bool frozen, bool fillActive, bool kickDrop, bool squareSource,
	float bpm, uint8_t swing255, uint8_t chaos, bool muteBd, bool muteSd,
	bool muteHh, bool muteFm) {
	sMapX.store(mapX, std::memory_order_release);
	sMapY.store(mapY, std::memory_order_release);
	for (int p = 0; p < 4; ++p) {
		sDensity[p].store(density[p], std::memory_order_release);
		sPert[p].store(perturb[p], std::memory_order_release);
	}
	sClockStep.store(clockStep, std::memory_order_release);
	sAudibleStep.store(audibleStep, std::memory_order_release);
	sFreezeStart.store(freezeStart, std::memory_order_release);
	uint32_t f = 0;
	if (running)
		f |= 1u << 0;
	if (frozen)
		f |= 1u << 1;
	if (fillActive)
		f |= 1u << 2;
	if (kickDrop)
		f |= 1u << 3;
	if (squareSource)
		f |= 1u << 4;
	if (muteBd)
		f |= 1u << 5;
	if (muteSd)
		f |= 1u << 6;
	if (muteHh)
		f |= 1u << 7;
	if (muteFm)
		f |= 1u << 8;
	sFlags.store(f, std::memory_order_release);
	sBpm.store(bpm, std::memory_order_release);
	sSwing.store(swing255, std::memory_order_release);
	sChaos.store(chaos, std::memory_order_release);
	sSeq.fetch_add(1, std::memory_order_release);
}

void GuiSnapshot::copyTo(float* out, unsigned maxFloats) {
	if (maxFloats < (unsigned)kBufferFloats)
		return;
	const uint8_t mapX = sMapX.load(std::memory_order_acquire);
	const uint8_t mapY = sMapY.load(std::memory_order_acquire);
	uint8_t density[4], pert[4];
	for (int p = 0; p < 4; ++p) {
		density[p] = sDensity[p].load(std::memory_order_acquire);
		pert[p] = sPert[p].load(std::memory_order_acquire);
	}
	const uint32_t flags = sFlags.load(std::memory_order_acquire);
	const bool muteBd = (flags & (1u << 5)) != 0;
	const bool muteSd = (flags & (1u << 6)) != 0;
	const bool muteHh = (flags & (1u << 7)) != 0;
	const bool muteFm = (flags & (1u << 8)) != 0;

	bool fire[kLevels] = {};
	bool accent[kLevels] = {};
	for (int p = 0; p < Patterns::kParts; ++p) {
		if (density[p] == 0) {
			for (int s = 0; s < Patterns::kSteps; ++s)
				out[p * Patterns::kSteps + s] = 0.f;
			continue;
		}
		if ((p == 0 && muteBd) || (p == 1 && muteSd) || (p == 2 && muteHh)) {
			for (int s = 0; s < Patterns::kSteps; ++s)
				out[p * Patterns::kSteps + s] = 0.f;
			continue;
		}
		const uint8_t thresh = (uint8_t)~density[p];
		for (int s = 0; s < Patterns::kSteps; ++s) {
			const int idx = p * Patterns::kSteps + s;
			const uint8_t raw = GridsEngine::levelAt((uint8_t)s, (uint8_t)p, mapX, mapY);
			const float level = levelWithPert(raw, pert[p]);
			out[idx] = level;
			if (level > (float)thresh) {
				fire[idx] = true;
				if (level > (float)kAccentThreshold)
					accent[idx] = true;
			}
		}
	}
	{
		const int p = kFmVoice;
		if (density[p] == 0 || muteFm) {
			for (int s = 0; s < Patterns::kSteps; ++s)
				out[p * Patterns::kSteps + s] = 0.f;
		} else {
			const uint8_t thresh = (uint8_t)~density[p];
			for (int s = 0; s < Patterns::kSteps; ++s) {
				const int idx = p * Patterns::kSteps + s;
				const uint8_t raw = GridsEngine::fmLevelAt((uint8_t)s, mapX, mapY);
				const float level = levelWithPert(raw, pert[p]);
				out[idx] = level;
				if (level > (float)thresh) {
					fire[idx] = true;
					if (level > (float)kAccentThreshold)
						accent[idx] = true;
				}
			}
		}
	}
	packBits(fire, kLevels, out + 128);
	packBits(accent, kLevels, out + 132);
	out[136] = (float)mapX;
	out[137] = (float)mapY;
	out[138] = (float)sClockStep.load(std::memory_order_acquire);
	out[139] = (float)sAudibleStep.load(std::memory_order_acquire);
	out[140] = (float)sFreezeStart.load(std::memory_order_acquire);
	out[141] = (float)flags;
	out[142] = sBpm.load(std::memory_order_acquire);
	out[143] = (float)sSwing.load(std::memory_order_acquire);
	out[144] = (float)sChaos.load(std::memory_order_acquire);
	out[145] = (float)density[0];
	out[146] = (float)density[1];
	out[147] = (float)density[2];
	out[148] = (float)density[3];
}