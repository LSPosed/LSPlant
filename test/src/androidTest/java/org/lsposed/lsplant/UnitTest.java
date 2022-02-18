package org.lsposed.lsplant;

import android.support.test.runner.AndroidJUnit4;

import org.junit.Assert;
import org.junit.FixMethodOrder;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.MethodSorters;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

@RunWith(AndroidJUnit4.class)
@FixMethodOrder(MethodSorters.NAME_ASCENDING)
public class UnitTest {

    private final List<Hooker> hookers = new ArrayList<>();

    @Test
    public void t00_initTest() {
        boolean result = LSPTest.initHooker();
        Assert.assertTrue(result);
    }

    @Test
    public void t01_hookTest() throws NoSuchMethodException, InvocationTargetException, IllegalAccessException {
        var staticMethod = LSPTest.class.getDeclaredMethod("staticMethod");
        var staticMethodReplacement = Replacement.class.getDeclaredMethod("staticMethodReplacement");
        Assert.assertFalse(LSPTest.staticMethod());
        Hooker hooker1 = Hooker.hook(staticMethod, staticMethodReplacement);
        hookers.add(hooker1);
        Assert.assertNotNull(hooker1);
        Assert.assertTrue(LSPTest.staticMethod());
        Assert.assertFalse((Boolean) ((Method) hooker1.backup).invoke(null));
    }

    @Test
    public void t02_unhookTest() {
        for (Hooker hooker : hookers) {
            Assert.assertTrue(hooker.unhook());
        }
        Assert.assertFalse(LSPTest.staticMethod());
    }
}
