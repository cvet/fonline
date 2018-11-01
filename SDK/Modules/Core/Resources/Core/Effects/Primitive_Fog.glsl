Effect Pass 0 BlendFunc GL_ONE GL_ONE
Effect Pass 0 BlendEquation GL_MAX

#version 110

const float FogBordersPower = 5.0;
const float FogBlackout = 0.2;
const float AttackBordersPower = 7.0;

#ifdef VERTEX_SHADER
uniform mat4 ProjectionMatrix;

attribute vec2 InPosition;
attribute vec4 InColor;

varying vec4 Color;

void main( void )
{
	gl_Position = ProjectionMatrix * vec4( InPosition, 0.0, 1.0 );
	Color = InColor;
}
#endif

#ifdef FRAGMENT_SHADER
varying vec4 Color;

void main( void )
{
	// Input:
	// a - fog area or attack area (0.0 or 1.0)
	// r - actual distance
	// g - whole distance
	
	// Place fog area in r channel
	if( Color.a < 0.5 )
	{
		float value = ( 1.0 - Color.r ) * FogBordersPower;
		gl_FragColor = vec4( max( value, FogBlackout ), 0.0, 0.0, 1.0 );
	}
	// Place attack area in g/b channel
	else
	{
		float value = ( 1.0 - Color.r ) * AttackBordersPower;
		gl_FragColor = vec4( 0.0, min( value, 0.65 ), Color.g, 1.0 );
	}
}
#endif
