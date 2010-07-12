//
// FOnline skinning effect
//

// Bones
int NumBones = 2;
float4 GroundPos = {0.0f, 0.0f, 0.0f, 0.0f};
static const float CameraAngle = 25.7 * 3.141592654f / 180.0f;
static const float CameraAngleCos = cos(CameraAngle);
static const float CameraAngleSin = sin(CameraAngle);
static const float ShadowAngle = 14.3 * 3.141592654f / 180.0f;
static const float ShadowAngleTan = tan(ShadowAngle);

// Lighting
float4 LightDir = {0.0f, 0.0f, -1.0f, 1.0f};    // Light Direction 
float4 LightDiffuse = {0.6f, 0.6f, 0.6f, 1.0f}; // Light Diffuse
float4 MaterialAmbient : MATERIALAMBIENT = {0.1f, 0.1f, 0.1f, 1.0f};
float4 MaterialDiffuse : MATERIALDIFFUSE = {0.8f, 0.8f, 0.8f, 1.0f};

// Matrix Pallette
static const int MAX_MATRICES = 26;
float4x3    WorldMatrices[MAX_MATRICES] : WORLDMATRIXARRAY;
float4x4    ProjMatrix : VIEWPROJECTION;

// Shaders
struct VS_SIMPLE_INPUT
{
    float4  Pos             : POSITION;
    float3  Normal          : NORMAL;
    float3  Tex             : TEXCOORD0;
};

struct VS_SIMPLE_INPUT_SHADOW
{
    float4  Pos             : POSITION;
};

struct VS_SKIN_INPUT
{
    float4  Pos             : POSITION;
    float4  BlendWeights    : BLENDWEIGHT;
    float4  BlendIndices    : BLENDINDICES;
    float3  Normal          : NORMAL;
    float3  Tex             : TEXCOORD0;
};

struct VS_SKIN_INPUT_SHADOW
{
    float4  Pos             : POSITION;
    float4  BlendWeights    : BLENDWEIGHT;
    float4  BlendIndices    : BLENDINDICES;
};

struct VS_OUTPUT
{
    float4  Pos     : POSITION;
    float4  Diffuse : COLOR;
    float2  Tex     : TEXCOORD0;
};

struct VS_OUTPUT_SHADOW
{
    float4  Pos     : POSITION;
    float4  Diffuse : COLOR;
};

float3 Diffuse(float3 Normal)
{
	return max(0.0f, dot(Normal, LightDir.xyz));
}

VS_OUTPUT VSSimpleNormal(VS_SIMPLE_INPUT input)
{
	VS_OUTPUT output;

	float3 Pos = mul(input.Pos, WorldMatrices[0]);
	float3 Normal = normalize( mul(input.Normal, WorldMatrices[0]) );

	output.Pos = mul(float4(Pos.xyz, 1.0f), ProjMatrix);
	output.Diffuse.xyz = MaterialAmbient.xyz + Diffuse(Normal) * MaterialDiffuse.xyz;
	output.Diffuse.xyz *= LightDiffuse.xyz;
	output.Diffuse.w = 1.0f;
	output.Tex = input.Tex.xy;

	return output;
}

VS_OUTPUT_SHADOW VSSimpleShadow(VS_SIMPLE_INPUT_SHADOW input)
{
    VS_OUTPUT_SHADOW output;

    float3 Pos = mul(input.Pos, WorldMatrices[0]);
	float d = (Pos.y - GroundPos.y) * CameraAngleCos - (Pos.z - GroundPos.z) * CameraAngleSin;
	Pos.y -= d * CameraAngleCos;
	Pos.z += d * CameraAngleSin;
	d *= ShadowAngleTan;
	Pos.y += d * CameraAngleSin;
	Pos.z += d * CameraAngleCos;

	output.Pos = mul(float4(Pos.xyz, 1.0f), ProjMatrix);
	output.Diffuse.xyz = 0.0f;
	output.Diffuse.w = 1.0f;

    return output;
}

VS_OUTPUT VSSkinNormal(VS_SKIN_INPUT input, uniform int bones)
{
    VS_OUTPUT   output;
    float3      Pos = 0.0f;
    float3      Normal = 0.0f;    
    float       LastWeight = 0.0f;

    // Compensate for lack of UBYTE4 on Geforce3
    int4 IndexVector = D3DCOLORtoUBYTE4(input.BlendIndices);

    // Cast the vectors to arrays for use in the for loop below
    float BlendWeightsArray[4] = (float[4])input.BlendWeights;
    int   IndexArray[4]        = (int[4])IndexVector;

    // Calculate the pos/normal using the "normal" weights 
    // and accumulate the weights to calculate the last weight
    for (int iBone = 0; iBone < bones-1; iBone++)
    {
        LastWeight = LastWeight + BlendWeightsArray[iBone];
        Pos += mul(input.Pos, WorldMatrices[IndexArray[iBone]]) * BlendWeightsArray[iBone];
        Normal += mul(input.Normal, WorldMatrices[IndexArray[iBone]]) * BlendWeightsArray[iBone];
    }
    LastWeight = 1.0f - LastWeight; 

    // Now that we have the calculated weight, add in the final influence
    Pos += (mul(input.Pos, WorldMatrices[IndexArray[bones-1]]) * LastWeight);
    Normal += (mul(input.Normal, WorldMatrices[IndexArray[bones-1]]) * LastWeight); 
    
    // Transform position from world space into view and then projection space
    output.Pos = mul(float4(Pos.xyz, 1.0f), ProjMatrix);

    // Normalize normals
    Normal = normalize(Normal);

	// Shade (Ambient + etc.)
	output.Diffuse.xyz = MaterialAmbient.xyz + Diffuse(Normal) * MaterialDiffuse.xyz;
	output.Diffuse.xyz *= LightDiffuse.xyz;
	output.Diffuse.w = 1.0f;

    // Copy the input texture coordinate through
    output.Tex = input.Tex.xy;

    return output;
}

VS_OUTPUT_SHADOW VSSkinShadow(VS_SKIN_INPUT_SHADOW input, uniform int bones)
{
    VS_OUTPUT_SHADOW   output;
    float3             Pos = 0.0f;
    float              LastWeight = 0.0f;

    // Compensate for lack of UBYTE4 on Geforce3
    int4 IndexVector = D3DCOLORtoUBYTE4(input.BlendIndices);

    // Cast the vectors to arrays for use in the for loop below
    float BlendWeightsArray[4] = (float[4])input.BlendWeights;
    int   IndexArray[4]        = (int[4])IndexVector;

    // Calculate the pos using the "normal" weights 
    // and accumulate the weights to calculate the last weight
    for (int iBone = 0; iBone < bones-1; iBone++)
    {
        LastWeight = LastWeight + BlendWeightsArray[iBone];
        Pos += mul(input.Pos, WorldMatrices[IndexArray[iBone]]) * BlendWeightsArray[iBone];
    }
    LastWeight = 1.0f - LastWeight;
	Pos += (mul(input.Pos, WorldMatrices[IndexArray[bones-1]]) * LastWeight);

	float d = (Pos.y - GroundPos.y) * CameraAngleCos - (Pos.z - GroundPos.z) * CameraAngleSin;
	Pos.y -= d * CameraAngleCos;
	Pos.z += d * CameraAngleSin;
	d *= ShadowAngleTan;
	Pos.y += d * CameraAngleSin;
	Pos.z += d * CameraAngleCos;

	output.Pos = mul(float4(Pos.xyz, 1.0f), ProjMatrix);
	output.Diffuse.xyz = 0.0f;
	output.Diffuse.w = 1.0f;

    return output;
}

// Techniques
VertexShader VSArray[10]=
{
	compile vs_1_1 VSSimpleNormal(),
	compile vs_1_1 VSSimpleShadow(),
	compile vs_1_1 VSSkinNormal(1),
	compile vs_1_1 VSSkinNormal(2),
	compile vs_1_1 VSSkinNormal(3),
	compile vs_1_1 VSSkinNormal(4),
	compile vs_1_1 VSSkinShadow(1),
	compile vs_1_1 VSSkinShadow(2),
	compile vs_1_1 VSSkinShadow(3),
	compile vs_1_1 VSSkinShadow(4)
};

technique Simple
{
	pass p0 { VertexShader=(VSArray[0]); }
}
technique SimpleWithShadow
{
	pass p0 { VertexShader=(VSArray[1]); }
	pass p1 { VertexShader=(VSArray[0]); }
}
technique Skin
{
	pass p0{ VertexShader=(VSArray[2+NumBones]); }
}
technique SkinWithShadow
{
	pass p0{ VertexShader=(VSArray[2+NumBones+4]); }
	pass p1{ VertexShader=(VSArray[2+NumBones]); }
}


