#version 110

#ifdef VERTEX_SHADER
uniform mat4 ProjectionMatrix;

attribute vec2 InPosition;
attribute vec4 InColor;
attribute vec2 InTexCoord;
attribute vec2 InTexEggCoord;

varying vec4 Color;
varying vec2 TexCoord;
varying vec2 TexEggCoord;

void main( void )
{
	gl_Position = ProjectionMatrix * vec4( InPosition, 0.0, 1.0 );
	Color = InColor;
	TexCoord = InTexCoord;
	TexEggCoord = InTexEggCoord;
}
#endif

#ifdef FRAGMENT_SHADER
uniform sampler2D ColorMap;
uniform sampler2D EggMap;

varying vec4 Color;
varying vec2 TexCoord;
varying vec2 TexEggCoord;

void main( void )
{
	vec4 texColor = texture2D( ColorMap, TexCoord );
	gl_FragColor.rgb = ( texColor.rgb * Color.rgb ) * 2.0;
	gl_FragColor.a = texColor.a * Color.a;
	if( TexEggCoord.x >= 0.0 )
		gl_FragColor.a *= texture2D( EggMap, TexEggCoord ).a;
}
#endif
