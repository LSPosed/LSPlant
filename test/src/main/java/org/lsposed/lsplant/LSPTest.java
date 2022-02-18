package org.lsposed.lsplant;

public class LSPTest {

    static {
        System.loadLibrary("test");
    }

    native static boolean initHooker();
}
