package org.fable2.recomp;

import org.junit.Test;
import static org.junit.Assert.*;

public class DriverPolicyTest {
    @Test public void hardwareDefaults() {
        assertEquals("turnip27", DriverPolicy.defaultDriver("Adreno (TM) 740"));
        assertEquals("turnip27", DriverPolicy.defaultDriver("Adreno 750"));
        for (String gpu : new String[] { "Adreno (TM) 830", "Adreno 840", "Adreno 640", "Mali-G715", "", "Adreno 7400" })
            assertEquals(gpu, "", DriverPolicy.defaultDriver(gpu));
        assertEquals("", DriverPolicy.defaultDriver(null));
    }
    @Test public void upgradesAndManualChoices() {
        assertEquals("", DriverPolicy.select(null, null, "Adreno (TM) 830"));
        assertEquals("", DriverPolicy.select(null, true, "Adreno (TM) 830"));
        assertEquals("turnip27", DriverPolicy.select(null, null, "Adreno (TM) 740"));
        assertEquals("", DriverPolicy.select(null, false, "Adreno (TM) 740"));
        assertEquals("", DriverPolicy.select("", null, "Adreno (TM) 740"));
        assertEquals("turnip27", DriverPolicy.select("turnip27", null, "Adreno (TM) 830"));
        String downloaded = "download-" + "a".repeat(64);
        assertEquals(downloaded, DriverPolicy.select(downloaded, null, "Adreno (TM) 830"));
        assertEquals("", DriverPolicy.select("../bad", null, "Adreno (TM) 830"));
    }
}
