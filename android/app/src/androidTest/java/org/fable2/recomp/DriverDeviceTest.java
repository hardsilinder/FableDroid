package org.fable2.recomp;

import org.junit.Test;
import static org.junit.Assert.*;
import java.util.List;

/** Hardware probe and live catalog smoke test; leaves settings and saves untouched. */
public class DriverDeviceTest {
    @Test public void testSystemGpuProbeAndLiveCatalog() throws Exception {
        String renderer = GpuDetector.renderer();
        assertFalse("System GPU detection returned empty", renderer.isEmpty());
        android.util.Log.i("DriverDeviceTest", "Detected: " + renderer);
        assertEquals(renderer, GpuDetector.renderer());
        List<DriverStore.Release> releases = DriverStore.releases();
        assertFalse("Live driver catalog is empty", releases.isEmpty());
        int matches = 0;
        DriverStore.Release firstMatch = null;
        for (DriverStore.Release release : releases)
            if (DriverPolicy.matchingDownload(renderer, release.name())) { matches++; if (firstMatch == null) firstMatch = release; }
        if (DriverPolicy.family(renderer).matches("Adreno [678]xx"))
            assertTrue("No matching live driver downloads", matches > 0);
        if (firstMatch != null) {
            java.io.File root = java.nio.file.Files.createTempDirectory(
                androidx.test.platform.app.InstrumentationRegistry.getInstrumentation().getTargetContext().getCacheDir().toPath(),
                "driver-smoke-").toFile();
            try {
                String id = DriverStore.installArchive(root, firstMatch,
                    DriverStore.fetch(firstMatch.url(), 64 * 1024 * 1024), android.os.Build.VERSION.SDK_INT);
                String name = DriverStore.validate(new java.io.File(root, id));
                assertFalse(name.isEmpty());
                android.util.Log.i("DriverDeviceTest", "Downloaded, checksum verified and installed in test cache: " + name);
            } finally { remove(root); }
        }
        android.util.Log.i("DriverDeviceTest", "Catalog: " + releases.size() + ", matching: " + matches
            + ", recommendation: " + DriverPolicy.recommendation(renderer));
    }
    private static void remove(java.io.File file) {
        java.io.File[] children = file.listFiles();
        if (children != null) for (java.io.File child : children) remove(child);
        file.delete();
    }
}
