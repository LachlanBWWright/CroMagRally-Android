// ANDROID ASSET EXTRACTION IMPLEMENTATION
// Copies game data from APK assets to the app's internal storage.
//
// SDL_EnumerateDirectory uses POSIX opendir() on Android and therefore
// CANNOT enumerate APK asset paths.  Instead we keep a complete, explicit
// list of every game data file.  SDL_IOFromFile() with a relative path
// DOES read from the APK asset bundle on Android, so we use that for the
// actual byte-for-byte copy.

#ifdef __ANDROID__

#include "AndroidAssets.h"

#include <SDL3/SDL.h>
#include <android/log.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO,  "CroMagRally", __VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, "CroMagRally", __VA_ARGS__)

// Version file: if this file exists and contains our version, skip extraction.
// Uses GAME_VERSION so it is always in sync with the game build.
#define EXTRACT_VERSION_FILE  ".extract_version"
#ifndef GAME_VERSION
#define GAME_VERSION "3.0.2"
#endif
#define EXTRACT_VERSION       GAME_VERSION

// -------------------------------------------------------------------------
// Complete list of all game data files, relative to the Data/ root.
// These are the exact paths that end up at the APK asset bundle root
// (because build.gradle.kts uses  assets.srcDirs("../../Data")).
// -------------------------------------------------------------------------
static const char *kAllDataFiles[] = {
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

// -------------------------------------------------------------------------
// Helper: create all directories in a path
// -------------------------------------------------------------------------
static bool MakeDirs(const char *path)
{
    char tmp[1024];
    SDL_strlcpy(tmp, path, sizeof(tmp));

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                LOGE("mkdir failed: %s  errno=%d", tmp, errno);
                return false;
            }
            *p = '/';
        }
    }
    return true;
}

// -------------------------------------------------------------------------
// Helper: copy one file from APK assets to the filesystem
// -------------------------------------------------------------------------
static bool CopyAssetFile(const char *srcRelPath, const char *destPath)
{
    SDL_IOStream *src = SDL_IOFromFile(srcRelPath, "rb");
    if (!src) {
        LOGE("Cannot open asset: %s  SDL_error=%s", srcRelPath, SDL_GetError());
        return false;
    }

    FILE *dst = fopen(destPath, "wb");
    if (!dst) {
        LOGE("Cannot create: %s  errno=%d", destPath, errno);
        SDL_CloseIO(src);
        return false;
    }

    static char buf[65536];
    Sint64 n;
    bool ok = true;
    while ((n = SDL_ReadIO(src, buf, sizeof(buf))) > 0) {
        if (fwrite(buf, 1, (size_t)n, dst) != (size_t)n) {
            LOGE("Write error: %s  errno=%d", destPath, errno);
            ok = false;
            break;
        }
    }

    fclose(dst);
    SDL_CloseIO(src);
    return ok;
}

// -------------------------------------------------------------------------
// Public API
// -------------------------------------------------------------------------
bool Android_ExtractAssets(const char *destDir)
{
    LOGI("Extracting assets to: %s", destDir);

    // Check if we already extracted the current version
    char versionPath[1024];
    SDL_snprintf(versionPath, sizeof(versionPath), "%s/" EXTRACT_VERSION_FILE, destDir);

    FILE *vf = fopen(versionPath, "r");
    if (vf) {
        char storedVersion[64] = {0};
        fgets(storedVersion, sizeof(storedVersion), vf);
        fclose(vf);
        // Trim trailing newline
        size_t len = strlen(storedVersion);
        while (len > 0 && (storedVersion[len-1] == '\n' || storedVersion[len-1] == '\r'))
            storedVersion[--len] = '\0';

        if (strcmp(storedVersion, EXTRACT_VERSION) == 0) {
            LOGI("Assets already extracted (version %s), skipping.", EXTRACT_VERSION);
            return true;
        }
        LOGI("Asset version mismatch (stored=%s, want=%s), re-extracting.", storedVersion, EXTRACT_VERSION);
    }

    // Ensure destDir exists
    if (mkdir(destDir, 0755) != 0 && errno != EEXIST) {
        LOGE("Cannot create destDir: %s  errno=%d", destDir, errno);
        return false;
    }

    int extracted = 0;
    int failed = 0;

    for (int i = 0; kAllDataFiles[i] != NULL; i++) {
        const char *relPath = kAllDataFiles[i];

        // Build full destination path
        char destPath[1024];
        SDL_snprintf(destPath, sizeof(destPath), "%s/%s", destDir, relPath);

        // Create parent directories
        if (!MakeDirs(destPath)) {
            failed++;
            continue;
        }

        // Copy file
        if (!CopyAssetFile(relPath, destPath)) {
            failed++;
            continue;
        }

        extracted++;
    }

    LOGI("Extracted %d files, %d failed.", extracted, failed);

    if (failed > 0) {
        LOGE("Asset extraction had %d failures!", failed);
        return false;
    }

    // Write version stamp only after all files succeed
    vf = fopen(versionPath, "w");
    if (vf) {
        fprintf(vf, "%s\n", EXTRACT_VERSION);
        fclose(vf);
    }

    return true;
}

#endif // __ANDROID__
