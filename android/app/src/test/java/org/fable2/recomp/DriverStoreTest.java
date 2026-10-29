package org.fable2.recomp;
import org.junit.Test;
import org.junit.Rule;
import org.junit.rules.TemporaryFolder;
import static org.junit.Assert.*;
import java.io.*;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.zip.*;

public class DriverStoreTest {
    @Rule public TemporaryFolder temp=new TemporaryFolder();
    byte[] archive(String lib, int api, boolean arm) throws Exception {
        ByteArrayOutputStream b=new ByteArrayOutputStream();
        try(ZipOutputStream z=new ZipOutputStream(b)) {
            z.putNextEntry(new ZipEntry("meta.json"));
            z.write(("{\"name\":\"Test\",\"libraryName\":\""+lib+"\",\"minApi\":"+api+"}").getBytes()); z.closeEntry();
            z.putNextEntry(new ZipEntry(lib)); byte[] h=new byte[64]; h[0]=127; h[1]='E';h[2]='L';h[3]='F';h[4]=2;h[5]=1;h[18]=(byte)(arm?183:62);z.write(h);z.closeEntry();
        } return b.toByteArray();
    }
    DriverStore.Release release(byte[] b)throws Exception {return new DriverStore.Release("test","",DriverStore.hex(MessageDigest.getInstance("SHA-256").digest(b)));}
    @Test public void installsAndReusesVerifiedDriver()throws Exception {
        File root=temp.newFolder();byte[] b=archive("driver.so",28,true);String id=DriverStore.installArchive(root,release(b),b,31);
        assertEquals("Test",DriverStore.validate(new File(root,id),31));
        assertEquals(id,DriverStore.installArchive(root,release(b),b,31));assertEquals(1,root.list().length);
    }
    @Test public void rejectsTraversalAndCleansStage()throws Exception {reject(archive("../escape.so",28,true));}
    @Test public void rejectsWrongArchitecture()throws Exception {reject(archive("driver.so",28,false));}
    @Test public void rejectsNewerAndroidRequirement()throws Exception {reject(archive("driver.so",99,true));}
    @Test public void rejectsChecksumAndPreservesExistingFiles()throws Exception {
        File root=temp.newFolder();File old=new File(root,"working");Files.write(old.toPath(),new byte[]{1,2});
        byte[] b=archive("driver.so",28,true);DriverStore.Release r=release(b);b[0]^=1;
        assertThrows(IOException.class,()->DriverStore.installArchive(root,r,b,31));assertArrayEquals(new byte[]{1,2},Files.readAllBytes(old.toPath()));assertEquals(1,root.list().length);
    }
    @Test public void publisherArchiveIntegration()throws Exception {
        String fixture=System.getenv("FABLE_DRIVER_FIXTURE");
        org.junit.Assume.assumeTrue(fixture!=null);
        byte[] b=Files.readAllBytes(Path.of(fixture));
        DriverStore.Release r=new DriverStore.Release("Turnip_v27.0.0-R1.zip", "", "07938ef843a73dd47ebb88ba9c2d3bca011667bd2b657e3b4a75d9ba8ed591a0");
        File root=temp.newFolder();String id=DriverStore.installArchive(root,r,b,31);
        assertFalse(DriverStore.validate(new File(root,id),31).isEmpty());
    }
    void reject(byte[] b)throws Exception {File root=temp.newFolder();assertThrows(Exception.class,()->DriverStore.installArchive(root,release(b),b,31));assertEquals(0,root.list().length);}
}
