val androidTargetSdkVersion by extra(32)
val androidMinSdkVersion by extra(27)
val androidBuildToolsVersion by extra("32.0.0")
val androidCompileSdkVersion by extra(32)
val androidNdkVersion by extra("23.1.7779620")
val androidCmakeVersion by extra("3.22.1")

tasks.register("Delete", Delete::class) {
    delete(rootProject.buildDir)
}
