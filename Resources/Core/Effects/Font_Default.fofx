#version 110

#ifdef VERTEX_SHADER
uniform mat4 ProjectionMatrix;

attribute vec2 InPosition;
attribute vec4 InColor;
attribute vec2 InTexCoord;

varying vec4 Color;
varying vec2 TexCoord;

void main( void )
{
	gl_Position = ProjectionMatrix * vec4( InPosition, 0.0, 1.0 );
	Color = InColor;
	TexCoord = InTexCoord;
}
#endif

#ifdef FRAGMENT_SHADER
uniform sampler2D ColorMap;

varying vec4 Color;
varying vec2 TexCoord;

void main( void )
{
	vec4 texColor = texture2D( ColorMap, TexCoord );
	gl_FragColor.rgb = ( texColor.rgb * Color.rgb ) * 2.0;
	gl_FragColor.a = texColor.a * Color.a;
}
#endif
