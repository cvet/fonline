//
// FOnline effects specification
//

// Default effects

2D_Default.glsl              // Game sprites
2D_WithoutEgg.glsl           // Game sprites without affecting of transparent egg
3D_Skinned.glsl              // Drawing skinned models
Contour_Default.glsl         // Default effect for contours
Flush_Map.glsl               // Game sprites flushing
Flush_Map_BlackWhite.glsl    // Game sprites flushing with Black/White effect
Flush_Primitive.glsl         // Primitive flushing
Flush_RenderTarget.glsl      // Flushing render target
Flush_RenderTargetMS.glsl    // Flushing multisample render target
Font_Default.glsl            // Default effect for fonts
Interface_Default.glsl       // Default effect for interface
Primitive_Default.glsl       // Default effect for primitives

// Effect directives
// Beginning from 'Effect', must be placed before '#version' directive

Effect Passes X - count of passes (default: 1), every pass compiled in separate shader program;
Effect Pass X Shadow - an indication that pass used for shadow rendering;
Effect Pass X BlendFunc Y Y - use custom blend function (default: GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA), where one of constant used in glBlendFunc;
Effect Pass X BlendEquation Y - use custom blend function (default: GL_FUNC_ADD), where one of constant used in glBlendEquation;

// Effect predefined names
// Use in preprocessor #ifdef, #endif, etc statements

VERTEX_SHADER - compilation of vertex shader;
FRAGMENT_SHADER - compilation of fragment shader;
PASSX - current rendering pass, beginning from zero.

// Effect attributes

vec3 InPosition;             // Vertex position
vec3 InNormal;               // Vertex normal
vec2 InTexCoord;             // Texture coordinate in atlas
vec2 InTexCoordBase;         // Original texture coordinate
vec3 InTangent;              // Tangent
vec3 InBitangent;            // Bitangent
vec4 InBlendWeights;         // Weight of bone
vec4 InBlendIndices;         // Index to WorldMatrices

// Effect uniform variables

float     ZoomFactor;        // Current zoom in game
sampler2D ColorMap;          // Main texture
vec4      ColorMapSize;      // Main texture size: x - width, y - height, z - texel width, w - texel height
float     ColorMapSamples;   // Count of samples
sampler2D EggMap;            // Egg texture
vec4      EggMapSize;        // Size same as ColorMapSize
sampler2D Texture[0..9];     // Custom textures
vec4      Texture[0..9]Size; // Custom textures size (same as ColorMapSize)
vec4      Texture[0..9]AtlasOffset; // Custom textures atlas offsets, xy offset, zw size

mat4      ProjectionMatrix;  // Projection matrix
vec3      GroundPosition;    // Ground position
mat4      WorldMatrices[];   // World matrices

float     Time;              // Current time [0,120), in seconds, cycled
float     TimeGame;          // Current game time [0,120), in seconds, cycled
float     Random1;           // Random value 1 [0,1]
float     Random2;           // Random value 2 [0,1]
float     Random3;           // Random value 3 [0,1]
float     Random4;           // Random value 4 [0,1]
float     EffectValue[0..9]; // Value that changed in scripts, names is same
float     AnimPosProc;       // Current animation process 0%..100% [0,1), (3d specific)
float     AnimPosTime;       // Current animation time, in seconds (3d specific)
