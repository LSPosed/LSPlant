package org.lsposed.lsplant;

import java.lang.reflect.Constructor;
import java.lang.reflect.Executable;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.Arrays;

public class Hooker {

    public Executable backup;

    private boolean isStatic;
    private Executable target, replacement;

    private Hooker() {
    }

    private native Executable doHook(Executable original, Executable callback);

    private native boolean doUnhook(Executable target);

    public Object callback(Object[] args) throws InvocationTargetException, IllegalAccessException, InstantiationException {
        if (replacement instanceof Method) {
            if (isStatic) {
                return ((Method) replacement).invoke(null, args);
            } else {
                return ((Method) replacement).invoke(args[0], Arrays.copyOfRange(args, 1, args.length));
            }
        } else if (replacement instanceof Constructor) {
            return ((Constructor<?>) replacement).newInstance(args);
        } else {
            throw new IllegalArgumentException("Unsupported executable type");
        }
    }

    public boolean unhook() {
        return doUnhook(target);
    }

    public static Hooker hook(Executable target, Executable replacement) {
        Hooker hooker = new Hooker();
        try {
            var callbackMethod = Hooker.class.getDeclaredMethod("callback", Object[].class);
            var result = hooker.doHook(target, callbackMethod);
            if (result == null) return null;
            hooker.isStatic = (replacement.getModifiers() & Modifier.STATIC) != 0;
            hooker.backup = result;
            hooker.target = target;
            hooker.replacement = replacement;
        } catch (NoSuchMethodException ignored) {
        }
        return hooker;
    }
}
