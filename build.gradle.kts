plugins {
    alias(libs.plugins.lsplugin.publish)
}

val androidTargetSdkVersion by extra(34)
val androidMinSdkVersion by extra(21)
val androidBuildToolsVersion by extra("34.0.0")
val androidCompileSdkVersion by extra(34)
val androidNdkVersion by extra("26.0.10792818")
val androidCmakeVersion by extra("3.22.1+")
