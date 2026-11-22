// The engine embeds Mono from C++ rather than through dotnet's JavaScript host, so it supplies the
// browser imports the runtime expects itself - see Docs/WebDebugging.md, "Managed Runtime On Wasm"

addToLibrary({
    // Mono asks for one background pass and coalesces further requests until it runs
    schedule_background_exec__deps: ['mono_background_exec'],
    schedule_background_exec: function () {
        if (Module._foBackgroundExecScheduled) {
            return;
        }

        Module._foBackgroundExecScheduled = true;
        setTimeout(function () {
            Module._foBackgroundExecScheduled = false;
            _mono_background_exec();
        }, 0);
    },

    // Only the nearest due time matters, so a pending timer is replaced rather than added to
    mono_wasm_schedule_timer__deps: ['mono_wasm_execute_timer'],
    mono_wasm_schedule_timer: function (shortestDueTimeMs) {
        if (Module._foTimerId !== undefined) {
            clearTimeout(Module._foTimerId);
        }

        Module._foTimerId = setTimeout(function () {
            Module._foTimerId = undefined;
            _mono_wasm_execute_timer();
        }, shortestDueTimeMs);
    },

    // getRandomValues rejects requests over 65536 bytes, so the buffer is filled in batches
    mono_wasm_browser_entropy: function (bufferPtr, bufferLength) {
        if (!globalThis.crypto || !globalThis.crypto.getRandomValues) {
            return -1;
        }

        var target = HEAPU8.subarray(bufferPtr, bufferPtr + bufferLength);

        for (var offset = 0; offset < bufferLength; offset += 65536) {
            globalThis.crypto.getRandomValues(target.subarray(offset, offset + Math.min(bufferLength - offset, 65536)));
        }

        return 0;
    },

    // A browser tab has no process identity; the runtime only needs a stable non-zero value
    mono_wasm_process_current_pid: function () {
        return 42;
    },

    // The jiterpreter is dotnet's JavaScript JIT for the interpreter, written in TypeScript against its own
    // host. Declining it is a path the interpreter supports - see Docs/WebDebugging.md, "Managed Runtime On Wasm"
    mono_interp_tier_prepare_jiterpreter: function () {
        return 1;
    },

    // Requests to compile a trampoline: granting none leaves every call on the interpreter's own slow path
    mono_interp_jit_wasm_jit_call_trampoline: function () {
    },

    mono_interp_flush_jitcall_queue: function () {
    },

    mono_interp_record_interp_entry: function () {
    },

    mono_interp_jit_wasm_entry_trampoline: function () {
    },

    // Releases per-method data the declined jiterpreter never allocated
    mono_wasm_free_method_data: function () {
    },

    // Only ever reached through a trampoline this build never compiles
    mono_interp_invoke_wasm_jit_call_trampoline: function () {
        abort('jiterpreter trampoline invoked although none is ever compiled');
    },

    // A WebAssembly indirect call must match the target's signature exactly, so the interpreter cannot call
    // a native function by address - it asks here for a wrapper matching one signature cookie at a time
    mono_wasm_interp_to_native_callback__deps: [
        '$UTF8ToString', '$addFunction', '$wasmTable',
        'mono_wasm_interp_method_args_get_iarg', 'mono_wasm_interp_method_args_get_larg',
        'mono_wasm_interp_method_args_get_farg', 'mono_wasm_interp_method_args_get_darg',
        'mono_wasm_interp_method_args_get_retval'],
    mono_wasm_interp_to_native_callback: function (cookiePtr) {
        var cookie = UTF8ToString(cookiePtr);
        var cache = Module._foInterpToNative;

        if (cache === undefined) {
            cache = Module._foInterpToNative = {};
        }

        if (cache[cookie] !== undefined) {
            return cache[cookie];
        }

        // An integer argument advances one slot and a 64-bit one two, while floats and doubles are counted
        // in a second sequence of their own - the interpreter stores them in a separate array
        var plan = [];
        var intSlot = 0;
        var floatSlot = 0;

        for (var i = 1; i < cookie.length; i++) {
            var kind = cookie[i];

            if (kind === 'I') {
                plan.push([kind, intSlot]);
                intSlot += 1;
            } else if (kind === 'L') {
                plan.push([kind, intSlot]);
                intSlot += 2;
            } else if (kind === 'F' || kind === 'D') {
                plan.push([kind, floatSlot]);
                floatSlot += 1;
            } else {
                // Returning nothing makes Mono report the cookie it cannot handle and stop, which is the
                // honest outcome: a wrapper built on a guess would corrupt the call frame silently
                return 0;
            }
        }

        var returnKind = cookie[0];

        // Emscripten's getWasmTableEntry is an internal symbol a user library may not depend on, so the
        // table is read here directly and mirrored the same way, since every managed call comes through
        var entries = [];

        var wrapper = function (targetFunc, margs) {
            var args = new Array(plan.length);

            for (var i = 0; i < plan.length; i++) {
                var kind = plan[i][0];
                var slot = plan[i][1];

                if (kind === 'I') {
                    args[i] = _mono_wasm_interp_method_args_get_iarg(margs, slot);
                } else if (kind === 'L') {
                    args[i] = _mono_wasm_interp_method_args_get_larg(margs, slot);
                } else if (kind === 'F') {
                    args[i] = _mono_wasm_interp_method_args_get_farg(margs, slot);
                } else {
                    args[i] = _mono_wasm_interp_method_args_get_darg(margs, slot);
                }
            }

            var entry = entries[targetFunc];

            if (entry === undefined) {
                entry = entries[targetFunc] = wasmTable.get(targetFunc);
            }

            var result = entry.apply(null, args);

            if (returnKind === 'V') {
                return;
            }

            var retval = _mono_wasm_interp_method_args_get_retval(margs);

            if (returnKind === 'I') {
                HEAP32[retval >> 2] = result;
            } else if (returnKind === 'L') {
                HEAP64[retval >> 3] = result;
            } else if (returnKind === 'F') {
                HEAPF32[retval >> 2] = result;
            } else {
                HEAPF64[retval >> 3] = result;
            }
        };

        cache[cookie] = addFunction(wrapper, 'vii');
        return cache[cookie];
    },
});
