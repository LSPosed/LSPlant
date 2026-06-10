plugins {
    alias(libs.plugins.agp.app) apply false
    alias(libs.plugins.agp.lib) apply false
    alias(libs.plugins.lsplugin.publish) apply false
}

val androidTargetSdkVersion by extra(37)
val androidMinSdkVersion by extra(21)
val androidBuildToolsVersion by extra("37.0.0")
val androidCompileSdkVersion by extra(37)
val androidNdkVersion by extra(libs.versions.ndk.get())
val androidCmakeVersion by extra("3.28.0+")
