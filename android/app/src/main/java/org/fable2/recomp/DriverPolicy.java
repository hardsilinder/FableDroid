package org.fable2.recomp;

import java.util.regex.Pattern;

/** Pure selection policy, independent of Android and the hardware probe. */
final class DriverPolicy {
    private static final Pattern ADRENO_7XX = Pattern.compile(
        "\\badreno\\s*(?:\\(tm\\)\\s*)?7[0-9]{2}\\b", Pattern.CASE_INSENSITIVE);

    static String defaultDriver(String renderer) {
        return renderer != null && ADRENO_7XX.matcher(renderer).find() ? "turnip27" : "";
    }

    static String family(String renderer) {
        String r = renderer == null ? "" : renderer.toLowerCase(java.util.Locale.ROOT);
        java.util.regex.Matcher m = Pattern.compile("adreno\\s*(?:\\(tm\\)\\s*)?([0-9]{3})\\b").matcher(r);
        if (m.find()) return "Adreno " + m.group(1).charAt(0) + "xx";
        if (r.contains("immortalis")) return "Immortalis";
        if (r.contains("mali")) return "Mali";
        if (r.contains("powervr")) return "PowerVR";
        if (r.contains("xclipse")) return "Xclipse";
        return r.isEmpty() ? "Unknown GPU" : "Other GPU";
    }

    static String recommendation(String renderer) {
        String family = family(renderer);
        if (renderer != null && Pattern.compile("(?i)adreno\\s*(?:\\(tm\\)\\s*)?740\\b").matcher(renderer).find())
            return "Bundled Turnip — tested on AYN Thor (Adreno 740). Other devices with this GPU may behave differently.";
        if (family.equals("Adreno 7xx"))
            return "Bundled Turnip — experimental on this GPU. Downloadable 6xx/7xx Turnip builds are alternatives.";
        if (family.equals("Adreno 8xx"))
            return "System driver — experimental. Try a matching Gen8 download below if needed; Fable II compatibility is not yet verified.";
        if (family.equals("Adreno 6xx"))
            return "System driver — experimental. Matching 6xx/7xx Turnip downloads are available to try.";
        return "System driver — experimental for " + family + ". No verified compatible driver downloads are available in this launcher.";
    }

    static boolean matchingDownload(String renderer, String name) {
        String family = family(renderer);
        if (family.equals("Adreno 8xx")) return name.matches("Turnip_Gen8_V[0-9]+\\.zip");
        if (family.equals("Adreno 6xx") || family.equals("Adreno 7xx"))
            return name.matches("Turnip_v[0-9]+\\.[0-9]+\\.[0-9]+-R[0-9]+\\.zip");
        return false;
    }

    static String select(String explicit, Boolean legacyBundled, String renderer) {
        if (explicit != null && (explicit.isEmpty() || explicit.equals("turnip27")
                || explicit.matches("download-[a-f0-9]{64}"))) return explicit;
        if (Boolean.FALSE.equals(legacyBundled)) return "";
        return defaultDriver(renderer);
    }
}
