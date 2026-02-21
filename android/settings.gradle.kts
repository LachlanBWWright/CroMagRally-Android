pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}

rootProject.name = "CroMagRally"
include(":app")

// Include SDL3 android project if available (provides SDLActivity Java class)
val sdlAndroidProject = file("../extern/SDL/android-project")
if (sdlAndroidProject.exists()) {
    includeBuild("../extern/SDL/android-project") {
        dependencySubstitution {
            substitute(module("org.libsdl.app:SDL3")).using(project(":app"))
        }
    }
}
