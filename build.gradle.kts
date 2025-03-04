plugins {
    alias(libs.plugins.lsplugin.publish)
}

val androidTargetSdkVersion by extra(35)
val androidMinSdkVersion by extra(21)
val androidBuildToolsVersion by extra("35.0.0")
val androidCompileSdkVersion by extra(35)
val androidNdkVersion by extra("28.0.13004108")
val androidCmakeVersion by extra("3.28.0+")
