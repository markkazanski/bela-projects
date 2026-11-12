#include "Grooves.h"
#include <cstring>

namespace Grooves {

Groove gGrid[kGridSize][kGridSize];

uint8_t parseLaneChar(char c) {
	if (c == '.' || c == ' ')
		return 0;
	if (c >= '1' && c <= '9')
		return (uint8_t)((c - '0') * 255 / 9);
	return 0;
}

static void parseLane(const char* text, uint8_t* out) {
	int step = 0;
	for (const char* p = text; *p && step < PepperIO::kStepsPerBar; ++p) {
		if (*p == ' ')
			continue;
		out[step++] = parseLaneChar(*p);
	}
	for (; step < PepperIO::kStepsPerBar; ++step)
		out[step] = 0;
}

struct GrooveText {
	const char* name;
	const char* kick;
	const char* snare;
	const char* hat;
	const char* hatOpen;
	const char* fm;
};

static const GrooveText kSource[kGridSize][kGridSize] = {
	{
		{"techno", "9..2 8... 9..3 8.4.", ".... 9..2 .... 9.3.", "4393 4393 4393 4395",
		 "..4. ..4. ..4. ..6.", ".... ..7. .3.. .5.8"},
		{"halftime", "9... ..6. .... 3.5.", ".... .2.. 9... ..3.", "7243 7243 7243 7253",
		 ".... .... .... ..8.", "...3 ...6 .... 5.7."},
		{"electro", "9..6 .... ..8. .4..", ".... 9... .... 9..3", "7353 7353 7353 7363",
		 ".... .... .... ....", "..7. ..4. 6... ..5."},
	},
	{
		{"house", "9... 8... 9... 8..3", ".... 9... .... 9.2.", "3294 3294 3294 3295",
		 "..8. ..8. ..8. ..8.", ".... ..6. ...7 .3.."},
		{"breakbeat", "9.7. .... ..85 ....", ".... 9..4 .3.. 9..6", "7253 7253 7253 7254",
		 ".... .... .... ..6.", "...5 ..3. .7.. ...4"},
		{"2-step", "9... ...4 ..8. .5..", ".... 9... .... 9..3", "3.96 3.96 3.96 3.97",
		 "..3. ..3. ..3. ..5.", ".5.. ...6 .... ..7."},
	},
	{
		{"poly-techno", "9... 8... 9... 8...", ".... 6... .... 7...", "4696 4696 4696 4697",
		 ".... ..5. .... ..5.", "9..8 ..8. .8.. 8..7"},
		{"jungle", "9.5. .... ..8. .3..", ".3.. 9.3. .4.3 9.53", "6364 6364 6364 6365",
		 ".... .... ..7. ....", "..4. .6.. ...5 .7.."},
		{"broken", "9..4 ..7. .3.6 ....", "...3 .9.. ..4. 8.2.", "5274 6372 5274 6382",
		 ".... ...6 .... .5..", ".6.. 7..3 .8.. .4.5"},
	},
};

void parseAllGrooves() {
	for (int gy = 0; gy < kGridSize; ++gy) {
		for (int gx = 0; gx < kGridSize; ++gx) {
			const GrooveText& src = kSource[gy][gx];
			gGrid[gy][gx].name = src.name;
			parseLane(src.kick, gGrid[gy][gx].lane[kLaneKick]);
			parseLane(src.snare, gGrid[gy][gx].lane[kLaneSnare]);
			parseLane(src.hat, gGrid[gy][gx].lane[kLaneHat]);
			parseLane(src.hatOpen, gGrid[gy][gx].lane[kLaneHatOpen]);
			parseLane(src.fm, gGrid[gy][gx].lane[kLaneFm]);
		}
	}
}

} // namespace Grooves
