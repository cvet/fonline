package $PACKAGE$;

import java.io.File;
import java.util.ArrayList;

import org.libsdl.app.SDLActivity;

public class FOnlineActivity extends SDLActivity {

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
        final String resourcesDir = getApplicationInfo().sourceDir + "!/assets/" + $RESOURCE_DIRECTORY$;
        final File cacheDir = new File(runtimeRoot, "Cache");
        if (!cacheDir.isDirectory() && !cacheDir.mkdirs()) {
            throw new IllegalStateException("Unable to create cache directory " + cacheDir);
        }

        args.add("--ApplySubConfig");
        args.add("$CONFIG$");
        // Android is handed its writable directory by the platform rather than choosing one from an
        // INSTALLED marker, so it arrives through the option every other platform uses
        args.add("--UserWritablePath");
        args.add(runtimeRoot.getAbsolutePath());
        args.add("--Baking.ClientResources");
        args.add(resourcesDir);
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

    private File getRuntimeRoot() {
        final File runtimeRoot = getFilesDir();
        if (runtimeRoot == null) {
            throw new IllegalStateException("Android files directory is unavailable");
        }
        return runtimeRoot;
    }
}
