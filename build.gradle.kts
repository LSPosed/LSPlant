plugins {
    alias(libs.plugins.lsplugin.publish)
}

val androidTargetSdkVersion by extra(34)
val androidMinSdkVersion by extra(21)
val androidBuildToolsVersion by extra("34.0.0")
val androidCompileSdkVersion by extra(34)
val androidNdkVersion by extra("27.0.11718014-beta1")
val androidCmakeVersion by extra("3.28.0+")
