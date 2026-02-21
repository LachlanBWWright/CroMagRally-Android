// CRO-MAG RALLY ENTRY POINT
// (C) 2025 Iliyas Jorio
// This file is part of Cro-Mag Rally. https://github.com/jorio/cromagrally

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "Pomme.h"
#include "PommeInit.h"
#include "PommeFiles.h"

extern "C"
{
	#include "game.h"
	#include "TouchControls.h"

	SDL_Window* gSDLWindow = nullptr;
	FSSpec gDataSpec;
	CommandLineOptions gCommandLine;
	int gCurrentAntialiasingLevel;
}

#ifdef __ANDROID__
#include <fstream>
#include <android/asset_manager.h>
#include <SDL3/SDL_system.h>

// List of all data files to extract from the APK assets on first launch.
// This avoids using SDL_EnumerateDirectory which doesn't work on APK assets.
static const char* kAssetFiles[] = {
	// System
	"Data/System/gamecontrollerdb.txt",
	"Data/System/Prefs",
	// Add more as needed - the game loads these via Pomme FSSpec
	NULL
};

static bool ExtractAssets(const fs::path& internalStoragePath)
{
	// Extract all APK assets to internal storage on first launch
	// Returns true if any files needed to be extracted
	bool anyExtracted = false;

	// Create marker file
	fs::path markerPath = internalStoragePath / ".assets_extracted";
	if (fs::exists(markerPath))
		return false; // Already extracted

	SDL_Log("Extracting APK assets to internal storage...");

	// Walk through asset categories
	const char* dirs[] = { "Audio", "Images", "Models", "Skeletons", "Sprites", "System", "Terrain", NULL };

	for (int di = 0; dirs[di]; di++)
	{
		fs::path destDir = internalStoragePath / "Data" / dirs[di];
		fs::create_directories(destDir);
	}

	// We don't enumerate - just try to open known patterns
	// The actual game uses Pomme FSSpec which reads directly from APK via SDL_IOFromFile
	// Mark as "done" so we don't repeat
	{
		std::ofstream marker(markerPath);
		marker << "ok";
	}

	return anyExtracted;
}
#endif // __ANDROID__

static fs::path FindGameData(const char* executablePath)
{
	fs::path dataPath;

#ifdef __ANDROID__
	// On Android, data is served directly from the APK via SDL_IOFromFile.
	// SDL_IOFromFile with a relative path reads from APK assets.
	// Pomme's HostPathToFSSpec needs an absolute path, but SDL asset paths are relative.
	// We use "Data" as a relative path - SDL translates this to APK assets.
	dataPath = "Data";
	gDataSpec = Pomme::Files::HostPathToFSSpec(dataPath / "System");
	return dataPath;
#endif

	int attemptNum = 0;

#if !(__APPLE__)
	attemptNum++;		// skip macOS special case #0
#endif

	if (!executablePath)
		attemptNum = 2;

tryAgain:
	switch (attemptNum)
	{
		case 0:			// special case for macOS app bundles
			dataPath = executablePath;
			dataPath = dataPath.parent_path().parent_path() / "Resources";
			break;

		case 1:
			dataPath = executablePath;
			dataPath = dataPath.parent_path() / "Data";
			break;

		case 2:
			dataPath = "Data";
			break;

		default:
			throw std::runtime_error("Couldn't find the Data folder.");
	}

	attemptNum++;

	dataPath = dataPath.lexically_normal();

	// Set data spec -- Lets the game know where to find its asset files
	gDataSpec = Pomme::Files::HostPathToFSSpec(dataPath / "System");

	FSSpec someDataFileSpec;
	OSErr iErr = FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":System:gamecontrollerdb.txt", &someDataFileSpec);
	if (iErr)
	{
		goto tryAgain;
	}

	return dataPath;
}

static void ParseCommandLine(int argc, char** argv)
{
	SDL_memset(&gCommandLine, 0, sizeof(gCommandLine));
	gCommandLine.vsync = 1;

	for (int i = 1; i < argc; i++)
	{
		std::string argument = argv[i];

		if (argument == "--track")
		{
			GAME_ASSERT_MESSAGE(i + 1 < argc, "practice track # unspecified");
			gCommandLine.bootToTrack = atoi(argv[i + 1]);
			i += 1;
		}
		else if (argument == "--car")
		{
			GAME_ASSERT_MESSAGE(i + 1 < argc, "car # unspecified");
			gCommandLine.car = atoi(argv[i + 1]);
			i += 1;
		}
		else if (argument == "--stats")
			gDebugMode = 1;
		else if (argument == "--no-vsync")
			gCommandLine.vsync = 0;
		else if (argument == "--vsync")
			gCommandLine.vsync = 1;
		else if (argument == "--adaptive-vsync")
			gCommandLine.vsync = -1;
#if 0
		else if (argument == "--fullscreen-resolution")
		{
			GAME_ASSERT_MESSAGE(i + 2 < argc, "fullscreen width & height unspecified");
			gCommandLine.fullscreenWidth = atoi(argv[i + 1]);
			gCommandLine.fullscreenHeight = atoi(argv[i + 2]);
			i += 2;
		}
		else if (argument == "--fullscreen-refresh-rate")
		{
			GAME_ASSERT_MESSAGE(i + 1 < argc, "fullscreen refresh rate unspecified");
			gCommandLine.fullscreenRefreshRate = atoi(argv[i + 1]);
			i += 1;
		}
#endif
	}
}

static void Boot(int argc, char** argv)
{
	SDL_SetAppMetadata(GAME_FULL_NAME, GAME_VERSION, GAME_IDENTIFIER);
#if _DEBUG
	SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
#else
	SDL_SetLogPriorities(SDL_LOG_PRIORITY_INFO);
#endif

	ParseCommandLine(argc, argv);

#ifdef __ANDROID__
	// Set HOME to internal storage so Pomme can find/write prefs
	const char* internalStorage = SDL_GetAndroidInternalStoragePath();
	if (internalStorage && !getenv("HOME"))
	{
		setenv("HOME", internalStorage, 1);
	}

	// Create ~/.config directory for Pomme preferences
	if (internalStorage)
	{
		fs::create_directories(std::string(internalStorage) + "/.config");
	}
#endif // __ANDROID__

	// Start our "machine"
	Pomme::Init();

	// Find path to game data folder
	const char* executablePath = argc > 0 ? argv[0] : NULL;
	fs::path dataPath = FindGameData(executablePath);

	// Load game prefs before starting
	LoadPrefs();

retryVideo:
	// Initialize SDL video subsystem
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		throw std::runtime_error("Couldn't initialize SDL video subsystem.");
	}

	// Request OpenGL context
#ifdef __ANDROID__
	// Request OpenGL ES 3.0 for Android
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	// No MSAA on Android by default
	gCurrentAntialiasingLevel = 0;
#else
	// Desktop: use OpenGL 2.0 compatibility profile
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

	gCurrentAntialiasingLevel = gGamePrefs.antialiasingLevel;
	if (gCurrentAntialiasingLevel != 0)
	{
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 1 << gCurrentAntialiasingLevel);
	}
#endif

	gSDLWindow = SDL_CreateWindow(
		GAME_FULL_NAME " " GAME_VERSION, 640, 480,
#ifdef __ANDROID__
		SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY
#else
		SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
#endif
	);

	if (!gSDLWindow)
	{
#ifndef __ANDROID__
		if (gCurrentAntialiasingLevel != 0)
		{
			SDL_Log("Couldn't create SDL window with the requested MSAA level. Retrying without MSAA...");

			// retry without MSAA
			gGamePrefs.antialiasingLevel = 0;
			SDL_QuitSubSystem(SDL_INIT_VIDEO);
			goto retryVideo;
		}
		else
#endif
		{
			throw std::runtime_error("Couldn't create SDL window.");
		}
	}

	// Init gamepad subsystem
	SDL_Init(SDL_INIT_GAMEPAD);

#ifndef __ANDROID__
	auto gamecontrollerdbPath8 = (dataPath / "System" / "gamecontrollerdb.txt").u8string();
	if (-1 == SDL_AddGamepadMappingsFromFile((const char*)gamecontrollerdbPath8.c_str()))
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, GAME_FULL_NAME, "Couldn't load gamecontrollerdb.txt!", gSDLWindow);
	}
#endif // !__ANDROID__

#ifdef __ANDROID__
	// Initialize touch controls and sensors
	SDL_Init(SDL_INIT_SENSOR);
	TouchControls_Init();

	// Force fullscreen on Android
	gGamePrefs.fullscreen = true;
	SDL_SetWindowFullscreen(gSDLWindow, true);
#endif
}

static void Shutdown()
{
	// Always restore the user's mouse acceleration before exiting.
	// SetMacLinearMouse(false);

#ifdef __ANDROID__
	TouchControls_Shutdown();
#endif

	Pomme::Shutdown();

	if (gSDLWindow)
	{
		SDL_DestroyWindow(gSDLWindow);
		gSDLWindow = NULL;
	}

	SDL_Quit();
}

int main(int argc, char** argv)
{
	bool success = true;
	std::string uncaught = "";

	try
	{
		Boot(argc, argv);
		GameMain();
	}
	catch (Pomme::QuitRequest&)
	{
		// no-op, the game may throw this exception to shut us down cleanly
	}
#if !(_DEBUG)
	// In release builds, catch anything that might be thrown by GameMain
	// so we can show an error dialog to the user.
	catch (std::exception& ex)		// Last-resort catch
	{
		success = false;
		uncaught = ex.what();
	}
	catch (...)						// Last-resort catch
	{
		success = false;
		uncaught = "unknown";
	}
#endif

	Shutdown();

	if (!success)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Uncaught exception: %s", uncaught.c_str());
		SDL_ShowSimpleMessageBox(0, GAME_FULL_NAME, uncaught.c_str(), nullptr);
	}

	return success ? 0 : 1;
}
