#version 110

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
	gl_FragColor = texture2D( ColorMap, TexCoord );
}
#endif
