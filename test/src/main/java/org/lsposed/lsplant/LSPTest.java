package org.lsposed.lsplant;

public class LSPTest {

    static {
        System.loadLibrary("test");
    }

    boolean field;

    LSPTest() {
        field = false;
    }

    native static boolean initHooker();

    static boolean staticMethod() {
        return false;
    }

    String normalMethod(String a, int b, long c) {
        return a + b + c;
    }
}
