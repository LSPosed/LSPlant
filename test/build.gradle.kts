import com.android.build.api.dsl.ManagedVirtualDevice

plugins {
    alias(libs.plugins.agp.app)
}

val androidTargetSdkVersion: Int by rootProject.extra
val androidMinSdkVersion: Int by rootProject.extra
val androidBuildToolsVersion: String by rootProject.extra
val androidCompileSdkVersion: Int by rootProject.extra
val androidNdkVersion: String by rootProject.extra
val androidCmakeVersion: String by rootProject.extra

android {
    namespace = "org.lsposed.lsplant.test"
    compileSdk = androidCompileSdkVersion
    ndkVersion = androidNdkVersion
    buildToolsVersion = androidBuildToolsVersion

    buildFeatures {
        buildConfig = false
        prefab = true
    }

    defaultConfig {
        applicationId = "org.lsposed.lsplant"
        minSdk = androidMinSdkVersion
        targetSdk = androidTargetSdkVersion
        versionCode = 1
        versionName = "1.0"
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        externalNativeBuild {
            cmake {
                arguments += "-DANDROID_STL=c++_shared"
            }
        }
    }

    buildTypes {
        debug {
            isDebuggable = false
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/jni/CMakeLists.txt")
            version = androidCmakeVersion
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }

    testOptions {
        managedDevices {
            allDevices {
                fun createDevice(api: Int, is64: Boolean, target: String = "default") = create<ManagedVirtualDevice>("""avd-$api-${if(is64) "x86_64" else "x86"}-$target""") {
                    device = "Pixel 2"
                    apiLevel = api
                    systemImageSource = target
                    require64Bit = is64
                }

                createDevice(21, false)
                createDevice(21, true)
                createDevice(22, false)
                createDevice(22, true)
                createDevice(23, false)
                createDevice(23, true)
                createDevice(24, false)
                createDevice(24, true)
                createDevice(25, false)
                createDevice(25, true)
                createDevice(26, false)
                createDevice(26, true)
                createDevice(27, false)
                createDevice(27, true)
                createDevice(28, false)
                createDevice(28, true)
                createDevice(29, false)
                createDevice(29, true)
                createDevice(30, false, "aosp_atd")
                createDevice(30, true)
//                createDevice(31, false, "android-tv")
                createDevice(31, true, "aosp_atd")
                createDevice(32, true, "google_apis")
                createDevice(33, true, "google_apis")
                createDevice(34, true, "google_apis")
            }
        }
    }
}

dependencies {
    implementation(project(":lsplant"))
    implementation(libs.dobby)

    androidTestImplementation(libs.test.ext.junit)
    androidTestImplementation(libs.test.runner)
    androidTestImplementation(libs.test.espresso)
}

