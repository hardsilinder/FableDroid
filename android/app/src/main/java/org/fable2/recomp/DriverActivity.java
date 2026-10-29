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
        String selected = settings.driver();
        String label = selected.isEmpty() ? "System driver" : "Bundled Turnip";
        if (selected.startsWith("download-")) try { label=DriverStore.validate(new File(getFilesDir(),"gpu_drivers/"+selected)); }
            catch(Exception e) { label="Unavailable driver — choose another below"; }
        status=Ui.text(this,"Selected: "+label,14,Ui.GOLD,false); content.addView(status);
        choice("System driver", ""); choice("Bundled Turnip (tested on Thor)",Settings.BUNDLED_DRIVER);
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
                new AlertDialog.Builder(this).setTitle("Download and use a driver")
                    .setItems(releases.stream().map(DriverStore.Release::name).toArray(String[]::new),(d,index) ->
                        work(() -> {
                            String id=DriverStore.install(getApplicationContext(),releases.get(index));
                            DriverStore.check(); settings.setDriver(id);
                            runOnUiThread(() -> { if(!isFinishing() && !isDestroyed())render(); });
                        },"Downloading and verifying driver…"))
                    .setNegativeButton("Cancel",null).show();
                status.setText("Select a package for your GPU. Source: StevenMXZ on GitHub.");
            });
        },"Checking GitHub releases…"));
        content.addView(Ui.text(this,"Source: "+DriverStore.SOURCE+"\nDownloads are checksum-verified and installed automatically. Your bundled driver stays available.",12,Ui.MUTED,false));
        Button back=Ui.secondary(this,"Back"); back.setOnClickListener(v->finish()); content.addView(back);
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
