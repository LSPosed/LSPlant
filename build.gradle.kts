plugins {
    alias(libs.plugins.lsplugin.publish)
}

val androidTargetSdkVersion by extra(33)
val androidMinSdkVersion by extra(21)
val androidBuildToolsVersion by extra("33.0.2")
val androidCompileSdkVersion by extra(33)
val androidNdkVersion by extra("25.2.9519653")
val androidCmakeVersion by extra("3.22.1+")
