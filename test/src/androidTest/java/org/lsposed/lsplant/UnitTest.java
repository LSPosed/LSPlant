package org.lsposed.lsplant;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Assert;
import org.junit.FixMethodOrder;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.MethodSorters;

import java.lang.reflect.InvocationTargetException;

@RunWith(AndroidJUnit4.class)
@FixMethodOrder(MethodSorters.NAME_ASCENDING)
public class UnitTest {

    @Test
    public void t00_init() {
        Assert.assertTrue(LSPTest.initHooker());
    }

    @Test
    public void t01_staticMethod() throws NoSuchMethodException, InvocationTargetException, IllegalAccessException {
        var staticMethod = LSPTest.class.getDeclaredMethod("staticMethod");
        var staticMethodReplacement = Replacement.class.getDeclaredMethod("staticMethodReplacement", Hooker.MethodCallback.class);
        Assert.assertFalse(LSPTest.staticMethod());

        Hooker hooker = Hooker.hook(staticMethod, staticMethodReplacement, null);
        Assert.assertNotNull(hooker);
        Assert.assertTrue(LSPTest.staticMethod());
        Assert.assertFalse((boolean) hooker.backup.invoke(null));

        Assert.assertTrue(hooker.unhook());
        Assert.assertFalse(LSPTest.staticMethod());
    }

    @Test
    public void t02_normalMethod() throws NoSuchMethodException, InvocationTargetException, IllegalAccessException {
        var normalMethod = LSPTest.class.getDeclaredMethod("normalMethod", String.class, int.class, long.class);
        var normalMethodReplacement = Replacement.class.getDeclaredMethod("normalMethodReplacement", Hooker.MethodCallback.class);
        var a = "test";
        var b = 114514;
        var c = 1919810L;
        var o = a + b + c;
        var r = a + b + c + "replace";
        LSPTest test = new LSPTest();
        Assert.assertEquals(o, test.normalMethod(a, b, c));

        Hooker hooker = Hooker.hook(normalMethod, normalMethodReplacement, new Replacement());
        Assert.assertNotNull(hooker);
        Assert.assertEquals(r, test.normalMethod(a, b, c));
        Assert.assertEquals(o, hooker.backup.invoke(test, a, b, c));

        Assert.assertTrue(hooker.unhook());
        Assert.assertEquals(o, test.normalMethod(a, b, c));
    }

    @Test
    public void t03_constructor() throws NoSuchMethodException, InvocationTargetException, IllegalAccessException, InstantiationException {
        var constructor = LSPTest.class.getDeclaredConstructor();
        var constructorReplacement = Replacement.class.getDeclaredMethod("constructorReplacement", Hooker.MethodCallback.class);
        Assert.assertFalse(new LSPTest().field);
        Assert.assertFalse(constructor.newInstance().field);

        Hooker hooker = Hooker.hook(constructor, constructorReplacement, new Replacement());
        Assert.assertNotNull(hooker);
        Assert.assertTrue(new LSPTest().field);
        Assert.assertTrue(constructor.newInstance().field);

        Assert.assertTrue(hooker.unhook());
        Assert.assertFalse(new LSPTest().field);
        Assert.assertFalse(constructor.newInstance().field);
    }

    @Test
    public void t04_manyParametersMethod() throws NoSuchMethodException, InvocationTargetException, IllegalAccessException {
        var manyParametersMethod = LSPTest.class.getDeclaredMethod("manyParametersMethod", String.class, boolean.class, byte.class, short.class, int.class, long.class, float.class, double.class, Integer.class, Long.class);
        var manyParametersReplacement = Replacement.class.getDeclaredMethod("manyParametersReplacement", Hooker.MethodCallback.class);
        var a = "test";
        var b = true;
        var c = (byte) 114;
        var d = (short) 514;
        var e = 19;
        var f = 19L;
        var g = 810f;
        var h = 12345f;
        var o = a + b + c + d + e + f + g + h + e + f;
        var r = a + b + c + d + e + f + g + h + e + f + "replace";
        LSPTest test = new LSPTest();
        Assert.assertEquals(o, test.manyParametersMethod(a, b, c, d, e, f, g, h, e, f));

        Hooker hooker = Hooker.hook(manyParametersMethod, manyParametersReplacement, new Replacement());
        Assert.assertNotNull(hooker);
        Assert.assertEquals(r, test.manyParametersMethod(a, b, c, d, e, f, g, h, e, f));
        Assert.assertEquals(o, hooker.backup.invoke(test, a, b, c, d, e, f, g, h, e, f));

        Assert.assertTrue(hooker.unhook());
        Assert.assertEquals(o, test.manyParametersMethod(a, b, c, d, e, f, g, h, e, f));
    }
}
