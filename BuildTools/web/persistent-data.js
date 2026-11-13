// Hydrate browser files before main reads settings or creates writable caches.
// Keep this in the linked runtime so custom HTML shells get the same ordering.
(function () {
    // Emscripten captures this array before preRun. Mutate the same array below.
    if (typeof window !== 'undefined' && !Module['arguments']) {
        Module['arguments'] = [];
    }

    var previous = Module['preRun'];

    if (typeof previous === 'function') {
        previous = [previous];
    }

    Module['preRun'] = previous || [];
    Module['preRun'].push(function () {
        // Node-based wasm tests keep their existing in-memory filesystem.
        if (typeof window === 'undefined' || Module['persistentDataInitialized']) {
            return;
        }

        var args = Module['arguments'] || [];

        if (!args.some(function (arg) {
            // Match FindWritablePathArg's ASCII whitespace trimming without changing the original token.
            var option = arg.replace(/^[ \n\r\t]+|[ \n\r\t]+$/g, '');
            return option === '--Common.UserWritablePath' || option === '-Common.UserWritablePath';
        })) {
            args.push('--Common.UserWritablePath', '/PersistentData');
        }

        Module['arguments'] = args;
        FS.mkdirTree('/PersistentData');
        FS.mount(IDBFS, { autoPersist: true }, '/PersistentData');
        Module['persistentDataInitialized'] = true;
        Module['syncfsDone'] = 0;

        var dependency = 'fo-persistent-data';
        addRunDependency(dependency);
        FS.syncfs(true, function (error) {
            if (error) {
                abort('Unable to load persistent client data: ' + error);
                return;
            }

            Module['syncfsDone'] = 1;
            removeRunDependency(dependency);
        });
    });
})();
