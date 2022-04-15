#version 110

const float FogBlackout = 0.2;
const float AttackBlackout = 0.4;
const float AttackAroundShining = 0.4;

#ifdef VERTEX_SHADER
uniform mat4 ProjectionMatrix;

attribute vec2 InPosition;
attribute vec2 InTexCoord;

varying vec2 TexCoord;

void main( void )
{
	gl_Position = ProjectionMatrix * vec4( InPosition, 0.0, 1.0 );
	TexCoord = InTexCoord;
}
#endif

#ifdef FRAGMENT_SHADER
uniform sampler2D ColorMap;

varying vec2 TexCoord;

void main( void )
{
	vec4 tex = texture2D( ColorMap, TexCoord );
	
	float fog = tex.r;
	float attack = tex.g;
	float attackDist = tex.b;
	
	float a = 1.0 - max( fog, FogBlackout );
	if( attackDist > 0.0 )
	{
		a = max( attack * AttackBlackout, a );
		gl_FragColor = vec4( attack - a, 0.0, 0.0, a - max( AttackAroundShining - attackDist, 0.0 ) );
	}
	else
	{
		if( a == 0.0 )
			discard;
		gl_FragColor = vec4( 0.0, 0.0, 0.0, a );
	}
}
#endif
