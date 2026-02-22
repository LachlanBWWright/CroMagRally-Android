// AndroidAssets.c
// Android asset extraction helper for Cro-Mag Rally.
// Copies game data from APK assets to internal storage on first launch.

#ifdef __ANDROID__

#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define ASSETS_VERSION 1
#define ASSETS_VERSION_FILE ".cmr_assets_version"

static const char * const kAllDataFiles[] = {
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

static bool ExtractOneFile(const char *destDir, const char *relPath)
{
    char destPath[2048];
    snprintf(destPath, sizeof(destPath), "%s/Data/%s", destDir, relPath);

    char parentDir[2048];
    snprintf(parentDir, sizeof(parentDir), "%s", destPath);
    char *lastSlash = strrchr(parentDir, '/');
    if (lastSlash) {
        *lastSlash = '\0';
        SDL_CreateDirectory(parentDir);
    }

    SDL_IOStream *src = SDL_IOFromFile(relPath, "rb");
    if (!src) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "ExtractOneFile: could not open APK asset: %s", relPath);
        return false;
    }

    FILE *dst = fopen(destPath, "wb");
    if (!dst) {
        SDL_CloseIO(src);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "ExtractOneFile: could not create: %s", destPath);
        return false;
    }

    char buf[65536];
    bool ok = true;
    while (true) {
        size_t nr = SDL_ReadIO(src, buf, sizeof(buf));
        if (nr == 0) break;
        size_t nw = fwrite(buf, 1, nr, dst);
        if (nw != nr) { ok = false; break; }
    }

    fclose(dst);
    SDL_CloseIO(src);
    return ok;
}

bool Android_ExtractAssets(void)
{
    const char *internalPath = SDL_GetAndroidInternalStoragePath();
    if (!internalPath) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "Android_ExtractAssets: no internal storage path");
        return false;
    }

    char stampPath[2048];
    snprintf(stampPath, sizeof(stampPath), "%s/" ASSETS_VERSION_FILE, internalPath);

    FILE *stampFile = fopen(stampPath, "r");
    if (stampFile) {
        int ver = 0;
        fscanf(stampFile, "%d", &ver);
        fclose(stampFile);
        if (ver >= ASSETS_VERSION) {
            SDL_Log("Android_ExtractAssets: assets already extracted (version %d)", ver);
            return true;
        }
    }

    SDL_Log("Android_ExtractAssets: extracting game data to %s", internalPath);

    char dataDir[2048];
    snprintf(dataDir, sizeof(dataDir), "%s/Data", internalPath);
    SDL_CreateDirectory(dataDir);

    for (int i = 0; kAllDataFiles[i]; i++) {
        if (!ExtractOneFile(internalPath, kAllDataFiles[i])) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "Android_ExtractAssets: failed on: %s", kAllDataFiles[i]);
            return false;
        }
    }

    stampFile = fopen(stampPath, "w");
    if (stampFile) {
        fprintf(stampFile, "%d\n", ASSETS_VERSION);
        fclose(stampFile);
    }

    SDL_Log("Android_ExtractAssets: done.");
    return true;
}

const char *Android_GetDataPath(void)
{
    const char *internalPath = SDL_GetAndroidInternalStoragePath();
    static char dataPath[2048];
    if (internalPath) {
        snprintf(dataPath, sizeof(dataPath), "%s/Data", internalPath);
    } else {
        snprintf(dataPath, sizeof(dataPath), "Data");
    }
    return dataPath;
}

#endif // __ANDROID__
