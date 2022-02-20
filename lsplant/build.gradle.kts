plugins {
    id("com.android.library")
    id("maven-publish")
    id("signing")
}

val androidTargetSdkVersion: Int by rootProject.extra
val androidMinSdkVersion: Int by rootProject.extra
val androidBuildToolsVersion: String by rootProject.extra
val androidCompileSdkVersion: Int by rootProject.extra
val androidNdkVersion: String by rootProject.extra
val androidCmakeVersion: String by rootProject.extra

android {
    compileSdk = androidCompileSdkVersion
    ndkVersion = androidNdkVersion
    buildToolsVersion = androidBuildToolsVersion

    buildFeatures {
        buildConfig = false
        prefabPublishing = true
    }

    packagingOptions {
        jniLibs {
            excludes += "**.so"
        }
    }

    prefab {
        register("lsplant") {
            headers = "src/main/jni/include"
        }
    }

    defaultConfig {
        minSdk = androidMinSdkVersion
        targetSdk = androidTargetSdkVersion
    }

    buildTypes {
        all {
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
                        "-ffunction-sections",
                        "-fdata-sections",
                        "-Wno-unused-value",
                        "-Wl,--gc-sections",
                        "-D__FILE__=__FILE_NAME__",
                        "-Wl,--exclude-libs,ALL",
                    )
                    cppFlags("-std=c++20", *flags)
                    cFlags("-std=c18", *flags)
                    val configFlags = arrayOf(
                        "-Oz",
                        "-DNDEBUG"
                    ).joinToString(" ")
                    arguments(
                        "-DANDROID_STL=c++_shared",
                        "-DCMAKE_CXX_FLAGS_RELEASE=$configFlags",
                        "-DCMAKE_CXX_FLAGS_RELWITHDEBINFO=$configFlags",
                        "-DCMAKE_C_FLAGS_RELEASE=$configFlags",
                        "-DCMAKE_C_FLAGS_RELWITHDEBINFO=$configFlags",
                        "-DDEBUG_SYMBOLS_PATH=${project.buildDir.absolutePath}/symbols/$name",
                    )
                }
            }
        }
    }

    lint {
        abortOnError = true
        checkReleaseBuilds = false
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/jni/CMakeLists.txt")
            version = androidCmakeVersion
        }
    }

    publishing {
        singleVariant("release") {
            withSourcesJar()
            withJavadocJar()
        }
    }
}

publishing {
    publications {
        register<MavenPublication>("lsplant") {
            group = "org.lsposed.lsplant"
            artifactId = "lsplant"
            version = "1.0"
            afterEvaluate {
                from(components.getByName("release"))
            }
            pom {
                name.set("LSPlant")
                description.set("A hook framework for Android Runtime (ART)")
                url.set("https://github.com/LSPosed/LSPlant")
                licenses {
                    license {
                        name.set("GNU Lesser General Public License v3.0")
                        url.set("https://github.com/LSPosed/LSPlant/blob/master/LICENSE")
                    }
                }
                developers {
                    developer {
                        name.set("Lsposed")
                        url.set("https://lsposed.org")
                    }
                }
                scm {
                    connection.set("scm:git:https://github.com/LSPosed/LSPlant.git")
                    url.set("https://github.com/LSPosed/LSPlant")
                }
            }
        }
    }
    repositories {
        maven {
            name = "ossrh"
            url = uri("https://s01.oss.sonatype.org/service/local/staging/deploy/maven2/")
            credentials(PasswordCredentials::class)
        }
    }
}

signing {
    val signingKey = findProperty("signingKey") as String?
    val signingPassword = findProperty("signingPassword") as String?
    if (signingKey != null && signingPassword != null) {
        useInMemoryPgpKeys(signingKey, signingPassword)
    }
    sign(publishing.publications)
}
