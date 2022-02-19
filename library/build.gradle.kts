/*
 * This file is part of LSPosed.
 *
 * LSPosed is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LSPosed is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LSPosed.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2021 LSPosed Contributors
 */

plugins {
    id("com.android.library")
}

val androidTargetSdkVersion: Int by rootProject.extra
val androidMinSdkVersion: Int by rootProject.extra
val androidBuildToolsVersion: String by rootProject.extra
val androidCompileSdkVersion: Int by rootProject.extra
val androidCompileNdkVersion: String by rootProject.extra

android {
    compileSdk = androidCompileSdkVersion
    ndkVersion = androidCompileNdkVersion
    buildToolsVersion = androidBuildToolsVersion

    buildFeatures {
        prefab = true
        prefabPublishing = true
    }

    prefab {
        create("lsplant") {
            headers = "jni/include"
            libraryName = "liblsplant"
            name = "lsplant"
        }

        // only for unit test
        create("dex_builder") {
            headers = "jni/external/dex_builder/include"
            libraryName = "libdex_builder"
            name = "dex_builder"
        }
    }

    defaultConfig {
        minSdk = androidMinSdkVersion
        targetSdk = androidTargetSdkVersion
        externalNativeBuild {
            cmake {
                abiFilters("arm64-v8a", "armeabi-v7a", "x86", "x86_64")
                val flags = arrayOf(
                    "-Wall",
                    "-Werror",
                    "-Qunused-arguments",
                    "-Wno-gnu-string-literal-operator-template",
                    "-fno-rtti",
                    "-fvisibility=hidden",
                    "-fvisibility-inlines-hidden",
                    "-fno-exceptions",
                    "-fno-stack-protector",
                    "-fomit-frame-pointer",
                    "-Wno-builtin-macro-redefined",
                    "-Wl,--strip-all",
                    "-ffunction-sections",
                    "-fdata-sections",
                    "-Wno-unused-value",
                    "-Wl,--gc-sections",
                    "-D__FILE__=__FILE_NAME__",
                    "-Wl,--exclude-libs,ALL",
                )
                cppFlags("-std=c++20", *flags)
                cFlags("-std=c18", *flags)
                arguments += "-DCMAKE_VERBOSE_MAKEFILE=ON"
                arguments += "-DANDROID_STL=c++_shared"
                val configFlags = arrayOf(
                    "-Oz",
                    "-DNDEBUG"
                ).joinToString(" ")
                arguments.addAll(
                    arrayOf(
                        "-DCMAKE_CXX_FLAGS_RELEASE=$configFlags",
                        "-DCMAKE_CXX_FLAGS_RELWITHDEBINFO=$configFlags",
                        "-DCMAKE_C_FLAGS_RELEASE=$configFlags",
                        "-DCMAKE_C_FLAGS_RELWITHDEBINFO=$configFlags"
                    )
                )
            }
        }
    }

    lint {
        abortOnError = true
        checkReleaseBuilds = false
    }

    externalNativeBuild {
        cmake {
            path("jni/CMakeLists.txt")
        }
    }
}
