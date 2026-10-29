package org.fable2.recomp;

import java.util.regex.Pattern;

/** Pure selection policy, independent of Android and the hardware probe. */
final class DriverPolicy {
    private static final Pattern ADRENO_7XX = Pattern.compile(
        "\\badreno\\s*(?:\\(tm\\)\\s*)?7[0-9]{2}\\b", Pattern.CASE_INSENSITIVE);

    static String defaultDriver(String renderer) {
        return renderer != null && ADRENO_7XX.matcher(renderer).find() ? "turnip27" : "";
    }

    static String select(String explicit, Boolean legacyBundled, String renderer) {
        if (explicit != null && (explicit.isEmpty() || explicit.equals("turnip27")
                || explicit.matches("download-[a-f0-9]{64}"))) return explicit;
        if (Boolean.FALSE.equals(legacyBundled)) return "";
        return defaultDriver(renderer);
    }
}
