Effect Pass 0 BlendFunc GL_ONE GL_ONE
Effect Pass 0 BlendEquation GL_MAX

#version 110

const float ShinePower = 5.0;
const float ResultPower = 0.5;

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
	float a = Color.a * ShinePower;
    float r = Color.r * min( a, 1.0 );
    float g = Color.g * min( a, 1.0 );
    float b = Color.b * min( a, 1.0 );
	gl_FragColor = vec4( r, g, b, a ) * ResultPower;
}
#endif
