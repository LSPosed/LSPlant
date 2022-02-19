package org.lsposed.lsplant;

import java.lang.reflect.InvocationTargetException;

public class Replacement {

    static boolean staticMethodReplacement(Hooker.MethodCallback callback) {
        return true;
    }

    String normalMethodReplacement(Hooker.MethodCallback callback) {
        var a = (String) callback.args[1];
        var b = (int) callback.args[2];
        var c = (long) callback.args[3];
        return a + b + c + "replace";
    }

    void constructorReplacement(Hooker.MethodCallback callback) throws InvocationTargetException, IllegalAccessException {
        var test = (LSPTest) callback.args[0];
        callback.backup.invoke(test);
        test.field = true;
    }
}
