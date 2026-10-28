package org.fable2.recomp;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.FileChannel;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

/**
 * Reads the Xbox 360 game disc file system (XDVDFS / "XISO") from a disc
 * image: a full XGD2/XGD3 dump straight from the disc, or a trimmed xiso.
 * Only plain Java and a FileChannel, so it also runs on a desktop JVM for
 * testing (see main).
 */
final class XisoReader {
    static final int SECTOR = 2048;
    private static final byte[] MAGIC = "MICROSOFT*XBOX*MEDIA".getBytes(StandardCharsets.US_ASCII);
    // Game partition offsets: trimmed xiso, XGD2 (most 360 games, including
    // Fable II), XGD3, XGD1 (original Xbox).
    private static final long[] PARTITIONS = { 0L, 0xFD90000L, 0x89D80000L, 0x2080000L, 0x18300000L };
    private static final int ATTRIBUTE_DIRECTORY = 0x10;

    static final class Entry {
        final String path; final long offset, size; final boolean directory;
        Entry(String path, long offset, long size, boolean directory) {
            this.path = path; this.offset = offset; this.size = size; this.directory = directory;
        }
    }

    interface Progress { void update(long done, long total, String path); }

    private final FileChannel channel;
    private final long partition;
    private final long rootSector, rootSize;

    XisoReader(FileChannel channel) throws IOException {
        this.channel = channel;
        long size = channel.size();
        for (long partition : PARTITIONS) {
            long header = partition + 32L * SECTOR;
            if (header + SECTOR > size) continue;
            ByteBuffer b = read(header, 28);
            boolean match = true;
            for (int i = 0; i < MAGIC.length && match; ++i) match = b.get(i) == MAGIC[i];
            if (!match) continue;
            this.partition = partition;
            this.rootSector = b.getInt(20) & 0xFFFFFFFFL;
            this.rootSize = b.getInt(24) & 0xFFFFFFFFL;
            return;
        }
        throw new IOException("Not an Xbox 360 disc image (no XDVDFS volume found)");
    }

    private ByteBuffer read(long position, int length) throws IOException {
        ByteBuffer b = ByteBuffer.allocate(length).order(ByteOrder.LITTLE_ENDIAN);
        while (b.hasRemaining()) {
            if (channel.read(b, position + b.position()) < 0) throw new IOException("Disc image is truncated");
        }
        b.flip();
        return b;
    }

    /** Every file and directory, parents before children. */
    List<Entry> list() throws IOException {
        List<Entry> out = new ArrayList<>();
        listDirectory(rootSector, rootSize, "", out, 0);
        return out;
    }

    private void listDirectory(long sector, long size, String prefix, List<Entry> out, int depth) throws IOException {
        if (size == 0) return;
        if (depth > 64 || size > (64 << 20)) throw new IOException("Corrupt disc directory");
        ByteBuffer dir = read(partition + sector * SECTOR, (int) size);
        // The directory is a binary tree of entries addressed in 4-byte units;
        // walk it iteratively so huge directories can't overflow the stack.
        ArrayList<Integer> pending = new ArrayList<>();
        pending.add(0);
        boolean[] seen = new boolean[(int) (size / 4) + 1];
        List<long[]> subdirectories = new ArrayList<>();
        List<String> subdirectoryNames = new ArrayList<>();
        while (!pending.isEmpty()) {
            int offset = pending.remove(pending.size() - 1) * 4;
            if (offset + 14 > size || seen[offset / 4]) continue;
            seen[offset / 4] = true;
            int left = dir.getShort(offset) & 0xFFFF, right = dir.getShort(offset + 2) & 0xFFFF;
            if (left == 0xFFFF && right == 0xFFFF) continue;  // Sector padding.
            long start = dir.getInt(offset + 4) & 0xFFFFFFFFL;
            long length = dir.getInt(offset + 8) & 0xFFFFFFFFL;
            int attributes = dir.get(offset + 12) & 0xFF;
            int nameLength = dir.get(offset + 13) & 0xFF;
            if (offset + 14 + nameLength > size) throw new IOException("Corrupt disc directory entry");
            byte[] nameBytes = new byte[nameLength];
            for (int i = 0; i < nameLength; ++i) nameBytes[i] = dir.get(offset + 14 + i);
            String name = new String(nameBytes, StandardCharsets.ISO_8859_1);
            if (name.isEmpty() || name.equals(".") || name.equals("..") || name.contains("/") || name.contains("\\"))
                throw new IOException("Invalid file name on disc: " + name);
            String path = prefix + name;
            boolean directory = (attributes & ATTRIBUTE_DIRECTORY) != 0;
            out.add(new Entry(path, partition + start * SECTOR, directory ? 0 : length, directory));
            if (directory) { subdirectories.add(new long[] { start, length }); subdirectoryNames.add(path); }
            if (left != 0 && left != 0xFFFF) pending.add(left);
            if (right != 0 && right != 0xFFFF) pending.add(right);
        }
        for (int i = 0; i < subdirectories.size(); ++i) {
            long[] d = subdirectories.get(i);
            listDirectory(d[0], d[1], subdirectoryNames.get(i) + "/", out, depth + 1);
        }
    }

    /** Copies one file to out, reporting bytes as they are written. */
    void extract(Entry entry, File out, byte[] buffer, Runnable perChunk) throws IOException {
        try (OutputStream dest = new FileOutputStream(out)) {
            ByteBuffer b = ByteBuffer.wrap(buffer);
            long done = 0;
            while (done < entry.size) {
                b.clear();
                b.limit((int) Math.min(buffer.length, entry.size - done));
                int n = channel.read(b, entry.offset + done);
                if (n < 0) throw new IOException("Disc image is truncated at " + entry.path);
                dest.write(buffer, 0, n);
                done += n;
                if (perChunk != null) perChunk.run();
            }
        }
    }

    /** Desktop test: java XisoReader.java image.iso [list|extract <dir>]. */
    public static void main(String[] args) throws Exception {
        try (FileChannel c = FileChannel.open(new File(args[0]).toPath())) {
            XisoReader r = new XisoReader(c);
            List<Entry> entries = r.list();
            long total = 0;
            for (Entry e : entries) total += e.size;
            System.out.println("partition 0x" + Long.toHexString(r.partition) + ", " + entries.size()
                + " entries, " + (total >> 20) + " MiB");
            if (args.length > 1 && args[1].equals("list")) for (Entry e : entries)
                System.out.println((e.directory ? "d " : "f ") + e.size + " " + e.path);
            if (args.length > 2 && args[1].equals("extract")) {
                byte[] buffer = new byte[1 << 20];
                for (Entry e : entries) {
                    if (e.directory) continue;
                    if (args.length > 3 && !e.path.equals(args[3])) continue;
                    File out = new File(args[2], e.path);
                    out.getParentFile().mkdirs();
                    r.extract(e, out, buffer, null);
                }
            }
        }
    }
}
