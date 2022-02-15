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
import org.eclipse.jgit.api.Git
import org.eclipse.jgit.internal.storage.file.FileRepository

buildscript {
    repositories {
        google()
        mavenCentral()
    }
    val agpVersion by extra("7.1.1")
    dependencies {
        classpath("com.android.tools.build:gradle:$agpVersion")
        classpath("org.eclipse.jgit:org.eclipse.jgit:6.0.0.202111291000-r")
    }
}

val androidTargetSdkVersion by extra(32)
val androidMinSdkVersion by extra(27)
val androidBuildToolsVersion by extra("32.0.0")
val androidCompileSdkVersion by extra(32)
val androidCompileNdkVersion by extra("23.1.7779620")

tasks.register("Delete", Delete::class) {
    delete(rootProject.buildDir)
}
