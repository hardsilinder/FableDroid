package org.fable2.recomp;

import android.app.Activity;
import android.app.AlertDialog;
import android.os.Bundle;
import android.widget.*;
import java.io.File;
import java.util.List;

public final class DriverActivity extends Activity {
    private LinearLayout content;
    private TextView status;
    private Thread worker;
    private Settings settings;
    @Override public void onCreate(Bundle state) { super.onCreate(state); settings=new Settings(this); render(); }
    private void render() {
        ScrollView scroll=new ScrollView(this); content=new LinearLayout(this); content.setOrientation(1);
        int pad=Ui.dp(this,24); content.setPadding(pad,pad,pad,pad); content.setBackgroundColor(Ui.BACKGROUND); scroll.addView(content); setContentView(scroll);
        content.addView(Ui.title(this,"GPU drivers"));
        content.addView(Ui.text(this,"Choose a driver for the next game launch. Turnip is for supported Adreno GPUs; use System on other GPUs. Gen8 packages target Adreno 8xx. A newer driver may not run faster.",14,Ui.MUTED,false));
        String renderer = GpuDetector.renderer();
        content.addView(Ui.text(this, "Detected GPU: " + (renderer.isEmpty() ? "Unknown (System recommended)" : renderer), 14, Ui.MUTED, false));
        String selected = settings.driver();
        String label = selected.isEmpty() ? "System driver" : "Bundled Turnip";
        if (selected.startsWith("download-")) try { label=DriverStore.validate(new File(getFilesDir(),"gpu_drivers/"+selected)); }
            catch(Exception e) { label="Unavailable driver — choose another below"; }
        status=Ui.text(this,"Selected: "+label,14,Ui.GOLD,false); content.addView(status);
        content.addView(Ui.title(this,"Recommended for " + DriverPolicy.family(renderer)));
        content.addView(Ui.text(this,DriverPolicy.recommendation(renderer),14,Ui.MUTED,false));
        content.addView(Ui.text(this,"Recommendations are based on GPU family and available testing, not a benchmark of your device.",12,Ui.MUTED,false));
        String recommended = DriverPolicy.defaultDriver(renderer);
        choice(recommended.isEmpty() ? "Use System (recommended)" : "Use bundled Turnip (recommended)", recommended);
        content.addView(Ui.title(this,"Manual selection"));
        choice("System driver", ""); choice("Bundled Turnip (Adreno 7xx; tested on Thor)",Settings.BUNDLED_DRIVER);
        File[] installed=new File(getFilesDir(),"gpu_drivers").listFiles();
        if(installed!=null)for(File dir:installed)if(dir.getName().matches("download-[a-f0-9]{64}")) {
            try { choice(DriverStore.validate(dir),dir.getName()); } catch(Exception ignored) {}
        }
        Button download=Ui.primary(this,"Check for driver downloads"); content.addView(download);
        download.setOnClickListener(v -> work(() -> {
            List<DriverStore.Release> releases=DriverStore.releases();
            runOnUiThread(() -> {
                if(isFinishing() || isDestroyed())return;
                if(releases.isEmpty()){status.setText("No verified Turnip downloads found.");return;}
                showDownloads(releases, renderer, true);
                status.setText("Downloads loaded. Matching packages are experimental until tested with Fable II on your device.");
            });
        },"Checking GitHub releases…"));
        content.addView(Ui.text(this,"Source: "+DriverStore.SOURCE+"\nDownloads are checksum-verified and installed automatically. Your bundled driver stays available.",12,Ui.MUTED,false));
        Button back=Ui.secondary(this,"Back"); back.setOnClickListener(v->finish()); content.addView(back);
    }
    private void showDownloads(List<DriverStore.Release> releases, String renderer, boolean matchingOnly) {
        List<DriverStore.Release> shown = new java.util.ArrayList<>();
        for (DriverStore.Release release : releases)
            if (!matchingOnly || DriverPolicy.matchingDownload(renderer, release.name())) shown.add(release);
        AlertDialog.Builder dialog = new AlertDialog.Builder(this)
            .setTitle(matchingOnly ? "Recommended downloads · experimental" : "All Turnip downloads · manual selection")
            .setNegativeButton("Cancel", null);
        if (shown.isEmpty()) dialog.setMessage("No matching downloads for " + DriverPolicy.family(renderer)
            + ". Use System. Turnip packages are for supported Adreno GPUs.");
        else dialog.setItems(shown.stream().map(r -> r.name() +
            (matchingOnly ? " (GPU family match; experimental)" : " (compatibility unverified)")).toArray(String[]::new),
            (d, index) -> {
                DriverStore.Release release = shown.get(index);
                new AlertDialog.Builder(this).setTitle("Download and use this driver?")
                    .setMessage(release.name() + "\n\n" + (DriverPolicy.matchingDownload(renderer, release.name())
                        ? "Matches your GPU family, but has not been verified with Fable II on your device."
                        : "This package is not a verified match for your GPU. It may fail to launch or render correctly.")
                        + " Your current selection changes only after installation succeeds.")
                    .setNegativeButton("Cancel", null)
                    .setPositiveButton("Download and use", (confirm, which) -> work(() -> {
                        String id = DriverStore.install(getApplicationContext(), release);
                        DriverStore.check(); settings.setDriver(id);
                        runOnUiThread(() -> { if (!isFinishing() && !isDestroyed()) render(); });
                    }, "Downloading and verifying driver…")).show();
            });
        if (matchingOnly) dialog.setNeutralButton("All packages", (d, which) -> showDownloads(releases, renderer, false));
        dialog.show();
    }
    private void choice(String label,String id) {
        Button b=Ui.secondary(this,label+(settings.driver().equals(id)?" ✓":"")); content.addView(b);
        b.setOnClickListener(v->{if(worker!=null && worker.isAlive())return; settings.setDriver(id);render();});
    }
    interface Job { void run() throws Exception; }
    private void work(Job job,String message) {
        if(worker!=null && worker.isAlive())return;
        status.setText(message);
        worker=new Thread(()->{try { job.run(); } catch(Exception e) {runOnUiThread(()->{if(!isFinishing() && !isDestroyed())status.setText("Could not install/check driver: "+e.getMessage()+". Current driver kept.");});}},"DriverDownload"); worker.start();
    }
    @Override protected void onDestroy(){if(worker!=null)worker.interrupt();super.onDestroy();}
}
