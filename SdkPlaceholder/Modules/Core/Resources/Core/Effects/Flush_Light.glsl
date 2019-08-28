Effect Passes 2
Effect Pass 0 BlendFunc GL_DST_COLOR GL_ONE
Effect Pass 1 BlendFunc GL_DST_COLOR GL_ONE

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
	vec4 tex = texture2D( ColorMap, TexCoord );
	if( tex.a == 0.0 )
		discard;
	
	#ifdef PASS0
	gl_FragColor = vec4( tex.a, tex.a, tex.a, 1.0 );
	#endif
	#ifdef PASS1
	gl_FragColor = vec4( tex.r, tex.g, tex.b, 1.0 );
	#endif
}
#endif
