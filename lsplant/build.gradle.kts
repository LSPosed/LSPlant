import java.nio.file.Paths

plugins {
    alias(libs.plugins.agp.lib)
    alias(libs.plugins.lsplugin.jgit)
    alias(libs.plugins.lsplugin.publish)
    alias(libs.plugins.lsplugin.cmaker)
    `maven-publish`
    signing
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
        androidResources = false
        prefab = true
    }

    packaging {
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
    }

    buildTypes {
        create("standalone") {
            initWith(getByName("release"))
            matchingFallbacks += "release"
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
    namespace = "org.lsposed.lsplant"

    publishing {
        singleVariant("release") {
            withSourcesJar()
            withJavadocJar()
        }
        singleVariant("standalone") {
            withSourcesJar()
            withJavadocJar()
        }
    }
}

cmaker {
    default {
        val flags = arrayOf(
            "-Werror",
            "-Wno-gnu-string-literal-operator-template",
            "-Wno-c++2b-extensions",
        )
        cppFlags += flags
        cFlags += flags
    }
    buildTypes {
        when (it.name) {
            "debug", "release" -> {
                arguments += "-DANDROID_STL=c++_shared"
            }
            "standalone" -> {
                arguments += "-DANDROID_STL=none"
                arguments += "-DLSPLANT_STANDALONE=ON"
            }
        }
        arguments += "-DDEBUG_SYMBOLS_PATH=${project.buildDir.absolutePath}/symbols/${it.name}"
    }
}

dependencies {
    "standaloneCompileOnly"(libs.cxx)
}

val symbolsReleaseTask = tasks.register<Jar>("generateReleaseSymbolsJar") {
    from("${project.buildDir.absolutePath}/symbols/release")
    exclude("**/dex_builder")
    archiveClassifier.set("symbols")
    archiveBaseName.set("release")
}

val symbolsStandaloneTask = tasks.register<Jar>("generateStandaloneSymbolsJar") {
    from("${project.buildDir.absolutePath}/symbols/standalone")
    exclude("**/dex_builder")
    archiveClassifier.set("symbols")
    archiveBaseName.set("standalone")
}

val repo = jgit.repo(true)
val ver = repo?.latestTag?.removePrefix("v") ?: "0.0"
println("${rootProject.name} version: $ver")

publish {
    githubRepo = "LSPosed/LSPlant"
    publications {
        fun MavenPublication.setup() {
            group = "org.lsposed.lsplant"
            version = ver
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
        register<MavenPublication>("lsplant") {
            artifactId = "lsplant"
            afterEvaluate {
                from(components.getByName("release"))
                artifact(symbolsReleaseTask)
            }
            setup()
        }
        register<MavenPublication>("lsplantStandalone") {
            artifactId = "lsplant-standalone"
            afterEvaluate {
                from(components.getByName("standalone"))
                artifact(symbolsStandaloneTask)
            }
            setup()
        }
    }
}
