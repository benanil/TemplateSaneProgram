plugins {
    alias(libs.plugins.android.application)
}

android {
    namespace = "com.anilgames.test"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.anilgames.test"
        minSdk = 30
        targetSdk = 34
        versionCode = 1
        versionName = "1.0"

        ndk.abiFilters.clear()
        ndk.abiFilters.add("arm64-v8a")

        externalNativeBuild {
            cmake {
                cppFlags.addAll(listOf("-std=c++17", "-mfloat-abi=softfp", "-mfpu=neon"))
                arguments.addAll(listOf("-DANDROID_STL=c++_shared", "-DANDROID_ARM_NEON=ON"))
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
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_1_8
        targetCompatibility = JavaVersion.VERSION_1_8
    }
    buildFeatures {
        prefab = true
    }
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
}

dependencies {

    implementation(libs.appcompat)
    implementation(libs.material)
    implementation(libs.games.activity)
    implementation(libs.games.frame.pacing)
    androidTestImplementation(libs.espresso.core)
}