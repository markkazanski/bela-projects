#include "GuiView.h"
#include "GuiSnapshot.h"
#include "PepperPins.h"
#include <libraries/Gui/Gui.h>
#include <unistd.h>

#if GRIDS_GUI

static Gui sGui;
static float sBuffer[GuiSnapshot::kBufferFloats];

static void guiAuxLoop(void*) {
	while (!Bela_stopRequested()) {
		GuiSnapshot::copyTo(sBuffer, GuiSnapshot::kBufferFloats);
		sGui.sendBuffer(0, sBuffer, GuiSnapshot::kBufferFloats);
		usleep(33000);
	}
}

bool GuiView::setup(BelaContext* context) {
	sGui.setup(context->projectName);
	sGui.setBuffer('f', GuiSnapshot::kBufferFloats);
	Bela_runAuxiliaryTask(guiAuxLoop);
	return true;
}

#else

bool GuiView::setup(BelaContext*) { return true; }

#endif
