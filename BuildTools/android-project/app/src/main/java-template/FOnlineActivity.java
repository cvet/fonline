package $PACKAGE$;

import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.ByteArrayOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;

import org.libsdl.app.SDLActivity;

public class FOnlineActivity extends SDLActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        prepareRuntimeResources();
        super.onCreate(savedInstanceState);
    }

    @Override
    protected String[] getLibraries() {
        return new String[]{
            "main"
        };
    }

    @Override
    protected String[] getArguments() {
        final ArrayList<String> args = new ArrayList<>();
        final File runtimeRoot = getRuntimeRoot();
        final File resourcesDir = new File(runtimeRoot, "Resources");
        final File cacheDir = new File(runtimeRoot, "Cache");
        if (!cacheDir.isDirectory() && !cacheDir.mkdirs()) {
            throw new IllegalStateException("Unable to create cache directory " + cacheDir);
        }

        args.add("--ApplySubConfig");
        args.add("$CONFIG$");
        args.add("--Baking.ClientResources");
        args.add(resourcesDir.getAbsolutePath());
        args.add("--Baking.CacheResources");
        args.add(cacheDir.getAbsolutePath());

        final String serverHost = getIntent() != null ? getIntent().getStringExtra("server_host") : null;
        if (serverHost != null) {
            final String trimmedServerHost = serverHost.trim();
            if (!trimmedServerHost.isEmpty()) {
                args.add("--ClientNetwork.ServerHost");
                args.add(trimmedServerHost);
            }
        }

        return args.toArray(new String[0]);
    }

    private void prepareRuntimeResources() {
        final File runtimeRoot = getRuntimeRoot();

        final File resourcesDir = new File(runtimeRoot, "Resources");
        final File revisionFile = new File(runtimeRoot, ".asset_revision");
        final String assetRevision = getAssetRevision();

        if (!assetRevision.equals(readSmallTextFile(revisionFile)) || !new File(resourcesDir, "Metadata.zip").isFile()) {
            deleteRecursively(resourcesDir);
            copyAssetTree("Resources", resourcesDir);
            writeSmallTextFile(revisionFile, assetRevision);
        }
    }

    private File getRuntimeRoot() {
        final File runtimeRoot = getFilesDir();
        if (runtimeRoot == null) {
            throw new IllegalStateException("Android files directory is unavailable");
        }
        return runtimeRoot;
    }

    private String getAssetRevision() {
        try {
            final PackageInfo packageInfo = getPackageManager().getPackageInfo(getPackageName(), 0);
            return Long.toString(packageInfo.lastUpdateTime);
        }
        catch (PackageManager.NameNotFoundException ex) {
            throw new IllegalStateException("Unable to resolve package metadata", ex);
        }
    }

    private String readSmallTextFile(File file) {
        if (!file.isFile()) {
            return "";
        }

        try (InputStream input = new FileInputStream(file)) {
            final ByteArrayOutputStream output = new ByteArrayOutputStream();
            final byte[] buffer = new byte[256];
            while (true) {
                final int read = input.read(buffer);
                if (read < 0) {
                    break;
                }
                output.write(buffer, 0, read);
            }
            return new String(output.toByteArray(), StandardCharsets.UTF_8).trim();
        }
        catch (IOException ex) {
            return "";
        }
    }

    private void writeSmallTextFile(File file, String text) {
        final File parent = file.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
            throw new IllegalStateException("Unable to create directory " + parent);
        }

        try (OutputStream output = new FileOutputStream(file, false)) {
            output.write(text.getBytes(StandardCharsets.UTF_8));
        }
        catch (IOException ex) {
            throw new IllegalStateException("Unable to write revision file " + file, ex);
        }
    }

    private void copyAssetTree(String assetPath, File targetPath) {
        try {
            final String[] children = getAssets().list(assetPath);
            if (children == null || children.length == 0) {
                copyAssetFile(assetPath, targetPath);
                return;
            }
        }
        catch (IOException ex) {
            throw new IllegalStateException("Unable to enumerate assets at " + assetPath, ex);
        }

        if (!targetPath.isDirectory() && !targetPath.mkdirs()) {
            throw new IllegalStateException("Unable to create directory " + targetPath);
        }

        try {
            final String[] children = getAssets().list(assetPath);
            if (children == null) {
                return;
            }

            for (String child : children) {
                final String childAssetPath = assetPath + "/" + child;
                final File childTargetPath = new File(targetPath, child);
                copyAssetTree(childAssetPath, childTargetPath);
            }
        }
        catch (IOException ex) {
            throw new IllegalStateException("Unable to copy assets from " + assetPath, ex);
        }
    }

    private void copyAssetFile(String assetPath, File targetPath) {
        final File parent = targetPath.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
            throw new IllegalStateException("Unable to create directory " + parent);
        }

        try (InputStream input = getAssets().open(assetPath); OutputStream output = new FileOutputStream(targetPath, false)) {
            final byte[] buffer = new byte[64 * 1024];
            while (true) {
                final int read = input.read(buffer);
                if (read < 0) {
                    break;
                }
                output.write(buffer, 0, read);
            }
        }
        catch (IOException ex) {
            throw new IllegalStateException("Unable to copy asset " + assetPath + " to " + targetPath, ex);
        }
    }

    private void deleteRecursively(File path) {
        if (!path.exists()) {
            return;
        }

        final File[] children = path.listFiles();
        if (children != null) {
            for (File child : children) {
                deleteRecursively(child);
            }
        }

        if (!path.delete()) {
            throw new IllegalStateException("Unable to delete " + path);
        }
    }
}