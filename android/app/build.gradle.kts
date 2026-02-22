plugins {
    id("com.android.application")
}

android {
    namespace = "com.lachlan.cromagrally"
    compileSdk = 34
    ndkVersion = "27.3.13750724"

    defaultConfig {
        applicationId = "com.lachlan.cromagrally"
        minSdk = 24
        targetSdk = 34
        versionCode = 1
        versionName = "3.0.2"

        ndk {
            abiFilters += listOf("armeabi-v7a", "arm64-v8a", "x86_64")
        }

        externalNativeBuild {
            cmake {
                arguments(
                    "-DANDROID_STL=c++_shared",
                    "-DBUILD_SDL_FROM_SOURCE=ON",
                    "-DSDL_STATIC=OFF",
                    "-DANDROID=TRUE"
                )
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
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
        }
    }

    packaging {
        jniLibs {
            useLegacyPackaging = true
        }
    }
}
