package org.fable2.recomp;

import android.content.Context;
import android.os.Build;
import org.json.*;
import java.io.*;
import java.net.*;
import java.nio.file.Files;
import java.security.MessageDigest;
import java.util.*;
import java.util.zip.*;

/** Immutable, checksum-addressed driver installs. A failed install never replaces a working one. */
final class DriverStore {
    static final String SOURCE = "https://github.com/StevenMXZ/Adreno-Tools-Drivers/releases";
    static final String API = "https://api.github.com/repos/StevenMXZ/Adreno-Tools-Drivers/releases?per_page=20";
    record Release(String name, String url, String hash) {}
    static void check() throws IOException {
        if (Thread.currentThread().isInterrupted()) throw new InterruptedIOException("Cancelled");
    }
    static byte[] fetch(String address, int limit) throws Exception {
        for (int redirects = 0; redirects < 6; redirects++) {
            check();
            URL url = new URL(address);
            String h = url.getHost();
            if (!url.getProtocol().equals("https") || !(h.equals("api.github.com") || h.equals("github.com")
                    || h.equals("release-assets.githubusercontent.com") || h.equals("objects.githubusercontent.com")))
                throw new IOException("Unexpected download destination");
            HttpURLConnection c = (HttpURLConnection)url.openConnection();
            c.setConnectTimeout(15000); c.setReadTimeout(15000); c.setInstanceFollowRedirects(false);
            c.setRequestProperty("User-Agent", "Fable2-Android");
            try {
                int status = c.getResponseCode();
                if (status >= 300 && status < 400) { address = new URL(url, c.getHeaderField("Location")).toString(); continue; }
                if (status != 200) throw new IOException("Download returned HTTP " + status);
                if (c.getContentLengthLong() > limit) throw new IOException("Download too large");
                try (InputStream in = c.getInputStream(); ByteArrayOutputStream out = new ByteArrayOutputStream()) {
                    byte[] buf = new byte[65536];
                    for (int n; (n = in.read(buf)) != -1;) {
                        check(); if (out.size() + n > limit) throw new IOException("Download too large");
                        out.write(buf, 0, n);
                    }
                    return out.toByteArray();
                }
            } finally { c.disconnect(); }
        }
        throw new IOException("Too many redirects");
    }
    static List<Release> releases() throws Exception {
        JSONArray rows = new JSONArray(new String(fetch(API, 4 * 1024 * 1024), java.nio.charset.StandardCharsets.UTF_8));
        List<Release> result = new ArrayList<>();
        for (int i=0; i<rows.length(); i++) {
            JSONObject r=rows.getJSONObject(i);
            if (r.optBoolean("draft") || r.optBoolean("prerelease")) continue;
            JSONArray assets=r.getJSONArray("assets");
            for (int j=0; j<assets.length(); j++) {
                JSONObject a=assets.getJSONObject(j); String name=a.getString("name"), hash=a.optString("digest");
                if (!name.startsWith("Turnip_") || !name.endsWith(".zip") || !hash.matches("sha256:[a-f0-9]{64}")) continue;
                result.add(new Release(name, a.getString("browser_download_url"), hash.substring(7)));
            }
        }
        return result;
    }
    static String hex(byte[] b) { StringBuilder s=new StringBuilder(); for(byte v:b)s.append(String.format("%02x",v&255)); return s.toString(); }
    static String install(Context context, Release release) throws Exception {
        return installArchive(new File(context.getFilesDir(), "gpu_drivers"), release,
            fetch(release.url(), 64*1024*1024), Build.VERSION.SDK_INT);
    }
    static String installArchive(File root, Release release, byte[] archive, int api) throws Exception {
        if (!hex(MessageDigest.getInstance("SHA-256").digest(archive)).equals(release.hash())) throw new IOException("Driver checksum mismatch");
        if (!root.isDirectory() && !root.mkdirs()) throw new IOException("Cannot create driver directory");
        String id="download-"+release.hash(); File target=new File(root,id);
        if(target.isDirectory()) { validate(target, api); return id; }
        File stage=Files.createTempDirectory(root.toPath(), ".install-").toFile();
        try {
            long total=0; int entries=0; Set<String> names=new HashSet<>();
            try(ZipInputStream zip=new ZipInputStream(new ByteArrayInputStream(archive))) {
                for(ZipEntry e; (e=zip.getNextEntry())!=null;) {
                    check(); String name=e.getName();
                    if(++entries>32 || !name.matches("[A-Za-z0-9_.-]+") || name.equals(".") || name.equals("..") || !names.add(name) || e.isDirectory())
                        throw new IOException("Unsupported driver archive layout");
                    try(OutputStream out=new FileOutputStream(new File(stage,name))) {
                        byte[] buf=new byte[65536];
                        for(int n;(n=zip.read(buf))!=-1;) { check(); total+=n; if(total>128L*1024*1024)throw new IOException("Expanded driver too large"); out.write(buf,0,n); }
                    }
                }
            }
            validate(stage, api); check();
            if(!stage.renameTo(target))throw new IOException("Could not finish driver install");
            return id;
        } finally { if(stage.exists()) { File[] files=stage.listFiles(); if(files!=null)for(File f:files)f.delete(); stage.delete(); } }
    }
    static String validate(File dir) throws Exception { return validate(dir, Build.VERSION.SDK_INT); }
    static String validate(File dir, int api) throws Exception {
        File meta=new File(dir,"meta.json");
        if(!meta.isFile() || meta.length()>65536)throw new IOException("Missing or invalid driver metadata");
        JSONObject m=new JSONObject(new String(Files.readAllBytes(meta.toPath()), java.nio.charset.StandardCharsets.UTF_8)); String lib=m.getString("libraryName");
        if(!lib.matches("[A-Za-z0-9_-]+\\.so") || m.optInt("minApi",Integer.MAX_VALUE)>api)throw new IOException("Driver is not compatible with this Android version");
        try(InputStream in=new FileInputStream(new File(dir,lib))) {
            byte[] h=new byte[20]; new DataInputStream(in).readFully(h);
            if(h.length!=20 || h[0]!=127 || h[1]!='E' || h[2]!='L' || h[3]!='F' || h[4]!=2 || h[5]!=1 || h[18]!=(byte)183 || h[19]!=0)
                throw new IOException("Driver is not an ARM64 library");
        }
        return m.optString("name",dir.getName());
    }
}
