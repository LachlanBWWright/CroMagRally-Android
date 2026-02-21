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
#include <SDL3/SDL_system.h>
#include <cerrno>
#include <cstring>

// Version stamp written after successful extraction.
// Bump this if the Data files change (forces re-extraction on update).
#define ASSET_STAMP_VERSION GAME_VERSION

// Complete list of every file inside the Data/ folder, relative to the APK
// asset root (which equals the Data/ directory on disk).
// SDL_IOFromFile with a relative path reads from APK assets on Android.
// SDL_EnumerateDirectory() uses POSIX opendir() and CANNOT see APK assets --
// so we must maintain this hardcoded list.
static const char* kAllDataFiles[] = {
	"Audio/Announcer/1st.aiff",
	"Audio/Announcer/2nd.aiff",
	"Audio/Announcer/3rd.aiff",
	"Audio/Announcer/4th.aiff",
	"Audio/Announcer/5th.aiff",
	"Audio/Announcer/6th.aiff",
	"Audio/Announcer/Arrowhead.aiff",
	"Audio/Announcer/BoneBomb.aiff",
	"Audio/Announcer/Candle.aiff",
	"Audio/Announcer/Completed.aiff",
	"Audio/Announcer/CostYa.aiff",
	"Audio/Announcer/FinalLap.aiff",
	"Audio/Announcer/Freeze.aiff",
	"Audio/Announcer/Go.aiff",
	"Audio/Announcer/GoodJob.aiff",
	"Audio/Announcer/GottaHurt.aiff",
	"Audio/Announcer/GreenTeamWins.aiff",
	"Audio/Announcer/Incomplete.aiff",
	"Audio/Announcer/Invisibility.aiff",
	"Audio/Announcer/Lap2.aiff",
	"Audio/Announcer/Mine.aiff",
	"Audio/Announcer/NiceDrivin.aiff",
	"Audio/Announcer/NiceShot.aiff",
	"Audio/Announcer/Nitro.aiff",
	"Audio/Announcer/OhYeah.aiff",
	"Audio/Announcer/Oil.aiff",
	"Audio/Announcer/Pigeon.aiff",
	"Audio/Announcer/Ready.aiff",
	"Audio/Announcer/RedTeamWins.aiff",
	"Audio/Announcer/Rocket.aiff",
	"Audio/Announcer/Set.aiff",
	"Audio/Announcer/StickyTires.aiff",
	"Audio/Announcer/Suspension.aiff",
	"Audio/Announcer/ThatsAll.aiff",
	"Audio/Announcer/Torpedo.aiff",
	"Audio/Announcer/WatchIt.aiff",
	"Audio/Announcer/Woah.aiff",
	"Audio/Announcer/YouLose.aiff",
	"Audio/Announcer/YouWin.aiff",
	"Audio/AtlantisSong.aiff",
	"Audio/ChinaSong.aiff",
	"Audio/CreteSong.aiff",
	"Audio/DesertSong.aiff",
	"Audio/EgyptSong.aiff",
	"Audio/EuroSong.aiff",
	"Audio/IceSong.aiff",
	"Audio/JungleSong.aiff",
	"Audio/LevelSpecific/BlowDart.aiff",
	"Audio/LevelSpecific/Bubbles.aiff",
	"Audio/LevelSpecific/Catapult.aiff",
	"Audio/LevelSpecific/Chant.aiff",
	"Audio/LevelSpecific/DustDevil.aiff",
	"Audio/LevelSpecific/Gong.aiff",
	"Audio/LevelSpecific/HitSnow.aiff",
	"Audio/LevelSpecific/Hum.aiff",
	"Audio/LevelSpecific/TorpedoFire.aiff",
	"Audio/LevelSpecific/VaseShatter.aiff",
	"Audio/LevelSpecific/Zap.aiff",
	"Audio/Main/BadSelect.aiff",
	"Audio/Main/BirdCaw.aiff",
	"Audio/Main/Boom.aiff",
	"Audio/Main/Cannon.aiff",
	"Audio/Main/Crash.aiff",
	"Audio/Main/Crash2.aiff",
	"Audio/Main/DropMine.aiff",
	"Audio/Main/Engine.aiff",
	"Audio/Main/GetPOW.aiff",
	"Audio/Main/NitroBurst.aiff",
	"Audio/Main/RomanCandleFall.aiff",
	"Audio/Main/RomanCandleLaunch.aiff",
	"Audio/Main/SelectClick.aiff",
	"Audio/Main/Skid.aiff",
	"Audio/Main/Skid2.aiff",
	"Audio/Main/Skid3.aiff",
	"Audio/Main/Snowball.aiff",
	"Audio/Main/Splash.aiff",
	"Audio/Main/Throw1.aiff",
	"Audio/Main/Throw2.aiff",
	"Audio/Main/Throw3.aiff",
	"Audio/ThemeSong.aiff",
	"Audio/VikingSong.aiff",
	"Audio/WinSong.aiff",
	"Images/Ages/BronzeAgeIntro.jpg",
	"Images/Ages/IronAgeIntro.jpg",
	"Images/Ages/StoneAgeIntro.jpg",
	"Images/BoneCollage.png",
	"Images/CharSelectScreen.jpg",
	"Images/Conquered/BronzeAgeConquered.png",
	"Images/Conquered/GameCompleted.png",
	"Images/Conquered/IronAgeConquered.png",
	"Images/Conquered/StoneAgeConquered.png",
	"Images/Credits.jpg",
	"Images/Loading1.jpg",
	"Images/MainMenuBackground.jpg",
	"Images/PangeaLogo.jpg",
	"Images/Pillarbox.jpg",
	"Images/TitleScreen.jpg",
	"Images/TrackSelectScreen.png",
	"Images/VehicleSelectScreen.jpg",
	"Images/Vignette.png",
	"Models/atlantis.bg3d",
	"Models/aztec.bg3d",
	"Models/carparts.bg3d",
	"Models/carselect.bg3d",
	"Models/china.bg3d",
	"Models/coliseum.bg3d",
	"Models/crete.bg3d",
	"Models/desert.bg3d",
	"Models/egypt.bg3d",
	"Models/europe.bg3d",
	"Models/global.bg3d",
	"Models/ice.bg3d",
	"Models/jungle.bg3d",
	"Models/ramps.bg3d",
	"Models/scandinavia.bg3d",
	"Models/stonehenge.bg3d",
	"Models/tarpits.bg3d",
	"Models/weapons.bg3d",
	"Models/winners.bg3d",
	"Skeletons/Beetle.bg3d",
	"Skeletons/Beetle.skeleton.rsrc",
	"Skeletons/BirdBomb.bg3d",
	"Skeletons/BirdBomb.skeleton.rsrc",
	"Skeletons/Brog.bg3d",
	"Skeletons/Brog.skeleton.rsrc",
	"Skeletons/BrogStanding.bg3d",
	"Skeletons/BrogStanding.skeleton.rsrc",
	"Skeletons/BrontoNeck.bg3d",
	"Skeletons/BrontoNeck.skeleton.rsrc",
	"Skeletons/Camel.bg3d",
	"Skeletons/Camel.skeleton.rsrc",
	"Skeletons/Catapult.bg3d",
	"Skeletons/Catapult.skeleton.rsrc",
	"Skeletons/Dragon.bg3d",
	"Skeletons/Dragon.skeleton.rsrc",
	"Skeletons/Druid.bg3d",
	"Skeletons/Druid.skeleton.rsrc",
	"Skeletons/Flag.bg3d",
	"Skeletons/Flag.skeleton.rsrc",
	"Skeletons/Flower.bg3d",
	"Skeletons/Flower.skeleton.rsrc",
	"Skeletons/Grag.bg3d",
	"Skeletons/Grag.skeleton.rsrc",
	"Skeletons/GragStanding.bg3d",
	"Skeletons/GragStanding.skeleton.rsrc",
	"Skeletons/Mummy.bg3d",
	"Skeletons/Mummy.skeleton.rsrc",
	"Skeletons/PolarBear.bg3d",
	"Skeletons/PolarBear.skeleton.rsrc",
	"Skeletons/Pterodactyl.bg3d",
	"Skeletons/Pterodactyl.skeleton.rsrc",
	"Skeletons/Shark.bg3d",
	"Skeletons/Shark.skeleton.rsrc",
	"Skeletons/Troll.bg3d",
	"Skeletons/Troll.skeleton.rsrc",
	"Skeletons/Viking.bg3d",
	"Skeletons/Viking.skeleton.rsrc",
	"Skeletons/Yeti.bg3d",
	"Skeletons/Yeti.skeleton.rsrc",
	"Sprites/Fences/aztec.png",
	"Sprites/Fences/camel.png",
	"Sprites/Fences/china1.png",
	"Sprites/Fences/china2.png",
	"Sprites/Fences/china3.png",
	"Sprites/Fences/china4.png",
	"Sprites/Fences/chinaconcrete.png",
	"Sprites/Fences/chinadesign.png",
	"Sprites/Fences/crete.png",
	"Sprites/Fences/desertskin.png",
	"Sprites/Fences/farm.png",
	"Sprites/Fences/hieroglyphs.png",
	"Sprites/Fences/horns.png",
	"Sprites/Fences/invisible.png",
	"Sprites/Fences/orangerock.png",
	"Sprites/Fences/rockpile.png",
	"Sprites/Fences/rockpile2.png",
	"Sprites/Fences/rockwall.png",
	"Sprites/Fences/seaweed1.png",
	"Sprites/Fences/seaweed2.png",
	"Sprites/Fences/seaweed3.png",
	"Sprites/Fences/seaweed4.png",
	"Sprites/Fences/seaweed5.png",
	"Sprites/Fences/seaweed6.png",
	"Sprites/Fences/tallrockwall.png",
	"Sprites/Fences/tribal.png",
	"Sprites/Fences/viking.png",
	"Sprites/Maps/AtlantisMap.png",
	"Sprites/Maps/AztecMap.png",
	"Sprites/Maps/CelticMap.png",
	"Sprites/Maps/ChinaMap.png",
	"Sprites/Maps/ColiseumMap.png",
	"Sprites/Maps/CreteMap.png",
	"Sprites/Maps/DesertMap.png",
	"Sprites/Maps/EgyptMap.png",
	"Sprites/Maps/EuropeMap.png",
	"Sprites/Maps/IceMap.png",
	"Sprites/Maps/JungleMap.png",
	"Sprites/Maps/MazeMap.png",
	"Sprites/Maps/RampsMap.png",
	"Sprites/Maps/ScandinaviaMap.png",
	"Sprites/Maps/SpiralMap.png",
	"Sprites/Maps/StonehengeMap.png",
	"Sprites/Maps/TarPitsMap.png",
	"Sprites/Skins/brog0.png",
	"Sprites/Skins/brog1.png",
	"Sprites/Skins/brog2.png",
	"Sprites/Skins/brog3.png",
	"Sprites/Skins/brog4.png",
	"Sprites/Skins/brog5.png",
	"Sprites/Skins/grag0.png",
	"Sprites/Skins/grag1.png",
	"Sprites/Skins/grag2.png",
	"Sprites/Skins/grag3.png",
	"Sprites/Skins/grag4.png",
	"Sprites/Skins/grag5.png",
	"Sprites/effects.png",
	"Sprites/effects.txt",
	"Sprites/infobar.png",
	"Sprites/infobar.txt",
	"Sprites/menus.png",
	"Sprites/menus.txt",
	"Sprites/qrcodes.png",
	"Sprites/qrcodes.txt",
	"Sprites/rockfont.png",
	"Sprites/rockfont.txt",
	"Sprites/scoreboard.png",
	"Sprites/scoreboard.txt",
	"Sprites/trackselectmp.png",
	"Sprites/trackselectmp.txt",
	"Sprites/trackselectsp.png",
	"Sprites/trackselectsp.txt",
	"Sprites/wallfont.png",
	"Sprites/wallfont.txt",
	"System/gamecontrollerdb.txt",
	"System/kerning.txt",
	"System/strings.csv",
	"System/twitch.csv",
	"Terrain/Battle_Aztec.ter",
	"Terrain/Battle_Aztec.ter.rsrc",
	"Terrain/Battle_Celtic.ter",
	"Terrain/Battle_Celtic.ter.rsrc",
	"Terrain/Battle_Coliseum.ter",
	"Terrain/Battle_Coliseum.ter.rsrc",
	"Terrain/Battle_Maze.ter",
	"Terrain/Battle_Maze.ter.rsrc",
	"Terrain/Battle_Ramps.ter",
	"Terrain/Battle_Ramps.ter.rsrc",
	"Terrain/Battle_Spiral.ter",
	"Terrain/Battle_Spiral.ter.rsrc",
	"Terrain/Battle_StoneHenge.ter",
	"Terrain/Battle_StoneHenge.ter.rsrc",
	"Terrain/Battle_TarPits.ter",
	"Terrain/Battle_TarPits.ter.rsrc",
	"Terrain/BronzeAge_China.ter",
	"Terrain/BronzeAge_China.ter.rsrc",
	"Terrain/BronzeAge_Crete.ter",
	"Terrain/BronzeAge_Crete.ter.rsrc",
	"Terrain/BronzeAge_Egypt.ter",
	"Terrain/BronzeAge_Egypt.ter.rsrc",
	"Terrain/IronAge_Atlantis.ter",
	"Terrain/IronAge_Atlantis.ter.rsrc",
	"Terrain/IronAge_Europe.ter",
	"Terrain/IronAge_Europe.ter.rsrc",
	"Terrain/IronAge_Scandinavia.ter",
	"Terrain/IronAge_Scandinavia.ter.rsrc",
	"Terrain/StoneAge_Desert.ter",
	"Terrain/StoneAge_Desert.ter.rsrc",
	"Terrain/StoneAge_Ice.ter",
	"Terrain/StoneAge_Ice.ter.rsrc",
	"Terrain/StoneAge_Jungle.ter",
	"Terrain/StoneAge_Jungle.ter.rsrc",
	NULL
};

// Subdirectories to create inside the Data folder on internal storage.
static const char* kDataDirs[] = {
	"Audio/Announcer",
	"Audio/LevelSpecific",
	"Audio/Main",
	"Images/Ages",
	"Images/Conquered",
	"Models",
	"Skeletons",
	"Sprites/Fences",
	"Sprites/Maps",
	"Sprites/Skins",
	"System",
	"Terrain",
	NULL
};

// Copy one APK asset (assetRelPath) to destPath on the filesystem.
// Returns true on success.
static bool CopyAssetToFile(const char* assetRelPath, const char* destPath)
{
	SDL_IOStream* src = SDL_IOFromFile(assetRelPath, "rb");
	if (!src)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
			"ExtractAssets: SDL_IOFromFile failed for %s: %s",
			assetRelPath, SDL_GetError());
		return false;
	}

	FILE* dst = fopen(destPath, "wb");
	if (!dst)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
			"ExtractAssets: fopen failed for %s: %s",
			destPath, strerror(errno));
		SDL_CloseIO(src);
		return false;
	}

	char buf[65536];
	bool ok = true;
	size_t n;
	while ((n = SDL_ReadIO(src, buf, sizeof(buf))) > 0)
	{
		if (fwrite(buf, 1, n, dst) != n)
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
				"ExtractAssets: fwrite failed for %s", destPath);
			ok = false;
			break;
		}
	}

	fclose(dst);
	SDL_CloseIO(src);
	return ok;
}

// Extract all APK assets to internalStoragePath/Data/.
// Returns true when extraction succeeds (or was already done).
// The stamp file is written ONLY after all files succeed, so a mid-extraction
// crash forces a retry on the next launch.
static bool ExtractAssets(const char* internalStorage)
{
	// Stamp file includes the game version so an app update re-extracts data.
	char stampPath[512];
	snprintf(stampPath, sizeof(stampPath), "%s/.assets_" ASSET_STAMP_VERSION, internalStorage);

	if (fs::exists(stampPath))
	{
		// Already extracted for this version.
		return true;
	}

	SDL_Log("ExtractAssets: extracting %s data to %s ...", ASSET_STAMP_VERSION, internalStorage);

	// Create all required subdirectories under Data/.
	for (int di = 0; kDataDirs[di]; di++)
	{
		char dirPath[512];
		snprintf(dirPath, sizeof(dirPath), "%s/Data/%s", internalStorage, kDataDirs[di]);
		fs::create_directories(dirPath);
	}

	// Copy every asset file.
	int total = 0, failed = 0;
	for (int i = 0; kAllDataFiles[i]; i++)
	{
		char destPath[512];
		snprintf(destPath, sizeof(destPath), "%s/Data/%s", internalStorage, kAllDataFiles[i]);

		if (!CopyAssetToFile(kAllDataFiles[i], destPath))
		{
			failed++;
		}
		total++;
	}

	if (failed > 0)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
			"ExtractAssets: %d/%d files failed to extract", failed, total);
		return false;
	}

	// Write stamp only after ALL files are successfully extracted.
	FILE* stamp = fopen(stampPath, "w");
	if (stamp) { fputs("ok", stamp); fclose(stamp); }

	SDL_Log("ExtractAssets: done (%d files)", total);
	return true;
}
#endif // __ANDROID__

static fs::path FindGameData(const char* executablePath)
{
	fs::path dataPath;

#ifdef __ANDROID__
	// On Android, assets are packed in the APK and cannot be read via std::fstream.
	// We extract them to internal storage on first launch, then point Pomme there.
	const char* internalStorage = SDL_GetAndroidInternalStoragePath();
	if (!internalStorage)
		throw std::runtime_error("SDL_GetAndroidInternalStoragePath() returned NULL");

	if (!ExtractAssets(internalStorage))
		throw std::runtime_error("Failed to extract game data from APK. Check logcat for details.");

	dataPath = std::string(internalStorage) + "/Data";
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

	{
		auto gamecontrollerdbPath8 = (dataPath / "System" / "gamecontrollerdb.txt").u8string();
		if (-1 == SDL_AddGamepadMappingsFromFile((const char*)gamecontrollerdbPath8.c_str()))
		{
			SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Couldn't load gamecontrollerdb.txt: %s", SDL_GetError());
		}
	}

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
#if !(_DEBUG) || defined(__ANDROID__)
	// In release builds, catch anything that might be thrown by GameMain
	// so we can show an error dialog to the user.
	// On Android we always catch because debug builds (assembleDebug) don't
	// have a console; an uncaught exception terminates the process silently.
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
