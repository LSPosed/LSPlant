package org.lsposed.lsplant;

import android.support.test.runner.AndroidJUnit4;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class UnitTest {

    @Test
    public void initTest() {
        boolean result = LSPTest.initHooker();
        Assert.assertTrue(result);
    }
}
