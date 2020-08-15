#include <mutex>
#include <atomic>
#include <thread>

#include "base/timeutil.h"
#include "base/NativeApp.h"
#include "i18n/i18n.h"
#include "input/input_state.h"
#include "util/text/utf8.h"

#include "Common/Log.h"
#include "Common/StringUtils.h"
#include "Common/GraphicsContext.h"
#include "Windows/EmuThread.h"
#include "Windows/W32Util/Misc.h"
#include "Windows/MainWindow.h"
#include "Windows/resource.h"
#include "Windows/WindowsHost.h"
#include "Core/Reporting.h"
#include "Core/MemMap.h"
#include "Core/Core.h"
#include "Core/Host.h"
#include "Core/System.h"
#include "Core/Config.h"
#include "Core/ConfigValues.h"
#include "thread/threadutil.h"

enum class EmuThreadState {
	DISABLED,
	START_REQUESTED,
	RUNNING,
	QUIT_REQUESTED,
	STOPPED,
};

static std::thread emuThread;
static std::atomic<int> emuThreadState((int)EmuThreadState::DISABLED);

static std::thread mainThread;
static bool useEmuThread;
static std::string g_error_message;
static bool g_inLoop;

extern std::vector<std::wstring> GetWideCmdLine();

class GraphicsContext;
static GraphicsContext *g_graphicsContext;

void MainThreadFunc();

// On most other platforms, we let the "main" thread become the render thread and
// start a separate emu thread from that, if needed. Should probably switch to that
// to make it the same on all platforms.
void MainThread_Start(bool separateEmuThread) {
	useEmuThread = separateEmuThread;
	mainThread = std::thread(&MainThreadFunc);
}

void MainThread_Stop() {
	// Already stopped?
	UpdateUIState(UISTATE_EXIT);
	Core_Stop();
	mainThread.join();
}

bool MainThread_Ready() {
	return g_inLoop;
}

static void EmuThreadFunc(GraphicsContext *graphicsContext) {
	setCurrentThreadName("Emu");

	// There's no real requirement that NativeInit happen on this thread.
	// We just call the update/render loop here.
	emuThreadState = (int)EmuThreadState::RUNNING;

	NativeInitGraphics(graphicsContext);

	while (emuThreadState != (int)EmuThreadState::QUIT_REQUESTED) {
		// We're here again, so the game quit.  Restart Core_Run() which controls the UI.
		// This way they can load a new game.
		if (!Core_IsActive())
			UpdateUIState(UISTATE_MENU);
		Core_Run(g_graphicsContext);
	}

	emuThreadState = (int)EmuThreadState::STOPPED;

	NativeShutdownGraphics();

	// Ask the main thread to stop.  This prevents a hang on a race condition.
	graphicsContext->StopThread();
}

static void EmuThreadStart(GraphicsContext *graphicsContext) {
	emuThreadState = (int)EmuThreadState::START_REQUESTED;
	emuThread = std::thread(&EmuThreadFunc, graphicsContext);
}

static void EmuThreadStop() {
	emuThreadState = (int)EmuThreadState::QUIT_REQUESTED;
}

static void EmuThreadJoin() {
	emuThread.join();
	emuThread = std::thread();
	INFO_LOG(SYSTEM, "EmuThreadJoin - joined");
}

void MainThreadFunc() {
	if (useEmuThread) {
		// We'll start up a separate thread we'll call Emu
		setCurrentThreadName("Render");
	} else {
		// This is both Emu and Render.
		setCurrentThreadName("Emu");
	}

	host = new WindowsHost(MainWindow::GetHInstance(), MainWindow::GetHWND(), MainWindow::GetDisplayHWND());
	host->SetWindowTitle(nullptr);

	// We convert command line arguments to UTF-8 immediately.
	std::vector<std::wstring> wideArgs = GetWideCmdLine();
	std::vector<std::string> argsUTF8;
	for (auto& string : wideArgs) {
		argsUTF8.push_back(ConvertWStringToUTF8(string));
	}
	std::vector<const char *> args;
	for (auto& string : argsUTF8) {
		args.push_back(string.c_str());
	}
	bool performingRestart = NativeIsRestarting();
	NativeInit(static_cast<int>(args.size()), &args[0], "1234", "1234", nullptr);

	if (g_Config.iGPUBackend == (int)GPUBackend::OPENGL) {
		if (!useEmuThread) {
			// Okay, we must've switched to OpenGL.  Let's flip the emu thread on.
			useEmuThread = true;
			setCurrentThreadName("Render");
		}
	} else if (useEmuThread) {
		// We must've failed over from OpenGL, flip the emu thread off.
		useEmuThread = false;
		setCurrentThreadName("Emu");
	}

	if (g_Config.sFailedGPUBackends.find("ALL") != std::string::npos) {
		Reporting::ReportMessage("Graphics init error: %s", "ALL");

		auto err = GetI18NCategory("Error");
		const char *defaultErrorAll = "PPSSPP failed to startup with any graphics backend. Try upgrading your graphics and other drivers.";
		const char *genericError = err->T("GenericAllStartupError", defaultErrorAll);
		std::wstring title = ConvertUTF8ToWString(err->T("GenericGraphicsError", "Graphics Error"));
		MessageBox(0, ConvertUTF8ToWString(genericError).c_str(), title.c_str(), MB_OK);

		// Let's continue (and probably crash) just so they have a way to keep trying.
	}

	host->UpdateUI();

	std::string error_string;
	bool success = host->InitGraphics(&error_string, &g_graphicsContext);

	if (success) {
		// Main thread is the render thread.
		success = g_graphicsContext->InitFromRenderThread(&error_string);
	}

	if (!success) {
		// Before anything: are we restarting right now?
		if (performingRestart) {
			// Okay, switching graphics didn't work out.  Probably a driver bug - fallback to restart.
			// This happens on NVIDIA when switching OpenGL -> Vulkan.
			g_Config.Save("switch_graphics_failed");
			W32Util::ExitAndRestart();
		}

		auto err = GetI18NCategory("Error");
		Reporting::ReportMessage("Graphics init error: %s", error_string.c_str());

		const char *defaultErrorVulkan = "Failed initializing graphics. Try upgrading your graphics drivers.\n\nWould you like to try switching to OpenGL?\n\nError message:";
		const char *defaultErrorOpenGL = "Failed initializing graphics. Try upgrading your graphics drivers.\n\nWould you like to try switching to DirectX 9?\n\nError message:";
		const char *defaultErrorDirect3D9 = "Failed initializing graphics. Try upgrading your graphics drivers and directx 9 runtime.\n\nWould you like to try switching to OpenGL?\n\nError message:";
		const char *genericError;
		GPUBackend nextBackend = GPUBackend::DIRECT3D9;
		switch (g_Config.iGPUBackend) {
		case (int)GPUBackend::DIRECT3D9:
			nextBackend = GPUBackend::OPENGL;
			genericError = err->T("GenericDirect3D9Error", defaultErrorDirect3D9);
			break;
		case (int)GPUBackend::VULKAN:
			nextBackend = GPUBackend::OPENGL;
			genericError = err->T("GenericVulkanError", defaultErrorVulkan);
			break;
		case (int)GPUBackend::OPENGL:
		default:
			nextBackend = GPUBackend::DIRECT3D9;
			genericError = err->T("GenericOpenGLError", defaultErrorOpenGL);
			break;
		}
		std::string full_error = StringFromFormat("%s\n\n%s", genericError, error_string.c_str());
		std::wstring title = ConvertUTF8ToWString(err->T("GenericGraphicsError", "Graphics Error"));
		bool yes = IDYES == MessageBox(0, ConvertUTF8ToWString(full_error).c_str(), title.c_str(), MB_ICONERROR | MB_YESNO);
		ERROR_LOG(BOOT, full_error.c_str());

		if (yes) {
			// Change the config to the alternative and restart.
			g_Config.iGPUBackend = (int)nextBackend;
			// Clear this to ensure we try their selection.
			g_Config.sFailedGPUBackends.clear();
			g_Config.Save("save_graphics_fallback");

			W32Util::ExitAndRestart();
		} else {
			if (g_Config.iGPUBackend == (int)GPUBackend::DIRECT3D9) {
				// Allow the user to download the DX9 runtime.
				LaunchBrowser("https://www.microsoft.com/en-us/download/details.aspx?id=34429");
			}
		}

		// No safe way out without graphics.
		ExitProcess(1);
	}

	GraphicsContext *graphicsContext = g_graphicsContext;

	if (!useEmuThread) {
		NativeInitGraphics(graphicsContext);
		NativeResized();
	}

	INFO_LOG(BOOT, "Done.");
	_dbg_update_();

	if (coreState == CORE_POWERDOWN) {
		INFO_LOG(BOOT, "Exit before core loop.");
		goto shutdown;
	}

	g_inLoop = true;

	if (useEmuThread) {
		EmuThreadStart(graphicsContext);
	}
	graphicsContext->ThreadStart();

	if (g_Config.bBrowse)
		PostMessage(MainWindow::GetHWND(), WM_COMMAND, ID_FILE_LOAD, 0);

	Core_EnableStepping(false);

	if (useEmuThread) {
		while (emuThreadState != (int)EmuThreadState::DISABLED) {
			graphicsContext->ThreadFrame();
			if (GetUIState() == UISTATE_EXIT) {
				break;
			}
		}
	} else {
		while (GetUIState() != UISTATE_EXIT) {
			// We're here again, so the game quit.  Restart Core_Run() which controls the UI.
			// This way they can load a new game.
			if (!Core_IsActive())
				UpdateUIState(UISTATE_MENU);
			Core_Run(g_graphicsContext);
		}
	}
	Core_Stop();
	if (!useEmuThread) {
		// Process the shutdown.  Without this, non-GL delays 800ms on shutdown.
		Core_Run(g_graphicsContext);
	}
	Core_WaitInactive(800);

	g_inLoop = false;

	if (useEmuThread) {
		EmuThreadStop();
		while (graphicsContext->ThreadFrame()) {
			// Need to keep eating frames to allow the EmuThread to exit correctly.
			continue;
		}
		EmuThreadJoin();
	}

shutdown:

	if (!useEmuThread) {
		NativeShutdownGraphics();
	}

	g_graphicsContext->ThreadEnd();
	g_graphicsContext->ShutdownFromRenderThread();

	// NativeShutdown deletes the graphics context through host->ShutdownGraphics().
	NativeShutdown();

	PostMessage(MainWindow::GetHWND(), MainWindow::WM_USER_UPDATE_UI, 0, 0);
}
