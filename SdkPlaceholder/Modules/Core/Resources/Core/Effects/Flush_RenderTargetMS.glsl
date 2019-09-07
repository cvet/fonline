#version 150

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
uniform sampler2DMS ColorMap;
uniform vec4        ColorMapSize;
uniform float       ColorMapSamples;

varying vec2 TexCoord;

void main( void )
{
	ivec2 coord = ivec2( TexCoord.x * ColorMapSize[ 0 ], TexCoord.y * ColorMapSize[ 1 ] );
	vec4 color = vec4( 0.0 );
	for( int i = 0; i < int( ColorMapSamples ); i++ )
		color += texelFetch( ColorMap, coord, i );
	color *= 1.0 / ColorMapSamples;
	gl_FragColor = color;
}
#endif
