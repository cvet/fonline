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
	
	gl_FragColor.rgb = vec3( ( gl_FragColor.r + gl_FragColor.g + gl_FragColor.b ) / 3.0 );
	
	if( gl_FragColor.r < 0.2 || gl_FragColor.r > 0.9 )
		gl_FragColor.r = 0.0;
	else
		gl_FragColor.r = 1.0;
	
	if( gl_FragColor.g < 0.2 || gl_FragColor.g > 0.9 )
		gl_FragColor.g = 0.0;
	else
		gl_FragColor.g = 1.0;
	
	if( gl_FragColor.b < 0.2 || gl_FragColor.b > 0.9 )
		gl_FragColor.b = 0.0;
	else
		gl_FragColor.b = 1.0;
}
#endif
