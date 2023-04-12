#include <string>
#include <switch.h>
#include <experimental/filesystem>
#include <dirent.h>

#include "gfx.h"
#include "dir.h"
#include "cons.h"
#include "dump.h"

/*
//https://github.com/J-D-K/JKSV
Information about button codes
"[A]", "\ue0e0"
"[B]", "\ue0e1"
"[X]", "\ue0e2"
"[Y]", "\ue0e3"
"[L]", "\ue0e4"
"[R]", "\ue0e5"
"[ZL]", "\ue0e6"
"[ZR]", "\ue0e7"
"[SL]", "\ue0e8"
"[SR]", "\ue0e9"
"[DPAD]", "\ue0ea"
"[DUP]", "\ue0eb"
"[DDOWN]", "\ue0ec"
"[DLEFT]", "\ue0ed"
"[DRIGHT]", "\ue0ee"
"[+]", "\ue0ef"
"[-]", "\ue0f0"

console colours
% = yellow (BGR-00FFFF)
^ = Green (BGR-FF00FF00)
& = Dark Yellow (BGR-009dff)
$ = Light pink (BGR-e266ff)
* = Faded red (BGR-3333FF)
# = Cyan (BGR-FFFF00)
@ = Purple (BGR-ff0059)
*/

console infoCons(20);
font *sysFont, *consFont;
const char* ctrlStr = "&\ue0e0 转储系统固件&   $\ue0e3 删除挂起的更新$   ^\ue0e4+\ue0e5 删除已转储固件^   %\ue0ef 退出%";

tex* top = texCreate(1280, 88);
tex* bot = texCreate(1280, 72);

int main(int argc, char* argv[]) {
	bool dumped;
	DIR* dir = opendir("sdmc:/Dumped-Firmware");
	if (dir) {
		dumped = true;
	} else {
		dumped = false;
	}
	closedir(dir);

	graphicsInit(1280, 720);
	hidInitialize();

	// Configure our supported input layout: a single player with standard controller styles
	padConfigureInput(1, HidNpadStyleSet_NpadStandard);
	// Initialize the default gamepad (which reads handheld mode inputs as well as the first connected controller)
	PadState pad;
	padInitializeDefault(&pad);

	sysFont = fontLoadSharedFonts();

	//Top
	texClearColor(top, clrCreateU32(0xFF2D2D2D));
	drawText("NS固件转储", top, sysFont, 56, 38, 24, clrCreateU32(0xFFFFFFFF));
	drawRect(top, 30, 87, 1220, 1, clrCreateU32(0xffe5e5e5));

	//Bot
	texClearColor(bot, clrCreateU32(0xFF2D2D2D));
	drawRect(bot, 30, 0, 1220, 1, clrCreateU32(0xffe5e5e5));
	unsigned ctrlX = 56;
	drawText(ctrlStr, bot, sysFont, ctrlX, 24, 18, clrCreateU32(0xFFFFFFFF));

	infoCons.out("按&\ue0e0&将当前固件转储到SD卡的Dumped-Firmware文件夹");
	infoCons.nl();
	infoCons.out("按$\ue0e3$从NAND中删除挂起的固件更新");
	infoCons.nl();
	infoCons.out("按^\ue0e4+\ue0e5^从SD卡中删除转储的固件");
	infoCons.nl();
	infoCons.out("按%\ue0ef%退出此应用程序");
	infoCons.nl();

	//Thread stuff so UI can update ithout feezing.
	Thread workThread;
	dumpArgs* da;
	bool threadRunning = false;
	bool threadFin = true;
	bool run = true;

	while (run) {
		// Scan the gamepad. This should be done once for each frame
		padUpdate(&pad);
		// padGetButtonsDown returns the set of buttons that have been newly pressed in this frame compared to the previous one
		u64 kDown = padGetButtonsDown(&pad);		
		// padGetButtons returns the set of buttons that are currently pressed
		u64 kHeld = padGetButtons(&pad);

		if (!threadRunning) {
			if (kDown & HidNpadButton_A) {
				infoCons.clear();
				DIR* dir = opendir("sdmc:/Dumped-Firmware");
				if (dir) {
					dumped = true;
				} else {
					dumped = false;
				}
				closedir(dir);
				if (!dumped) {
					threadRunning = true, threadFin = false;
					//Struct to send and receive stuff from thread
					da = dumpArgsCreate(&infoCons, &threadFin);
					threadCreate(&workThread, dumpThread, da, NULL, 0x4000, 0x2B, -2);
					threadStart(&workThread);
				} else {
					threadFin = true;
					threadRunning = false;
					infoCons.clear();
					infoCons.out("SD卡上存在转储的固件，请按^\ue0e4+\ue0e5^删除当前转储的固件！");
					infoCons.nl();
				}
			}
			
			if ((kHeld & HidNpadButton_L) && ( kHeld & HidNpadButton_R)) {
				infoCons.clear();
				DIR* dir = opendir("sdmc:/Dumped-Firmware");
				if (dir) {
					dumped = true;
				} else {
					dumped = false;
				}
				closedir(dir);
				if (dumped) {
					threadRunning = true, threadFin = false;
					da = dumpArgsCreate(&infoCons, &threadFin);
					threadCreate(&workThread, cleanThread, da, NULL, 0x4000, 0x2B, -2);
					threadStart(&workThread);
				} else {
					infoCons.clear();
					infoCons.out("SD卡上^不^存在转储的固件。");
					infoCons.nl();
				}
			}

			if (kDown & HidNpadButton_Y) {
				infoCons.clear();			
				threadRunning = true, threadFin = false;
				da = dumpArgsCreate(&infoCons, &threadFin);
				threadCreate(&workThread, cleanPending, da, NULL, 0x4000, 0x2B, -2);
				threadStart(&workThread);
			}

			if (kDown & HidNpadButton_Plus) {
				run = false;
				break;
			}
		} else {
			if (threadFin) {
				//threadFin = true;
				threadClose(&workThread);
				dumpArgsDestroy(da);
				threadRunning = false;
			}
		}

		gfxBeginFrame();
		texClearColor(frameBuffer, clrCreateU32(0xFF2D2D2D));
		texDrawNoAlpha(top, frameBuffer, 0, 0);
		texDrawNoAlpha(bot, frameBuffer, 0, 648);
		infoCons.draw(sysFont);
		gfxEndFrame();
	}

	hidExit();
	fontDestroy(sysFont);
	texDestroy(top);
	texDestroy(bot);
	graphicsExit();

	return 0;
}