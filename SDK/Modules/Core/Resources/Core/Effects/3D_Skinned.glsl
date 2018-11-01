Effect Passes 2
Effect Pass 0 Shadow

#version 110

#ifdef PASS1
#ifdef VERTEX_SHADER
uniform mat4 ProjectionMatrix;
uniform mat4 WorldMatrices[ MAX_BONES ];

const vec3 LightDir = vec3( 0.0, 0.0, 1.0 );
const vec3 AmbientColor = vec3( 0.0, 0.0, 0.0 );

attribute vec3 InPosition;
attribute vec3 InNormal;
attribute vec2 InTexCoord;
attribute vec4 InBlendWeights;
attribute vec4 InBlendIndices;

varying vec4 Color;
varying vec2 TexCoord;

void main( void )
{
	// Skinning
	vec4 weights = InBlendWeights;
	vec4 indices = InBlendIndices;
	vec4 position = vec4( 0.0 );
	vec3 normal = vec3( 0.0 );
	for( int i = 0; i < 4; i++ )
	{
		mat4 m44 = WorldMatrices[ int( indices.x ) ];
		mat3 m33 = mat3( m44[ 0 ].xyz, m44[ 1 ].xyz, m44[ 2 ].xyz );
		float w = weights.x;
		position += m44 * vec4( InPosition, 1.0 ) * w;
		normal += m33 * InNormal * w;
		weights = weights.yzwx;
		indices = indices.yzwx;
	}
	
	// Position
	gl_Position = ProjectionMatrix * position;
	
	// Normal
	normal = normalize( normal );
	
	// Shade
	Color.rgb = AmbientColor + max( 0.0, dot( normal, LightDir ) );
	Color.a = 1.0;
	
	// Pass to fragment shader
	TexCoord = InTexCoord;
}
#endif

#ifdef FRAGMENT_SHADER
uniform sampler2D ColorMap;

varying vec4 Color;
varying vec2 TexCoord;

void main( void )
{
	gl_FragColor = texture2D( ColorMap, TexCoord ) * Color;
}
#endif
#endif // PASS1

#ifdef PASS0
#ifdef VERTEX_SHADER
uniform mat4 ProjectionMatrix;
uniform mat4 WorldMatrices[ MAX_BONES ];
uniform vec3 GroundPosition;

attribute vec3 InPosition;
attribute vec2 InTexCoord;
attribute vec4 InBlendWeights;
attribute vec4 InBlendIndices;

varying vec2 TexCoord;

const float CameraAngleCos = 0.9010770213221; // cos( 25.7 )
const float CameraAngleSin = 0.4336590845875; // sin( 25.7 )
const float ShadowAngleTan = 0.2548968037538; // tan( 14.3 )

void main( void )
{
	// Skinning
	vec4 weights = InBlendWeights;
	vec4 indices = InBlendIndices;
	vec4 position = vec4( 0.0 );
	for( int i = 0; i < 4; i++ )
	{
		mat4 m44 = WorldMatrices[ int( indices.x ) ];
		float w = weights.x;
		position += m44 * vec4( InPosition, 1.0 ) * w;
		weights = weights.yzwx;
		indices = indices.yzwx;
	}
	
	// Calculate shadow position
	float d = ( position.y - GroundPosition.y ) * CameraAngleCos;
	d -= ( GroundPosition.z - position.z ) * CameraAngleSin;
	position.y -= d * CameraAngleCos;
	d *= ShadowAngleTan;
	position.y += d * CameraAngleSin;
	position.z -= 10.0;
	
	// Position
	gl_Position = ProjectionMatrix * position;
	
	// Pass to fragment shader
	TexCoord = InTexCoord;
}
#endif

#ifdef FRAGMENT_SHADER
uniform sampler2D ColorMap;

varying vec2 TexCoord;

void main( void )
{
	gl_FragColor.rgb = vec3( 0.0 );
	gl_FragColor.a = texture2D( ColorMap, TexCoord ).a;
}
#endif
#endif // PASS0
