const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const HOST_SOURCE = fs.readFileSync(path.resolve(__dirname, '..', 'web', 'wasm-host.js'), 'utf8');

const KIND_NONE = 0;
const KIND_I32 = 1;
const KIND_I64 = 2;
const KIND_F32 = 3;
const KIND_F64 = 4;
const ABI_SCALAR = 0;
const ABI_UTF8_STRING_POINTER = 1;
const ABI_UTF8_STRING_LENGTH = 2;
const ABI_UTF8_STRING_OUTPUT_POINTER = 3;
const ABI_UTF8_STRING_OUTPUT_LENGTH = 4;
const ABI_MUTABLE_VALUE_POINTER = 5;
const ABI_MUTABLE_VALUE_LENGTH = 6;
const ABI_ARRAY_POINTER = 7;
const ABI_ARRAY_BYTE_LENGTH = 8;
const ABI_ARRAY_OUTPUT_POINTER = 9;
const ABI_ARRAY_OUTPUT_BYTE_LENGTH = 10;
const ABI_MUTABLE_ARRAY_POINTER = 11;
const ABI_MUTABLE_ARRAY_BYTE_LENGTH = 12;
const ABI_MUTABLE_ARRAY_CAPACITY_BYTE_LENGTH = 13;
const ABI_MUTABLE_ARRAY_REQUIRED_BYTE_LENGTH_POINTER = 14;
const ABI_DICT_POINTER = 15;
const ABI_DICT_BYTE_LENGTH = 16;
const ABI_DICT_OUTPUT_POINTER = 17;
const ABI_DICT_OUTPUT_BYTE_LENGTH = 18;
const ABI_MUTABLE_DICT_POINTER = 19;
const ABI_MUTABLE_DICT_BYTE_LENGTH = 20;
const ABI_MUTABLE_DICT_CAPACITY_BYTE_LENGTH = 21;
const ABI_MUTABLE_DICT_REQUIRED_BYTE_LENGTH_POINTER = 22;
const ABI_CALLBACK_POINTER = 23;
const ABI_CALLBACK_LENGTH = 24;
const ABI_MUTABLE_UTF8_STRING_POINTER = 25;
const ABI_MUTABLE_UTF8_STRING_BYTE_LENGTH = 26;
const ABI_MUTABLE_UTF8_STRING_CAPACITY_BYTE_LENGTH = 27;
const ABI_MUTABLE_UTF8_STRING_REQUIRED_BYTE_LENGTH_POINTER = 28;
const ABI_VALUE_POINTER = 29;
const ABI_VALUE_BYTE_LENGTH = 30;
const ABI_VALUE_OUTPUT_POINTER = 31;
const ABI_VALUE_OUTPUT_BYTE_LENGTH = 32;

function makeHostContext(options = {}) {
  const heapBuffer = new ArrayBuffer(4096);
  const logs = [];
  const errors = [];
  const dependencies = new Set();
  let heapOffset = 512;

  const context = {
    ArrayBuffer,
    BigInt,
    BigInt64Array,
    DataView,
    Error,
    Int32Array,
    Math,
    Number,
    Object,
    Promise,
    Set,
    String,
    TextDecoder,
    TextEncoder,
    Uint8Array,
    WebAssembly: options.webAssembly || WebAssembly,
    console: {
      log: (...args) => logs.push(args),
      error: (...args) => errors.push(args)
    },
    Module: {
      preRun: [],
      calledRun: Boolean(options.calledRun),
      foRuntimeInitialized: Boolean(options.foRuntimeInitialized),
      addRunDependency: (name) => dependencies.add(name),
      removeRunDependency: (name) => dependencies.delete(name),
      printErr: (text) => errors.push([String(text)])
    },
    HEAPU8: new Uint8Array(heapBuffer),
    HEAP32: new Int32Array(heapBuffer),
    HEAP64: new BigInt64Array(heapBuffer),
    HEAPF32: new Float32Array(heapBuffer),
    HEAPF64: new Float64Array(heapBuffer),
    UTF8ToString: (ptr) => {
      let end = ptr;
      const heap = context.HEAPU8;

      while (end < heap.length && heap[end] !== 0) {
        end += 1;
      }

      return new TextDecoder('utf-8').decode(heap.subarray(ptr, end));
    },
    _malloc: (size) => {
      const alignedSize = (size + 7) & ~7;
      const ptr = heapOffset;
      heapOffset += alignedSize;
      return ptr;
    },
    _free: () => {},
    dynCall: options.dynCall || (() => {
      throw new Error('Unexpected dynCall');
    }),
    fetch: options.fetch || (() => Promise.reject(new Error('Unexpected fetch')))
  };

  context.globalThis = context;
  vm.createContext(context);
  vm.runInContext(HOST_SOURCE, context, { filename: 'wasm-host.js' });

  context.__logs = logs;
  context.__errors = errors;
  context.__dependencies = dependencies;
  return context;
}

function writeCString(context, ptr, text) {
  const bytes = new TextEncoder().encode(text);
  context.HEAPU8.set(bytes, ptr);
  context.HEAPU8[ptr + bytes.length] = 0;
}

function writeContextValues(context, ptr, values) {
  const base = ptr >> 3;

  values.forEach((value, index) => {
    context.HEAP64[base + index] = BigInt(value);
  });
}

function testApiImportTrampolinePacksScalars() {
  let context;
  const calls = [];

  context = makeHostContext({
    dynCall: (signature, callbackPtr, args) => {
      calls.push({ signature, callbackPtr, args });

      const argValuesPtr = args[3];
      const resultValuePtr = args[4];

      assert.strictEqual(signature, 'iiiiii');
      assert.strictEqual(callbackPtr, 1234);
      assert.strictEqual(args[0], 77);
      assert.strictEqual(args[1], 5);
      assert.strictEqual(args[2], 4);
      assert.strictEqual(context.HEAP32[argValuesPtr >> 2], 7);
      assert.strictEqual(context.HEAP64[(argValuesPtr >> 3) + 1], 9n);
      assert.strictEqual(Math.fround(context.HEAPF32[(argValuesPtr >> 2) + 4]), Math.fround(1.5));
      assert.strictEqual(context.HEAPF64[(argValuesPtr >> 3) + 3], 2.5);

      context.HEAP32[resultValuePtr >> 2] = 42;
      return 1;
    }
  });

  const host = context.FOnlineWasmHost;
  context.HEAP32[64 >> 2] = KIND_I32;
  context.HEAP32[(64 >> 2) + 1] = KIND_I64;
  context.HEAP32[(64 >> 2) + 2] = KIND_F32;
  context.HEAP32[(64 >> 2) + 3] = KIND_F64;

  host.registerApiImport('Game_Test__int32_int64_float32_float64__int32', 4, 64, 0, KIND_I32, 77, 5, 1234);

  const result = host.imports['fonline.api'].Game_Test__int32_int64_float32_float64__int32.func(7, 9n, 1.5, 2.5);

  assert.strictEqual(result, 42);
  assert.strictEqual(calls.length, 1);
}

function testApiImportTrampolineHandlesNoArgGetter() {
  let context;

  context = makeHostContext({
    dynCall: (signature, callbackPtr, args) => {
      const argValuesPtr = args[3];
      const resultValuePtr = args[4];

      assert.strictEqual(signature, 'iiiiii');
      assert.strictEqual(callbackPtr, 5678);
      assert.strictEqual(args[0], 91);
      assert.strictEqual(args[1], 12);
      assert.strictEqual(args[2], 0);
      assert.strictEqual(argValuesPtr, 0);

      context.HEAP64[resultValuePtr >> 3] = 123n;
      return 1;
    }
  });

  const host = context.FOnlineWasmHost;
  host.registerApiImport('Game_get_Ticks__int64', 0, 0, 0, KIND_I64, 91, 12, 5678);

  const result = host.imports['fonline.api'].Game_get_Ticks__int64.func();

  assert.strictEqual(result, 123n);
}

function testApiImportTrampolineCopiesUtf8StringArguments() {
  let context;
  const memory = new WebAssembly.Memory({ initial: 1 });
  const input = new TextEncoder().encode('hello api');

  new Uint8Array(memory.buffer).set(input, 96);

  context = makeHostContext({
    dynCall: (signature, callbackPtr, args) => {
      const argValuesPtr = args[3];
      const rawBase = argValuesPtr >> 2;
      const textPtr = context.HEAP32[rawBase];
      const textLen = context.HEAP32[rawBase + 2];
      const scalar = context.HEAP32[rawBase + 4];
      const copied = context.HEAPU8.slice(textPtr, textPtr + textLen);

      assert.strictEqual(signature, 'iiiiii');
      assert.strictEqual(callbackPtr, 9101);
      assert.strictEqual(args[0], 22);
      assert.strictEqual(args[1], 3);
      assert.strictEqual(args[2], 3);
      assert.strictEqual(textLen, input.length);
      assert.deepStrictEqual(Array.from(copied), Array.from(input));
      assert.strictEqual(scalar, 99);
      return 1;
    }
  });

  const host = context.FOnlineWasmHost;
  context.HEAP32[64 >> 2] = KIND_I32;
  context.HEAP32[(64 >> 2) + 1] = KIND_I32;
  context.HEAP32[(64 >> 2) + 2] = KIND_I32;
  context.HEAP32[80 >> 2] = ABI_UTF8_STRING_POINTER;
  context.HEAP32[(80 >> 2) + 1] = ABI_UTF8_STRING_LENGTH;
  context.HEAP32[(80 >> 2) + 2] = ABI_SCALAR;
  host.currentContext = {
    module: {
      instance: {
        exports: {
          memory
        }
      }
    }
  };

  host.registerApiImport('Game_TextAndValue__string_int32__void', 3, 64, 80, KIND_NONE, 22, 3, 9101);
  host.imports['fonline.api'].Game_TextAndValue__string_int32__void.func(96, input.length, 99);
  host.currentContext = null;
}

function testApiImportTrampolineCopiesCallbackArguments() {
  let context;
  const memory = new WebAssembly.Memory({ initial: 1 });
  const input = new TextEncoder().encode('Callbacks::AddOne');

  new Uint8Array(memory.buffer).set(input, 112);

  context = makeHostContext({
    dynCall: (signature, callbackPtr, args) => {
      const argValuesPtr = args[3];
      const rawBase = argValuesPtr >> 2;
      const callbackNamePtr = context.HEAP32[rawBase];
      const callbackNameLen = context.HEAP32[rawBase + 2];
      const value = context.HEAP32[rawBase + 4];
      const copied = context.HEAPU8.slice(callbackNamePtr, callbackNamePtr + callbackNameLen);

      assert.strictEqual(signature, 'iiiiii');
      assert.strictEqual(callbackPtr, 9102);
      assert.strictEqual(args[0], 23);
      assert.strictEqual(args[1], 13);
      assert.strictEqual(args[2], 3);
      assert.strictEqual(callbackNameLen, input.length);
      assert.deepStrictEqual(Array.from(copied), Array.from(input));
      assert.strictEqual(value, 41);
      return 1;
    }
  });

  const host = context.FOnlineWasmHost;
  context.HEAP32[64 >> 2] = KIND_I32;
  context.HEAP32[(64 >> 2) + 1] = KIND_I32;
  context.HEAP32[(64 >> 2) + 2] = KIND_I32;
  context.HEAP32[80 >> 2] = ABI_CALLBACK_POINTER;
  context.HEAP32[(80 >> 2) + 1] = ABI_CALLBACK_LENGTH;
  context.HEAP32[(80 >> 2) + 2] = ABI_SCALAR;
  host.currentContext = {
    module: {
      instance: {
        exports: {
          memory
        }
      }
    }
  };

  host.registerApiImport('Game_InvokeCallback__callback_int32__int32', 3, 64, 80, KIND_I32, 23, 13, 9102);
  host.imports['fonline.api'].Game_InvokeCallback__callback_int32__int32.func(112, input.length, 41);
  host.currentContext = null;
}

function testApiImportTrampolineCopiesUtf8StringOutputs() {
  let context;
  const memory = new WebAssembly.Memory({ initial: 1 });
  const output = new TextEncoder().encode('hello output');

  context = makeHostContext({
    dynCall: (signature, callbackPtr, args) => {
      const argValuesPtr = args[3];
      const resultValuePtr = args[4];
      const tempPtr = context.HEAP32[argValuesPtr >> 2];
      const tempLen = context.HEAP32[(argValuesPtr >> 2) + 2];

      assert.strictEqual(signature, 'iiiiii');
      assert.strictEqual(callbackPtr, 9911);
      assert.strictEqual(args[0], 33);
      assert.strictEqual(args[1], 4);
      assert.strictEqual(args[2], 2);
      assert.strictEqual(tempLen, 32);

      context.HEAPU8.set(output, tempPtr);
      context.HEAP32[resultValuePtr >> 2] = output.length;
      return 1;
    }
  });

  const host = context.FOnlineWasmHost;
  context.HEAP32[64 >> 2] = KIND_I32;
  context.HEAP32[(64 >> 2) + 1] = KIND_I32;
  context.HEAP32[80 >> 2] = ABI_UTF8_STRING_OUTPUT_POINTER;
  context.HEAP32[(80 >> 2) + 1] = ABI_UTF8_STRING_OUTPUT_LENGTH;
  host.currentContext = {
    module: {
      instance: {
        exports: {
          memory
        }
      }
    }
  };

  host.registerApiImport('Game_GetText__void__string', 2, 64, 80, KIND_I32, 33, 4, 9911);

  const result = host.imports['fonline.api'].Game_GetText__void__string.func(128, 32);
  const copied = new Uint8Array(memory.buffer).slice(128, 128 + output.length);

  assert.strictEqual(result, output.length);
  assert.deepStrictEqual(Array.from(copied), Array.from(output));
  host.currentContext = null;
}

function testApiImportTrampolineCopiesMutableValueArgumentsBack() {
  let context;
  const memory = new WebAssembly.Memory({ initial: 1 });
  const moduleHeap = new Uint8Array(memory.buffer);
  const moduleView = new DataView(memory.buffer);

  moduleView.setInt32(144, 21, true);

  context = makeHostContext({
    dynCall: (signature, callbackPtr, args) => {
      const argValuesPtr = args[3];
      const tempPtr = context.HEAP32[argValuesPtr >> 2];
      const tempLen = context.HEAP32[(argValuesPtr >> 2) + 2];

      assert.strictEqual(signature, 'iiiiii');
      assert.strictEqual(callbackPtr, 9988);
      assert.strictEqual(args[0], 44);
      assert.strictEqual(args[1], 6);
      assert.strictEqual(args[2], 2);
      assert.strictEqual(tempLen, 4);
      assert.strictEqual(context.HEAP32[tempPtr >> 2], 21);

      context.HEAP32[tempPtr >> 2] = 34;
      return 1;
    }
  });

  const host = context.FOnlineWasmHost;
  context.HEAP32[64 >> 2] = KIND_I32;
  context.HEAP32[(64 >> 2) + 1] = KIND_I32;
  context.HEAP32[80 >> 2] = ABI_MUTABLE_VALUE_POINTER;
  context.HEAP32[(80 >> 2) + 1] = ABI_MUTABLE_VALUE_LENGTH;
  host.currentContext = {
    module: {
      instance: {
        exports: {
          memory
        }
      }
    }
  };

  host.registerApiImport('Game_MutateValue__int32_mut__void', 2, 64, 80, KIND_NONE, 44, 6, 9988);
  host.imports['fonline.api'].Game_MutateValue__int32_mut__void.func(144, 4);

  assert.strictEqual(moduleHeap[144], 34);
  assert.strictEqual(moduleView.getInt32(144, true), 34);
  host.currentContext = null;
}

function testApiImportTrampolineCopiesMutableUtf8StringArgumentsBack() {
  let context;
  const memory = new WebAssembly.Memory({ initial: 1 });
  const moduleHeap = new Uint8Array(memory.buffer);
  const moduleView = new DataView(memory.buffer);
  const input = new TextEncoder().encode('hi');
  const output = new TextEncoder().encode('hi!');
  const smallInput = new TextEncoder().encode('xy');
  let calls = 0;

  moduleHeap.set(input, 160);

  context = makeHostContext({
    dynCall: (signature, callbackPtr, args) => {
      const argValuesPtr = args[3];
      const tempPtr = context.HEAP32[argValuesPtr >> 2];
      const tempLen = context.HEAP32[(argValuesPtr >> 2) + 2];
      const tempCapacity = context.HEAP32[(argValuesPtr >> 2) + 4];
      const requiredTempPtr = context.HEAP32[(argValuesPtr >> 2) + 6];

      assert.strictEqual(signature, 'iiiiii');
      assert.strictEqual(callbackPtr, 9989);
      assert.strictEqual(args[0], 45);
      assert.strictEqual(args[1], 7);
      assert.strictEqual(args[2], 4);
      const expectedInput = calls === 0 ? input : smallInput;
      assert.deepStrictEqual(Array.from(context.HEAPU8.slice(tempPtr, tempPtr + tempLen)), Array.from(expectedInput));
      assert.ok(tempCapacity >= tempLen);

      context.HEAPU8.set(output, tempPtr);
      context.HEAP32[requiredTempPtr >> 2] = output.length;
      calls += 1;
      return 1;
    }
  });

  const host = context.FOnlineWasmHost;
  context.HEAP32[64 >> 2] = KIND_I32;
  context.HEAP32[(64 >> 2) + 1] = KIND_I32;
  context.HEAP32[(64 >> 2) + 2] = KIND_I32;
  context.HEAP32[(64 >> 2) + 3] = KIND_I32;
  context.HEAP32[80 >> 2] = ABI_MUTABLE_UTF8_STRING_POINTER;
  context.HEAP32[(80 >> 2) + 1] = ABI_MUTABLE_UTF8_STRING_BYTE_LENGTH;
  context.HEAP32[(80 >> 2) + 2] = ABI_MUTABLE_UTF8_STRING_CAPACITY_BYTE_LENGTH;
  context.HEAP32[(80 >> 2) + 3] = ABI_MUTABLE_UTF8_STRING_REQUIRED_BYTE_LENGTH_POINTER;
  host.currentContext = {
    module: {
      instance: {
        exports: {
          memory
        }
      }
    }
  };

  host.registerApiImport('Game_MutateText__string_mut__void', 4, 64, 80, KIND_NONE, 45, 7, 9989);
  host.imports['fonline.api'].Game_MutateText__string_mut__void.func(160, input.length, 16, 224);

  assert.deepStrictEqual(Array.from(moduleHeap.slice(160, 160 + output.length)), Array.from(output));
  assert.strictEqual(moduleView.getUint32(224, true), output.length);

  moduleHeap.set(smallInput, 256);
  host.imports['fonline.api'].Game_MutateText__string_mut__void.func(256, smallInput.length, smallInput.length, 288);

  assert.deepStrictEqual(Array.from(moduleHeap.slice(256, 256 + smallInput.length)), Array.from(smallInput));
  assert.strictEqual(moduleView.getUint32(288, true), output.length);
  host.currentContext = null;
}

function testApiImportTrampolineCopiesArrayBuffers() {
  let context;
  const memory = new WebAssembly.Memory({ initial: 1 });
  const moduleHeap = new Uint8Array(memory.buffer);
  const input = Uint8Array.from([1, 2, 3, 4]);
  const output = Uint8Array.from([8, 7, 6]);

  moduleHeap.set(input, 192);

  context = makeHostContext({
    dynCall: (signature, callbackPtr, args) => {
      const argValuesPtr = args[3];
      const resultValuePtr = args[4];
      const inputTempPtr = context.HEAP32[argValuesPtr >> 2];
      const inputTempLen = context.HEAP32[(argValuesPtr >> 2) + 2];
      const outputTempPtr = context.HEAP32[(argValuesPtr >> 2) + 4];
      const outputTempLen = context.HEAP32[(argValuesPtr >> 2) + 6];

      assert.strictEqual(signature, 'iiiiii');
      assert.strictEqual(callbackPtr, 8877);
      assert.strictEqual(args[0], 55);
      assert.strictEqual(args[1], 7);
      assert.strictEqual(args[2], 4);
      assert.strictEqual(inputTempLen, input.length);
      assert.strictEqual(outputTempLen, 8);
      assert.deepStrictEqual(Array.from(context.HEAPU8.slice(inputTempPtr, inputTempPtr + inputTempLen)), Array.from(input));

      context.HEAPU8.set(output, outputTempPtr);
      context.HEAP32[resultValuePtr >> 2] = output.length;
      return 1;
    }
  });

  const host = context.FOnlineWasmHost;
  context.HEAP32[64 >> 2] = KIND_I32;
  context.HEAP32[(64 >> 2) + 1] = KIND_I32;
  context.HEAP32[(64 >> 2) + 2] = KIND_I32;
  context.HEAP32[(64 >> 2) + 3] = KIND_I32;
  context.HEAP32[80 >> 2] = ABI_ARRAY_POINTER;
  context.HEAP32[(80 >> 2) + 1] = ABI_ARRAY_BYTE_LENGTH;
  context.HEAP32[(80 >> 2) + 2] = ABI_ARRAY_OUTPUT_POINTER;
  context.HEAP32[(80 >> 2) + 3] = ABI_ARRAY_OUTPUT_BYTE_LENGTH;
  host.currentContext = {
    module: {
      instance: {
        exports: {
          memory
        }
      }
    }
  };

  host.registerApiImport('Game_ArrayRoundtrip__uint8_array__uint8_array', 4, 64, 80, KIND_I32, 55, 7, 8877);

  const result = host.imports['fonline.api'].Game_ArrayRoundtrip__uint8_array__uint8_array.func(192, input.length, 256, 8);
  const copied = new Uint8Array(memory.buffer).slice(256, 256 + output.length);

  assert.strictEqual(result, output.length);
  assert.deepStrictEqual(Array.from(copied), Array.from(output));
  host.currentContext = null;
}

function testApiImportTrampolineCopiesValueBuffers() {
  let context;
  const memory = new WebAssembly.Memory({ initial: 1 });
  const moduleHeap = new Uint8Array(memory.buffer);
  const input = Uint8Array.from([1, 0, 0, 0, 2, 0, 0, 0, 3, 0, 0, 0, 4, 0, 0, 0]);
  const output = Uint8Array.from([5, 0, 0, 0, 6, 0, 0, 0, 7, 0, 0, 0, 8, 0, 0, 0]);

  moduleHeap.set(input, 192);

  context = makeHostContext({
    dynCall: (signature, callbackPtr, args) => {
      const argValuesPtr = args[3];
      const resultValuePtr = args[4];
      const inputTempPtr = context.HEAP32[argValuesPtr >> 2];
      const inputTempLen = context.HEAP32[(argValuesPtr >> 2) + 2];
      const outputTempPtr = context.HEAP32[(argValuesPtr >> 2) + 4];
      const outputTempLen = context.HEAP32[(argValuesPtr >> 2) + 6];

      assert.strictEqual(signature, 'iiiiii');
      assert.strictEqual(callbackPtr, 1199);
      assert.strictEqual(args[0], 77);
      assert.strictEqual(args[1], 11);
      assert.strictEqual(args[2], 4);
      assert.strictEqual(inputTempLen, input.length);
      assert.ok(outputTempLen === output.length || outputTempLen === 8);
      assert.deepStrictEqual(Array.from(context.HEAPU8.slice(inputTempPtr, inputTempPtr + inputTempLen)), Array.from(input));

      context.HEAPU8.set(output.subarray(0, Math.min(output.length, outputTempLen)), outputTempPtr);
      context.HEAP32[resultValuePtr >> 2] = output.length;
      return 1;
    }
  });

  const host = context.FOnlineWasmHost;
  context.HEAP32[64 >> 2] = KIND_I32;
  context.HEAP32[(64 >> 2) + 1] = KIND_I32;
  context.HEAP32[(64 >> 2) + 2] = KIND_I32;
  context.HEAP32[(64 >> 2) + 3] = KIND_I32;
  context.HEAP32[80 >> 2] = ABI_VALUE_POINTER;
  context.HEAP32[(80 >> 2) + 1] = ABI_VALUE_BYTE_LENGTH;
  context.HEAP32[(80 >> 2) + 2] = ABI_VALUE_OUTPUT_POINTER;
  context.HEAP32[(80 >> 2) + 3] = ABI_VALUE_OUTPUT_BYTE_LENGTH;
  host.currentContext = {
    module: {
      instance: {
        exports: {
          memory
        }
      }
    }
  };

  host.registerApiImport('Game_RectRoundtrip__irect__irect', 4, 64, 80, KIND_I32, 77, 11, 1199);

  const result = host.imports['fonline.api'].Game_RectRoundtrip__irect__irect.func(192, input.length, 256, output.length);
  const copied = new Uint8Array(memory.buffer).slice(256, 256 + output.length);

  assert.strictEqual(result, output.length);
  assert.deepStrictEqual(Array.from(copied), Array.from(output));

  moduleHeap.fill(0xAA, 320, 328);

  const smallResult = host.imports['fonline.api'].Game_RectRoundtrip__irect__irect.func(192, input.length, 320, 8);

  assert.strictEqual(smallResult, output.length);
  assert.deepStrictEqual(Array.from(moduleHeap.slice(320, 328)), [0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA]);
  host.currentContext = null;
}

function testApiImportTrampolineCopiesDictBuffers() {
  let context;
  const memory = new WebAssembly.Memory({ initial: 1 });
  const moduleHeap = new Uint8Array(memory.buffer);
  const input = Uint8Array.from([1, 0, 0, 0, 3, 0, 0, 0, 111, 110, 101, 1, 0, 0, 0, 49]);
  const output = Uint8Array.from([1, 0, 0, 0, 3, 0, 0, 0, 116, 119, 111, 2, 0, 0, 0, 50, 50]);

  moduleHeap.set(input, 288);

  context = makeHostContext({
    dynCall: (signature, callbackPtr, args) => {
      const argValuesPtr = args[3];
      const resultValuePtr = args[4];
      const rawBase = argValuesPtr >> 2;
      const inputTempPtr = context.HEAP32[rawBase];
      const inputTempLen = context.HEAP32[rawBase + 2];
      const outputTempPtr = context.HEAP32[rawBase + 4];
      const outputTempLen = context.HEAP32[rawBase + 6];

      assert.strictEqual(signature, 'iiiiii');
      assert.strictEqual(callbackPtr, 6655);
      assert.strictEqual(args[0], 88);
      assert.strictEqual(args[1], 9);
      assert.strictEqual(args[2], 4);
      assert.strictEqual(inputTempLen, input.length);
      assert.ok(outputTempLen === 32 || outputTempLen === 4);
      assert.deepStrictEqual(Array.from(context.HEAPU8.slice(inputTempPtr, inputTempPtr + inputTempLen)), Array.from(input));

      context.HEAPU8.set(output.subarray(0, Math.min(output.length, outputTempLen)), outputTempPtr);
      context.HEAP32[resultValuePtr >> 2] = output.length;
      return 1;
    }
  });

  const host = context.FOnlineWasmHost;
  context.HEAP32[64 >> 2] = KIND_I32;
  context.HEAP32[(64 >> 2) + 1] = KIND_I32;
  context.HEAP32[(64 >> 2) + 2] = KIND_I32;
  context.HEAP32[(64 >> 2) + 3] = KIND_I32;
  context.HEAP32[80 >> 2] = ABI_DICT_POINTER;
  context.HEAP32[(80 >> 2) + 1] = ABI_DICT_BYTE_LENGTH;
  context.HEAP32[(80 >> 2) + 2] = ABI_DICT_OUTPUT_POINTER;
  context.HEAP32[(80 >> 2) + 3] = ABI_DICT_OUTPUT_BYTE_LENGTH;
  host.currentContext = {
    module: {
      instance: {
        exports: {
          memory
        }
      }
    }
  };

  host.registerApiImport('Game_DictRoundtrip__string_string_dict__string_string_dict', 4, 64, 80, KIND_I32, 88, 9, 6655);

  const result = host.imports['fonline.api'].Game_DictRoundtrip__string_string_dict__string_string_dict.func(288, input.length, 384, 32);
  const copied = new Uint8Array(memory.buffer).slice(384, 384 + output.length);

  assert.strictEqual(result, output.length);
  assert.deepStrictEqual(Array.from(copied), Array.from(output));

  moduleHeap.fill(0xAA, 448, 452);

  const smallResult = host.imports['fonline.api'].Game_DictRoundtrip__string_string_dict__string_string_dict.func(288, input.length, 448, 4);

  assert.strictEqual(smallResult, output.length);
  assert.deepStrictEqual(Array.from(moduleHeap.slice(448, 452)), [0xAA, 0xAA, 0xAA, 0xAA]);
  host.currentContext = null;
}

function testApiImportTrampolineCopiesMutableArrayBuffersBack() {
  let context;
  const memory = new WebAssembly.Memory({ initial: 1 });
  const moduleHeap = new Uint8Array(memory.buffer);
  const moduleView = new DataView(memory.buffer);
  const input = Uint8Array.from([1, 2]);
  const output = Uint8Array.from([9, 8, 7]);

  moduleHeap.set(input, 320);
  moduleHeap[323] = 0xAA;

  context = makeHostContext({
    dynCall: (signature, callbackPtr, args) => {
      const argValuesPtr = args[3];
      const rawBase = argValuesPtr >> 2;
      const tempPtr = context.HEAP32[rawBase];
      const tempByteLen = context.HEAP32[rawBase + 2];
      const tempCapacity = context.HEAP32[rawBase + 4];
      const requiredTempPtr = context.HEAP32[rawBase + 6];

      assert.strictEqual(signature, 'iiiiii');
      assert.strictEqual(callbackPtr, 7766);
      assert.strictEqual(args[0], 66);
      assert.strictEqual(args[1], 8);
      assert.strictEqual(args[2], 4);
      assert.strictEqual(tempByteLen, input.length);
      assert.strictEqual(tempCapacity, 4);
      assert.deepStrictEqual(Array.from(context.HEAPU8.slice(tempPtr, tempPtr + tempByteLen)), Array.from(input));

      context.HEAPU8.set(output, tempPtr);
      context.HEAP32[requiredTempPtr >> 2] = output.length;
      return 1;
    }
  });

  const host = context.FOnlineWasmHost;
  context.HEAP32[64 >> 2] = KIND_I32;
  context.HEAP32[(64 >> 2) + 1] = KIND_I32;
  context.HEAP32[(64 >> 2) + 2] = KIND_I32;
  context.HEAP32[(64 >> 2) + 3] = KIND_I32;
  context.HEAP32[80 >> 2] = ABI_MUTABLE_ARRAY_POINTER;
  context.HEAP32[(80 >> 2) + 1] = ABI_MUTABLE_ARRAY_BYTE_LENGTH;
  context.HEAP32[(80 >> 2) + 2] = ABI_MUTABLE_ARRAY_CAPACITY_BYTE_LENGTH;
  context.HEAP32[(80 >> 2) + 3] = ABI_MUTABLE_ARRAY_REQUIRED_BYTE_LENGTH_POINTER;
  host.currentContext = {
    module: {
      instance: {
        exports: {
          memory
        }
      }
    }
  };

  host.registerApiImport('Game_MutateValues__uint8_array_mut__void', 4, 64, 80, KIND_NONE, 66, 8, 7766);
  host.imports['fonline.api'].Game_MutateValues__uint8_array_mut__void.func(320, input.length, 4, 400);

  assert.deepStrictEqual(Array.from(moduleHeap.slice(320, 320 + output.length)), Array.from(output));
  assert.strictEqual(moduleHeap[323], 0xAA);
  assert.strictEqual(moduleView.getUint32(400, true), output.length);
  host.currentContext = null;
}

function testApiImportTrampolineCopiesMutableDictBuffersBack() {
  let context;
  const memory = new WebAssembly.Memory({ initial: 1 });
  const moduleHeap = new Uint8Array(memory.buffer);
  const moduleView = new DataView(memory.buffer);
  const input = Uint8Array.from([1, 0, 0, 0, 3, 0, 0, 0, 111, 108, 100, 1, 0, 0, 0, 48]);
  const emptyInput = Uint8Array.from([0, 0, 0, 0]);
  const output = Uint8Array.from([1, 0, 0, 0, 3, 0, 0, 0, 110, 101, 119, 1, 0, 0, 0, 49]);

  moduleHeap.set(input, 512);

  context = makeHostContext({
    dynCall: (signature, callbackPtr, args) => {
      const argValuesPtr = args[3];
      const rawBase = argValuesPtr >> 2;
      const tempPtr = context.HEAP32[rawBase];
      const tempByteLen = context.HEAP32[rawBase + 2];
      const tempCapacity = context.HEAP32[rawBase + 4];
      const requiredTempPtr = context.HEAP32[rawBase + 6];

      assert.strictEqual(signature, 'iiiiii');
      assert.strictEqual(callbackPtr, 5544);
      assert.strictEqual(args[0], 99);
      assert.strictEqual(args[1], 10);
      assert.strictEqual(args[2], 4);
      assert.ok(tempByteLen === input.length || tempByteLen === emptyInput.length);
      assert.ok(tempCapacity === 64 || tempCapacity === 4);
      assert.deepStrictEqual(Array.from(context.HEAPU8.slice(tempPtr, tempPtr + tempByteLen)), Array.from(tempByteLen === input.length ? input : emptyInput));

      context.HEAPU8.set(output.subarray(0, Math.min(output.length, tempCapacity)), tempPtr);
      context.HEAP32[requiredTempPtr >> 2] = output.length;
      return 1;
    }
  });

  const host = context.FOnlineWasmHost;
  context.HEAP32[64 >> 2] = KIND_I32;
  context.HEAP32[(64 >> 2) + 1] = KIND_I32;
  context.HEAP32[(64 >> 2) + 2] = KIND_I32;
  context.HEAP32[(64 >> 2) + 3] = KIND_I32;
  context.HEAP32[80 >> 2] = ABI_MUTABLE_DICT_POINTER;
  context.HEAP32[(80 >> 2) + 1] = ABI_MUTABLE_DICT_BYTE_LENGTH;
  context.HEAP32[(80 >> 2) + 2] = ABI_MUTABLE_DICT_CAPACITY_BYTE_LENGTH;
  context.HEAP32[(80 >> 2) + 3] = ABI_MUTABLE_DICT_REQUIRED_BYTE_LENGTH_POINTER;
  host.currentContext = {
    module: {
      instance: {
        exports: {
          memory
        }
      }
    }
  };

  host.registerApiImport('Game_MutateConfig__string_string_dict_mut__void', 4, 64, 80, KIND_NONE, 99, 10, 5544);
  host.imports['fonline.api'].Game_MutateConfig__string_string_dict_mut__void.func(512, input.length, 64, 608);

  assert.deepStrictEqual(Array.from(moduleHeap.slice(512, 512 + output.length)), Array.from(output));
  assert.strictEqual(moduleView.getUint32(608, true), output.length);

  moduleHeap.set(emptyInput, 640);

  host.imports['fonline.api'].Game_MutateConfig__string_string_dict_mut__void.func(640, emptyInput.length, emptyInput.length, 704);

  assert.deepStrictEqual(Array.from(moduleHeap.slice(640, 640 + emptyInput.length)), Array.from(emptyInput));
  assert.strictEqual(moduleView.getUint32(704, true), output.length);
  host.currentContext = null;
}

function testScalarCallAndContextImports() {
  const context = makeHostContext();
  const host = context.FOnlineWasmHost;

  host.modules.math = {
    instance: {
      exports: {
        add_context: (left, right) => {
          return left + right + host.imports.fonline.get_side.func() + Number(host.imports.fonline.get_frame_delta_time_ms.func());
        }
      }
    }
  };

  writeCString(context, 16, 'math');
  writeCString(context, 64, 'add_context');
  context.HEAP32[128 >> 2] = KIND_I32;
  context.HEAP32[(128 >> 2) + 1] = KIND_I32;
  context.HEAP32[160 >> 2] = 4;
  context.HEAP32[168 >> 2] = 5;
  writeContextValues(context, 256, [1, 100, 3, 1, 200]);

  assert.strictEqual(host.call(16, 64, 2, 128, 0, 160, KIND_I32, 224, 256), true);
  assert.strictEqual(context.HEAP32[224 >> 2], 13);
  assert.strictEqual(host.currentContext, null);
  assert.strictEqual(host.takeLastError(), '');
}

function testExportCallCopiesUtf8StringArguments() {
  const context = makeHostContext();
  const host = context.FOnlineWasmHost;
  const memory = new WebAssembly.Memory({ initial: 1 });
  const input = new TextEncoder().encode('hello export');
  const freed = [];
  let nextPtr = 1024;
  let seen = null;

  context.HEAPU8.set(input, 320);
  host.modules.text = {
    instance: {
      exports: {
        memory,
        fonline_malloc: (size) => {
          const ptr = nextPtr;
          nextPtr += (Number(size) + 7) & ~7;
          return ptr;
        },
        fonline_free: (ptr) => freed.push(Number(ptr) >>> 0),
        text_len__string__int32: (ptr, len) => {
          seen = new Uint8Array(memory.buffer).slice(ptr, ptr + len);
          return len;
        }
      }
    }
  };

  writeCString(context, 16, 'text');
  writeCString(context, 64, 'text_len__string__int32');
  context.HEAP32[128 >> 2] = KIND_I32;
  context.HEAP32[(128 >> 2) + 1] = KIND_I32;
  context.HEAP32[144 >> 2] = ABI_UTF8_STRING_POINTER;
  context.HEAP32[(144 >> 2) + 1] = ABI_UTF8_STRING_LENGTH;
  context.HEAP32[160 >> 2] = 320;
  context.HEAP32[(160 >> 2) + 2] = input.length;
  writeContextValues(context, 256, [1, 100, 3, 1, 200]);

  assert.strictEqual(host.call(16, 64, 2, 128, 144, 160, KIND_I32, 224, 256), true);
  assert.strictEqual(context.HEAP32[224 >> 2], input.length);
  assert.deepStrictEqual(Array.from(seen), Array.from(input));
  assert.deepStrictEqual(freed, [1024]);
  assert.strictEqual(host.currentContext, null);
  assert.strictEqual(host.takeLastError(), '');
}

function testExportCallCopiesCallbackArguments() {
  const context = makeHostContext();
  const host = context.FOnlineWasmHost;
  const memory = new WebAssembly.Memory({ initial: 1 });
  const freed = [];
  const callbackName = 'CallbackModule::AddOne';
  const callbackBytes = new TextEncoder().encode(callbackName);
  let nextPtr = 1024;

  context.HEAPU8.set(callbackBytes, 320);
  host.modules.callbacks = {
    instance: {
      exports: {
        memory,
        fonline_malloc: (size) => {
          const ptr = nextPtr;
          nextPtr += (Number(size) + 7) & ~7;
          return ptr;
        },
        fonline_free: (ptr) => freed.push(Number(ptr) >>> 0),
        callback_name_len__callback_int32_int32_callback__int32: (ptr, len) => {
          assert.strictEqual(new TextDecoder().decode(new Uint8Array(memory.buffer, ptr, len)), callbackName);
          return len;
        }
      }
    }
  };

  writeCString(context, 16, 'callbacks');
  writeCString(context, 64, 'callback_name_len__callback_int32_int32_callback__int32');
  context.HEAP32[128 >> 2] = KIND_I32;
  context.HEAP32[(128 >> 2) + 1] = KIND_I32;
  context.HEAP32[144 >> 2] = ABI_CALLBACK_POINTER;
  context.HEAP32[(144 >> 2) + 1] = ABI_CALLBACK_LENGTH;
  context.HEAP32[160 >> 2] = 320;
  context.HEAP32[(160 >> 2) + 2] = callbackBytes.length;
  writeContextValues(context, 256, [1, 100, 3, 1, 200]);

  assert.strictEqual(host.call(16, 64, 2, 128, 144, 160, KIND_I32, 224, 256), true);
  assert.strictEqual(context.HEAP32[224 >> 2], callbackBytes.length);
  assert.deepStrictEqual(freed, [1024]);
  assert.strictEqual(host.currentContext, null);
  assert.strictEqual(host.takeLastError(), '');
}

function testExportCallCopiesUtf8StringResult() {
  const context = makeHostContext();
  const host = context.FOnlineWasmHost;
  const memory = new WebAssembly.Memory({ initial: 1 });
  const output = new TextEncoder().encode('hello result');

  new Uint8Array(memory.buffer).set(output, 1536);
  host.modules.text = {
    instance: {
      exports: {
        memory,
        get_text__void__string: () => {
          return (BigInt(output.length) << 32n) | 1536n;
        }
      }
    }
  };

  writeCString(context, 16, 'text');
  writeCString(context, 64, 'get_text__void__string');
  writeContextValues(context, 256, [1, 100, 3, 1, 200]);

  assert.strictEqual(host.call(16, 64, 0, 128, 144, 160, KIND_I64, 224, 256, 1), true);

  const packed = BigInt.asUintN(64, context.HEAP64[224 >> 3]);
  const enginePtr = Number(packed & 0xFFFFFFFFn) >>> 0;
  const engineLen = Number((packed >> 32n) & 0xFFFFFFFFn) >>> 0;

  assert.strictEqual(engineLen, output.length);
  assert.deepStrictEqual(Array.from(context.HEAPU8.slice(enginePtr, enginePtr + engineLen)), Array.from(output));
  assert.strictEqual(host.currentContext, null);
  assert.strictEqual(host.takeLastError(), '');
}

function testExportCallCopiesArrayArguments() {
  const context = makeHostContext();
  const host = context.FOnlineWasmHost;
  const memory = new WebAssembly.Memory({ initial: 1 });
  const input = Uint8Array.from([3, 0, 0, 0, 4, 0, 0, 0]);
  const freed = [];
  let nextPtr = 1024;
  let seen = null;

  context.HEAPU8.set(input, 320);
  host.modules.arrays = {
    instance: {
      exports: {
        memory,
        fonline_malloc: (size) => {
          const ptr = nextPtr;
          nextPtr += (Number(size) + 7) & ~7;
          return ptr;
        },
        fonline_free: (ptr) => freed.push(Number(ptr) >>> 0),
        sum_pair__int32_array__int32: (ptr, len) => {
          seen = new Uint8Array(memory.buffer).slice(ptr, ptr + len);
          return len;
        }
      }
    }
  };

  writeCString(context, 16, 'arrays');
  writeCString(context, 64, 'sum_pair__int32_array__int32');
  context.HEAP32[128 >> 2] = KIND_I32;
  context.HEAP32[(128 >> 2) + 1] = KIND_I32;
  context.HEAP32[144 >> 2] = ABI_ARRAY_POINTER;
  context.HEAP32[(144 >> 2) + 1] = ABI_ARRAY_BYTE_LENGTH;
  context.HEAP32[160 >> 2] = 320;
  context.HEAP32[(160 >> 2) + 2] = input.length;
  writeContextValues(context, 256, [1, 100, 3, 1, 200]);

  assert.strictEqual(host.call(16, 64, 2, 128, 144, 160, KIND_I32, 224, 256), true);
  assert.strictEqual(context.HEAP32[224 >> 2], input.length);
  assert.deepStrictEqual(Array.from(seen), Array.from(input));
  assert.deepStrictEqual(freed, [1024]);
  assert.strictEqual(host.currentContext, null);
  assert.strictEqual(host.takeLastError(), '');
}

function testExportCallCopiesValueArguments() {
  const context = makeHostContext();
  const host = context.FOnlineWasmHost;
  const memory = new WebAssembly.Memory({ initial: 1 });
  const input = Uint8Array.from([1, 0, 0, 0, 2, 0, 0, 0, 3, 0, 0, 0, 4, 0, 0, 0]);
  const freed = [];
  let nextPtr = 1024;
  let seen = null;

  context.HEAPU8.set(input, 320);
  host.modules.values = {
    instance: {
      exports: {
        memory,
        fonline_malloc: (size) => {
          const ptr = nextPtr;
          nextPtr += (Number(size) + 7) & ~7;
          return ptr;
        },
        fonline_free: (ptr) => freed.push(Number(ptr) >>> 0),
        rect_area__irect__int32: (ptr, len) => {
          seen = new Uint8Array(memory.buffer).slice(ptr, ptr + len);
          return new DataView(memory.buffer).getInt32(ptr + 8, true) * new DataView(memory.buffer).getInt32(ptr + 12, true);
        }
      }
    }
  };

  writeCString(context, 16, 'values');
  writeCString(context, 64, 'rect_area__irect__int32');
  context.HEAP32[128 >> 2] = KIND_I32;
  context.HEAP32[(128 >> 2) + 1] = KIND_I32;
  context.HEAP32[144 >> 2] = ABI_VALUE_POINTER;
  context.HEAP32[(144 >> 2) + 1] = ABI_VALUE_BYTE_LENGTH;
  context.HEAP32[160 >> 2] = 320;
  context.HEAP32[(160 >> 2) + 2] = input.length;
  writeContextValues(context, 256, [1, 100, 3, 1, 200]);

  assert.strictEqual(host.call(16, 64, 2, 128, 144, 160, KIND_I32, 224, 256), true);
  assert.strictEqual(context.HEAP32[224 >> 2], 12);
  assert.deepStrictEqual(Array.from(seen), Array.from(input));
  assert.deepStrictEqual(freed, [1024]);
  assert.strictEqual(host.currentContext, null);
  assert.strictEqual(host.takeLastError(), '');
}

function testExportCallCopiesDictArguments() {
  const context = makeHostContext();
  const host = context.FOnlineWasmHost;
  const memory = new WebAssembly.Memory({ initial: 1 });
  const input = Uint8Array.from([
    2, 0, 0, 0,
    3, 0, 0, 0, 111, 110, 101,
    1, 0, 0, 0, 49,
    3, 0, 0, 0, 116, 119, 111,
    2, 0, 0, 0, 50, 50
  ]);
  const freed = [];
  let nextPtr = 1024;
  let seen = null;

  context.HEAPU8.set(input, 320);
  host.modules.dicts = {
    instance: {
      exports: {
        memory,
        fonline_malloc: (size) => {
          const ptr = nextPtr;
          nextPtr += (Number(size) + 7) & ~7;
          return ptr;
        },
        fonline_free: (ptr) => freed.push(Number(ptr) >>> 0),
        dict_size__string_string_dict__int32: (ptr, len) => {
          seen = new Uint8Array(memory.buffer).slice(ptr, ptr + len);
          return new DataView(memory.buffer).getUint32(ptr, true);
        }
      }
    }
  };

  writeCString(context, 16, 'dicts');
  writeCString(context, 64, 'dict_size__string_string_dict__int32');
  context.HEAP32[128 >> 2] = KIND_I32;
  context.HEAP32[(128 >> 2) + 1] = KIND_I32;
  context.HEAP32[144 >> 2] = ABI_DICT_POINTER;
  context.HEAP32[(144 >> 2) + 1] = ABI_DICT_BYTE_LENGTH;
  context.HEAP32[160 >> 2] = 320;
  context.HEAP32[(160 >> 2) + 2] = input.length;
  writeContextValues(context, 256, [1, 100, 3, 1, 200]);

  assert.strictEqual(host.call(16, 64, 2, 128, 144, 160, KIND_I32, 224, 256), true);
  assert.strictEqual(context.HEAP32[224 >> 2], 2);
  assert.deepStrictEqual(Array.from(seen), Array.from(input));
  assert.deepStrictEqual(freed, [1024]);
  assert.strictEqual(host.currentContext, null);
  assert.strictEqual(host.takeLastError(), '');
}

function testExportCallCopiesValueTypeDictArrayArguments() {
  const context = makeHostContext();
  const host = context.FOnlineWasmHost;
  const memory = new WebAssembly.Memory({ initial: 1 });
  const input = Uint8Array.from([
    1, 0, 0, 0,
    6, 0, 0, 0, 99, 111, 108, 111, 114, 115,
    2, 0, 0, 0,
    0x44, 0x33, 0x22, 0x11,
    0x88, 0x77, 0x66, 0x55
  ]);
  const freed = [];
  let nextPtr = 1024;
  let seen = null;

  context.HEAPU8.set(input, 320);
  host.modules.color_groups = {
    instance: {
      exports: {
        memory,
        fonline_malloc: (size) => {
          const ptr = nextPtr;
          nextPtr += (Number(size) + 7) & ~7;
          return ptr;
        },
        fonline_free: (ptr) => freed.push(Number(ptr) >>> 0),
        color_groups_size__string_ucolor_array_dict__int32: (ptr, len) => {
          seen = new Uint8Array(memory.buffer).slice(ptr, ptr + len);
          return len;
        }
      }
    }
  };

  writeCString(context, 16, 'color_groups');
  writeCString(context, 64, 'color_groups_size__string_ucolor_array_dict__int32');
  context.HEAP32[128 >> 2] = KIND_I32;
  context.HEAP32[(128 >> 2) + 1] = KIND_I32;
  context.HEAP32[144 >> 2] = ABI_DICT_POINTER;
  context.HEAP32[(144 >> 2) + 1] = ABI_DICT_BYTE_LENGTH;
  context.HEAP32[160 >> 2] = 320;
  context.HEAP32[(160 >> 2) + 2] = input.length;
  writeContextValues(context, 256, [1, 100, 3, 1, 200]);

  assert.strictEqual(host.call(16, 64, 2, 128, 144, 160, KIND_I32, 224, 256), true);
  assert.strictEqual(context.HEAP32[224 >> 2], input.length);
  assert.deepStrictEqual(Array.from(seen), Array.from(input));
  assert.deepStrictEqual(freed, [1024]);
  assert.strictEqual(host.currentContext, null);
  assert.strictEqual(host.takeLastError(), '');
}

function testExportCallCopiesMutableValueArgumentsBack() {
  const context = makeHostContext();
  const host = context.FOnlineWasmHost;
  const memory = new WebAssembly.Memory({ initial: 1 });
  const freed = [];
  let nextPtr = 1024;

  context.HEAP32[320 >> 2] = 37;
  host.modules.mutable = {
    instance: {
      exports: {
        memory,
        fonline_malloc: (size) => {
          const ptr = nextPtr;
          nextPtr += (Number(size) + 7) & ~7;
          return ptr;
        },
        fonline_free: (ptr) => freed.push(Number(ptr) >>> 0),
        bump_ref__int32_mut__void: (ptr, len) => {
          assert.strictEqual(len, 4);
          const view = new DataView(memory.buffer);
          view.setInt32(ptr, view.getInt32(ptr, true) + 5, true);
        }
      }
    }
  };

  writeCString(context, 16, 'mutable');
  writeCString(context, 64, 'bump_ref__int32_mut__void');
  context.HEAP32[128 >> 2] = KIND_I32;
  context.HEAP32[(128 >> 2) + 1] = KIND_I32;
  context.HEAP32[144 >> 2] = ABI_MUTABLE_VALUE_POINTER;
  context.HEAP32[(144 >> 2) + 1] = ABI_MUTABLE_VALUE_LENGTH;
  context.HEAP32[160 >> 2] = 320;
  context.HEAP32[(160 >> 2) + 2] = 4;
  writeContextValues(context, 256, [1, 100, 3, 1, 200]);

  assert.strictEqual(host.call(16, 64, 2, 128, 144, 160, KIND_NONE, 224, 256), true);
  assert.strictEqual(context.HEAP32[320 >> 2], 42);
  assert.deepStrictEqual(freed, [1024]);
  assert.strictEqual(host.currentContext, null);
  assert.strictEqual(host.takeLastError(), '');
}

function testExportCallCopiesMutableUtf8StringArgumentsBack() {
  const context = makeHostContext();
  const host = context.FOnlineWasmHost;
  const memory = new WebAssembly.Memory({ initial: 1 });
  const freed = [];
  const input = new TextEncoder().encode('hello');
  const output = new TextEncoder().encode('HELLO');
  let nextPtr = 1024;

  context.HEAPU8.set(input, 320);
  context.HEAP32[352 >> 2] = input.length;
  host.modules.mutable_texts = {
    instance: {
      exports: {
        memory,
        fonline_malloc: (size) => {
          const ptr = nextPtr;
          nextPtr += (Number(size) + 7) & ~7;
          return ptr;
        },
        fonline_free: (ptr) => freed.push(Number(ptr) >>> 0),
        mutate_text__string_mut__void: (ptr, len, capacity, requiredPtr) => {
          assert.strictEqual(len, input.length);
          assert.strictEqual(capacity, input.length);
          const view = new DataView(memory.buffer);
          view.setUint32(requiredPtr, len, true);
          new Uint8Array(memory.buffer).set(output, ptr);
        }
      }
    }
  };

  writeCString(context, 16, 'mutable_texts');
  writeCString(context, 64, 'mutate_text__string_mut__void');
  context.HEAP32[128 >> 2] = KIND_I32;
  context.HEAP32[(128 >> 2) + 1] = KIND_I32;
  context.HEAP32[(128 >> 2) + 2] = KIND_I32;
  context.HEAP32[(128 >> 2) + 3] = KIND_I32;
  context.HEAP32[144 >> 2] = ABI_MUTABLE_UTF8_STRING_POINTER;
  context.HEAP32[(144 >> 2) + 1] = ABI_MUTABLE_UTF8_STRING_BYTE_LENGTH;
  context.HEAP32[(144 >> 2) + 2] = ABI_MUTABLE_UTF8_STRING_CAPACITY_BYTE_LENGTH;
  context.HEAP32[(144 >> 2) + 3] = ABI_MUTABLE_UTF8_STRING_REQUIRED_BYTE_LENGTH_POINTER;
  context.HEAP32[160 >> 2] = 320;
  context.HEAP32[(160 >> 2) + 2] = input.length;
  context.HEAP32[(160 >> 2) + 4] = input.length;
  context.HEAP32[(160 >> 2) + 6] = 352;
  writeContextValues(context, 256, [1, 100, 3, 1, 200]);

  assert.strictEqual(host.call(16, 64, 4, 128, 144, 160, KIND_NONE, 224, 256), true);
  assert.deepStrictEqual(Array.from(context.HEAPU8.slice(320, 325)), Array.from(output));
  assert.strictEqual(context.HEAP32[352 >> 2], input.length);
  assert.deepStrictEqual(freed, [1032, 1024]);
  assert.strictEqual(host.currentContext, null);
  assert.strictEqual(host.takeLastError(), '');
}

function testExportCallCopiesMutableArrayArgumentsBack() {
  const context = makeHostContext();
  const host = context.FOnlineWasmHost;
  const memory = new WebAssembly.Memory({ initial: 1 });
  const freed = [];
  let nextPtr = 1024;

  context.HEAP32[320 >> 2] = 3;
  context.HEAP32[(320 >> 2) + 1] = 4;
  context.HEAP32[352 >> 2] = 8;
  host.modules.mutable_arrays = {
    instance: {
      exports: {
        memory,
        fonline_malloc: (size) => {
          const ptr = nextPtr;
          nextPtr += (Number(size) + 7) & ~7;
          return ptr;
        },
        fonline_free: (ptr) => freed.push(Number(ptr) >>> 0),
        mutate_values__int32_array_mut__void: (ptr, len, capacity, requiredPtr) => {
          assert.strictEqual(len, 8);
          assert.strictEqual(capacity, 8);
          const view = new DataView(memory.buffer);
          view.setUint32(requiredPtr, len, true);
          view.setInt32(ptr, view.getInt32(ptr, true) + 5, true);
          view.setInt32(ptr + 4, view.getInt32(ptr + 4, true) + 6, true);
        }
      }
    }
  };

  writeCString(context, 16, 'mutable_arrays');
  writeCString(context, 64, 'mutate_values__int32_array_mut__void');
  context.HEAP32[128 >> 2] = KIND_I32;
  context.HEAP32[(128 >> 2) + 1] = KIND_I32;
  context.HEAP32[(128 >> 2) + 2] = KIND_I32;
  context.HEAP32[(128 >> 2) + 3] = KIND_I32;
  context.HEAP32[144 >> 2] = ABI_MUTABLE_ARRAY_POINTER;
  context.HEAP32[(144 >> 2) + 1] = ABI_MUTABLE_ARRAY_BYTE_LENGTH;
  context.HEAP32[(144 >> 2) + 2] = ABI_MUTABLE_ARRAY_CAPACITY_BYTE_LENGTH;
  context.HEAP32[(144 >> 2) + 3] = ABI_MUTABLE_ARRAY_REQUIRED_BYTE_LENGTH_POINTER;
  context.HEAP32[160 >> 2] = 320;
  context.HEAP32[(160 >> 2) + 2] = 8;
  context.HEAP32[(160 >> 2) + 4] = 8;
  context.HEAP32[(160 >> 2) + 6] = 352;
  writeContextValues(context, 256, [1, 100, 3, 1, 200]);

  assert.strictEqual(host.call(16, 64, 4, 128, 144, 160, KIND_NONE, 224, 256), true);
  assert.strictEqual(context.HEAP32[320 >> 2], 8);
  assert.strictEqual(context.HEAP32[(320 >> 2) + 1], 10);
  assert.strictEqual(context.HEAP32[352 >> 2], 8);
  assert.deepStrictEqual(freed, [1032, 1024]);
  assert.strictEqual(host.currentContext, null);
  assert.strictEqual(host.takeLastError(), '');
}

function testExportCallCopiesMutableDictArgumentsBack() {
  const context = makeHostContext();
  const host = context.FOnlineWasmHost;
  const memory = new WebAssembly.Memory({ initial: 1 });
  const freed = [];
  let nextPtr = 1024;

  context.HEAPU8.set([1, 2, 3, 4, 5, 6, 7, 8], 320);
  context.HEAP32[352 >> 2] = 8;
  host.modules.mutable_dicts = {
    instance: {
      exports: {
        memory,
        fonline_malloc: (size) => {
          const ptr = nextPtr;
          nextPtr += (Number(size) + 7) & ~7;
          return ptr;
        },
        fonline_free: (ptr) => freed.push(Number(ptr) >>> 0),
        mutate_config__string_string_dict_mut__void: (ptr, len, capacity, requiredPtr) => {
          assert.strictEqual(len, 8);
          assert.strictEqual(capacity, 8);
          const view = new DataView(memory.buffer);
          view.setUint32(requiredPtr, len, true);
          new Uint8Array(memory.buffer)[ptr] = 9;
          new Uint8Array(memory.buffer)[ptr + 7] = 10;
        }
      }
    }
  };

  writeCString(context, 16, 'mutable_dicts');
  writeCString(context, 64, 'mutate_config__string_string_dict_mut__void');
  context.HEAP32[128 >> 2] = KIND_I32;
  context.HEAP32[(128 >> 2) + 1] = KIND_I32;
  context.HEAP32[(128 >> 2) + 2] = KIND_I32;
  context.HEAP32[(128 >> 2) + 3] = KIND_I32;
  context.HEAP32[144 >> 2] = ABI_MUTABLE_DICT_POINTER;
  context.HEAP32[(144 >> 2) + 1] = ABI_MUTABLE_DICT_BYTE_LENGTH;
  context.HEAP32[(144 >> 2) + 2] = ABI_MUTABLE_DICT_CAPACITY_BYTE_LENGTH;
  context.HEAP32[(144 >> 2) + 3] = ABI_MUTABLE_DICT_REQUIRED_BYTE_LENGTH_POINTER;
  context.HEAP32[160 >> 2] = 320;
  context.HEAP32[(160 >> 2) + 2] = 8;
  context.HEAP32[(160 >> 2) + 4] = 8;
  context.HEAP32[(160 >> 2) + 6] = 352;
  writeContextValues(context, 256, [1, 100, 3, 1, 200]);

  assert.strictEqual(host.call(16, 64, 4, 128, 144, 160, KIND_NONE, 224, 256), true);
  assert.deepStrictEqual(Array.from(context.HEAPU8.slice(320, 328)), [9, 2, 3, 4, 5, 6, 7, 10]);
  assert.strictEqual(context.HEAP32[352 >> 2], 8);
  assert.deepStrictEqual(freed, [1032, 1024]);
  assert.strictEqual(host.currentContext, null);
  assert.strictEqual(host.takeLastError(), '');
}

function testLogUtf8ReadsCurrentModuleMemory() {
  const context = makeHostContext();
  const host = context.FOnlineWasmHost;
  const memory = new WebAssembly.Memory({ initial: 1 });
  const bytes = new TextEncoder().encode('hello');

  new Uint8Array(memory.buffer).set(bytes, 32);
  host.currentContext = {
    module: {
      instance: {
        exports: {
          memory
        }
      }
    }
  };

  host.imports.fonline.log_utf8.func(32, bytes.length);

  assert.deepStrictEqual(context.__logs.pop(), ['[wasm:utf8]', 'hello']);
}

function testCallbackLifecycleImportsCallEngineBridge() {
  const calls = [];
  let context;

  context = makeHostContext({
    dynCall: (signature, callbackPtr, args) => {
      calls.push({ signature, callbackPtr, args });

      assert.strictEqual(signature, 'iiii');
      assert.strictEqual(args[0], 123);
      assert.strictEqual(args[2], tokenBytes.length);
      assert.strictEqual(new TextDecoder().decode(context.HEAPU8.subarray(args[1], args[1] + args[2])), '__fonline_callback_7');
      return callbackPtr === 456 || callbackPtr === 789 ? 1 : 0;
    }
  });

  const host = context.FOnlineWasmHost;
  const memory = new WebAssembly.Memory({ initial: 1 });
  const tokenBytes = new TextEncoder().encode('__fonline_callback_7');

  new Uint8Array(memory.buffer).set(tokenBytes, 32);
  host.currentContext = {
    module: {
      instance: {
        exports: {
          memory
        }
      }
    },
    scriptSysPtr: 123,
    callbackRetainPtr: 456,
    callbackReleasePtr: 789
  };

  assert.strictEqual(host.imports.fonline.callback_retain.func(32, tokenBytes.length), 1);
  assert.strictEqual(host.imports.fonline.callback_release.func(32, tokenBytes.length), 1);
  assert.strictEqual(calls.length, 2);
  assert.strictEqual(calls[0].callbackPtr, 456);
  assert.strictEqual(calls[1].callbackPtr, 789);
}

async function testManifestImportSignatureMismatchIsReported() {
  const context = makeHostContext({
    calledRun: true,
    webAssembly: {
      Memory: WebAssembly.Memory,
      instantiate: () => {
        throw new Error('WebAssembly.instantiate should not be reached');
      }
    },
    fetch: async (url) => {
      if (url === 'WasmScripts/manifest.json') {
        return {
          ok: true,
          json: async () => ({
            version: 1,
            modules: [{
              name: 'bad',
              path: 'WasmScripts/bad.wasm',
              imports: [{
                module: 'fonline',
                name: 'log_i32',
                kind: 'func',
                params: ['i64'],
                results: []
              }],
              exports: []
            }]
          })
        };
      }

      return {
        ok: true,
        arrayBuffer: async () => new ArrayBuffer(8)
      };
    }
  });

  await context.FOnlineWasmHost.ready;

  assert.strictEqual(context.__dependencies.size, 0);
  assert.match(context.FOnlineWasmHost.takeLastError(), /import signature mismatch/);
}

async function testLateLoadedHostStartsAfterRuntimeInitialization() {
  const context = makeHostContext({
    foRuntimeInitialized: true,
    fetch: async (url) => {
      assert.strictEqual(url, 'WasmScripts/manifest.json');
      return {
        ok: true,
        json: async () => ({
          version: 1,
          modules: []
        })
      };
    }
  });

  await context.FOnlineWasmHost.ready;

  assert.strictEqual(context.FOnlineWasmHost.loadingStarted, true);
  assert.strictEqual(context.__dependencies.size, 0);
  assert.strictEqual(context.FOnlineWasmHost.takeLastError(), '');
}

async function main() {
  testScalarCallAndContextImports();
  testExportCallCopiesUtf8StringArguments();
  testExportCallCopiesCallbackArguments();
  testExportCallCopiesUtf8StringResult();
  testExportCallCopiesArrayArguments();
  testExportCallCopiesValueArguments();
  testExportCallCopiesDictArguments();
  testExportCallCopiesValueTypeDictArrayArguments();
  testExportCallCopiesMutableValueArgumentsBack();
  testExportCallCopiesMutableUtf8StringArgumentsBack();
  testExportCallCopiesMutableArrayArgumentsBack();
  testExportCallCopiesMutableDictArgumentsBack();
  testLogUtf8ReadsCurrentModuleMemory();
  testCallbackLifecycleImportsCallEngineBridge();
  testApiImportTrampolinePacksScalars();
  testApiImportTrampolineHandlesNoArgGetter();
  testApiImportTrampolineCopiesUtf8StringArguments();
  testApiImportTrampolineCopiesCallbackArguments();
  testApiImportTrampolineCopiesUtf8StringOutputs();
  testApiImportTrampolineCopiesMutableValueArgumentsBack();
  testApiImportTrampolineCopiesMutableUtf8StringArgumentsBack();
  testApiImportTrampolineCopiesArrayBuffers();
  testApiImportTrampolineCopiesValueBuffers();
  testApiImportTrampolineCopiesDictBuffers();
  testApiImportTrampolineCopiesMutableArrayBuffersBack();
  testApiImportTrampolineCopiesMutableDictBuffersBack();
  await testManifestImportSignatureMismatchIsReported();
  await testLateLoadedHostStartsAfterRuntimeInitialization();
}

main().catch((error) => {
  console.error(error);
  process.exit(1);
});
