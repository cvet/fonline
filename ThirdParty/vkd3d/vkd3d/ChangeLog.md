# What's new in vkd3d 2.1 (24 August 2026)

### libvkd3d-shader

  - New features and improvements for the HLSL source type:
    - A flaw has been fixed where nested function calls of the form
      ‘g(f(1), f(2), f(3));’ would be compiled like ‘g(f(3), f(3), f(3));’.
    - A new dead store elimination pass. Its primary purpose is to eliminate
      unused uniforms in shader model 1-3 target profiles, allowing vkd3d-shader
      to more closely match the constant table layout produced by
      d3dcompiler/fxc.
    - Arithmetic operations on ‘half’ types in shader model 4 and 5 target
      profiles.
    - Stricter validation of allowed and required semantics on shader inputs and
      outputs. Notably:
      - User-defined semantics on outputs are explicitly rejected in shader
        model 4 and 5 pixel shader target profiles. These never worked, but
        would previously either be rejected by the target, or produce invalid
        output.
      - Semantics on unused input and outputs are not validated in shader model
        1-3 target profiles. This matches the behaviour of d3dcompiler/fxc.
      - Shader model 1-3 vertex shader target profiles require ‘position’
        outputs.
      - Shader model 1-3 pixel shader target profiles require ‘color’ outputs.

  - The effects (FX) target can output numeric states and shader objects in
    ‘fx_2_0’ profiles.

  - New features and improvements for the legacy Direct3D byte-code source type:
    - The ‘sub’ and ‘break_cmp’ instructions.
    - The ‘\_x2’, ‘\_x4’, ‘\_x8’, ‘\_d2’, ‘\_d4’, and ‘\_d8’ instruction
      modifiers.
    - The ‘\_bias’, ‘\_x2’, ‘\_bx2’, ‘negate’ (‘-register’), and ‘invert’
      (‘1 - register’) source operand modifiers.

  - The SPIR-V target supports floating-point to 64-bit signed and unsigned
    integer conversion operations. These typically originate from DXIL sources.

  - New features and improvements for the experimental OpenGL Shading Language
    target type:
    - Indirect addressing of shader input registers.
    - Load and store operations on raw and structured unordered access views and
      thread group shared memory.
    - Atomic compare/exchange and integer addition operations.
    - Bit scanning and counting operations.
    - Barrier operations.
    - ‘SV_GroupID’, ‘SV_GroupIndex’, and ‘SV_GroupThreadID’ inputs.

  - New interfaces:
    - The VKD3D_SHADER_COMPILE_OPTION_BACKCOMPAT_ALLOW_HALF_GLOBALS flag
      specifies that global variables with ‘half’ types in HLSL sources should
      be compiled as if they had the corresponding ‘float’ types.

### libvkd3d-utils

  - New shader reflection features and improvements:
    - The GetDesc() method of the ID3D12ShaderReflection interface now returns
      all information contained in the D3D12_SHADER_DESC structure.
    - The following methods of the ID3D12ShaderReflection interface are
      implemented:
      - GetBitwiseInstructionCount()
      - GetConstantBufferByName()
      - GetConversionInstructionCount()
      - GetGSInputPrimitive()
      - GetMovInstructionCount()
      - GetMovcInstructionCount()
      - GetResourceBindingDescByName()
      - GetThreadGroupSize()
      - GetVariableByName()
      - IsSampleFrequencyShader()
    - The GetVariableByName() method of the ID3D12ShaderReflectionConstantBuffer
      interface is implemented.
    - The following methods of the ID3D12ShaderReflectionType interface are
      implemented:
      - GetMemberTypeByName()
      - GetMemberTypeName()
      - IsEqual()
    - The ID3D10ShaderReflection, ID3D10ShaderReflection1, and
      ID3D11ShaderReflection interfaces are implemented. Where applicable,
      everything above also applies to the version 10, 10.1, and 11 equivalents
      of the methods mentioned above.

  - When the D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY flag is specified,
    D3DCompile2VKD3D() will consider global variables with ‘half’ types to have
    the corresponding ‘float’ types. Without this flag, global variables with
    ‘half’ types are not allowed. This change also applies to D3DCompile() and
    D3DCompile2(), which behave like D3DCompile2VKD3D() called with a
    ‘compiler_version’ of 47.

  - New interfaces:
    - D3DPreprocessVKD3D() is a variant of D3DPreprocess() that allows
      targeting the behaviour of a specific d3dcompiler version.
    - D3DReflectVKD3D() is a variant of D3DReflect() that allows
      targeting the behaviour of a specific d3dcompiler version.
    - D3D10ReflectShader() is equivalent to calling D3DReflectVKD3D() with an
      ‘iid’ of ‘IID_ID3D10ShaderReflection’ and a ‘version’ of 0.

### vkd3d-compiler

  - The ‘--allow-half-globals’ option specifies that global variables with
    ‘half’ types in HLSL sources should be compiled as if they had the
    corresponding ‘float’ types.

  - The Windows build of vkd3d-compiler will default to colour output when it's
    able to determine that the output is a device capable of interpreting
    virtual terminal sequences.

### vkd3d-dxbc

  - The ‘--input’ option specifies the input to use for subsequent ‘--update’
    options.

  - The ‘--update’ option specifies a section to update with the contents of the
    current input. For example,
    ‘vkd3d-dxbc -i priv.bin -u t:PRIV in.dxbc > out.dxbc’ would replace the
    private data section of ‘in.dxbc’ with the contents of ‘priv.bin’, and write
    the result to ‘out.dxbc’.

  - The Windows build of vkd3d-dxbc will default to colour output when it's able
    to determine that the output is a device capable of interpreting virtual
    terminal sequences.

# What's new in vkd3d 2.0 (21 May 2026)

### libvkd3d

  - Resources with a combined depth/stencil format like
    DXGI_FORMAT_R32G8X24_TYPELESS can be created without the
    D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL flag being set. It's somewhat rare
    for applications to create such resources; typically resources with a
    combined depth/stencil format are either used with both a depth/stencil view
    and a shader resource view, or exclusively with a depth/stencil view.

  - The syntax for the VKD3D_DEBUG environment variable has been extended to
    allow more precise control over the debug output. See the ‘Environment
    variables’ section of the README for a description of the extended syntax.

### libvkd3d-shader

  - New features and improvements for the HLSL source type:
    - Store operations to structured thread group shared memory.
    - Interlocked operations on structured and/or arrayed unordered access views
      and thread group shared memory.
    - A new common subexpression elimination optimisation pass.
    - Initial support for loops in shader model 2-3 target profiles.
    - More efficient usage of temporary registers, primarily due to improvements
      to the register allocator and the introduction of an output write hoisting
      pass. This is particularly relevant for shader model 1-3 target profiles,
      where the number of temporary registers is relatively limited, and we may
      otherwise not be able to compile some shaders.
    - The tex3Dbias(), tex3Dlod(), and texCUBElod() intrinsic functions.
    - The ‘SV_ClipDistance’ and ‘SV_CullDistance’ input/output semantics.
    - The ‘SV_StencilRef’ pixel shader output semantic.
    - Corrected handling of InterlockedMin() and InterlockedMax() operations on
      inputs with differing signedness. When either of the inputs has an ‘uint’
      type, the other is converted/promoted to ‘uint’ as well.
    - The VKD3D_SHADER_COMPILE_OPTION_BACKCOMPAT_MAP_SEMANTIC_NAMES flag now
      additionally maps the shader model 3 ‘VFACE’ and ‘VPOS’ semantic names to
      their shader model 4+ equivalents.
    - Constant folding of floating-point modulo expressions. I.e., ‘x % y’ where
      ‘x’ and ‘y’ are floating-point constants.
    - The scope of variables declared in ‘for’ loop initialisation clauses has
      been expanded to extend beyond the end of the loop. I.e.,  
      ‘for (int i = 0; i < 10; ++i); return i;’ is valid HLSL. That's different
      from e.g. C99, where the scope of ‘i’ would be limited to the loop.
    - Floating-point literal parsing has been made independent of the current
      locale. Previously, the HLSL parser would use the current locale's decimal
      separator when parsing floating-point literals, instead of the HLSL
      decimal separator, which is always the ‘.’ character. Note that this only
      affects programs that set a locale other than the initial ‘C’ locale, for
      example by calling the setlocale() function.

  - New features and improvements for the effects (FX) source type:
    - Parsing of operand indices in eval() block was broken and has been fixed.
    - The ‘umod’ FXLVM operation is recognised.

  - New features and improvements for the legacy Direct3D byte-code source type:
    - The following instructions:
      - m4x4, m3x4, m4x3, m3x3, and m3x2
      - phase
      - texdepth
      - texreg2ar, texreg2gb, and texreg2rgb
    - The ‘\_dz’, ‘\_db’, ‘\_dw’, and ‘\_da’ source modifiers. These are used
      with the ‘texcrd’ and ‘texld’ instructions.
    - The ‘vFace’ and ‘vPos’ pixel shader input registers.
    - Shader model 3 vertex shader point size outputs. In shader model 1 and 2
      point size output uses the ‘oPts’ output register; shader model 3 uses a
      generic ‘o’ register with a ‘dcl_psize’ declaration.
    - Shader model 1 and 2 vertex shader outputs are clamped to the 0 to 1
      range.

  - The DXIL source type supports forward-referencing pointers in load, store,
    atomic read/modify/write, and compare/exchange operations.

  - The DXIL source type supports pixel shader specified stencil reference
    values.

  - The experimental Metal Shading Language (MSL) target supports pixel shader
    specified stencil reference values.

  - The Direct3D shader assembly target supports 16-bit immediate constants.
    These are typically produced by DXIL sources.

  - The Direct3D shader assembly target supports the following global flags:
    - ‘64UAVs’
    - ‘ROVs’
    - ‘UAVLoadAdditionalFormats’
    - ‘UAVsAtEveryStage’
    - ‘allResourcesBound’
    - ‘enable11_1ShaderExtensions’
    - ‘int64Ops’
    - ‘nativeLowPrecision’
    - ‘stencilRef’
    - ‘viewportAndRTArrayIndex’
    - ‘waveOps’

  - New interfaces:
    - The vkd3d_shader_scan_denormal_mode_info structure extends the
      vkd3d_shader_compile_info structure, and can be used to retrieve the
      denormal mode used for floating-point numbers.
    - The VKD3D_SHADER_SPIRV_EXTENSION_KHR_FLOAT_CONTROLS enumeration value
      indicates support for the SPV_KHR_float_controls extension in the SPIR-V
      target environment.
    - The VKD3D_SHADER_COMPILE_OPTION_DENORMAL_MODE_F16 compile option specifies
      the denormal mode to use for 16-bit floating-point numbers.
    - The VKD3D_SHADER_COMPILE_OPTION_DENORMAL_MODE_F32 compile option specifies
      the denormal mode to use for 32-bit floating-point numbers.
    - The VKD3D_SHADER_COMPILE_OPTION_DENORMAL_MODE_F64 compile option specifies
      the denormal mode to use for 64-bit floating-point numbers.
    - The VKD3D_SHADER_COMPILE_OPTION_CONST_GLOBAL_UNIFORMS flag specifies that
      all uniforms with global scope should be considered ‘const’ in HLSL
      sources.
    - When targeting VKD3D_SHADER_API_2_0, compilation will fail when a required
      floating-point denormal mode can't be specified in the target shader. For
      the SPIR-V target, this requires the SPV_KHR_float_controls extension, as
      well as the corresponding capabilities in the target environment. The
      earlier mentioned compile options allow overriding the denormal modes
      specified by the source shader.

  - The syntax for the VKD3D_SHADER_DEBUG environment variable has been extended
    to allow more precise control over the debug output. See the ‘Environment
    variables’ section of the README for a description of the extended syntax.

### libvkd3d-utils

  - The D3DCompile(), D3DCompile2(), D3DCompile2VKD3D(), and D3DPreprocess()
    functions support using D3D_COMPILE_STANDARD_FILE_INCLUDE as include
    handler.

  - Unless the D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY flags is specified,
    D3DCompile2VKD3D() will consider all uniforms with global scope to be
    ‘const’ for compiler versions 37 and up. This is also the case for
    D3DCompile() and D3DCompile2(), which behave like compiler version 47.

# What's new in vkd3d 1.19 (19 February 2026)

### libvkd3d

  - Barriers are inserted as required between render passes targeting the same
    images. Typical desktop GPUs tend to not care much about these, but this is
    important for correctness on tile-based renderers.

  - The ‘minimum’ and ‘maximum’ texture filtering reduction modes are supported.

### libvkd3d-shader

  - Much improved handling of clip and cull distance shader input/output arrays
    for SPIR-V targets. Direct3D source formats represent clip and cull
    distances as 2 4-component registers, while SPIR-V represents them as scalar
    arrays. The previous code for mapping between these conventions was
    unfortunately relatively fragile and unable to handle a number of cases.

  - Several new features and improvements for the HLSL source type:
    - Effects (FX) target profiles can output embedded shader blobs. These are
      created by the CompileShader() and ConstructGSWithSO() intrinsic
      functions.
    - Statically initialised variables are considered to be static expressions.
      For example, ‘int count = 4; float4 array[count];’ is now accepted as
      valid by the compiler.
    - Store operations to ‘RWStructuredBuffer’ resources.
    - Load operations from structured thread group shared memory.
    - Copy operations involving structured resources.
    - Casts of floating-point constants to Boolean values now correctly fold any
      non-zero value to ‘true’. Previously ‘(bool)0.25f’ for example would get
      constant folded to ‘false’.
    - The ‘static’ storage-class specifier is allowed on functions.
    - The ‘Buffer’, ‘StructuredBuffer’, and ‘ByteAddressBuffer’ types are
      supported in effects (FX) target profiles.

  - The following legacy Direct3D byte-code instructions are implemented:
    - dp2add
    - loop/endloop
    - lrp
    - nrm
    - pow
    - rep/endrep

  - When the VKD3D_SHADER_DUMP_PATH environment variable is used to dump input
    and output shaders, compilation messages like errors and warnings produced
    by the compiler will be dumped as well.

  - New interfaces:
    - The VKD3D_SHADER_PARAMETER_NAME_PROJECTED_TEXTURE_MASK shader parameter
      specifies which textures in a shader model 1.0-1.3 pixel shader should be
      considered to be projected textures.

### build

  - New environment variables:
    - VKD3D_TEST_WRAPPER can be used to wrap test execution.

# What's new in vkd3d 1.18 (20 November 2025)

### libvkd3d

  - The CreateCommandList1() method of the ID3D12Device4 interface is
    implemented.

### libvkd3d-shader

  - Several new features and improvements for the HLSL source type:
    - Flattening of branched code. I.e., code using if/else statements can be
      turned into conditional moves when that's either advantageous or
      necessary. Cases where this is necessary include targeting shader model
      2.0 or earlier, as well as code using the ‘flatten’ attribute.
    - More constant folding improvements. For example:
      - asfloat(), asint(), asuint(), cos(), mad(), round(), and sin()
        operations are constant folded.
      - ‘true ? x : y’ is folded to ‘x’
      - ‘x ? true : false’ is folded to ‘x’
      - ‘||x||’ is folded to ‘|x|’
      - ‘~~x’ is folded to ‘x’
      - ‘x < 0’ is folded to ‘false’ for unsigned ‘x’
    - The following intrinsic functions:
      - countbits()
      - firstbithigh()
      - firstbitlow()
      - frexp()
      - texCUBEbias()
    - Integer-typed conditional expressions in shader model 1-3 target
      profiles. Previously these were only supported in shader model 4+ target
      profiles.
    - Resource loads from ‘StructuredBuffer’ resources.
    - ‘centroid’ interpolation modifiers in shader model 1-3 target profiles.
    - Support for ‘\_centroid’ modifier suffixes on I/O semantics. I.e.,
      ‘float2 t : TEXCOORD0_centroid’ is equivalent to ‘centroid float2 t :
      TEXCOORD0’.

  - The following legacy Direct3D byte-code instructions are implemented:
    - bem
    - tex
    - texbem
    - texbeml
    - texcoord

  - The experimental Metal Shading Language (MSL) target supports the
    following features:
    - Compute shaders.
    - Immediate-constant buffers.
    - Standard, inverse, and hyperbolic trigonometric functions.
    - Integer division and remainder operations.
    - Floating-point remainder operations.
    - Bit scanning and counting operations.
    - Boolean operands, as produced by DXIL sources.
    - Barrier operations.

  - When the experimental MSL target is enabled, the ‘hlsl’ source type can be
    used in combination with the ‘msl’ target type to convert HLSL shaders to
    MSL.

  - Signed integer operations like min() and max() are correctly translated
    from DXIL to MSL. Previously these would end up getting translated as
    unsigned operations, and would thus mishandle negative values.

  - The experimental OpenGL Shading Language (GLSL) target supports the
    following features:
    - Screen-space partial derivatives.
    - sin() and cos() operations.
    - Unsigned integer division and remainder operations.
    - Static texel offsets on texture load operations.
    - ‘SV_Coverage’ fragment shader outputs.
    - ‘SV_InstanceID’ inputs.

  - Textual output formats are documented to have a zero byte following their
    output. This can be convenient for passing such output to functions
    expecting a null-terminated string. For some output formats this was
    already true in earlier versions of vkd3d, but not guaranteed by the API.

  - New interfaces:
    - The vkd3d_shader_d3dbc_source_info structure extends the
      vkd3d_shader_compile_info structure, and can be used to specify
      auxiliary information for legacy Direct3D byte-code (‘d3dbc’) shaders
      that's necessary for target formats like SPIR-V and GLSL, but not
      specified by ‘d3dbc’ shaders. In particular, it allows specifying
      resource/sampler types for shader model 1 fragment shaders, as well as
      which samplers are comparison-mode samplers for shader model 1-3
      shaders.
    - The vkd3d_shader_scan_thread_group_size_info structure extends the
      vkd3d_shader_compile_info structure, and can be used to to retrieve the
      thread group size expected by a compute shader. This information is
      particularly useful when targeting Metal environments, because the Metal
      API requires the thread group size to be specified through its dispatch
      API, instead of being specified by the shader.
    - The VKD3D_SHADER_PARAMETER_NAME_BUMP_LUMINANCE_OFFSET_[0-5] shader
      parameters specify the corresponding bump-mapping luminance offsets.
    - The VKD3D_SHADER_PARAMETER_NAME_BUMP_LUMINANCE_SCALE_[0-5] shader
      parameters specify the corresponding bump-mapping luminance scale
      factors.
    - The VKD3D_SHADER_PARAMETER_NAME_BUMP_MATRIX_[0-5] shader parameters
      specify the corresponding bump-mapping transformation matrices.

# What's new in vkd3d 1.17 (21 August 2025)

### libvkd3d

  - The EnumerateMetaCommands() method of the ID3D12Device5 interface is
    implemented.

### libvkd3d-shader

  - Several new features and improvements for the HLSL source type:
    - Initial thread group shared memory support.
    - Improved support for geometry shaders: shader model 5 multiple output
      streams, as well as SV_IsFrontFace, SV_RenderTargetArrayIndex, and
      SV_ViewportArrayIndex outputs.
    - Structure variable input/output semantics are propagated to the
      constituent structure fields.
    - Shader entry point return values are allocated before any other outputs
      in the output signature. This matches d3dcompiler/fxc more closely.
    - Hull shader control point pass-through.
    - Reflection information can be retrieved using vkd3d_shader_scan().
    - Improved preprocessor handling of comments inside include directives, as
      well as inclusion of empty files.
    - Memory barrier intrinsics are supported in shader model 4 target
      profiles. Previous these were only supported in shader model 5 target
      profiles.
    - Parser support for the noise() intrinsic. Although the intrinsic itself
      isn't implemented, parser support for the intrinsic is required to allow
      compilation of any other shaders in the same source file to succeed as
      well.
    - Parser support for StructuredBuffer resources.

  - Various new features and improvements for the effects (FX) source type:
    - Shader blob assignments and FXLVM value expressions in ‘fx_2_0’ effects.
    - Nameless structure types.
    - Explicit constant buffer bind points and constant packing offsets.
    - The ‘d3ds_noiseswiz’, ‘ge’, ‘lt’, and ‘noise’ FXLVM operations.

  - The experimental Metal Shading Language (MSL) target supports the
    following features:
    - Texture sampling and gather operations.
    - Loops and switches.
    - Screen-space partial derivatives.
    - Various integer arithmetic and comparison operations.
    - Indirect addressing of constant buffers.
    - Indexable temporary registers.
    - Fragment shader output sample coverage masks.
    - SV_Position and SV_SampleIndex fragment shader inputs.
    - SV_VertexID inputs.

  - When the experimental MSL target is enabled, the ‘dxbc-dxil’ source type
    can be used in combination with the ‘msl’ target type to convert DXIL
    shaders to MSL.

  - The new ‘tx’ source type can be used in combination with the ‘d3d-asm’
    target type to disassemble D3DX ‘tx_1_0’ texture shaders.

  - The FX target takes alignment and padding into account in ‘fx_4_0’ buffer
    size calculations.

  - The SPIR-V target is capable of outputting OpSource and OpLine debug
    information.

  - The core grammar for the experimental SPIR-V disassembler has been updated
    to the ‘vulkan-sdk-1.4.313.0’ release.

  - New interfaces:
    - The VKD3D_SHADER_SOURCE_TX source type specifies D3DX ‘tx_1_0’ texture
      shaders.

### vkd3d-compiler

  - The new ‘tx’ source type specifies D3DX ‘tx_1_0’ texture shaders.

### demos

  - The new vkd3d-teapot demo uses tessellation shaders to render a version of
    Martin Newell's famous teapot. It should be noted that current versions of
    MoltenVK unfortunately do not support all features required to execute
    this demo correctly.

# What's new in vkd3d 1.16 (20 May 2025)

### libvkd3d

  - DXIL shaders are supported in the default configuration. Previously this
    required building vkd3d with the ‘-DVKD3D_SHADER_UNSUPPORTED_DXIL’
    preprocessor option. The also raises the maximum supported shader model to
    version 6.0.

  - Graphics pipeline state objects can be created from shaders with embedded
    root signatures. This was already possible for compute pipeline state
    objects.

  - The SetEventOnMultipleFenceCompletion() method of the ID3D12Device1
    interface is implemented.

  - When the VK_KHR_zero_initialize_workgroup_memory extension is supported,
    libvkd3d supports zero-initialising compute shader thread group shared
    memory.

  - The VK_KHR_maintenance2 extension is now explicitly required. libvkd3d
    already unconditionally used features provided by this extension, but
    unfortunately didn't explicitly require it. Support for this extension is
    widespread, and the extension is part of Vulkan 1.1.

### libvkd3d-shader

  - The previously experimental support for compiling DXIL shaders is now a
    supported feature and enabled by default. Please note that this feature is
    nevertheless still far from perfect.

  - New features for the HLSL source type:
    - Initial support for geometry shaders.
    - Indirect addressing in shader model 1-3 target profiles.
    - Modulus and truncation operations in shader model 1-3 target profiles.
    - Vectorised output code.
    - Further improved constant folding and propagation.
    - The following intrinsic functions are supported:
      - AllMemoryBarrier()
      - AllMemoryBarrierWithGroupSync()
      - DeviceMemoryBarrier()
      - DeviceMemoryBarrierWithGroupSync()
      - GroupMemoryBarrier()
      - GroupMemoryBarrierWithGroupSync()
    - The ‘.Length’ Texture object property.
    - The ‘SV_RenderTargetArrayIndex’ and ‘SV_ViewportArrayIndex’ semantics in
      tessellation shaders.

  - Disassembler support for binary ‘fx_2_0’ effects.

  - Experimental built-in support for disassembling SPIR-V shaders, enabled by
    building vkd3d with the ‘-DVKD3D_SHADER_UNSUPPORTED_SPIRV_PARSER’
    preprocessor option. When enabled, the built-in SPIR-V disassembler is
    used instead of SPIRV-Tools for the ‘spirv-text’ target type, as well as
    for the debug output enabled by the VKD3D_SHADER_DEBUG environment
    variable.

  - The experimental OpenGL Shading Language (GLSL) target supports indirect
    addressing of constant buffers.

  - The experimental Metal Shading Language (MSL) target supports texture
    loads.

  - New interfaces:
    - The VKD3D_SHADER_COMPILE_OPTION_FEATURE_ZERO_INITIALIZE_WORKGROUP_MEMORY
      flag indicates support for zero-initialising workgroup memory in the
      SPIR-V target environment.
    - The VKD3D_SHADER_COMPONENT_INT64 enumeration value indicates a 64-bit
      signed integer value.
    - The VKD3D_SHADER_COMPONENT_FLOAT16 enumeration value indicates a 16-bit
      IEEE floating-point value.
    - The VKD3D_SHADER_COMPONENT_UINT16 enumeration value indicates a 16-bit
      unsigned integer value.
    - The VKD3D_SHADER_COMPONENT_INT16 enumeration value indicates a 16-bit
      signed integer value.
    - When targeting VKD3D_SHADER_API_1_16, the
      VKD3D_SHADER_RESOURCE_DATA_NONE enumeration value is returned for the
      ‘resource_data_type’ field in the vkd3d_shader_descriptor_info structure
      for sampler descriptors. VKD3D_SHADER_API_1_15 and before use the
      VKD3D_SHADER_RESOURCE_DATA_UINT enumeration value for this.

### demos

  - The vkd3d demos now work on both the Microsoft Windows and Apple macOS
    builds. The macOS versions of the vkd3d demos are completely new in vkd3d
    1.16, while the Windows demos could previously be built, but only worked
    on Wine. Note that the vkd3d demos produced by a Windows build of vkd3d
    are distinct from those produced by the ‘make crosstest’ target: the
    former are Vulkan applications using vkd3d, while the latter are Direct3D
    12 applications.

  - The vkd3d demos have basic support for DPI scaling.

### build

  - Perl and the ‘JSON’ Perl module have been added as build dependencies.
    These are used for the experimental built-in SPIR-V disassembler, as well
    as for the macOS versions of the vkd3d demos.

# What's new in vkd3d 1.15 (19 February 2025)

### libvkd3d

  - New interfaces:
    - vkd3d_queue_signal_on_cpu() allows a Direct3D 12 fence to be signalled
      when all preceding work on a Direct3D 12 command queue has been submitted
      to the corresponding Vulkan queue.

### libvkd3d-shader

  - New features for the HLSL source type:
    - ‘InputPatch’ and ‘OutputPatch’ tessellation shader objects. This was the
      main feature required by most tessellation shaders that was still missing,
      and tessellation shaders should be considered generally usable now.
    - Unrolling of loops containing conditional jumps.
    - Improved function overload resolution. Previously the compiler was unable
      to decide between multiple function overloads with the same number of
      parameters.
    - The parser is able to continue parsing in a larger number of error cases.
      This allows more issues in the input to be reported during a single
      compilaton attempt.
    - The following intrinsic functions are supported:
      - GatherCmp()
      - GatherCmpAlpha(), GatherCmpBlue(), GatherCmpGreen(), and GatherCmpRed()
      - InterlockedAdd(), InterlockedAnd(), InterlockedCompareExchange(),
        InterlockedCompareStore(), InterlockedExchange(), InterlockedMax(),
        InterlockedMin(), InterlockedOr(), and InterlockedXor()
      - isinf()
    - Separate resource and sampler support for shader model 1-3 target
      profiles.
    - Casts on the left hand side of assignments.
    - Reassociation and redistribution of constants in binary expressions, to
      facilitate constant folding.
    - Packing of interstage I/O variables with the ‘SV_IsFrontFace’,
      ‘SV_PrimitiveID’, ‘SV_RenderTargetArrayIndex’, ‘SV_SampleIndex’, and
      ‘SV_ViewPortArrayIndex’ semantics matches d3dcompiler/fxc more closely.
    - Parser support for the ‘LineStream’, ‘PointStream’, and ‘TriangleStream’
      Stream-Output objects.

  - A number of instructions have been implemented for the experimental MSL
    target. Although more and more shaders are starting to work, support is
    still fairly limited. For example, shader resource views and unordered
    access views are still entirely unsupported.

  - Shader code generation for fixed-function fog. Like the existing shader code
    generation for other fixed-function features, this is mainly relevant for
    executing shader model 1-3 sources in modern target environments like
    Vulkan.

  - The ‘fx’ parser can parse binary effects containing inline shader blobs.

  - Internal validator support for validating I/O signatures, as well as I/O
    source and destination parameters. The validator is enabled by the
    ‘force_validation’ option, specified through the VKD3D_SHADER_CONFIG
    environment variable.

  - Internal validator support for validating the number of indices used with a
    register, as well as basic bounds checking for static indices.

  - New interfaces:
    - The vkd3d_shader_scan_hull_shader_tessellation_info structure extends the
      vkd3d_shader_compile_info structure, and can be used to retrieve the
      output primitive type and partitioning mode used by a hull shader. This
      information is particularly useful for specifying
      vkd3d_shader_spirv_domain_shader_target_info structures when targetting
      SPIR-V in OpenGL environments.
    - The VKD3D_SHADER_PARAMETER_NAME_FOG_FRAGMENT_MODE shader parameter
      specifies the kind of fog to generate in a fragment shader.
    - The VKD3D_SHADER_PARAMETER_NAME_FOG_COLOUR shader parameter
      specifies the fog colour.
    - The VKD3D_SHADER_PARAMETER_NAME_FOG_END shader parameter
      specifies the ‘end’ parameter used for linear fog generation.
    - The VKD3D_SHADER_PARAMETER_NAME_FOG_SCALE shader parameter
      specifies the ‘scale’ parameter used for fog generation.
    - The VKD3D_SHADER_PARAMETER_NAME_FOG_SOURCE shader parameter
      specifies the kind of fog coordinate to output from a pre-rasterisation
      shader.

### vkd3d-compiler

  - The new ‘dxbc-fx’ source type specifies an effect binary embedded in a DXBC
    container. This is a convenience feature;
    ‘vkd3d-compiler -x dxbc-fx blob.dxbc’ is equivalent to
    ‘vkd3d-dxbc -x t:FX10 blob.dxbc | vkd3d-compiler -x fx’.

# What's new in vkd3d 1.14 (21 November 2024)

### libvkd3d

  - Depth bounds can be changed dynamically using the OMSetDepthBounds() method
    of the ID3D12GraphicsCommandList1 interface.

  - The new VKD3D_CAPS_OVERRIDE environment variable can be used to override the
    value of capabilities like the maximum feature level and resource binding
    tier reported to applications.

### libvkd3d-shader

  - New features for the HLSL source type:
    - Interstage I/O variable packing matches d3dcompiler/fxc more closely.
    - The following intrinsic functions are supported:
      - dst()
      - f32tof16()
      - mad()
      - modf()
      - sincos()
    - ‘discard’ support for shader model 1-3 target profiles.
    - The ‘SV_SampleIndex’ input semantic for fragment shaders.
    - The ‘SV_GroupIndex’ input semantic for compute shaders.
    - The ‘earlydepthstencil’ function attribute.
    - Constant folding of expressions like ‘x && c’ and ‘x || c’, where ‘c’ is a
      constant.
    - Preprocessor support for namespaces in macro identifiers. I.e., syntax
      like ‘#define NAME1::NAME2::NAME3 1’ works as intended now.
    - Structure inheritance. Multiple inheritance is not supported.
    - Register assignments for unused constant buffers are allowed to overlap
      register assignments for used constant buffers.
    - Instruction count reflection data for shader model 4+ target profiles.
      This data is contained in the ‘STAT’ DXBC section, and can be queried with
      the GetDesc() method of the ID3D11ShaderReflection and
      ID3D12ShaderReflection interfaces. Note that the ID3D12ShaderReflection
      implementation provided by vkd3d-utils does not currently correctly report
      this information.
    - ‘unorm’ and ‘snorm’ resource format modifiers. For example,
      ‘Texture2D<unorm float4> t;’
    - Parser support for ‘ByteAddressBuffer’ resources, as well as their
      Load()/Load2()/Load3()/Load4() methods.
    - Parser support for ‘RWByteAddressBuffer’ resources, as well as their
      Store()/Store2()/Store3()/Store4() methods.
    - Parser support for the ‘compile’ keyword, as well as the CompileShader()
      and ConstructGSWithSO() intrinsic functions. Actual compilation of
      embedded shaders is not implemented yet, but parser support is sufficient
      for allowing compilation of HLSL sources containing this syntax to succeed
      when targetting shader target profiles like ‘vs_5_0’ or ‘ps_5_0’.
    - Initial support for tessellation shaders. Only the most trivial shaders
      are supported in this release. Perhaps most notably, both ‘InputPatch’ and
      ‘OutputPatch’ are not implemented yet.

  - The new ‘fx’ source type can be used in combination with the ‘d3d-asm’
    target type to disassemble binary effects.

  - More complete support for fx_2_0 binary effect output, including support for
    annotations, default values, as well as object initialiser data used for e.g.
    string, texture, and shader objects.

  - A significant number of instructions have been implemented for the
    experimental GLSL target. The GLSL output currently targets version 4.40
    without extensions, but the intention is to make this configurable in a
    future release of vkd3d.

  - Experimental support for Metal Shading Language (MSL) output, enabled by
    building vkd3d with the ‘-DVKD3D_SHADER_UNSUPPORTED_MSL’ preprocessor
    option. The current release is only able to compile the most basic shaders
    when targetting MSL. Being an experimental feature, both the ABI and API may
    change in future releases; the feature may even go away completely.
    Nevertheless, we hope our users find this feature useful, and welcome
    feedback and contributions.

  - Shader code generation for various legacy fixed-function features, including
    clip planes, point sizes, and point sprites. This is mainly relevant for
    executing shader model 1-3 sources in modern target environments like
    Vulkan, because those target environments do not implement equivalent
    fixed-function features.

  - The amount of shader validation done by the internal validator has been
    greatly extended. The validator is enabled by the ‘force_validation’ option,
    specified through the VKD3D_SHADER_CONFIG environment variable.

  - New interfaces:
    - The VKD3D_SHADER_COMPILE_OPTION_DOUBLE_AS_FLOAT_ALIAS flag specifies that
      the ‘double’ type behaves as an alias for the ‘float’ type in HLSL sources
      with shader model 1-3 target profiles.
    - The VKD3D_SHADER_PARAMETER_DATA_TYPE_FLOAT32_VEC4 enumeration value
      specifies that a shader parameter contains a 4-dimensional vector of
      32-bit floating-point data.
    - The VKD3D_SHADER_PARAMETER_NAME_CLIP_PLANE_MASK shader parameter specifies
      which clip planes are enabled.
    - The VKD3D_SHADER_PARAMETER_NAME_CLIP_PLANE_[0-7] shader parameters specify
      the corresponding clip planes.
    - The VKD3D_SHADER_PARAMETER_NAME_POINT_SIZE shader parameter specifies the
      point size to output when the source shader does not explicitly output a
      point size.
    - The VKD3D_SHADER_PARAMETER_NAME_POINT_SIZE_MIN shader parameter specifies
      the minimum point size to clamp point size outputs to.
    - The VKD3D_SHADER_PARAMETER_NAME_POINT_SIZE_MAX shader parameter specifies
      the maximum point size to clamp point size outputs to.
    - The VKD3D_SHADER_PARAMETER_NAME_POINT_SPRITE shader parameter specifies
      whether texture coordinate inputs in fragment shaders should be replaced
      with point coordinates.
    - The VKD3D_SHADER_SOURCE_FX source type specifies binary Direct3D effects.
    - The VKD3D_SHADER_TARGET_MSL target type specifies Metal Shading Language
      shaders.

### libvkd3d-utils

  - The GetDesc() method of the ID3D12ShaderReflection interface returned by
    D3DReflect() returns shader version information.

  - New interfaces:
    - D3DCompile2VKD3D() is a variant of D3DCompile2() that allows targeting the
      behaviour of a specific d3dcompiler version.

### vkd3d-compiler

  - The ‘--alias-double-as-float’ option specifies that the ‘double’ type
    behaves as an alias for the ‘float’ type in HLSL sources with shader model
    1-3 target profiles.

  - The ‘fx’ source type specifies binary Direct3D effects.

  - The ‘msl’ target type specifies Metal Shading Language shaders.

# What's new in vkd3d 1.13 (29 August 2024)

### libvkd3d

  - The ID3D12CommandList6 interface is supported.

  - Block-compressed textures can be created with unaligned dimensions. This
    corresponds to
    D3D12_FEATURE_D3D12_OPTIONS8.UnalignedBlockTexturesSupported.

  - Some minor issues pointed out by the Vulkan validation layers have been
    addressed. These are not known to affect applications in practice, but
    should make libvkd3d slightly more well-behaved.

### libvkd3d-shader

  - New features for the HLSL source type:
    - Basic loop unrolling support. Some of the more complicated cases like
      loops containing conditional jumps are still unsupported.
    - Initialisation values for global variables, function parameters, and
      annotation variables are parsed and stored in output formats supporting
      them.
    - Shader model 5.1 register spaces are supported when using the
      corresponding target profiles, as well as shader model 5.1 reflection
      data.
    - Register reservations support expressions as offsets. For example:
      ‘float f : register(c0[1 + 1 * 2]);’
    - The tex1D(), tex2D(), tex3D(), and texCUBE() intrinsic function variants
      with explicit derivatives are supported.
    - The following intrinsic functions are supported:
      - asint()
      - f16tof32()
      - faceforward()
      - GetRenderTargetSampleCount()
      - rcp()
      - tex2Dbias()
      - tex1Dgrad(), tex2Dgrad(), tex3Dgrad(), and texCUBEgrad()
    - The sin() and cos() intrinsic functions are supported in shader model
      1-3 profiles. These were already supported in shader model 4+ profiles.
    - The following features specific to effects target profiles:
      - Types supported in version 4.0+:
        - BlendState
        - ComputeShader, DomainShader, GeometryShader, and HullShader
        - DepthStencilState
        - RasterizerState
      - State application functions implemented for version 4.0+ effects:
        - OMSetRenderTargets()
        - SetBlendState()
        - SetComputeShader(), SetDomainShader(), SetGeometryShader(),
          SetHullShader(), SetPixelShader(), and SetVertexShader()
        - SetDepthStencilState()
        - SetRasterizerState()
      - String types. These are mainly used for annotations.
      - Annotations on global variables.
      - Support for the ‘Texture’ field of the ‘SamplerState’ type.
      - Support for NULL values.
    - Stores to swizzled matrix variables.
    - The ‘unsigned’ type modifier is supported. (For example,
      ‘unsigned int’.) Note that ‘uint’ and related types were already
      supported.
    - ‘ConstantBuffer<>’ types.
    - The ‘SV_Coverage’ output semantic for fragment shaders.

  - The experimental DXIL source type supports quad group operations.

  - The Direct3D shader model 2-3 ‘texldb’ instruction is correctly disassembled
    when outputting Direct3D shader assembly.

  - New interfaces:
    - The vkd3d_shader_parameter_info structure extends the
      vkd3d_shader_compile_info structure, and can be used to specify shader
      parameters. This is a more generic version of the shader parameter
      interface for SPIR-V targets in struct vkd3d_shader_spirv_target_info.
    - The VKD3D_SHADER_PARAMETER_DATA_TYPE_FLOAT32 enumeration value specifies
      that a shader parameter contains 32-bit floating-point data.
    - The VKD3D_SHADER_PARAMETER_NAME_ALPHA_TEST_FUNC shader parameter
      specifies the alpha test function.
    - The VKD3D_SHADER_PARAMETER_NAME_ALPHA_TEST_REF shader parameter
      specifies the alpha test reference value.
    - The VKD3D_SHADER_PARAMETER_NAME_FLAT_INTERPOLATION shader parameter
      specifies the interpolation mode for colour inputs in Direct3D shader
      model 1-3 fragment shaders.
    - The VKD3D_SHADER_PARAMETER_TYPE_BUFFER enumeration value specifies that
      the value of a shader parameter is provided at run-time through a buffer
      resource.

# What's new in vkd3d 1.12 (28 May 2024)

### libvkd3d

  - When the VK_EXT_fragment_shader_interlock extension is available, libvkd3d
    supports rasteriser-ordered views.

  - Compute pipeline state objects can be created from compute shaders with
    embedded root signatures.

  - When supported by the underlying Vulkan implementation, libvkd3d supports
    the DXGI_FORMAT_B5G6R5_UNORM, DXGI_FORMAT_B5G5R5A1_UNORM, and
    DXGI_FORMAT_B4G4R4A4_UNORM formats.

  - The ID3D12ShaderCacheSession interface is supported.

  - The ID3D12Device9 interface is supported.

  - The CreateCommittedResource2(), CreatePlacedResource1(),
    GetCopyableFootprints1(), and GetResourceAllocationInfo2() methods of the
    ID3D12Device8 interface are implemented.

  - Several new feature queries are supported:
    - D3D12_FEATURE_D3D12_OPTIONS14
    - D3D12_FEATURE_D3D12_OPTIONS15
    - D3D12_FEATURE_D3D12_OPTIONS16
    - D3D12_FEATURE_D3D12_OPTIONS17
    - D3D12_FEATURE_D3D12_OPTIONS18

### libvkd3d-shader

  - The experimental DXIL source type supports the majority of Direct3D shader
    model 6.0 instructions and features. Note that this is currently still an
    unsupported feature, enabled by building vkd3d with the
    ‘-DVKD3D_SHADER_UNSUPPORTED_DXIL’ preprocessor option. No API or ABI
    stability guarantees are provided for experimental features.

  - New features for the HLSL source type:
    - Support for compiling directly to Direct3D shader assembly and SPIR-V
      target types. This is primarily a convenience feature, as targeting
      these could previously be achieved by going through either the ‘Legacy
      Direct3D byte-code’ or ‘Tokenized Program Format’ formats as
      intermediates.
    - Improved support for shader model 1-3 profiles. In particular:
      - The ternary, comparison, and logical operators are now supported for
        these profiles.
      - Support for integer and Boolean types has been improved.
      - Shader constants are allocated in an order compatible with the
        Microsoft implementation.
    - More complex array size expressions. For example, matrix and vector
      swizzles are allowed, as well as (constant) array dereferences.
    - The following intrinsic functions are supported:
      - cosh()
      - determinant()
      - refract()
      - sinh()
      - tanh()
    - Reflection data for ‘Tokenized Program Format’ targets more accurately
      reflects the source shader.
    - Constant folding of expressions like ‘x + 0’ and ‘x * 1’.
    - Support for the ‘single’ qualifier on constant buffer declarations.
    - Parser support for annotations on constant buffer declarations.
    - Parser support for effect state objects.

  - When the SPV_EXT_fragment_shader_interlock extension is supported, SPIR-V
    targets support rasteriser-ordered views.

  - New interfaces:
    - The VKD3D_SHADER_PARSE_DXBC_IGNORE_CHECKSUM flag indicates that
      vkd3d_shader_parse_dxbc() should skip validating the checksum of the
      DXBC blob. This allows otherwise valid blobs with a missing or invalid
      checksum to be parsed.
    - The VKD3D_SHADER_SPIRV_ENVIRONMENT_VULKAN_1_1 enumeration value
      specifies the Vulkan 1.1 environment for SPIR-V targets. Most notably,
      the Vulkan 1.1 environment implies support for SPIR-V 1.3, which is a
      requirement for supporting Direct3D shader model 6 wave operations on
      SPIR-V targets.
    - The VKD3D_SHADER_SPIRV_EXTENSION_EXT_FRAGMENT_SHADER_INTERLOCK
      enumeration value indicates support for the
      SPV_EXT_fragment_shader_interlock extension in the SPIR-V target
      environment.
    - The VKD3D_SHADER_COMPILE_OPTION_FEATURE_WAVE_OPS flag indicates support
      for Direct3D shader model 6 wave operations in the SPIR-V target
      environment.
    - The VKD3D_SHADER_COMPILE_OPTION_FORMATTING_IO_SIGNATURES flag indicates
      that vkd3d_shader_compile() should include information about input,
      output, and patch constant shader signatures when targeting Direct3D
      shader assembly. Note that this is a libvkd3d-shader extension, and
      potentially makes the output incompatible with other implementations.
    - The VKD3D_SHADER_COMPILE_OPTION_CHILD_EFFECT compile option specifies
      whether libvkd3d-shader should produce child effects for ‘fx_4_0’ and
      ‘fx_4_1’ HLSL target profiles.
    - The VKD3D_SHADER_COMPILE_OPTION_INCLUDE_EMPTY_BUFFERS_IN_EFFECTS compile
      option specifies whether empty constant buffer descriptions should be
      included in the output for ‘fx_4_0’ and ‘fx_4_1’ HLSL target profiles.
    - The VKD3D_SHADER_COMPILE_OPTION_WARN_IMPLICIT_TRUNCATION compile option
      specifies whether the HLSL compiler should emit warnings for vector and
      matrix truncation in implicit type conversions.

### libvkd3d-utils

  - D3D12CreateDeviceVKD3D() and D3D12CreateDevice() no longer require the
    VK_KHR_surface and VK_KHR_swapchain extensions to be available. This
    allows them to be used in environments with a windowing system, for
    example for off-screen rendering and compute tasks.

  - The GetConstantBufferByIndex() and GetResourceBindingDesc() methods of the
    ID3D12ShaderReflection interface are implemented.

  - The GetVariableByIndex() method of the
    ID3D12ShaderReflectionConstantBuffer interface is implemented.

  - The GetMemberTypeByIndex() method of the ID3D12ShaderReflectionType
    interface is implemented.

  - The GetType() method of the ID3D12ShaderReflectionVariable interface is
    implemented.

### vkd3d-compiler

  - The ‘+signatures’ flag for the ‘--formatting’ option can be used to
    specify that vkd3d-compiler should include information about input,
    output, and patch constant shader signatures when outputting Direct3D
    shader assembly. Note that this is a vkd3d-compiler extension, and
    potentially makes the output incompatible with other implementations.

  - The ‘--child-effect’ option can be used to specify that vkd3d-compiler
    should produce child effects for ‘fx_4_0’ and ‘fx_4_1’ HLSL target
    profiles.

  - The ‘--fx-include-empty-buffers’ option can be used to specify that
    vkd3d-compiler should include empty constant buffer descriptions in the
    output for ‘fx_4_0’ and ‘fx_4_1’ HLSL target profiles.

### vkd3d-dxbc

  - The ‘--ignore-checksum’ option can be used to specify that vkd3d-dxbc
    should skip validating the checksum of the DXBC input blob. This allows
    vkd3d-dxbc to operate on otherwise valid blobs with missing or invalid
    checksums.

  - The ‘--emit’ option can be used to indicate that vkd3d-dxbc should output
    a new DXBC blob.

  - The ‘--extract’ option can be used to specify a section to extract out of
    the input blob. For example, ‘vkd3d-dxbc -x t:PRIV blob.dxbc > priv.bin’
    would extract the private data section of ‘blob.dxbc’ to ‘priv.bin’, and
    ‘vkd3d-dxbc -x t:RTS0 blob.dxbc > root_signature.bin’ would extract the
    root signature to ‘root_signature.bin’.

  - The ‘--output’ option can be used to specify where vkd3d-dxbc should write
    its output for the ‘--emit’ and ‘--extract’ options.

# What's new in vkd3d 1.11 (5 Mar 2024)

### libvkd3d

  - Descriptor updates happen asynchronously on an internal worker thread, for
    a minor performance improvement in applications that update many
    descriptors per frame.

  - When the VK_EXT_mutable_descriptor_type extension is available, libvkd3d
    will make more efficient use of descriptor pools and sets.

  - When the VK_EXT_shader_viewport_index_layer extension is available,
    libvkd3d supports indexing viewport and render target arrays from vertex
    and tessellation evaluation shaders.

  - Support for standard (i.e., black and white) border colours is
    implemented.

  - The GetResourceAllocationInfo1() method of the ID3D12Device4 interface is
    implemented.

  - The ID3D12Device7 interface is supported.

  - The ID3D12Resource2 interface is supported.

  - Several new feature queries are supported:
    - D3D12_FEATURE_D3D12_OPTIONS6
    - D3D12_FEATURE_D3D12_OPTIONS7
    - D3D12_FEATURE_D3D12_OPTIONS8
    - D3D12_FEATURE_D3D12_OPTIONS9
    - D3D12_FEATURE_D3D12_OPTIONS10
    - D3D12_FEATURE_D3D12_OPTIONS11
    - D3D12_FEATURE_D3D12_OPTIONS12
    - D3D12_FEATURE_D3D12_OPTIONS13

### libvkd3d-shader

  - Initial support for compiling legacy Direct3D bytecode to SPIR-V.

  - Experimental support for compiling DirectX Intermediate Language (DXIL) to
    SPIR-V and Direct3D shader assembly. Being an experimental feature, this
    requires building vkd3d with the ‘-DVKD3D_SHADER_UNSUPPORTED_DXIL’
    preprocessor option. Note that enabling this feature will affect the
    capabilities reported by libvkd3d as well, and may cause previously
    working applications to break due to attempting to use incomplete DXIL
    support. No API or ABI stability guarantees are provided for experimental
    features.

  - New features for the HLSL source type:
    - Initial support for the ‘fx_2_0’, ‘fx_4_0’, ‘fx_4_1’, and ‘fx_5_0’
      profiles, using the new ‘VKD3D_SHADER_TARGET_FX’ target type.
    - Support for ‘Buffer’ resources.
    - The acos(), asin(), atan(), and atan2() intrinsic functions are
      supported.
    - Explicit register assignment using the ‘register()’ keyword in shader
      model 1-3 profiles. This was previously only supported in shader model
      4+ profiles.
    - Casts from integer to floating-point types in shader model 1-3 profiles.
    - Support for various input/output semantics:
      - SV_InstanceID in shader model 4+ fragment shaders.
      - SV_PrimitiveID in shader model 4+ fragment shaders. In previous
        versions this was only supported in shader model 4+ geometry shaders.
      - SV_RenderTargetArrayIndex in shader model 4+ vertex and fragment shaders.
      - SV_ViewportArrayIndex in shader model 4+ vertex and fragment shaders.
    - Support for various rasteriser-ordered view types. Specifically:
      - RasterizerOrderedBuffer
      - RasterizerOrderedStructuredBuffer
      - RasterizerOrderedTexture1D
      - RasterizerOrderedTexture1DArray
      - RasterizerOrderedTexture2D
      - RasterizerOrderedTexture2DArray
      - RasterizerOrderedTexture3D

  - New features for the SPIR-V target type:
    - Support for globally coherent unordered access views. These have the
      ‘globallycoherent’ storage class in HLSL, and the ‘_glc’ suffix in
      Direct3D assembly.
    - Support for thread group unordered access view barriers. This
      corresponds to ‘sync_ugroup’ instructions in Direct3D assembly.
    - When the SPV_EXT_viewport_index_layer extension is supported, vertex and
      tessellation evaluation shaders can write render target and viewport
      array indices. This corresponds to the ‘SV_RenderTargetArrayIndex’ and
      ‘SV_ViewportArrayIndex’ HLSL output semantics.

  - New interfaces:
    - The VKD3D_SHADER_COMPILE_OPTION_FEATURE compile option can be used to
      specify features available in the target environment. The
      VKD3D_SHADER_COMPILE_OPTION_FEATURE_INT64 flag indicates support for
      64-bit integer types in the SPIR-V target environment. The
      VKD3D_SHADER_COMPILE_OPTION_FEATURE_FLOAT64 flag indicates support for
      64-bit floating-point types in the SPIR-V target environment. For
      backward compatibility, VKD3D_SHADER_API_VERSION_1_10 and earlier also
      imply support for 64-bit floating-point types.
    - The VKD3D_SHADER_SPIRV_EXTENSION_EXT_VIEWPORT_INDEX_LAYER enumeration
      value indicates support for the SPV_EXT_viewport_index_layer extension
      in the SPIR-V target environment.

### libvkd3d-utils

  - When available, the following Vulkan extensions are enabled by
    D3D12CreateDeviceVKD3D() and D3D12CreateDevice():
    - VK_KHR_android_surface
    - VK_KHR_wayland_surface
    - VK_KHR_win32_surface
    - VK_KHR_xlib_surface
    - VK_EXT_metal_surface
    - VK_MVK_ios_surface

    Previous versions of vkd3d-utils enabled VK_KHR_xcb_surface and
    VK_MVK_macos_surface. In practice this means that
    D3D12CreateDevice()/D3D12CreateDeviceVKD3D() can be used on the
    corresponding additional window systems.

  - New interfaces:
    - D3DReflect() is used to retrieve information about shaders. It currently
      supports retrieving information about input, output, and patch constant
      parameters using the ID3D12ShaderReflection interface.
    - D3DDisassemble() is used to disassemble legacy Direct3D bytecode (shader
      model 1-3) and ‘Tokenized Program Format’ (shader model 4 and 5)
      shaders.

### vkd3d-compiler

  - The new ‘fx’ target is used for outputting Direct3D effects when compiling
    HLSL ‘fx_2_0’, ‘fx_4_0’, ‘fx_4_1’, and ‘fx_5_0’ profiles.

### build

  - The minimum required version of Vulkan-Headers for this release is version
    1.3.228.

# What's new in vkd3d 1.10 (6 Dec 2023)

### libvkd3d

  - Creating pipeline state objects from pipeline state stream descriptions is
    implemented.
  - Depth-bounds testing is implemented.
  - When the VK_KHR_maintenance2 extension is available, libvkd3d will
    explicitly specify the usage flags of Vulkan image views. This is
    particularly useful on MoltenVK, where 2D-array views of 3D textures are
    subject to usage restrictions.
  - The D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD and/or
    D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE feature flags are reported for
    UAV formats when the ‘shaderStorageImageReadWithoutFormat’ and/or
    ‘shaderStorageImageWriteWithoutFormat’ Vulkan device features are
    supported.
  - The ID3D12Device5 interface is supported.
  - The ID3D12GraphicsCommandList5 interface is supported.
  - The ID3D12Resource1 interface is supported.

### libvkd3d-shader

  - New features for the HLSL source type:
    - Support for the following intrinsic functions:
      - ceil()
      - degrees() and radians()
      - fwidth()
      - tan()
      - tex2Dlod(), tex2Dproj(), texCUBEproj(), and tex3Dproj()
    - Constant folding support for more expression types. In particular:
      - ternary operators and branches
      - reciprocal square roots
      - exponentials
      - logical ‘not’ on booleans
      - bitwise complements
      - left/right shifts
      - ceil(), floor(), frac(), and saturate()
    - Support for dynamic indexing of arrays.
    - Support for ‘break’ and ‘continue’ statements.
    - Support for ‘switch’ statements.
    - The ‘linear’, ‘centroid’, and ‘noperspective’ interpolation modifiers
      are supported.
    - The ‘RWTexture1DArray’ and ‘RWTexture2DArray’ unordered access view
      types are supported.
    - ‘\[loop\]’ attributes are accepted on loops.
    - u/U and l/L suffixes on integer constants.

  - Floating-point values are explicitly clamped to the upper and lower bounds
    of the target type by ‘ftoi’ and ‘ftou’ instructions when targeting
    SPIR-V. Similarly, NaNs are flushed to zero. Some hardware/drivers would
    already do this implicitly, but behaviour for such inputs is undefined as
    far as SPIR-V is concerned.

  - The VKD3D_SHADER_CONFIG environment variable can be used to modify the
    behaviour of libvkd3d-shader at run-time, analogous to the existing
    VKD3D_CONFIG environment variable for libvkd3d. See the README for a list
    of supported options.

  - When scanning legacy Direct3D bytecode using vkd3d_shader_scan(),
    descriptor information for shader model 2 and 3 combined resource-sampler
    pairs is returned in the vkd3d_shader_scan_descriptor_info structure.
    Note that this information is not yet available for shader model 1
    sources, although this will likely be added in a future release.

  - The Direct3D shader assembly target supports the ‘rasteriser ordered view’
    flag (‘_rov’) on unordered access view declarations.

  - New interfaces:
    - The VKD3D_SHADER_COMPILE_OPTION_BACKWARD_COMPATIBILITY compile
      option can be used to specify backward compatibility options. The
      VKD3D_SHADER_COMPILE_OPTION_BACKCOMPAT_MAP_SEMANTIC_NAMES flag is
      the only currently supported flag, and can be used to specify that
      shader model 1-3 semantic names should be mapped to their shader model
      4+ system value equivalents when compiling HLSL sources.
    - The VKD3D_SHADER_COMPILE_OPTION_FRAGMENT_COORDINATE_ORIGIN compile
      option can be used to specify the origin of fragment coordinates for
      SPIR-V targets. This is especially useful in OpenGL environments, where
      the origin may be different than in Direct3D or Vulkan environments.
    - The vkd3d_shader_scan_combined_resource_sampler_info structure
      extends the vkd3d_shader_compile_info structure, and can be used to
      retrieve information about the combined resource-sampler pairs used by a
      shader. This is especially useful when compiling shaders for usage in
      environments without separate binding points for samplers and resources,
      like OpenGL.
    - vkd3d_shader_free_scan_combined_resource_sampler_info() is used
      to free vkd3d_shader_scan_combined_resource_sampler_info
      structures.

### libvkd3d-utils

  - Passing the D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY flag to
    D3DCompile() and D3DCompile2() will enable mapping shader model 1-3
    semantic names to their shader model 4+ system value equivalents.

  - New interfaces:
    - D3DGetBlobPart() is used to retrieve specific parts of DXBC blobs.
    - D3DGetDebugInfo() is used to retrieve debug information from DXBC blobs.
    - D3DGetInputAndOutputSignatureBlob() is used to retrieve input and output
      signatures from DXBC blobs.
    - D3DGetInputSignatureBlob() is used to retrieve input signatures from
      DXBC blobs.
    - D3DGetOutputSignatureBlob() is used to retrieve output signatures from
      DXBC blobs.
    - D3DStripShader() is used to remove specific parts from DXBC blobs.

### vkd3d-compiler

  - The ‘--fragment-coordinate-origin’ option can be used to specify the
    origin of fragment coordinates for SPIR-V targets.

  - The ‘--semantic-compat-map’ option can be used to specify that shader
    model 1-3 semantic names should be mapped to their shader model 4+ system
    value equivalents when compiling HLSL sources.

### vkd3d-dxbc

  - The ‘--list’ and ‘--list-data’ options now also output the offsets of
    sections inside the input data.

### build

  - The minimum required version of Vulkan-Headers for this release is version
    1.2.148.

  - When available, the libEGL and libOpenGL libraries are used to run the
    vkd3d tests in additional configurations. These libraries are not used by
    vkd3d itself.

  - The SONAME_LIBDXCOMPILER configure variable can be used specify the
    shared object name of the dxcompiler library. When available, it's used to
    run the vkd3d tests in additional configurations. The dxcompiler library
    is not used by vkd3d itself.


# What's new in vkd3d 1.9 (21 Sep 2023)

### libvkd3d

- Copying between depth/stencil and colour formats in
  ID3D12GraphicsCommandList::CopyResource() is supported.
- The ID3D12Fence1 interface is supported.


### libvkd3d-shader

- vkd3d_shader_scan() supports retrieving descriptor information for ‘d3dbc’
  shaders. This is one of the requirements for eventual SPIR-V generation from
  ‘d3dbc’ sources.

- New features for the HLSL source type:
  - Support for the following intrinsic functions:
    - clip()
    - ddx_coarse() and ddy_coarse()
    - ddx_fine() and ddy_fine()
    - tex1D(), tex2D(), texCUBE(), and tex3D()
  - Constant folding support for more expression types. In particular:
    - comparison operators
    - floating-point min() and max()
    - logical ‘and’ and ‘or’
    - dot products
    - square roots
    - logarithms
  - Support for multi-sample texture object declarations without explicit
    sample counts in shader model 4.1 and later shaders.
  - Support for using constant expressions as sample counts in multi-sample
    texture object declarations.
  - Support for variable initialisers using variables declared earlier in the
    same declaration list. E.g., ‘float a = 1, b = a, c = b + 1;’.
  - The GetDimensions() texture object method is implemented.
  - Matrix swizzles are implemented.
  - Parser support for if-statement attributes like ‘[branch]’ and
    ‘[flatten]’.
  - Support for the ‘inline’ function modifier.

- Previously, vkd3d_shader_compile() would in some cases return VKD3D_OK
  despite compilation failing when targeting legacy Direct3D bytecode. These
  cases have been fixed.

- Various HLSL preprocessor fixes for edge cases related to stringification.

- SPIR-V target support for the ‘linear noperspective centroid’ input
  interpolation mode.

- New interfaces:
  - The vkd3d_shader_scan_signature_info structure extends the
    vkd3d_shader_compile_info structure, and can be used to retrieve
    descriptions of ‘dxbc-tpf’ and ‘d3dbc’ shader inputs and outputs.
  - vkd3d_shader_free_scan_signature_info() is used to free
    vkd3d_shader_scan_signature_info structures.
  - The VKD3D_SHADER_COMPILE_OPTION_PACK_MATRIX_ORDER compile option can be
    used to specify the default matrix packing order for HLSL sources.
  - The vkd3d_shader_varying_map_info structure extends the
    vkd3d_shader_compile_info structure, and can be used to specify a mapping
    between the outputs of a shader stage and the inputs of the next shader
    stage.
  - vkd3d_shader_build_varying_map() is used to build a mapping between the
    outputs of a shader stage and the inputs of the next shader stage.
  - The VKD3D_SHADER_DESCRIPTOR_INFO_FLAG_RAW_BUFFER flag returned as part of
    the vkd3d_shader_descriptor_info structure indicates the descriptor refers
    to a byte-addressed (‘raw’) buffer resource.


### vkd3d-compiler

- The ‘--matrix-storage-order’ option can used to specify the default matrix
  storage order for HLSL sources.


### vkd3d-dxbc

- vkd3d-dxbc is a new utility that can be used to inspect the contents of DXBC
  blobs.


# What's new in vkd3d 1.8 (22 Jun 2023)

### libvkd3d

- Performance improvements have been made to the code that handles descriptor
  updates. In some applications the improvement can be quite significant.

- Host-visible descriptor heaps are persistently mapped on creation. Some
  applications access resource data from the CPU after calling Unmap(), and
  that's supposed to work in practice.

- 1-dimensional texture unordered-access views and shader resource views are
  implemented.

- Shader resource view, unordered access view, and constant buffer view root
  descriptors with NULL GPU addresses are supported.

- Direct3D 12 descriptor heap destruction is delayed until all contained
  resources are destroyed.


### libvkd3d-shader

- New features for the HLSL source type:
  - Support for the ternary conditional operator "?:".
  - Support for "discard" statements.
  - Support for the "packoffset" keyword.
  - Support for semantics on array types.
  - Support for RWBuffer loads and stores.
  - Register allocation for arrays and structures of resources and samplers
    is implemented.
  - Support for the SV_IsFrontFace pixel shader system-value semantics.
  - Support for using constant expressions as array sizes and indices.
  - Support for dynamic selection of vector components.
  - Support for the following intrinsic functions:
    - D3DCOLORtoUBYTE4()
    - any()
    - asfloat()
    - ddx() and ddy()
    - fmod()
    - log(), log2(), and log10()
    - sign()
    - trunc()
  - The SampleBias(), SampleCmp(), SampleCmpLevelZero(), and SampleGrad()
    texture object methods are implemented.
  - Support for the case-insensitive variants of the "vector" and "matrix"
    data types.
  - Parser support for the "unroll" loop attribute. A warning is output for
    "unroll" without iteration count, and an error is output when an iteration
    count is specified. Actual unrolling is not implemented yet.
  - Parser support for RWStructuredBuffer resources.
  - Parser support for SamplerComparisonState objects. Note that outputting
    compiled effects is not supported yet, but parsing these allows shaders
    containing SamplerComparisonState state objects to be compiled.

- More improvements to HLSL support for the Direct3D shader model 1/2/3
  profiles.

- The section alignment of DXBC blobs produced by
  vkd3d_shader_serialize_dxbc() matches those produced by d3dcompiler more
  closely.

- The "main" function for shaders produced by the SPIR-V target is always
  terminated, even when the source was a TPF shader without explicit "ret"
  instruction.

- Relative addressing of shader input registers is supported by SPIR-V
  targets.


# What's new in vkd3d 1.7 (23 Mar 2023)

### libvkd3d-shader

- New features for the HLSL source type:
  - Support for calling user-defined functions.
  - Support for array parameters to user-defined functions.
  - Much improved support for the Direct3D shader model 1/2/3 profiles.
  - Support for the SV_DispatchThreadID, SV_GroupID, and SV_GroupThreadID
    compute shader system-value semantics.
  - Support for the optional "offset" parameter of the texture object Load()
    method.
  - Support for the all() intrinsic function.
  - Support for the distance() intrinsic function.
  - Support for the exp() and exp2() intrinsic functions.
  - Support for the frac() intrinsic function.
  - Support for the lit() intrinsic function.
  - Support for the reflect() intrinsic function.
  - Support for the sin() and cos() intrinsic functions.
  - Support for the smoothstep() intrinsic function.
  - Support for the sqrt() and rsqrt() intrinsic functions.
  - Support for the step() intrinsic function.
  - Support for the transpose() intrinsic function.
  - Support for the case-insensitive variants of the "float" and "dword" data
    types.
  - Partial support for minimum precision data types like "min16float". These
    are currently interpreted as their regular counterparts.
  - Improved constant propagation support, in particular to constant
    propagation through swizzles.

- HLSL static variables are now properly zero-initialised.

- The Direct3D shader model 4 and 5 disassembler outputs sample counts for
  multi-sampled resource declarations.

- New interfaces:
  - vkd3d_shader_parse_dxbc() provides support for parsing DXBC blobs.
  - vkd3d_shader_serialize_dxbc() provides support for serialising DXBC blobs.
  - vkd3d_shader_free_dxbc() is used to free vkd3d_shader_dxbc_desc
    structures, as returned by vkd3d_shader_parse_dxbc().
  - The VKD3D_SHADER_COMPILE_OPTION_WRITE_TESS_GEOM_POINT_SIZE compile option
    can be used to specify whether SPIR-V shaders targeting Vulkan
    environments should write point sizes for geometry and tessellation
    shaders. If left unspecified, point sizes will be written.


# What's new in vkd3d 1.6 (7 Dec 2022)

### libvkd3d-shader

- New features for the HLSL source type:
  - Initial support for compute shaders.
  - Improved support for initialisation and assignment of compound objects
    like structures and arrays, including casts and implicit conversions.
  - Support for loads and stores of texture resource unordered-access views.
  - Support for function attributes. In particular, the required "numthreads"
    attribute for compute shader entry points is now supported.
  - Support for the asuint() intrinsic function.
  - Support for the length() intrinsic function.
  - Support for the normalize() intrinsic function.
  - Support for integer division and modulus.
  - Support for taking the absolute value of integers.
  - Support for floating-point modulus.


- New interfaces:
  - The VKD3D_SHADER_DESCRIPTOR_INFO_FLAG_UAV_ATOMICS descriptor info flag is
    used to indicate that atomic operations are used on unordered-access view
    descriptors.


### libvkd3d-common

- vkd3d debug output is prefixed with "vkd3d:" in order to make it easier to
  distinguish from output produced by applications or other libraries.


### demos

- The demos now use libvkd3d-shader to compile HLSL shaders at run-time.


# What's new in vkd3d 1.5 (22 Sep 2022)

### libvkd3d-shader

- New features for the HLSL source type:
  - Improved support for HLSL object types (like e.g. ‘Texture2D’) inside
    structures and arrays.
  - Implicitly sized array initialisers.
  - Support for the dot() intrinsic function.
  - Support for the ldexp() intrinsic function.
  - Support for the lerp() intrinsic function.
  - Support for the logical ‘and’, ‘or’, and ‘not’ operators in shader model 4
    and 5 targets.
  - Support for casts from ‘bool’ types in shader model 4 and 5 targets.
  - Constant folding for integer bitwise operations.
  - Constant folding for integer min() and max().

- New interfaces:
  - The VKD3D_SHADER_COMPILE_OPTION_TYPED_UAV compile option can be used to
    specify the SPIR-V format to use for typed unordered access view loads.
    When set to ‘Unknown’, and the ‘shaderStorageImageReadWithoutFormat’
    feature is enabled in the target environment, this allows typed loads from
    multicomponent format unordered access views. If left unspecified, a R32
    format will be used, like in previous versions of libvkd3d-shader.


# What's new in vkd3d 1.4 (22 Jun 2022)

### libvkd3d

- A new descriptor heap implementation using the VK_EXT_descriptor_indexing
  extension. In particular, the new implementation is more efficient when
  large descriptor heaps are used by multiple command lists. The new
  ‘virtual_heaps’ configuration option can be used to select the original
  implementation even when the VK_EXT_descriptor_indexing extension is
  available.

- A new fence implementation using the VK_KHR_timeline_semaphore extension.
  The new implementation addresses a number of edge cases the original
  implementation was unable to, as well as being somewhat more efficient.

- When the VK_EXT_robustness2 extension is available, it is used to implement
  null views. This more accurately matches Direct3D 12 behaviour. For example,
  all reads from such a null view return zeroes, while that isn't necessarily
  the case for out-of-bounds reads with the original implementation.

- New interfaces:
  - vkd3d_set_log_callback() allows writing log output via a custom callback.
    This can be used to integrate vkd3d's log output with other logging
    systems.


### libvkd3d-shader

- New features for the HLSL source type:
  - Support for integer arithmetic, bitwise and shift operations.
  - Support for matrix and vector subscripting.
  - Support for the mul() intrinsic function.
  - Support for matrix copying, casting, and entry-wise operations.
  - Support for complex initialisers.
  - Support for the ‘nointerpolation’ modifier. This modifier is applied by
    default to integer variables.
  - Support for the SV_VertexID semantic.
  - Support for matrix-typed varyings.
  - Constant folding for a number of operators.
  - Copy propagation across branches and loops. This allows use of non-numeric
    variables anywhere in a program, as well as more optimised code for
    accessing numeric variables within branches and loops.

- The disassembler supports the shader model 5 ‘msad’ instruction.

- New interfaces:
  - vkd3d_shader_set_log_callback() allows writing log output via a custom
    callback.


### libvkd3d-utils

- New interfaces:
  - vkd3d_utils_set_log_callback() allows writing log output via a custom
    callback.


### build

- The minimum required version of Vulkan-Headers and SPIRV-Headers for this
  release is version 1.2.139.

- The SONAME_LIBVULKAN configure variable can be used to specify the shared
  object name of the Vulkan library. Because vkd3d loads the Vulkan library
  dynamically, specifying this removes the need for a Vulkan import library at
  build time.

- The ‘crosstests’ target no longer builds Win32/PE demos or tests when these
  were not enabled at configure time.


# What's new in vkd3d 1.3 (2 Mar 2022)

### libvkd3d

- Newly implemented Direct3D 12 features:
  - Root signature support for unbounded descriptor tables.
  - Unordered-access view counters in pixel shaders. These were previously
    only supported in compute shaders.
  - Output merger logical operations.
  - Retrieving CPU/GPU timestamp calibration values. This requires support for
    the VK_EXT_calibrated_timestamps extension.
  - The ‘mirror_once’ texture addressing mode. This requires support for the
    VK_KHR_sampler_mirror_clamp_to_edge extension.

- New interfaces:
  - The vkd3d_host_time_domain_info structure extends the
    vkd3d_instance_create_info structure, and can be used to specify how to
    convert between timestamps and tick counts. If left unspecified, a tick
    is assumed to take 100 nanoseconds.

- Various bug fixes.


### libvkd3d-shader

- New features:
  - Initial support for HLSL compilation and preprocessing. This is an ongoing
    effort; although support for many features is already implemented, support
    for many more isn't yet.
  - Support for disassembling Direct3D byte-code shaders to Direct3D assembly.
  - Support for parsing the legacy Direct3D byte-code format used by Direct3D
    shader model 1, 2, and 3 shaders. In the current vkd3d-shader release,
    only Direct3D assembly is supported as a target for these; we intend to
    support SPIR-V as a target in a future release.

- New features for the SPIR-V target:
  - Support for various aspects of Direct3D shader model 5.1 descriptor
    arrays, including unbounded descriptor arrays, UAV counter arrays, dynamic
    indexing of descriptor arrays, and non-uniform indexing of descriptor
    arrays. With the exception of some special cases, this requires support
    for the SPV_EXT_descriptor_indexing extension in the target environment.
  - Support for double precision floating-point operations.
  - Support for indirect addressing of tessellation control shader inputs.
  - Stencil export. I.e., writing stencil values from shaders. This requires
    support for the SPV_EXT_shader_stencil_export extension in the target
    environment.
  - Support for the Direct3D shader model 4+ ‘precise’ modifier.
  - Support for Direct3D shader model 4+ global resource memory barriers.

New interfaces:
  - vkd3d_shader_preprocess() provides support for preprocessing shaders.
  - The vkd3d_shader_preprocess_info structure extends the
    vkd3d_shader_compile_info structure, and can be used to specify
    preprocessing parameters like preprocessor macro definitions.
  - The vkd3d_shader_hlsl_source_info structure extends the
    vkd3d_shader_compile_info structure, and can be used to specify HLSL
    compilation parameters like the target profile and entry point.
  - The vkd3d_shader_descriptor_offset_info structure extends the
    vkd3d_shader_interface_info structure, and can be used to specify offsets
    into descriptor arrays referenced by shader interface bindings. This
    allows mapping multiple descriptor arrays in a shader to a single binding
    point in the target environment, and helps with mapping between the
    Direct3D 12 and Vulkan binding models.
  - The VKD3D_SHADER_COMPILE_OPTION_API_VERSION compile option can
    be used to specify the version of the libvkd3d-shader API the
    application is targetting. If left unspecified,
    VKD3D_SHADER_API_VERSION_1_2 will be used.

- Various shader translation fixes, for tessellation shaders in particular.


### vkd3d-compiler

- New source and target types:
  - The ‘hlsl’ source type specifies High Level Shader Language source code.
  - The ‘d3d-asm’ target type specifies Direct3D assembly shaders.
  - The ‘d3dbc’ format specifies legacy Direct3D byte-code, which is used for
    Direct3D shader model 1, 2, and 3 shaders.
  - The existing ‘dxbc-tpf’ format can now also be used as a target format.

- New command line options:
  - ‘-E’ can be used to specify the input should only be preprocessed.
  - ‘-e’/‘--entry’ can be used to specify the entry point for HLSL and/or
    SPIR-V shaders.
  - ‘-p’/‘--profile’ can be used to specify the target profile for HLSL
    shaders.

- When no source type is explicitly specified, vkd3d-compiler will attempt to
  determine the source type from the provided input. Note that this is
  intended as a convenience for interactive usage only, and the heuristics
  used are subject to future change. Non-interactive usage of vkd3d-compiler,
  for example in build scripts, should always explicitly specify source and
  target types.

- When no target type is explicitly specified, a default will be chosen based
  on the source type. Like the earlier mentioned source type detection, this
  is intended for interactive usage only.

- vkd3d-compiler will default to colour output if it can determine that the
  output is a colour-capable teleprinter.

- New environment variables:
  - NO_COLOUR/NO_COLOR can be used to disable default colour output.
  See the README for more detailed descriptions and how to use these.


### libvkd3d-utils

- New interfaces:
  - D3DCreateBlob() provides support for creating ID3DBlob objects.
  - D3DPreprocess() provides support for preprocessing HLSL source code.
  - D3DCompile() and D3DCompile2() provide support for compiling HLSL source
    code.


### build

- The ‘gears’ and ‘triangle’ demo applications are installed as ‘vkd3d-gears’
  and ‘vkd3d-triangle’. These were originally intended more as documentation
  than as end-user executables, but there's some value in using them for
  diagnostic purposes, much like e.g. ‘glxgears’.

- The VULKAN_LIBS configure variable is used when detecting the Vulkan
  library.

- Builds for the Microsoft Windows target platform no longer require support
  for POSIX threads. Windows synchronisation primitives are used instead.

- If ncurses is available, it will be use by vkd3d-compiler to determine the
  capabilities of the connected teleprinter, if any.


# What's new in vkd3d 1.2 (22 Sep 2020)

### libvkd3d

- Newly implemented Direct3D 12 features:
  - Multi-sampling.
  - Reserved resources.
  - Instance data step rates. This requires the
    VK_EXT_vertex_attribute_divisor extension.
  - ‘Private data’ APIs for all interfaces.
  - Shader-resource view component mappings.
  - Indirect indexed draws.
  - Indirect draws with a count buffer. This requires the
    VK_KHR_draw_indirect_count extension.
  - Stream output and stream output queries. This requires the
    VK_EXT_transform_feedback extension.
  - Predicated/conditional rendering.
  - Primitive restart.
  - Depth rendering without a pixel shader.
  - Depth clipping. This requires the VK_EXT_depth_clip_enable extension.
  - Rasteriser discard.
  - Dual-source blending.
  - Mapping placed resources.
  - The ReadFromSubresource() and WriteToSubresource() ID3D12Resource methods.
  - Simultaneous access to resources from multiple queues.
  - Null-views. I.e., views without an underlying resource.
  - Several more feature support queries.

- New interfaces:
  - vkd3d_serialize_versioned_root_signature() and
    vkd3d_create_versioned_root_signature_deserializer() provide support for
    versioned root signatures.
  - The vkd3d_application_info structure extends the
    vkd3d_instance_create_info structure, and can be used to pass information
    about the application to libvkd3d. It is analogous to the
    VkApplicationInfo structure in Vulkan. Its ‘api_version’ field should be
    set to the version of the libvkd3d API that the application targets.
  - The vkd3d_optional_device_extensions_info structure extends the
    vkd3d_device_create_info structure, and can be used to pass a list of
    device extensions to enable only when available to libvkd3d. It is
    analogous to the vkd3d_optional_instance_extensions_info structure for
    instance extensions.

- New environment variables:
  - VKD3D_CONFIG can be used to set options that change the behaviour of
    libvkd3d.
  - VKD3D_TEST_BUG can be used to disable bug_if() conditions in the test
    suite.
  - VKD3D_TEST_FILTER can be used to control which tests are run.
  - VKD3D_VULKAN_DEVICE can be used to override the Vulkan physical device
    used by vkd3d.
  See the README for more detailed descriptions and how to use these.

- When the VK_KHR_dedicated_allocation extension is available, dedicated
  allocations may be used for committed resources.

- When the VK_KHR_image_format_list extension is available, it will be used to
  inform the driver about the view formats that a particular mutable Vulkan
  image can be used with. This improves performance on some Vulkan
  implementations.

- When the VK_EXT_debug_marker extension is available, object names set with
  the ID3D12Object SetName() method will be propagated to the underlying
  Vulkan objects.

- Unordered-access view clears are supported on more formats. Previously these
  were limited to integer formats for texture resources, and single component
  integer formats for buffer resources.

- When the D24_UNORM_S8_UINT format is not supported by the Vulkan
  implementation, the D32_SFLOAT_S8_UINT format will be used instead to
  implement the D24_UNORM_S8_UINT and related DXGI formats. This is required
  because the DXGI D24_UNORM_S8_UINT format is mandatory, while the Vulkan
  D24_UNORM_S8_UINT format is optional.

- Various bug fixes.


### libvkd3d-shader

- libvkd3d-shader is now available as a public instead of an internal library.

- New features:
  - Tessellation shaders.
  - Root signature version 1.1 serialisation, deserialisation, and conversion.
  - Multi-sample masks.
  - Per-sample shading.
  - Early depth/stencil test.
  - Conservative depth output.
  - Dual-source blending.
  - Stream output.
  - Viewport arrays.

- Support for OpenGL SPIR-V target environments. This allows SPIR-V produced
  by libvkd3d-shader to be used with GL_ARB_gl_spirv. This includes support
  for OpenGL atomic counters and combined samplers.

- Preliminary support for shader model 5.1 shaders. This is still a work in
  progress. Notably, support for resource arrays is not yet implemented.

- When the SPV_EXT_demote_to_helper_invocation is available, it will be used
  to implement the ‘discard’ shader instruction instead of using SpvOpKill. In
  particular, this ensures the ‘deriv_rtx’ and ‘deriv_rty’ instruction return
  accurate results after a (conditional) ‘discard’ instruction.

- Support for using SPIR-V specialisation constants for shader parameters.

- Support for more shader instructions:
  - bufinfo,
  - eval_centroid,
  - eval_sample_index,
  - ld2ms,
  - sample_b,
  - sample_d,
  - sample_info,
  - samplepos.

- When built against SPIRV-Tools, libvkd3d-shader can produce SPIR-V shaders
  in text form.

- libvkd3d-shader now has its own environment variable (VKD3D_SHADER_DEBUG) to
  control debug output.

- Various shader translation fixes.


### vkd3d-compiler

- When supported by libvkd3d-shader, text form SPIR-V is available as a target
  format, in addition to the existing binary form SPIR-V target format.

- Input from standard input, and output to standard output is supported.


### libvkd3d-utils

- To specify the libvkd3d API version to use when creating vkd3d instances,
  define VKD3D_UTILS_API_VERSION to the desired version before including
  vkd3d_utils.h. If VKD3D_UTILS_API_VERSION is not explicitly defined,
  VKD3D_API_VERSION_1_0 will be used.

- Support for versioned root signatures is provided by the
  D3D12SerializeVersionedRootSignature() and
  D3D12CreateVersionedRootSignatureDeserializer() entry points.


### build

- The minimum required version of Vulkan-Headers and SPIRV-Headers for this
  release is version 1.1.113.

- The minimum required version of widl for this release is version 3.20.

- If doxygen is available, it will be used to build API documentation. By
  default, documentation will be generated in HTML and PDF formats.

- If debug logs are not required or desired, defining VKD3D_NO_TRACE_MESSAGES
  and VKD3D_NO_DEBUG_MESSAGES will prevent them from being included in the
  build. For example, a release build may want to configure with
  ‘CPPFLAGS="-DNDEBUG -DVKD3D_NO_TRACE_MESSAGES -DVKD3D_NO_DEBUG_MESSAGES"’.

- Microsoft Windows is now a supported target platform. To create a build for
  Windows, either cross-compile by configuring with an appropriate --host
  option like for example ‘--host=x86_64-w64-mingw32’, or build on Windows
  itself using an environment like MSYS2 or Cygwin.


# What's new in vkd3d 1.1 (5 Oct 2018)

### libvkd3d

- Initial support for memory heaps and placed resources.

- Improved support for resource views.

- ClearUnorderedAccessViewUint() is implemented for textures.

- Blend factor is implemented.

- Performance improvements.

- A new interface is available for enabling additional Vulkan instance
  extensions.

- A new public function is available for mapping VkFormats to DXGI_FORMATs.

- Support for more DXGI formats.

- Various bug fixes.


### libvkd3d-shader

- Support for geometry shaders.

- Pretty printing is implemented for shader code extracted from DXBC.

- Clip and cull distances are supported.

- Support for more shader instructions:
  - round_ne,
  - sincos,
  - ineg,
  - continue,
  - continuec,
  - gather4_po,
  - gather4_po_c,
  - gather4_c.

- Texel offsets are supported.

- Various shader translation fixes.


### libvkd3d-utils

- Vulkan WSI extensions are detected at runtime.


### build

- Demos are not built by default.

- libxcb is now an optional dependency required only for demos.

- MoltenVK is supported.


# What's included in vkd3d 1.0 (23 May 2018)

### libvkd3d

- libvkd3d is the main component of the vkd3d project. It's a 3D graphics
  library built on top of Vulkan with an API very similar to Direct3D 12.

- A significant number of Direct3D 12 features are implemented, including:
  - Graphics and compute pipelines.
  - Command lists, command allocators and command queues.
  - Descriptors and descriptor heaps.
  - Root signatures.
  - Constant buffer, shader resource, unordered access, render target and depth
    stencil views.
  - Samplers.
  - Static samplers.
  - Descriptors copying.
  - Committed resources.
  - Fences.
  - Queries and query heaps.
  - Resource barriers.
  - Root constants.
  - Basic support for indirect draws and dispatches.
  - Basic support for command signatures.
  - Various Clear*() methods.
  - Various Copy*() methods.


### libvkd3d-shader

- libvkd3d-shader is a library which translates shader model 4 and 5 bytecode
  to SPIR-V. In this release, libvkd3d-shader is an internal library. Its API
  isn't set in stone yet.

- Vertex, pixel and compute shaders are supported. Also, very simple geometry
  shaders should be translated correctly. Issues are expected when trying to
  translate tessellation shaders or more complex geometry shaders.

- A significant number of shader instructions are supported in this release,
  including:
  - Arithmetic instructions.
  - Bit instructions.
  - Comparison instructions.
  - Control flow instructions.
  - Sample, gather and load instructions.
  - Atomic instructions.
  - UAV instructions.

- Root signature serialization and deserialization is implemented.

- Shader model 4 and 5 bytecode parser is imported from wined3d.

- Shader model 5.1 is not supported yet.


### libvkd3d-utils

- libvkd3d-utils contains simple implementations of various functions which
  might be useful for source ports of Direct3D 12 applications.


### demos

- A simple hello triangle Direct3D 12 demo.
- A Direct3D 12 port of glxgears.
