(function (global) {
  'use strict';

  var KIND_NONE = 0;
  var KIND_I32 = 1;
  var KIND_I64 = 2;
  var KIND_F32 = 3;
  var KIND_F64 = 4;
  var ABI_SCALAR = 0;
  var ABI_UTF8_STRING_POINTER = 1;
  var ABI_UTF8_STRING_LENGTH = 2;
  var ABI_UTF8_STRING_OUTPUT_POINTER = 3;
  var ABI_UTF8_STRING_OUTPUT_LENGTH = 4;
  var ABI_MUTABLE_VALUE_POINTER = 5;
  var ABI_MUTABLE_VALUE_LENGTH = 6;
  var ABI_ARRAY_POINTER = 7;
  var ABI_ARRAY_BYTE_LENGTH = 8;
  var ABI_ARRAY_OUTPUT_POINTER = 9;
  var ABI_ARRAY_OUTPUT_BYTE_LENGTH = 10;
  var ABI_MUTABLE_ARRAY_POINTER = 11;
  var ABI_MUTABLE_ARRAY_BYTE_LENGTH = 12;
  var ABI_MUTABLE_ARRAY_CAPACITY_BYTE_LENGTH = 13;
  var ABI_MUTABLE_ARRAY_REQUIRED_BYTE_LENGTH_POINTER = 14;
  var ABI_DICT_POINTER = 15;
  var ABI_DICT_BYTE_LENGTH = 16;
  var ABI_DICT_OUTPUT_POINTER = 17;
  var ABI_DICT_OUTPUT_BYTE_LENGTH = 18;
  var ABI_MUTABLE_DICT_POINTER = 19;
  var ABI_MUTABLE_DICT_BYTE_LENGTH = 20;
  var ABI_MUTABLE_DICT_CAPACITY_BYTE_LENGTH = 21;
  var ABI_MUTABLE_DICT_REQUIRED_BYTE_LENGTH_POINTER = 22;
  var ABI_CALLBACK_POINTER = 23;
  var ABI_CALLBACK_LENGTH = 24;
  var ABI_MUTABLE_UTF8_STRING_POINTER = 25;
  var ABI_MUTABLE_UTF8_STRING_BYTE_LENGTH = 26;
  var ABI_MUTABLE_UTF8_STRING_CAPACITY_BYTE_LENGTH = 27;
  var ABI_MUTABLE_UTF8_STRING_REQUIRED_BYTE_LENGTH_POINTER = 28;
  var ABI_VALUE_POINTER = 29;
  var ABI_VALUE_BYTE_LENGTH = 30;
  var ABI_VALUE_OUTPUT_POINTER = 31;
  var ABI_VALUE_OUTPUT_BYTE_LENGTH = 32;

  var host = {
    manifest: { version: 1, modules: [] },
    modules: Object.create(null),
    imports: Object.create(null),
    currentContext: null,
    lastError: '',

    getManifest: function () {
      return host.manifest;
    },

    takeLastError: function () {
      var text = host.lastError || '';
      host.lastError = '';
      return text;
    },

    registerImport: function (moduleName, importName, params, results, func) {
      if (typeof params === 'function') {
        func = params;
        params = [];
        results = [];
      }

      if (!host.imports[moduleName]) {
        host.imports[moduleName] = Object.create(null);
      }

      host.imports[moduleName][importName] = {
        func: func,
        params: Array.isArray(params) ? params.slice() : [],
        results: Array.isArray(results) ? results.slice() : []
      };
    },

    registerApiImport: function (importName, argc, argKindsPtr, paramAbiPtr, resultKind, backendPtr, methodIndex, callbackPtr) {
      var argKinds = [];
      var paramAbi = [];
      var params = [];

      for (var i = 0; i < argc; i++) {
        var kind = HEAP32[(argKindsPtr >> 2) + i];
        argKinds.push(kind);
        paramAbi.push(paramAbiPtr ? HEAP32[(paramAbiPtr >> 2) + i] : ABI_SCALAR);
        params.push(kindToTypeName(kind));
      }

      var results = resultKind === KIND_NONE ? [] : [kindToTypeName(resultKind)];

      host.registerImport('fonline.api', importName, params, results, function () {
        var argValuesPtr = 0;
        var resultValuePtr = 0;
        var temporaryPointers = [];
        var outputCopies = [];

        try {
          if (argKinds.length > 0) {
            argValuesPtr = engineMalloc(argKinds.length * 8);

            for (var argIndex = 0; argIndex < argKinds.length; argIndex++) {
              var abi = paramAbi[argIndex];

              if (abi === ABI_UTF8_STRING_POINTER) {
                if (argIndex + 1 >= argKinds.length || paramAbi[argIndex + 1] !== ABI_UTF8_STRING_LENGTH || argKinds[argIndex] !== KIND_I32 || argKinds[argIndex + 1] !== KIND_I32) {
                  throw new Error('Invalid Web WASM engine API UTF-8 parameter ABI: fonline.api.' + importName);
                }

                var text = readUtf8(arguments[argIndex], arguments[argIndex + 1]);
                var bytes = new TextEncoder().encode(text);
                var tempPtr = 0;

                if (bytes.length > 0) {
                  tempPtr = engineMalloc(bytes.length);
                  HEAPU8.set(bytes, tempPtr);
                  temporaryPointers.push(tempPtr);
                }

                writeRawScalar(KIND_I32, tempPtr, argValuesPtr + argIndex * 8);
                writeRawScalar(KIND_I32, bytes.length, argValuesPtr + (argIndex + 1) * 8);
                argIndex += 1;
              }
              else if (abi === ABI_CALLBACK_POINTER) {
                if (argIndex + 1 >= argKinds.length || paramAbi[argIndex + 1] !== ABI_CALLBACK_LENGTH || argKinds[argIndex] !== KIND_I32 || argKinds[argIndex + 1] !== KIND_I32) {
                  throw new Error('Invalid Web WASM engine API callback parameter ABI: fonline.api.' + importName);
                }

                var callbackText = readUtf8(arguments[argIndex], arguments[argIndex + 1]);
                var callbackBytes = new TextEncoder().encode(callbackText);
                var callbackTempPtr = 0;

                if (callbackBytes.length > 0) {
                  callbackTempPtr = engineMalloc(callbackBytes.length);
                  HEAPU8.set(callbackBytes, callbackTempPtr);
                  temporaryPointers.push(callbackTempPtr);
                }

                writeRawScalar(KIND_I32, callbackTempPtr, argValuesPtr + argIndex * 8);
                writeRawScalar(KIND_I32, callbackBytes.length, argValuesPtr + (argIndex + 1) * 8);
                argIndex += 1;
              }
              else if (abi === ABI_UTF8_STRING_OUTPUT_POINTER) {
                if (argIndex + 1 >= argKinds.length || paramAbi[argIndex + 1] !== ABI_UTF8_STRING_OUTPUT_LENGTH || argKinds[argIndex] !== KIND_I32 || argKinds[argIndex + 1] !== KIND_I32) {
                  throw new Error('Invalid Web WASM engine API UTF-8 output parameter ABI: fonline.api.' + importName);
                }

                var wasmPtr = Number(arguments[argIndex]) >>> 0;
                var outputLen = Number(arguments[argIndex + 1]) | 0;

                if (outputLen < 0) {
                  throw new Error('Negative Web WASM engine API UTF-8 output buffer length: fonline.api.' + importName);
                }

                var outputTempPtr = 0;
                if (outputLen > 0) {
                  outputTempPtr = engineMalloc(outputLen);
                  temporaryPointers.push(outputTempPtr);
                }

                outputCopies.push({ wasmPtr: wasmPtr, tempPtr: outputTempPtr, length: outputLen });
                writeRawScalar(KIND_I32, outputTempPtr, argValuesPtr + argIndex * 8);
                writeRawScalar(KIND_I32, outputLen, argValuesPtr + (argIndex + 1) * 8);
                argIndex += 1;
              }
              else if (abi === ABI_ARRAY_POINTER) {
                if (argIndex + 1 >= argKinds.length || paramAbi[argIndex + 1] !== ABI_ARRAY_BYTE_LENGTH || argKinds[argIndex] !== KIND_I32 || argKinds[argIndex + 1] !== KIND_I32) {
                  throw new Error('Invalid Web WASM engine API array parameter ABI: fonline.api.' + importName);
                }

                var arrayWasmPtr = Number(arguments[argIndex]) >>> 0;
                var arrayByteLen = Number(arguments[argIndex + 1]) | 0;

                if (arrayByteLen < 0) {
                  throw new Error('Negative Web WASM engine API array buffer length: fonline.api.' + importName);
                }

                var arrayTempPtr = 0;
                if (arrayByteLen > 0) {
                  var arrayMemory = getCurrentMemory();
                  if (arrayWasmPtr + arrayByteLen > arrayMemory.buffer.byteLength) {
                    throw new Error('Web WASM engine API array buffer is out of bounds: fonline.api.' + importName);
                  }

                  arrayTempPtr = engineMalloc(arrayByteLen);
                  HEAPU8.set(new Uint8Array(arrayMemory.buffer, arrayWasmPtr, arrayByteLen), arrayTempPtr);
                  temporaryPointers.push(arrayTempPtr);
                }

                writeRawScalar(KIND_I32, arrayTempPtr, argValuesPtr + argIndex * 8);
                writeRawScalar(KIND_I32, arrayByteLen, argValuesPtr + (argIndex + 1) * 8);
                argIndex += 1;
              }
              else if (abi === ABI_VALUE_POINTER) {
                if (argIndex + 1 >= argKinds.length || paramAbi[argIndex + 1] !== ABI_VALUE_BYTE_LENGTH || argKinds[argIndex] !== KIND_I32 || argKinds[argIndex + 1] !== KIND_I32) {
                  throw new Error('Invalid Web WASM engine API value parameter ABI: fonline.api.' + importName);
                }

                var valueWasmPtr = Number(arguments[argIndex]) >>> 0;
                var valueByteLen = Number(arguments[argIndex + 1]) | 0;

                if (valueByteLen < 0) {
                  throw new Error('Negative Web WASM engine API value buffer length: fonline.api.' + importName);
                }

                var valueTempPtr = 0;
                if (valueByteLen > 0) {
                  var valueMemory = getCurrentMemory();
                  if (valueWasmPtr + valueByteLen > valueMemory.buffer.byteLength) {
                    throw new Error('Web WASM engine API value buffer is out of bounds: fonline.api.' + importName);
                  }

                  valueTempPtr = engineMalloc(valueByteLen);
                  HEAPU8.set(new Uint8Array(valueMemory.buffer, valueWasmPtr, valueByteLen), valueTempPtr);
                  temporaryPointers.push(valueTempPtr);
                }

                writeRawScalar(KIND_I32, valueTempPtr, argValuesPtr + argIndex * 8);
                writeRawScalar(KIND_I32, valueByteLen, argValuesPtr + (argIndex + 1) * 8);
                argIndex += 1;
              }
              else if (abi === ABI_ARRAY_OUTPUT_POINTER) {
                if (argIndex + 1 >= argKinds.length || paramAbi[argIndex + 1] !== ABI_ARRAY_OUTPUT_BYTE_LENGTH || argKinds[argIndex] !== KIND_I32 || argKinds[argIndex + 1] !== KIND_I32) {
                  throw new Error('Invalid Web WASM engine API array output parameter ABI: fonline.api.' + importName);
                }

                var arrayOutputWasmPtr = Number(arguments[argIndex]) >>> 0;
                var arrayOutputLen = Number(arguments[argIndex + 1]) | 0;

                if (arrayOutputLen < 0) {
                  throw new Error('Negative Web WASM engine API array output buffer length: fonline.api.' + importName);
                }

                var arrayOutputTempPtr = 0;
                if (arrayOutputLen > 0) {
                  arrayOutputTempPtr = engineMalloc(arrayOutputLen);
                  temporaryPointers.push(arrayOutputTempPtr);
                }

                outputCopies.push({ wasmPtr: arrayOutputWasmPtr, tempPtr: arrayOutputTempPtr, length: arrayOutputLen, role: 'array output' });
                writeRawScalar(KIND_I32, arrayOutputTempPtr, argValuesPtr + argIndex * 8);
                writeRawScalar(KIND_I32, arrayOutputLen, argValuesPtr + (argIndex + 1) * 8);
                argIndex += 1;
              }
              else if (abi === ABI_VALUE_OUTPUT_POINTER) {
                if (argIndex + 1 >= argKinds.length || paramAbi[argIndex + 1] !== ABI_VALUE_OUTPUT_BYTE_LENGTH || argKinds[argIndex] !== KIND_I32 || argKinds[argIndex + 1] !== KIND_I32) {
                  throw new Error('Invalid Web WASM engine API value output parameter ABI: fonline.api.' + importName);
                }

                var valueOutputWasmPtr = Number(arguments[argIndex]) >>> 0;
                var valueOutputLen = Number(arguments[argIndex + 1]) | 0;

                if (valueOutputLen < 0) {
                  throw new Error('Negative Web WASM engine API value output buffer length: fonline.api.' + importName);
                }

                var valueOutputTempPtr = 0;
                if (valueOutputLen > 0) {
                  valueOutputTempPtr = engineMalloc(valueOutputLen);
                  temporaryPointers.push(valueOutputTempPtr);
                }

                outputCopies.push({ wasmPtr: valueOutputWasmPtr, tempPtr: valueOutputTempPtr, length: valueOutputLen, role: 'value output', noPartial: true });
                writeRawScalar(KIND_I32, valueOutputTempPtr, argValuesPtr + argIndex * 8);
                writeRawScalar(KIND_I32, valueOutputLen, argValuesPtr + (argIndex + 1) * 8);
                argIndex += 1;
              }
              else if (abi === ABI_DICT_POINTER) {
                if (argIndex + 1 >= argKinds.length || paramAbi[argIndex + 1] !== ABI_DICT_BYTE_LENGTH || argKinds[argIndex] !== KIND_I32 || argKinds[argIndex + 1] !== KIND_I32) {
                  throw new Error('Invalid Web WASM engine API dict parameter ABI: fonline.api.' + importName);
                }

                var dictWasmPtr = Number(arguments[argIndex]) >>> 0;
                var dictByteLen = Number(arguments[argIndex + 1]) | 0;

                if (dictByteLen < 0) {
                  throw new Error('Negative Web WASM engine API dict buffer length: fonline.api.' + importName);
                }

                var dictTempPtr = 0;
                if (dictByteLen > 0) {
                  var dictMemory = getCurrentMemory();
                  if (dictWasmPtr + dictByteLen > dictMemory.buffer.byteLength) {
                    throw new Error('Web WASM engine API dict buffer is out of bounds: fonline.api.' + importName);
                  }

                  dictTempPtr = engineMalloc(dictByteLen);
                  HEAPU8.set(new Uint8Array(dictMemory.buffer, dictWasmPtr, dictByteLen), dictTempPtr);
                  temporaryPointers.push(dictTempPtr);
                }

                writeRawScalar(KIND_I32, dictTempPtr, argValuesPtr + argIndex * 8);
                writeRawScalar(KIND_I32, dictByteLen, argValuesPtr + (argIndex + 1) * 8);
                argIndex += 1;
              }
              else if (abi === ABI_DICT_OUTPUT_POINTER) {
                if (argIndex + 1 >= argKinds.length || paramAbi[argIndex + 1] !== ABI_DICT_OUTPUT_BYTE_LENGTH || argKinds[argIndex] !== KIND_I32 || argKinds[argIndex + 1] !== KIND_I32) {
                  throw new Error('Invalid Web WASM engine API dict output parameter ABI: fonline.api.' + importName);
                }

                var dictOutputWasmPtr = Number(arguments[argIndex]) >>> 0;
                var dictOutputLen = Number(arguments[argIndex + 1]) | 0;

                if (dictOutputLen < 0) {
                  throw new Error('Negative Web WASM engine API dict output buffer length: fonline.api.' + importName);
                }

                var dictOutputTempPtr = 0;
                if (dictOutputLen > 0) {
                  dictOutputTempPtr = engineMalloc(dictOutputLen);
                  temporaryPointers.push(dictOutputTempPtr);
                }

                outputCopies.push({ wasmPtr: dictOutputWasmPtr, tempPtr: dictOutputTempPtr, length: dictOutputLen, role: 'dict output', noPartial: true });
                writeRawScalar(KIND_I32, dictOutputTempPtr, argValuesPtr + argIndex * 8);
                writeRawScalar(KIND_I32, dictOutputLen, argValuesPtr + (argIndex + 1) * 8);
                argIndex += 1;
              }
              else if (abi === ABI_MUTABLE_DICT_POINTER) {
                if (argIndex + 3 >= argKinds.length || paramAbi[argIndex + 1] !== ABI_MUTABLE_DICT_BYTE_LENGTH || paramAbi[argIndex + 2] !== ABI_MUTABLE_DICT_CAPACITY_BYTE_LENGTH || paramAbi[argIndex + 3] !== ABI_MUTABLE_DICT_REQUIRED_BYTE_LENGTH_POINTER || argKinds[argIndex] !== KIND_I32 || argKinds[argIndex + 1] !== KIND_I32 || argKinds[argIndex + 2] !== KIND_I32 || argKinds[argIndex + 3] !== KIND_I32) {
                  throw new Error('Invalid Web WASM engine API mutable dict parameter ABI: fonline.api.' + importName);
                }

                var mutableDictWasmPtr = Number(arguments[argIndex]) >>> 0;
                var mutableDictByteLen = Number(arguments[argIndex + 1]) | 0;
                var mutableDictCapacity = Number(arguments[argIndex + 2]) | 0;
                var mutableDictRequiredPtr = Number(arguments[argIndex + 3]) >>> 0;

                if (mutableDictByteLen < 0 || mutableDictCapacity < 0) {
                  throw new Error('Negative Web WASM engine API mutable dict buffer length: fonline.api.' + importName);
                }
                if (mutableDictCapacity < mutableDictByteLen) {
                  throw new Error('Web WASM engine API mutable dict capacity is smaller than input length: fonline.api.' + importName);
                }

                var mutableDictTempPtr = 0;
                var mutableDictRequiredTempPtr = engineMalloc(4);
                HEAP32[mutableDictRequiredTempPtr >> 2] = 0;
                temporaryPointers.push(mutableDictRequiredTempPtr);

                if (mutableDictCapacity > 0) {
                  var mutableDictMemory = getCurrentMemory();
                  if (mutableDictWasmPtr + mutableDictByteLen > mutableDictMemory.buffer.byteLength) {
                    throw new Error('Web WASM engine API mutable dict buffer is out of bounds: fonline.api.' + importName);
                  }

                  mutableDictTempPtr = engineMalloc(mutableDictCapacity);
                  HEAPU8.set(new Uint8Array(mutableDictMemory.buffer, mutableDictWasmPtr, mutableDictByteLen), mutableDictTempPtr);
                  temporaryPointers.push(mutableDictTempPtr);
                }

                outputCopies.push({ wasmPtr: mutableDictWasmPtr, tempPtr: mutableDictTempPtr, length: mutableDictCapacity, requiredWasmPtr: mutableDictRequiredPtr, requiredTempPtr: mutableDictRequiredTempPtr, mutableArray: true, role: 'mutable dict' });
                writeRawScalar(KIND_I32, mutableDictTempPtr, argValuesPtr + argIndex * 8);
                writeRawScalar(KIND_I32, mutableDictByteLen, argValuesPtr + (argIndex + 1) * 8);
                writeRawScalar(KIND_I32, mutableDictCapacity, argValuesPtr + (argIndex + 2) * 8);
                writeRawScalar(KIND_I32, mutableDictRequiredTempPtr, argValuesPtr + (argIndex + 3) * 8);
                argIndex += 3;
              }
              else if (abi === ABI_MUTABLE_ARRAY_POINTER) {
                if (argIndex + 3 >= argKinds.length || paramAbi[argIndex + 1] !== ABI_MUTABLE_ARRAY_BYTE_LENGTH || paramAbi[argIndex + 2] !== ABI_MUTABLE_ARRAY_CAPACITY_BYTE_LENGTH || paramAbi[argIndex + 3] !== ABI_MUTABLE_ARRAY_REQUIRED_BYTE_LENGTH_POINTER || argKinds[argIndex] !== KIND_I32 || argKinds[argIndex + 1] !== KIND_I32 || argKinds[argIndex + 2] !== KIND_I32 || argKinds[argIndex + 3] !== KIND_I32) {
                  throw new Error('Invalid Web WASM engine API mutable array parameter ABI: fonline.api.' + importName);
                }

                var mutableArrayWasmPtr = Number(arguments[argIndex]) >>> 0;
                var mutableArrayByteLen = Number(arguments[argIndex + 1]) | 0;
                var mutableArrayCapacity = Number(arguments[argIndex + 2]) | 0;
                var mutableArrayRequiredPtr = Number(arguments[argIndex + 3]) >>> 0;

                if (mutableArrayByteLen < 0 || mutableArrayCapacity < 0) {
                  throw new Error('Negative Web WASM engine API mutable array buffer length: fonline.api.' + importName);
                }
                if (mutableArrayCapacity < mutableArrayByteLen) {
                  throw new Error('Web WASM engine API mutable array capacity is smaller than input length: fonline.api.' + importName);
                }

                var mutableArrayTempPtr = 0;
                var mutableArrayRequiredTempPtr = engineMalloc(4);
                HEAP32[mutableArrayRequiredTempPtr >> 2] = 0;
                temporaryPointers.push(mutableArrayRequiredTempPtr);

                if (mutableArrayCapacity > 0) {
                  var mutableArrayMemory = getCurrentMemory();
                  if (mutableArrayWasmPtr + mutableArrayByteLen > mutableArrayMemory.buffer.byteLength) {
                    throw new Error('Web WASM engine API mutable array buffer is out of bounds: fonline.api.' + importName);
                  }

                  mutableArrayTempPtr = engineMalloc(mutableArrayCapacity);
                  HEAPU8.set(new Uint8Array(mutableArrayMemory.buffer, mutableArrayWasmPtr, mutableArrayByteLen), mutableArrayTempPtr);
                  temporaryPointers.push(mutableArrayTempPtr);
                }

                outputCopies.push({ wasmPtr: mutableArrayWasmPtr, tempPtr: mutableArrayTempPtr, length: mutableArrayCapacity, requiredWasmPtr: mutableArrayRequiredPtr, requiredTempPtr: mutableArrayRequiredTempPtr, mutableArray: true });
                writeRawScalar(KIND_I32, mutableArrayTempPtr, argValuesPtr + argIndex * 8);
                writeRawScalar(KIND_I32, mutableArrayByteLen, argValuesPtr + (argIndex + 1) * 8);
                writeRawScalar(KIND_I32, mutableArrayCapacity, argValuesPtr + (argIndex + 2) * 8);
                writeRawScalar(KIND_I32, mutableArrayRequiredTempPtr, argValuesPtr + (argIndex + 3) * 8);
                argIndex += 3;
              }
              else if (abi === ABI_MUTABLE_VALUE_POINTER) {
                if (argIndex + 1 >= argKinds.length || paramAbi[argIndex + 1] !== ABI_MUTABLE_VALUE_LENGTH || argKinds[argIndex] !== KIND_I32 || argKinds[argIndex + 1] !== KIND_I32) {
                  throw new Error('Invalid Web WASM engine API mutable value parameter ABI: fonline.api.' + importName);
                }

                var mutableWasmPtr = Number(arguments[argIndex]) >>> 0;
                var mutableLen = Number(arguments[argIndex + 1]) | 0;

                if (mutableLen < 0) {
                  throw new Error('Negative Web WASM engine API mutable value buffer length: fonline.api.' + importName);
                }

                var mutableTempPtr = 0;
                if (mutableLen > 0) {
                  var memory = getCurrentMemory();
                  if (mutableWasmPtr + mutableLen > memory.buffer.byteLength) {
                    throw new Error('Web WASM engine API mutable value buffer is out of bounds: fonline.api.' + importName);
                  }

                  mutableTempPtr = engineMalloc(mutableLen);
                  HEAPU8.set(new Uint8Array(memory.buffer, mutableWasmPtr, mutableLen), mutableTempPtr);
                  temporaryPointers.push(mutableTempPtr);
                }

                outputCopies.push({ wasmPtr: mutableWasmPtr, tempPtr: mutableTempPtr, length: mutableLen, mutable: true });
                writeRawScalar(KIND_I32, mutableTempPtr, argValuesPtr + argIndex * 8);
                writeRawScalar(KIND_I32, mutableLen, argValuesPtr + (argIndex + 1) * 8);
                argIndex += 1;
              }
              else if (abi === ABI_MUTABLE_UTF8_STRING_POINTER) {
                if (argIndex + 3 >= argKinds.length || paramAbi[argIndex + 1] !== ABI_MUTABLE_UTF8_STRING_BYTE_LENGTH || paramAbi[argIndex + 2] !== ABI_MUTABLE_UTF8_STRING_CAPACITY_BYTE_LENGTH || paramAbi[argIndex + 3] !== ABI_MUTABLE_UTF8_STRING_REQUIRED_BYTE_LENGTH_POINTER || argKinds[argIndex] !== KIND_I32 || argKinds[argIndex + 1] !== KIND_I32 || argKinds[argIndex + 2] !== KIND_I32 || argKinds[argIndex + 3] !== KIND_I32) {
                  throw new Error('Invalid Web WASM engine API mutable UTF-8 string parameter ABI: fonline.api.' + importName);
                }

                var mutableStringWasmPtr = Number(arguments[argIndex]) >>> 0;
                var mutableStringByteLen = Number(arguments[argIndex + 1]) | 0;
                var mutableStringCapacity = Number(arguments[argIndex + 2]) | 0;
                var mutableStringRequiredPtr = Number(arguments[argIndex + 3]) >>> 0;

                if (mutableStringByteLen < 0 || mutableStringCapacity < 0) {
                  throw new Error('Negative Web WASM engine API mutable UTF-8 string buffer length: fonline.api.' + importName);
                }
                if (mutableStringCapacity < mutableStringByteLen) {
                  throw new Error('Web WASM engine API mutable UTF-8 string capacity is smaller than input length: fonline.api.' + importName);
                }

                var mutableStringTempPtr = 0;
                var mutableStringRequiredTempPtr = engineMalloc(4);
                HEAP32[mutableStringRequiredTempPtr >> 2] = 0;
                temporaryPointers.push(mutableStringRequiredTempPtr);

                if (mutableStringCapacity > 0) {
                  var mutableStringMemory = getCurrentMemory();
                  if (mutableStringWasmPtr + mutableStringByteLen > mutableStringMemory.buffer.byteLength) {
                    throw new Error('Web WASM engine API mutable UTF-8 string buffer is out of bounds: fonline.api.' + importName);
                  }

                  mutableStringTempPtr = engineMalloc(mutableStringCapacity);
                  HEAPU8.set(new Uint8Array(mutableStringMemory.buffer, mutableStringWasmPtr, mutableStringByteLen), mutableStringTempPtr);
                  temporaryPointers.push(mutableStringTempPtr);
                }

                outputCopies.push({ wasmPtr: mutableStringWasmPtr, tempPtr: mutableStringTempPtr, length: mutableStringCapacity, requiredWasmPtr: mutableStringRequiredPtr, requiredTempPtr: mutableStringRequiredTempPtr, mutableArray: true, role: 'mutable UTF-8 string' });
                writeRawScalar(KIND_I32, mutableStringTempPtr, argValuesPtr + argIndex * 8);
                writeRawScalar(KIND_I32, mutableStringByteLen, argValuesPtr + (argIndex + 1) * 8);
                writeRawScalar(KIND_I32, mutableStringCapacity, argValuesPtr + (argIndex + 2) * 8);
                writeRawScalar(KIND_I32, mutableStringRequiredTempPtr, argValuesPtr + (argIndex + 3) * 8);
                argIndex += 3;
              }
              else if (abi === ABI_UTF8_STRING_LENGTH) {
                throw new Error('Invalid standalone Web WASM engine API UTF-8 length parameter: fonline.api.' + importName);
              }
              else if (abi === ABI_CALLBACK_LENGTH) {
                throw new Error('Invalid standalone Web WASM engine API callback length parameter: fonline.api.' + importName);
              }
              else if (abi === ABI_UTF8_STRING_OUTPUT_LENGTH) {
                throw new Error('Invalid standalone Web WASM engine API UTF-8 output length parameter: fonline.api.' + importName);
              }
              else if (abi === ABI_MUTABLE_VALUE_LENGTH) {
                throw new Error('Invalid standalone Web WASM engine API mutable value length parameter: fonline.api.' + importName);
              }
              else if (abi === ABI_MUTABLE_UTF8_STRING_BYTE_LENGTH) {
                throw new Error('Invalid standalone Web WASM engine API mutable UTF-8 string byte length parameter: fonline.api.' + importName);
              }
              else if (abi === ABI_MUTABLE_UTF8_STRING_CAPACITY_BYTE_LENGTH) {
                throw new Error('Invalid standalone Web WASM engine API mutable UTF-8 string capacity byte length parameter: fonline.api.' + importName);
              }
              else if (abi === ABI_MUTABLE_UTF8_STRING_REQUIRED_BYTE_LENGTH_POINTER) {
                throw new Error('Invalid standalone Web WASM engine API mutable UTF-8 string required byte length parameter: fonline.api.' + importName);
              }
              else if (abi === ABI_VALUE_BYTE_LENGTH) {
                throw new Error('Invalid standalone Web WASM engine API value byte length parameter: fonline.api.' + importName);
              }
              else if (abi === ABI_VALUE_OUTPUT_BYTE_LENGTH) {
                throw new Error('Invalid standalone Web WASM engine API value output byte length parameter: fonline.api.' + importName);
              }
              else if (abi === ABI_ARRAY_BYTE_LENGTH) {
                throw new Error('Invalid standalone Web WASM engine API array byte length parameter: fonline.api.' + importName);
              }
              else if (abi === ABI_ARRAY_OUTPUT_BYTE_LENGTH) {
                throw new Error('Invalid standalone Web WASM engine API array output byte length parameter: fonline.api.' + importName);
              }
              else if (abi === ABI_MUTABLE_ARRAY_BYTE_LENGTH) {
                throw new Error('Invalid standalone Web WASM engine API mutable array byte length parameter: fonline.api.' + importName);
              }
              else if (abi === ABI_MUTABLE_ARRAY_CAPACITY_BYTE_LENGTH) {
                throw new Error('Invalid standalone Web WASM engine API mutable array capacity byte length parameter: fonline.api.' + importName);
              }
              else if (abi === ABI_MUTABLE_ARRAY_REQUIRED_BYTE_LENGTH_POINTER) {
                throw new Error('Invalid standalone Web WASM engine API mutable array required byte length parameter: fonline.api.' + importName);
              }
              else if (abi === ABI_DICT_BYTE_LENGTH) {
                throw new Error('Invalid standalone Web WASM engine API dict byte length parameter: fonline.api.' + importName);
              }
              else if (abi === ABI_DICT_OUTPUT_BYTE_LENGTH) {
                throw new Error('Invalid standalone Web WASM engine API dict output byte length parameter: fonline.api.' + importName);
              }
              else if (abi === ABI_MUTABLE_DICT_BYTE_LENGTH) {
                throw new Error('Invalid standalone Web WASM engine API mutable dict byte length parameter: fonline.api.' + importName);
              }
              else if (abi === ABI_MUTABLE_DICT_CAPACITY_BYTE_LENGTH) {
                throw new Error('Invalid standalone Web WASM engine API mutable dict capacity byte length parameter: fonline.api.' + importName);
              }
              else if (abi === ABI_MUTABLE_DICT_REQUIRED_BYTE_LENGTH_POINTER) {
                throw new Error('Invalid standalone Web WASM engine API mutable dict required byte length parameter: fonline.api.' + importName);
              }
              else {
                writeRawScalar(argKinds[argIndex], arguments[argIndex], argValuesPtr + argIndex * 8);
              }
            }
          }

          if (resultKind !== KIND_NONE) {
            resultValuePtr = engineMalloc(8);
          }

          var ok = engineDynCall('iiiiii', callbackPtr, [backendPtr, methodIndex, argKinds.length, argValuesPtr, resultValuePtr]);

          if (!ok) {
            throw new Error('Web WASM engine API call failed: fonline.api.' + importName);
          }

          var result = resultKind !== KIND_NONE ? readRawScalar(resultKind, resultValuePtr) : undefined;

          for (var outputIndex = 0; outputIndex < outputCopies.length; outputIndex++) {
            if (outputCopies[outputIndex].mutableArray) {
              copyApiMutableArrayBuffer(outputCopies[outputIndex], importName);
            }
            else if (outputCopies[outputIndex].mutable) {
              copyApiMutableBuffer(outputCopies[outputIndex], importName);
            }
            else {
              copyApiOutputBuffer(outputCopies[outputIndex], result, importName);
            }
          }

          return result;
        }
        finally {
          for (var tempIndex = 0; tempIndex < temporaryPointers.length; tempIndex++) {
            engineFree(temporaryPointers[tempIndex]);
          }
          if (resultValuePtr) {
            engineFree(resultValuePtr);
          }
          if (argValuesPtr) {
            engineFree(argValuesPtr);
          }
        }
      });
    },

    call: function (moduleNamePtr, exportNamePtr, argc, argKindsPtr, argAbiPtr, argValuesPtr, resultKind, resultValuePtr, contextValuesPtr, resultBuffer) {
      var previousContext = host.currentContext;
      var temporaryModulePointers = [];
      var mutableModuleBuffers = [];

      try {
        var moduleName = UTF8ToString(moduleNamePtr);
        var exportName = UTF8ToString(exportNamePtr);
        var module = host.modules[moduleName];

        if (!module || !module.instance) {
          throw new Error('Web WASM module is not loaded: ' + moduleName);
        }

        var func = module.instance.exports[exportName];
        if (typeof func !== 'function') {
          throw new Error('Web WASM export is not a function: ' + moduleName + '::' + exportName);
        }

        var args = [];
        for (var i = 0; i < argc; i++) {
          var kind = HEAP32[(argKindsPtr >> 2) + i];
          var abi = argAbiPtr ? HEAP32[(argAbiPtr >> 2) + i] : ABI_SCALAR;
          var valuePtr = argValuesPtr + i * 8;

          if (abi === ABI_UTF8_STRING_POINTER) {
            if (i + 1 >= argc || (argAbiPtr ? HEAP32[(argAbiPtr >> 2) + i + 1] : ABI_SCALAR) !== ABI_UTF8_STRING_LENGTH || kind !== KIND_I32 || HEAP32[(argKindsPtr >> 2) + i + 1] !== KIND_I32) {
              throw new Error('Invalid Web WASM export UTF-8 parameter ABI: ' + moduleName + '::' + exportName);
            }

            var enginePtr = Number(readScalar(kind, valuePtr)) >>> 0;
            var engineLen = Number(readScalar(HEAP32[(argKindsPtr >> 2) + i + 1], argValuesPtr + (i + 1) * 8)) | 0;
            var modulePtr = copyEngineBufferToModule(module, enginePtr, engineLen, temporaryModulePointers, moduleName + '::' + exportName);

            args.push(modulePtr);
            args.push(engineLen);
            i += 1;
            continue;
          }
          if (abi === ABI_UTF8_STRING_LENGTH) {
            throw new Error('Invalid standalone Web WASM export UTF-8 length parameter: ' + moduleName + '::' + exportName);
          }
          if (abi === ABI_CALLBACK_POINTER) {
            if (i + 1 >= argc || (argAbiPtr ? HEAP32[(argAbiPtr >> 2) + i + 1] : ABI_SCALAR) !== ABI_CALLBACK_LENGTH || kind !== KIND_I32 || HEAP32[(argKindsPtr >> 2) + i + 1] !== KIND_I32) {
              throw new Error('Invalid Web WASM export callback parameter ABI: ' + moduleName + '::' + exportName);
            }

            var callbackEnginePtr = Number(readScalar(kind, valuePtr)) >>> 0;
            var callbackEngineLen = Number(readScalar(HEAP32[(argKindsPtr >> 2) + i + 1], argValuesPtr + (i + 1) * 8)) | 0;
            var callbackModulePtr = copyEngineBufferToModule(module, callbackEnginePtr, callbackEngineLen, temporaryModulePointers, moduleName + '::' + exportName + ' callback argument');

            args.push(callbackModulePtr);
            args.push(callbackEngineLen);
            i += 1;
            continue;
          }
          if (abi === ABI_CALLBACK_LENGTH) {
            throw new Error('Invalid standalone Web WASM export callback length parameter: ' + moduleName + '::' + exportName);
          }
          if (abi === ABI_VALUE_POINTER) {
            if (i + 1 >= argc || (argAbiPtr ? HEAP32[(argAbiPtr >> 2) + i + 1] : ABI_SCALAR) !== ABI_VALUE_BYTE_LENGTH || kind !== KIND_I32 || HEAP32[(argKindsPtr >> 2) + i + 1] !== KIND_I32) {
              throw new Error('Invalid Web WASM export value parameter ABI: ' + moduleName + '::' + exportName);
            }

            var valueEnginePtr = Number(readScalar(kind, valuePtr)) >>> 0;
            var valueEngineLen = Number(readScalar(HEAP32[(argKindsPtr >> 2) + i + 1], argValuesPtr + (i + 1) * 8)) | 0;
            var valueModulePtr = copyEngineBufferToModule(module, valueEnginePtr, valueEngineLen, temporaryModulePointers, moduleName + '::' + exportName + ' value argument');

            args.push(valueModulePtr);
            args.push(valueEngineLen);
            i += 1;
            continue;
          }
          if (abi === ABI_VALUE_BYTE_LENGTH) {
            throw new Error('Invalid standalone Web WASM export value byte length parameter: ' + moduleName + '::' + exportName);
          }
          if (abi === ABI_VALUE_OUTPUT_POINTER) {
            throw new Error('Invalid Web WASM export value output parameter ABI: ' + moduleName + '::' + exportName);
          }
          if (abi === ABI_VALUE_OUTPUT_BYTE_LENGTH) {
            throw new Error('Invalid standalone Web WASM export value output byte length parameter: ' + moduleName + '::' + exportName);
          }
          if (abi === ABI_MUTABLE_VALUE_POINTER) {
            if (i + 1 >= argc || (argAbiPtr ? HEAP32[(argAbiPtr >> 2) + i + 1] : ABI_SCALAR) !== ABI_MUTABLE_VALUE_LENGTH || kind !== KIND_I32 || HEAP32[(argKindsPtr >> 2) + i + 1] !== KIND_I32) {
              throw new Error('Invalid Web WASM export mutable value parameter ABI: ' + moduleName + '::' + exportName);
            }

            var mutableEnginePtr = Number(readScalar(kind, valuePtr)) >>> 0;
            var mutableEngineLen = Number(readScalar(HEAP32[(argKindsPtr >> 2) + i + 1], argValuesPtr + (i + 1) * 8)) | 0;
            var mutableRole = moduleName + '::' + exportName + ' mutable value argument';
            var mutableModulePtr = copyEngineBufferToModule(module, mutableEnginePtr, mutableEngineLen, temporaryModulePointers, mutableRole);

            mutableModuleBuffers.push({ module: module, modulePtr: mutableModulePtr, enginePtr: mutableEnginePtr, byteLen: mutableEngineLen, role: mutableRole });
            args.push(mutableModulePtr);
            args.push(mutableEngineLen);
            i += 1;
            continue;
          }
          if (abi === ABI_MUTABLE_VALUE_LENGTH) {
            throw new Error('Invalid standalone Web WASM export mutable value length parameter: ' + moduleName + '::' + exportName);
          }
          if (abi === ABI_MUTABLE_UTF8_STRING_POINTER) {
            if (i + 3 >= argc || (argAbiPtr ? HEAP32[(argAbiPtr >> 2) + i + 1] : ABI_SCALAR) !== ABI_MUTABLE_UTF8_STRING_BYTE_LENGTH || (argAbiPtr ? HEAP32[(argAbiPtr >> 2) + i + 2] : ABI_SCALAR) !== ABI_MUTABLE_UTF8_STRING_CAPACITY_BYTE_LENGTH || (argAbiPtr ? HEAP32[(argAbiPtr >> 2) + i + 3] : ABI_SCALAR) !== ABI_MUTABLE_UTF8_STRING_REQUIRED_BYTE_LENGTH_POINTER || kind !== KIND_I32 || HEAP32[(argKindsPtr >> 2) + i + 1] !== KIND_I32 || HEAP32[(argKindsPtr >> 2) + i + 2] !== KIND_I32 || HEAP32[(argKindsPtr >> 2) + i + 3] !== KIND_I32) {
              throw new Error('Invalid Web WASM export mutable UTF-8 string parameter ABI: ' + moduleName + '::' + exportName);
            }

            var mutableTextEnginePtr = Number(readScalar(kind, valuePtr)) >>> 0;
            var mutableTextEngineLen = Number(readScalar(HEAP32[(argKindsPtr >> 2) + i + 1], argValuesPtr + (i + 1) * 8)) | 0;
            var mutableTextCapacity = Number(readScalar(HEAP32[(argKindsPtr >> 2) + i + 2], argValuesPtr + (i + 2) * 8)) | 0;
            var mutableTextRequiredEnginePtr = Number(readScalar(HEAP32[(argKindsPtr >> 2) + i + 3], argValuesPtr + (i + 3) * 8)) >>> 0;
            var mutableTextRole = moduleName + '::' + exportName + ' mutable UTF-8 string argument';

            if (mutableTextEngineLen < 0 || mutableTextCapacity < 0) {
              throw new Error('Negative Web WASM export mutable UTF-8 string buffer length: ' + mutableTextRole);
            }
            if (mutableTextCapacity < mutableTextEngineLen) {
              throw new Error('Web WASM export mutable UTF-8 string capacity is smaller than input length: ' + mutableTextRole);
            }
            if (mutableTextRequiredEnginePtr + 4 > HEAPU8.length) {
              throw new Error('Web WASM export mutable UTF-8 string required length pointer is out of bounds: ' + mutableTextRole);
            }

            var mutableTextModulePtr = copyEngineBufferToModuleWithCapacity(module, mutableTextEnginePtr, mutableTextEngineLen, mutableTextCapacity, temporaryModulePointers, mutableTextRole);
            var mutableTextRequiredModulePtr = copyEngineBufferToModule(module, mutableTextRequiredEnginePtr, 4, temporaryModulePointers, mutableTextRole + ' required length');

            mutableModuleBuffers.push({
              module: module,
              modulePtr: mutableTextModulePtr,
              enginePtr: mutableTextEnginePtr,
              capacityByteLen: mutableTextCapacity,
              requiredModulePtr: mutableTextRequiredModulePtr,
              requiredEnginePtr: mutableTextRequiredEnginePtr,
              role: mutableTextRole
            });
            args.push(mutableTextModulePtr);
            args.push(mutableTextEngineLen);
            args.push(mutableTextCapacity);
            args.push(mutableTextRequiredModulePtr);
            i += 3;
            continue;
          }
          if (abi === ABI_MUTABLE_UTF8_STRING_BYTE_LENGTH) {
            throw new Error('Invalid standalone Web WASM export mutable UTF-8 string byte length parameter: ' + moduleName + '::' + exportName);
          }
          if (abi === ABI_MUTABLE_UTF8_STRING_CAPACITY_BYTE_LENGTH) {
            throw new Error('Invalid standalone Web WASM export mutable UTF-8 string capacity byte length parameter: ' + moduleName + '::' + exportName);
          }
          if (abi === ABI_MUTABLE_UTF8_STRING_REQUIRED_BYTE_LENGTH_POINTER) {
            throw new Error('Invalid standalone Web WASM export mutable UTF-8 string required byte length parameter: ' + moduleName + '::' + exportName);
          }
          if (abi === ABI_MUTABLE_ARRAY_POINTER) {
            if (i + 3 >= argc || (argAbiPtr ? HEAP32[(argAbiPtr >> 2) + i + 1] : ABI_SCALAR) !== ABI_MUTABLE_ARRAY_BYTE_LENGTH || (argAbiPtr ? HEAP32[(argAbiPtr >> 2) + i + 2] : ABI_SCALAR) !== ABI_MUTABLE_ARRAY_CAPACITY_BYTE_LENGTH || (argAbiPtr ? HEAP32[(argAbiPtr >> 2) + i + 3] : ABI_SCALAR) !== ABI_MUTABLE_ARRAY_REQUIRED_BYTE_LENGTH_POINTER || kind !== KIND_I32 || HEAP32[(argKindsPtr >> 2) + i + 1] !== KIND_I32 || HEAP32[(argKindsPtr >> 2) + i + 2] !== KIND_I32 || HEAP32[(argKindsPtr >> 2) + i + 3] !== KIND_I32) {
              throw new Error('Invalid Web WASM export mutable array parameter ABI: ' + moduleName + '::' + exportName);
            }

            var mutableArrayEnginePtr = Number(readScalar(kind, valuePtr)) >>> 0;
            var mutableArrayEngineLen = Number(readScalar(HEAP32[(argKindsPtr >> 2) + i + 1], argValuesPtr + (i + 1) * 8)) | 0;
            var mutableArrayCapacity = Number(readScalar(HEAP32[(argKindsPtr >> 2) + i + 2], argValuesPtr + (i + 2) * 8)) | 0;
            var mutableArrayRequiredEnginePtr = Number(readScalar(HEAP32[(argKindsPtr >> 2) + i + 3], argValuesPtr + (i + 3) * 8)) >>> 0;
            var mutableArrayRole = moduleName + '::' + exportName + ' mutable array argument';

            if (mutableArrayEngineLen < 0 || mutableArrayCapacity < 0) {
              throw new Error('Negative Web WASM export mutable array buffer length: ' + mutableArrayRole);
            }
            if (mutableArrayCapacity < mutableArrayEngineLen) {
              throw new Error('Web WASM export mutable array capacity is smaller than input length: ' + mutableArrayRole);
            }
            if (mutableArrayRequiredEnginePtr + 4 > HEAPU8.length) {
              throw new Error('Web WASM export mutable array required length pointer is out of bounds: ' + mutableArrayRole);
            }

            var mutableArrayModulePtr = copyEngineBufferToModuleWithCapacity(module, mutableArrayEnginePtr, mutableArrayEngineLen, mutableArrayCapacity, temporaryModulePointers, mutableArrayRole);
            var mutableArrayRequiredModulePtr = copyEngineBufferToModule(module, mutableArrayRequiredEnginePtr, 4, temporaryModulePointers, mutableArrayRole + ' required length');

            mutableModuleBuffers.push({
              module: module,
              modulePtr: mutableArrayModulePtr,
              enginePtr: mutableArrayEnginePtr,
              capacityByteLen: mutableArrayCapacity,
              requiredModulePtr: mutableArrayRequiredModulePtr,
              requiredEnginePtr: mutableArrayRequiredEnginePtr,
              role: mutableArrayRole
            });
            args.push(mutableArrayModulePtr);
            args.push(mutableArrayEngineLen);
            args.push(mutableArrayCapacity);
            args.push(mutableArrayRequiredModulePtr);
            i += 3;
            continue;
          }
          if (abi === ABI_MUTABLE_ARRAY_BYTE_LENGTH) {
            throw new Error('Invalid standalone Web WASM export mutable array byte length parameter: ' + moduleName + '::' + exportName);
          }
          if (abi === ABI_MUTABLE_ARRAY_CAPACITY_BYTE_LENGTH) {
            throw new Error('Invalid standalone Web WASM export mutable array capacity byte length parameter: ' + moduleName + '::' + exportName);
          }
          if (abi === ABI_MUTABLE_ARRAY_REQUIRED_BYTE_LENGTH_POINTER) {
            throw new Error('Invalid standalone Web WASM export mutable array required byte length parameter: ' + moduleName + '::' + exportName);
          }
          if (abi === ABI_ARRAY_POINTER) {
            if (i + 1 >= argc || (argAbiPtr ? HEAP32[(argAbiPtr >> 2) + i + 1] : ABI_SCALAR) !== ABI_ARRAY_BYTE_LENGTH || kind !== KIND_I32 || HEAP32[(argKindsPtr >> 2) + i + 1] !== KIND_I32) {
              throw new Error('Invalid Web WASM export array parameter ABI: ' + moduleName + '::' + exportName);
            }

            var arrayEnginePtr = Number(readScalar(kind, valuePtr)) >>> 0;
            var arrayEngineLen = Number(readScalar(HEAP32[(argKindsPtr >> 2) + i + 1], argValuesPtr + (i + 1) * 8)) | 0;
            var arrayModulePtr = copyEngineBufferToModule(module, arrayEnginePtr, arrayEngineLen, temporaryModulePointers, moduleName + '::' + exportName + ' array argument');

            args.push(arrayModulePtr);
            args.push(arrayEngineLen);
            i += 1;
            continue;
          }
          if (abi === ABI_ARRAY_BYTE_LENGTH) {
            throw new Error('Invalid standalone Web WASM export array byte length parameter: ' + moduleName + '::' + exportName);
          }
          if (abi === ABI_MUTABLE_DICT_POINTER) {
            if (i + 3 >= argc || (argAbiPtr ? HEAP32[(argAbiPtr >> 2) + i + 1] : ABI_SCALAR) !== ABI_MUTABLE_DICT_BYTE_LENGTH || (argAbiPtr ? HEAP32[(argAbiPtr >> 2) + i + 2] : ABI_SCALAR) !== ABI_MUTABLE_DICT_CAPACITY_BYTE_LENGTH || (argAbiPtr ? HEAP32[(argAbiPtr >> 2) + i + 3] : ABI_SCALAR) !== ABI_MUTABLE_DICT_REQUIRED_BYTE_LENGTH_POINTER || kind !== KIND_I32 || HEAP32[(argKindsPtr >> 2) + i + 1] !== KIND_I32 || HEAP32[(argKindsPtr >> 2) + i + 2] !== KIND_I32 || HEAP32[(argKindsPtr >> 2) + i + 3] !== KIND_I32) {
              throw new Error('Invalid Web WASM export mutable dict parameter ABI: ' + moduleName + '::' + exportName);
            }

            var mutableDictEnginePtr = Number(readScalar(kind, valuePtr)) >>> 0;
            var mutableDictEngineLen = Number(readScalar(HEAP32[(argKindsPtr >> 2) + i + 1], argValuesPtr + (i + 1) * 8)) | 0;
            var mutableDictCapacity = Number(readScalar(HEAP32[(argKindsPtr >> 2) + i + 2], argValuesPtr + (i + 2) * 8)) | 0;
            var mutableDictRequiredEnginePtr = Number(readScalar(HEAP32[(argKindsPtr >> 2) + i + 3], argValuesPtr + (i + 3) * 8)) >>> 0;
            var mutableDictRole = moduleName + '::' + exportName + ' mutable dict argument';

            if (mutableDictEngineLen < 0 || mutableDictCapacity < 0) {
              throw new Error('Negative Web WASM export mutable dict buffer length: ' + mutableDictRole);
            }
            if (mutableDictCapacity < mutableDictEngineLen) {
              throw new Error('Web WASM export mutable dict capacity is smaller than input length: ' + mutableDictRole);
            }
            if (mutableDictRequiredEnginePtr + 4 > HEAPU8.length) {
              throw new Error('Web WASM export mutable dict required length pointer is out of bounds: ' + mutableDictRole);
            }

            var mutableDictModulePtr = copyEngineBufferToModuleWithCapacity(module, mutableDictEnginePtr, mutableDictEngineLen, mutableDictCapacity, temporaryModulePointers, mutableDictRole);
            var mutableDictRequiredModulePtr = copyEngineBufferToModule(module, mutableDictRequiredEnginePtr, 4, temporaryModulePointers, mutableDictRole + ' required length');

            mutableModuleBuffers.push({
              module: module,
              modulePtr: mutableDictModulePtr,
              enginePtr: mutableDictEnginePtr,
              capacityByteLen: mutableDictCapacity,
              requiredModulePtr: mutableDictRequiredModulePtr,
              requiredEnginePtr: mutableDictRequiredEnginePtr,
              role: mutableDictRole
            });
            args.push(mutableDictModulePtr);
            args.push(mutableDictEngineLen);
            args.push(mutableDictCapacity);
            args.push(mutableDictRequiredModulePtr);
            i += 3;
            continue;
          }
          if (abi === ABI_MUTABLE_DICT_BYTE_LENGTH) {
            throw new Error('Invalid standalone Web WASM export mutable dict byte length parameter: ' + moduleName + '::' + exportName);
          }
          if (abi === ABI_MUTABLE_DICT_CAPACITY_BYTE_LENGTH) {
            throw new Error('Invalid standalone Web WASM export mutable dict capacity byte length parameter: ' + moduleName + '::' + exportName);
          }
          if (abi === ABI_MUTABLE_DICT_REQUIRED_BYTE_LENGTH_POINTER) {
            throw new Error('Invalid standalone Web WASM export mutable dict required byte length parameter: ' + moduleName + '::' + exportName);
          }
          if (abi === ABI_DICT_POINTER) {
            if (i + 1 >= argc || (argAbiPtr ? HEAP32[(argAbiPtr >> 2) + i + 1] : ABI_SCALAR) !== ABI_DICT_BYTE_LENGTH || kind !== KIND_I32 || HEAP32[(argKindsPtr >> 2) + i + 1] !== KIND_I32) {
              throw new Error('Invalid Web WASM export dict parameter ABI: ' + moduleName + '::' + exportName);
            }

            var dictEnginePtr = Number(readScalar(kind, valuePtr)) >>> 0;
            var dictEngineLen = Number(readScalar(HEAP32[(argKindsPtr >> 2) + i + 1], argValuesPtr + (i + 1) * 8)) | 0;
            var dictModulePtr = copyEngineBufferToModule(module, dictEnginePtr, dictEngineLen, temporaryModulePointers, moduleName + '::' + exportName + ' dict argument');

            args.push(dictModulePtr);
            args.push(dictEngineLen);
            i += 1;
            continue;
          }
          if (abi === ABI_DICT_BYTE_LENGTH) {
            throw new Error('Invalid standalone Web WASM export dict byte length parameter: ' + moduleName + '::' + exportName);
          }

          args.push(readScalar(kind, valuePtr));
        }

        host.currentContext = readRuntimeContext(contextValuesPtr);
        host.currentContext.module = module;
        var result = func.apply(null, args);

        for (var copybackIndex = 0; copybackIndex < mutableModuleBuffers.length; copybackIndex++) {
          var copyback = mutableModuleBuffers[copybackIndex];
          if (copyback.requiredModulePtr !== undefined) {
            copyModuleBufferToEngineFixed(copyback.module, copyback.requiredModulePtr, copyback.requiredEnginePtr, 4, copyback.role + ' required length');
            var requiredByteLen = HEAP32[copyback.requiredEnginePtr >> 2] >>> 0;

            if (requiredByteLen <= copyback.capacityByteLen) {
              copyModuleBufferToEngineFixed(copyback.module, copyback.modulePtr, copyback.enginePtr, requiredByteLen, copyback.role);
            }
          }
          else {
            copyModuleBufferToEngineFixed(copyback.module, copyback.modulePtr, copyback.enginePtr, copyback.byteLen, copyback.role);
          }
        }

        if (resultKind !== KIND_NONE) {
          if (resultBuffer) {
            if (resultKind !== KIND_I64) {
              throw new Error('Invalid Web WASM export buffer result kind: ' + moduleName + '::' + exportName);
            }

            result = copyModuleBufferToEngine(module, result, moduleName + '::' + exportName + ' return');
          }

          writeScalar(resultKind, resultValuePtr, result);
        }

        return true;
      }
      catch (error) {
        host.lastError = error && error.stack ? String(error.stack) : String(error);
        if (console && console.error) {
          console.error(host.lastError);
        }
        return false;
      }
      finally {
        freeModulePointers(temporaryModulePointers);
        host.currentContext = previousContext;
      }
    }
  };

  function readRuntimeContext(contextValuesPtr) {
    if (!contextValuesPtr) {
      return {
        side: 0,
        frameTimeMs: 0n,
        frameDeltaTimeMs: 0n,
        timeSynchronized: 0,
        synchronizedTimeMs: 0n,
        scriptSysPtr: 0,
        callbackRetainPtr: 0,
        callbackReleasePtr: 0
      };
    }

    var base = contextValuesPtr >> 3;
    return {
      side: Number(HEAP64[base]),
      frameTimeMs: HEAP64[base + 1],
      frameDeltaTimeMs: HEAP64[base + 2],
      timeSynchronized: Number(HEAP64[base + 3]),
      synchronizedTimeMs: HEAP64[base + 4],
      scriptSysPtr: Number(HEAP64[base + 5]) >>> 0,
      callbackRetainPtr: Number(HEAP64[base + 6]) >>> 0,
      callbackReleasePtr: Number(HEAP64[base + 7]) >>> 0
    };
  }

  function getRuntimeContext() {
    if (!host.currentContext) {
      throw new Error('Web WASM runtime context is not active');
    }

    return host.currentContext;
  }

  function getCurrentMemory() {
    var context = getRuntimeContext();
    var instance = context.module && context.module.instance;
    return getModuleMemory({ instance: instance }, 'pointer imports');
  }

  function callEngineCallbackLifecycle(callbackPtr, tokenPtr, tokenLen, role) {
    var context = getRuntimeContext();

    if (!callbackPtr) {
      throw new Error('Web WASM callback ' + role + ' bridge is not registered');
    }

    tokenLen = Number(tokenLen) | 0;

    if (tokenLen < 0) {
      throw new Error('Negative Web WASM callback ' + role + ' token length');
    }

    var token = readUtf8(tokenPtr, tokenLen);
    var tokenBytes = new TextEncoder().encode(token);
    var enginePtr = 0;

    if (tokenBytes.length > 0) {
      enginePtr = engineMalloc(tokenBytes.length);
      HEAPU8.set(tokenBytes, enginePtr);
    }

    try {
      return engineDynCall('iiii', callbackPtr, [context.scriptSysPtr | 0, enginePtr, tokenBytes.length]) | 0;
    }
    finally {
      if (enginePtr) {
        engineFree(enginePtr);
      }
    }
  }

  function getModuleMemory(module, role) {
    var instance = module && module.instance;
    var memory = instance && instance.exports && instance.exports.memory;

    if (!(memory instanceof WebAssembly.Memory)) {
      throw new Error('Web WASM module must export memory to use ' + role);
    }

    return memory;
  }

  function getModuleAllocator(module, role) {
    var instance = module && module.instance;
    var exports = instance && instance.exports;
    var malloc = exports && (exports.fonline_malloc || exports.malloc || exports.__wasm_malloc);
    var free = exports && (exports.fonline_free || exports.free || exports.__wasm_free);

    if (typeof malloc !== 'function' || typeof free !== 'function') {
      throw new Error('Web WASM module must export malloc/free to use ' + role);
    }

    return { malloc: malloc, free: free };
  }

  function copyEngineBufferToModule(module, enginePtr, byteLen, temporaryPointers, role) {
    return copyEngineBufferToModuleWithCapacity(module, enginePtr, byteLen, byteLen, temporaryPointers, role);
  }

  function copyEngineBufferToModuleWithCapacity(module, enginePtr, byteLen, capacityByteLen, temporaryPointers, role) {
    byteLen = Number(byteLen) | 0;
    capacityByteLen = Number(capacityByteLen) | 0;

    if (byteLen < 0) {
      throw new Error('Negative Web WASM export buffer length: ' + role);
    }
    if (capacityByteLen < 0) {
      throw new Error('Negative Web WASM export buffer capacity: ' + role);
    }
    if (capacityByteLen < byteLen) {
      throw new Error('Web WASM export buffer capacity is smaller than input length: ' + role);
    }
    if (capacityByteLen === 0) {
      return 0;
    }

    enginePtr = Number(enginePtr) >>> 0;

    if (enginePtr + byteLen > HEAPU8.length) {
      throw new Error('Web WASM export engine buffer is out of bounds: ' + role);
    }

    var allocator = getModuleAllocator(module, role);
    var modulePtr = Number(allocator.malloc(capacityByteLen)) >>> 0;

    if (modulePtr === 0) {
      throw new Error('Web WASM export buffer allocation failed: ' + role);
    }

    var memory = getModuleMemory(module, role);

    if (modulePtr + capacityByteLen > memory.buffer.byteLength) {
      throw new Error('Web WASM export module buffer is out of bounds: ' + role);
    }

    if (byteLen > 0) {
      new Uint8Array(memory.buffer, modulePtr, byteLen).set(HEAPU8.subarray(enginePtr, enginePtr + byteLen));
    }
    temporaryPointers.push({ ptr: modulePtr, free: allocator.free });
    return modulePtr;
  }

  function copyModuleBufferToEngineFixed(module, modulePtr, enginePtr, byteLen, role) {
    byteLen = Number(byteLen) | 0;

    if (byteLen < 0) {
      throw new Error('Negative Web WASM export copy-back buffer length: ' + role);
    }
    if (byteLen === 0) {
      return;
    }

    modulePtr = Number(modulePtr) >>> 0;
    enginePtr = Number(enginePtr) >>> 0;

    if (enginePtr + byteLen > HEAPU8.length) {
      throw new Error('Web WASM export copy-back engine buffer is out of bounds: ' + role);
    }

    var memory = getModuleMemory(module, role);

    if (modulePtr === 0 || modulePtr + byteLen > memory.buffer.byteLength) {
      throw new Error('Web WASM export copy-back module buffer is out of bounds: ' + role);
    }

    HEAPU8.set(new Uint8Array(memory.buffer, modulePtr, byteLen), enginePtr);
  }

  function unpackPointerLength(packed, role) {
    var raw = BigInt.asUintN(64, BigInt(packed));
    var ptr = Number(raw & 0xFFFFFFFFn) >>> 0;
    var byteLen = Number((raw >> 32n) & 0xFFFFFFFFn) >>> 0;

    if (byteLen > 0x7FFFFFFF) {
      throw new Error('Web WASM export buffer result is too large: ' + role);
    }

    return { ptr: ptr, byteLen: byteLen };
  }

  function packPointerLength(ptr, byteLen) {
    return (BigInt(byteLen >>> 0) << 32n) | BigInt(ptr >>> 0);
  }

  function copyModuleBufferToEngine(module, packed, role) {
    var pointerLength = unpackPointerLength(packed, role);
    var modulePtr = pointerLength.ptr;
    var byteLen = pointerLength.byteLen;

    if (byteLen === 0) {
      return 0n;
    }

    var memory = getModuleMemory(module, role);

    if (modulePtr + byteLen > memory.buffer.byteLength) {
      throw new Error('Web WASM export UTF-8 result buffer is out of bounds: ' + role);
    }

    var enginePtr = Number(engineMalloc(byteLen)) >>> 0;

    if (enginePtr === 0) {
      throw new Error('Web WASM export UTF-8 result allocation failed: ' + role);
    }

    try {
      if (enginePtr + byteLen > HEAPU8.length) {
        throw new Error('Web WASM export UTF-8 engine result buffer is out of bounds: ' + role);
      }

      HEAPU8.set(new Uint8Array(memory.buffer, modulePtr, byteLen), enginePtr);
      return packPointerLength(enginePtr, byteLen);
    }
    catch (error) {
      engineFree(enginePtr);
      throw error;
    }
  }

  function freeModulePointers(temporaryPointers) {
    for (var i = temporaryPointers.length - 1; i >= 0; i--) {
      temporaryPointers[i].free(temporaryPointers[i].ptr);
    }
  }

  function readUtf8(ptr, len) {
    ptr = Number(ptr) >>> 0;
    len = Number(len) | 0;

    if (len < 0) {
      throw new Error('Negative Web WASM UTF-8 buffer length: ' + len);
    }

    var memory = getCurrentMemory();

    if (ptr + len > memory.buffer.byteLength) {
      throw new Error('Web WASM UTF-8 buffer is out of bounds');
    }

    return new TextDecoder('utf-8', { fatal: false }).decode(new Uint8Array(memory.buffer, ptr, len));
  }

  function copyApiOutputBuffer(output, result, importName) {
    var neededLen = Number(result) | 0;

    if (neededLen < 0) {
      throw new Error('Negative Web WASM engine API output result length: fonline.api.' + importName);
    }

    var copyLen = Math.min(neededLen, output.length);

    if (output.noPartial && neededLen > output.length) {
      return;
    }

    if (copyLen === 0) {
      return;
    }

    var memory = getCurrentMemory();

    if (output.wasmPtr + copyLen > memory.buffer.byteLength) {
      throw new Error('Web WASM engine API output buffer is out of bounds: fonline.api.' + importName);
    }

    new Uint8Array(memory.buffer, output.wasmPtr, copyLen).set(HEAPU8.subarray(output.tempPtr, output.tempPtr + copyLen));
  }

  function copyApiMutableBuffer(output, importName) {
    if (output.length === 0) {
      return;
    }

    var memory = getCurrentMemory();

    if (output.wasmPtr + output.length > memory.buffer.byteLength) {
      throw new Error('Web WASM engine API mutable value buffer is out of bounds: fonline.api.' + importName);
    }

    new Uint8Array(memory.buffer, output.wasmPtr, output.length).set(HEAPU8.subarray(output.tempPtr, output.tempPtr + output.length));
  }

  function copyApiMutableArrayBuffer(output, importName) {
    var requiredLen = HEAP32[output.requiredTempPtr >> 2] | 0;
    var memory = getCurrentMemory();
    var role = output.role || 'mutable array';

    if (requiredLen < 0) {
      throw new Error('Negative Web WASM engine API ' + role + ' required byte length: fonline.api.' + importName);
    }
    if (output.requiredWasmPtr + 4 > memory.buffer.byteLength) {
      throw new Error('Web WASM engine API ' + role + ' required byte length pointer is out of bounds: fonline.api.' + importName);
    }

    new DataView(memory.buffer).setUint32(output.requiredWasmPtr, requiredLen >>> 0, true);

    if (requiredLen === 0 || requiredLen > output.length) {
      return;
    }
    if (output.wasmPtr + requiredLen > memory.buffer.byteLength) {
      throw new Error('Web WASM engine API ' + role + ' buffer is out of bounds: fonline.api.' + importName);
    }

    new Uint8Array(memory.buffer, output.wasmPtr, requiredLen).set(HEAPU8.subarray(output.tempPtr, output.tempPtr + requiredLen));
  }

  function readScalar(kind, valuePtr) {
    switch (kind) {
      case KIND_I32:
        return HEAP32[valuePtr >> 2] | 0;
      case KIND_I64:
        return HEAP64[valuePtr >> 3];
      case KIND_F32:
        return Math.fround(HEAPF32[valuePtr >> 2]);
      case KIND_F64:
        return HEAPF64[valuePtr >> 3];
      default:
        throw new Error('Unsupported Web WASM argument kind: ' + kind);
    }
  }

  function engineMalloc(size) {
    if (typeof _malloc === 'function') {
      return _malloc(size);
    }
    if (typeof Module !== 'undefined' && typeof Module._malloc === 'function') {
      return Module._malloc(size);
    }

    throw new Error('Web WASM host can not allocate engine memory');
  }

  function engineFree(ptr) {
    if (typeof _free === 'function') {
      _free(ptr);
      return;
    }
    if (typeof Module !== 'undefined' && typeof Module._free === 'function') {
      Module._free(ptr);
      return;
    }

    throw new Error('Web WASM host can not free engine memory');
  }

  function engineDynCall(signature, callbackPtr, args) {
    if (typeof dynCall === 'function') {
      return dynCall(signature, callbackPtr, args);
    }
    if (typeof Module !== 'undefined' && typeof Module.dynCall === 'function') {
      return Module.dynCall(signature, callbackPtr, args);
    }

    throw new Error('Web WASM host can not call engine API callback');
  }

  function kindToTypeName(kind) {
    switch (kind) {
      case KIND_I32:
        return 'i32';
      case KIND_I64:
        return 'i64';
      case KIND_F32:
        return 'f32';
      case KIND_F64:
        return 'f64';
      default:
        throw new Error('Unsupported Web WASM type kind: ' + kind);
    }
  }

  function writeRawScalar(kind, value, valuePtr) {
    switch (kind) {
      case KIND_I32:
        HEAP32[valuePtr >> 2] = Number(value) | 0;
        HEAP32[(valuePtr >> 2) + 1] = 0;
        return;
      case KIND_I64:
        HEAP64[valuePtr >> 3] = BigInt.asIntN(64, BigInt(value));
        return;
      case KIND_F32:
        HEAPF32[valuePtr >> 2] = Math.fround(Number(value));
        HEAP32[(valuePtr >> 2) + 1] = 0;
        return;
      case KIND_F64:
        HEAPF64[valuePtr >> 3] = Number(value);
        return;
      default:
        throw new Error('Unsupported Web WASM API argument kind: ' + kind);
    }
  }

  function readRawScalar(kind, valuePtr) {
    switch (kind) {
      case KIND_I32:
        return HEAP32[valuePtr >> 2] | 0;
      case KIND_I64:
        return HEAP64[valuePtr >> 3];
      case KIND_F32:
        return Math.fround(HEAPF32[valuePtr >> 2]);
      case KIND_F64:
        return HEAPF64[valuePtr >> 3];
      default:
        throw new Error('Unsupported Web WASM API result kind: ' + kind);
    }
  }

  function writeScalar(kind, valuePtr, value) {
    switch (kind) {
      case KIND_I32:
        HEAP32[valuePtr >> 2] = Number(value) | 0;
        return;
      case KIND_I64:
        HEAP64[valuePtr >> 3] = BigInt.asIntN(64, BigInt(value));
        return;
      case KIND_F32:
        HEAPF32[valuePtr >> 2] = Math.fround(Number(value));
        return;
      case KIND_F64:
        HEAPF64[valuePtr >> 3] = Number(value);
        return;
      default:
        throw new Error('Unsupported Web WASM result kind: ' + kind);
    }
  }

  function makeMissingImport(moduleName, importName) {
    return function () {
      throw new Error('Missing Web WASM import: ' + moduleName + '.' + importName);
    };
  }

  function sameTypeList(left, right) {
    if (!Array.isArray(left) || !Array.isArray(right) || left.length !== right.length) {
      return false;
    }

    for (var i = 0; i < left.length; i++) {
      if (left[i] !== right[i]) {
        return false;
      }
    }

    return true;
  }

  function validateImportSignature(moduleInfo, importInfo, importEntry) {
    var params = Array.isArray(importInfo.params) ? importInfo.params : [];
    var results = Array.isArray(importInfo.results) ? importInfo.results : [];

    if (!sameTypeList(params, importEntry.params) || !sameTypeList(results, importEntry.results)) {
      throw new Error('Web WASM import signature mismatch in ' + moduleInfo.path + ': ' + importInfo.module + '.' + importInfo.name);
    }
  }

  function buildImportObject(moduleInfo) {
    var importObject = Object.create(null);
    var imports = Array.isArray(moduleInfo.imports) ? moduleInfo.imports : [];

    for (var i = 0; i < imports.length; i++) {
      var importInfo = imports[i];
      if (!importInfo || importInfo.kind !== 'func') {
        continue;
      }

      var moduleName = importInfo.module || '';
      var importName = importInfo.name || '';

      if (!importObject[moduleName]) {
        importObject[moduleName] = Object.create(null);
      }

      var moduleImports = host.imports[moduleName];
      var importEntry = moduleImports ? moduleImports[importName] : null;

      if (importEntry) {
        validateImportSignature(moduleInfo, importInfo, importEntry);
      }

      importObject[moduleName][importName] = importEntry ? importEntry.func : makeMissingImport(moduleName, importName);
    }

    return importObject;
  }

  function loadModule(moduleInfo) {
    if (!moduleInfo || !moduleInfo.name || !moduleInfo.path) {
      return Promise.reject(new Error('Invalid Web WASM module manifest entry'));
    }

    return fetch(moduleInfo.path, { cache: 'no-cache' })
      .then(function (response) {
        if (!response.ok) {
          throw new Error('Failed to fetch Web WASM module ' + moduleInfo.path + ': HTTP ' + response.status);
        }
        return response.arrayBuffer();
      })
      .then(function (bytes) {
        return WebAssembly.instantiate(bytes, buildImportObject(moduleInfo));
      })
      .then(function (result) {
        host.modules[moduleInfo.name] = {
          info: moduleInfo,
          instance: result.instance,
          module: result.module
        };
      });
  }

  function loadAll() {
    return fetch('WasmScripts/manifest.json', { cache: 'no-cache' })
      .then(function (response) {
        if (!response.ok) {
          throw new Error('Failed to fetch Web WASM manifest: HTTP ' + response.status);
        }
        return response.json();
      })
      .then(function (manifest) {
        host.manifest = manifest || { version: 1, modules: [] };
        var modules = Array.isArray(host.manifest.modules) ? host.manifest.modules : [];
        return Promise.all(modules.map(loadModule));
      });
  }

  host.registerImport('fonline', 'log_i32', ['i32'], [], function (value) {
    console.log('[wasm:i32]', value | 0);
  });
  host.registerImport('fonline', 'log_i64', ['i64'], [], function (value) {
    console.log('[wasm:i64]', String(value));
  });
  host.registerImport('fonline', 'log_f32', ['f32'], [], function (value) {
    console.log('[wasm:f32]', Math.fround(Number(value)));
  });
  host.registerImport('fonline', 'log_f64', ['f64'], [], function (value) {
    console.log('[wasm:f64]', Number(value));
  });
  host.registerImport('fonline', 'log_utf8', ['i32', 'i32'], [], function (ptr, len) {
    console.log('[wasm:utf8]', readUtf8(ptr, len));
  });
  host.registerImport('fonline', 'callback_retain', ['i32', 'i32'], ['i32'], function (ptr, len) {
    return callEngineCallbackLifecycle(getRuntimeContext().callbackRetainPtr, ptr, len, 'retain');
  });
  host.registerImport('fonline', 'callback_release', ['i32', 'i32'], ['i32'], function (ptr, len) {
    return callEngineCallbackLifecycle(getRuntimeContext().callbackReleasePtr, ptr, len, 'release');
  });
  host.registerImport('fonline', 'get_side', [], ['i32'], function () {
    return getRuntimeContext().side | 0;
  });
  host.registerImport('fonline', 'get_frame_time_ms', [], ['i64'], function () {
    return getRuntimeContext().frameTimeMs;
  });
  host.registerImport('fonline', 'get_frame_delta_time_ms', [], ['i64'], function () {
    return getRuntimeContext().frameDeltaTimeMs;
  });
  host.registerImport('fonline', 'is_time_synchronized', [], ['i32'], function () {
    return getRuntimeContext().timeSynchronized | 0;
  });
  host.registerImport('fonline', 'get_synchronized_time_ms', [], ['i64'], function () {
    return getRuntimeContext().synchronizedTimeMs;
  });

  global.FOnlineWasmHost = host;

  function startLoading() {
    if (host.loadingStarted) {
      return;
    }

    host.loadingStarted = true;
    Module.addRunDependency('fo-wasm-scripting');
    host.ready = loadAll().then(function () {
      Module.removeRunDependency('fo-wasm-scripting');
    }).catch(function (error) {
      host.lastError = error && error.stack ? String(error.stack) : String(error);
      if (global.foShowError) {
        global.foShowError('Web WASM Error', host.lastError, true);
      }
      Module.printErr(host.lastError);
      Module.removeRunDependency('fo-wasm-scripting');
    });
  }

  if (typeof Module !== 'undefined') {
    Module.preRun = Module.preRun || [];
    Module.preRun.push(startLoading);

    if (Module.addRunDependency && Module.removeRunDependency && (Module.foRuntimeInitialized || Module.calledRun)) {
      startLoading();
    }
  }
}(globalThis));
