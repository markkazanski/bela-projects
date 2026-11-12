#include "PepperIO.h"

namespace PepperIO {

Calib gCalib = {
	.trigHigh = 0.85f,
	.accentMin = 0.15f,
	.accentMax = 0.85f,
	.hatDecayClosed = 0.1f,
	.hatDecayOpen = 0.75f,
	.trigPulseFrames = 22,
};

} // namespace PepperIO
