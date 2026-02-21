plugins {
    id("com.android.application")
}

android {
    namespace = "io.jor.cromagrally"
    compileSdk = 34
    ndkVersion = "27.3.13750724"

    defaultConfig {
        applicationId = "io.jor.cromagrally"
        minSdk = 24
        targetSdk = 34
        versionCode = 1
        versionName = "3.0.2"

        ndk {
            abiFilters += listOf("arm64-v8a", "armeabi-v7a", "x86", "x86_64")
        }

        externalNativeBuild {
            cmake {
                arguments(
                    "-DANDROID_STL=c++_shared",
                    "-DBUILD_SDL_FROM_SOURCE=ON",
                    "-DSDL3_DIR=${rootProject.projectDir.parentFile}/extern/SDL",
                    "-DSDL_STATIC=OFF",
                    "-DANDROID=TRUE",
                    "-DANDROID_PLATFORM=android-24"
                )
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("../../CMakeLists.txt")
            version = "3.22.1"
        }
    }

    sourceSets {
        getByName("main") {
            assets.srcDirs("../../Data")
            // SDL3 Java files will be in java/ after CI copies them
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }
}
