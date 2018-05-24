#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include <3ds.h>
#include "libs/httpc-curl/httpc.h"
#include "downloader.h"

void Sleep(u64 nanoseconds)
{
    svcSleepThread(nanoseconds);
}

s64 Seconds(float amount)
{
    return (s64)(amount*1000000000);
}

int main()
{
	fsInit();
    amInit();
	gfxInitDefault();
	httpc.Init(0);
	consoleInit(GFX_BOTTOM, NULL);

	printf("ACNL PLG Auto by Slattz\n");
	printf("Auto setup tool for Rydog's ACNL NTR\nPlugin\n\n");

	if(!PLGDownloader("https://api.github.com/repos/rydoginator/ACNL-NTR-Cheats/releases/latest", "ACNL-NTR-Cheats.plg"))
	{
		printf("\nPlugin Install Failed!!\n\n");
	}

	else {
		printf("\nPlugin Install Success!\n\n");
	}

	Sleep(Seconds(1));

	if(!Installer("https://api.github.com/repos/Nanquitas/BootNTR/releases/latest", "BootNTRSelector-FONZD-Banner.cia"))
	{
		printf("\nNTR Install Failed!!\n\n");
	}

	else {
		printf("\nNTR Install Success!\n\n");
	}

	printf("\n\nEverything is finished!\n\n");
	printf("To launch the NTR plugin:\n\n");
	printf("1: Go into the new app, BootNTRSelector\n");
	printf("2: If asked to setup:\n    Choose 'Default', Then press OK.\n");
	printf("3: Choose 3.6.\n");
	printf("4: BootNTRSelector will then auto exit.\n");
	printf("5: Now select your ACNL game.\n");
	printf("6: If the topscreen flashes green, the\n    plugin has loaded successfully!\n");
	printf("   Otherwise, reboot your 3DS and try\n    this process again.\n");
	printf("If you ever need this information again,\nfind it in our discord: http://bit.ly/ACNLHKD.\n\n\n");
	printf("This setup tool will automatically\n uninstall itself.\n");
	printf("Press Start to uninstall this tool and\nreboot your 3DS.\n");
	// Main loop
	while (aptMainLoop()) {
		hidScanInput();

		if (hidKeysDown() & KEY_START) {
			Result res = AM_DeleteAppTitle(MEDIATYPE_SD, (u64)0x0004000004E4C200);
			if(res) printf("\nSelf-uninstall failed!!!\n");
			else    printf("\nSelf-uninstall success!\n");

			ptmSysmInit();
			printf("Rebooting...\n");
            PTMSYSM_RebootAsync(0);
            ptmSysmExit();
            break;
        }

        // Flush and swap framebuffers
		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
	}

	// Exit services
	httpcExit();
	gfxExit();
	amExit();
    fsExit();
	return 0;
}

