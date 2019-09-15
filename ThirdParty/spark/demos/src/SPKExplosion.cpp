//////////////////////////////////////////////////////////////////////////////////
// SPARK particle engine														//
// Copyright (C) 2008-2011 - Julien Fryer - julienfryer@gmail.com				//
//																				//
// This software is provided 'as-is', without any express or implied			//
// warranty.  In no event will the authors be held liable for any damages		//
// arising from the use of this software.										//
//																				//
// Permission is granted to anyone to use this software for any purpose,		//
// including commercial applications, and to alter it and redistribute it		//
// freely, subject to the following restrictions:								//
//																				//
// 1. The origin of this software must not be misrepresented; you must not		//
//    claim that you wrote the original software. If you use this software		//
//    in a product, an acknowledgment in the product documentation would be		//
//    appreciated but is not required.											//
// 2. Altered source versions must be plainly marked as such, and must not be	//
//    misrepresented as being the original software.							//
// 3. This notice may not be removed or altered from any source distribution.	//
//////////////////////////////////////////////////////////////////////////////////

#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <ctime>

#if defined(WIN32) || defined(_WIN32)
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glu.h>

#include <SDL.h>
#include <SPARK.h>
#include <SPARK_GL.h>

SPK::Vector3D camPos;
float camAngleY = 0.0f;
float camAngleX = 0.0f;
float camPosZ = 10.0f;

int deltaTime = 0;

int screenWidth;
int screenHeight;
float screenRatio;

typedef std::list<SPK::Ref<SPK::System> > SystemList;
SystemList particleSystems;
SPK::Ref<SPK::System> baseSystem;

const float PI = 3.14159f;

bool enableBB = false;

// Loads a texture
bool loadTexture(GLuint& index,char* path,GLuint type,GLuint clamp,bool mipmap)
{
	SDL_Surface *particleImg; 
	particleImg = SDL_LoadBMP(path);
	if (particleImg == NULL)
	{
		std::cout << "Unable to load bitmap :" << SDL_GetError() << std::endl;
		return false;
	}

	// converts from BGR to RGB
	if ((type == GL_RGB)||(type == GL_RGBA))
	{
		const int offset = (type == GL_RGB ? 3 : 4);
		unsigned char* iterator = static_cast<unsigned char*>(particleImg->pixels);
		unsigned char *tmp0,*tmp1;
		for (int i = 0; i < particleImg->w * particleImg->h; ++i)
		{
			tmp0 = iterator;
			tmp1 = iterator + 2;
			std::swap(*tmp0,*tmp1);
			iterator += offset;
		}
	}

	glGenTextures(1,&index);
	glBindTexture(GL_TEXTURE_2D,index);

	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,clamp);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,clamp);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

	if (mipmap)
	{
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);

		gluBuild2DMipmaps(GL_TEXTURE_2D,
			type,
			particleImg->w,
			particleImg->h,
			type,
			GL_UNSIGNED_BYTE,
			particleImg->pixels);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);

		glTexImage2D(GL_TEXTURE_2D,
			0,
			type,
			particleImg->w,
			particleImg->h,
			0,
			type,
			GL_UNSIGNED_BYTE,
			particleImg->pixels);
	}

	SDL_FreeSurface(particleImg);

	return true;
}

// Draws the bounding box for a particle group
void drawBoundingBox(const SPK::System& system)
{
	if (!system.isAABBComputationEnabled())
		return;

	const SPK::Vector3D& AABBMin = system.getAABBMin();
	const SPK::Vector3D& AABBMax = system.getAABBMax();

	glDisable(GL_TEXTURE_2D);
	glBegin(GL_LINES);
	glColor3f(1.0f,0.0f,0.0f);

	glVertex3f(AABBMin.x,AABBMin.y,AABBMin.z);
	glVertex3f(AABBMax.x,AABBMin.y,AABBMin.z);
	
	glVertex3f(AABBMin.x,AABBMin.y,AABBMin.z);
	glVertex3f(AABBMin.x,AABBMax.y,AABBMin.z);

	glVertex3f(AABBMin.x,AABBMin.y,AABBMin.z);
	glVertex3f(AABBMin.x,AABBMin.y,AABBMax.z);

	glVertex3f(AABBMax.x,AABBMax.y,AABBMax.z);
	glVertex3f(AABBMin.x,AABBMax.y,AABBMax.z);

	glVertex3f(AABBMax.x,AABBMax.y,AABBMax.z);
	glVertex3f(AABBMax.x,AABBMin.y,AABBMax.z);

	glVertex3f(AABBMax.x,AABBMax.y,AABBMax.z);
	glVertex3f(AABBMax.x,AABBMax.y,AABBMin.z);

	glVertex3f(AABBMin.x,AABBMin.y,AABBMax.z);
	glVertex3f(AABBMax.x,AABBMin.y,AABBMax.z);

	glVertex3f(AABBMin.x,AABBMin.y,AABBMax.z);
	glVertex3f(AABBMin.x,AABBMax.y,AABBMax.z);

	glVertex3f(AABBMin.x,AABBMax.y,AABBMin.z);
	glVertex3f(AABBMax.x,AABBMax.y,AABBMin.z);

	glVertex3f(AABBMin.x,AABBMax.y,AABBMin.z);
	glVertex3f(AABBMin.x,AABBMax.y,AABBMax.z);

	glVertex3f(AABBMax.x,AABBMin.y,AABBMin.z);
	glVertex3f(AABBMax.x,AABBMax.y,AABBMin.z);

	glVertex3f(AABBMax.x,AABBMin.y,AABBMin.z);
	glVertex3f(AABBMax.x,AABBMin.y,AABBMax.z);
	glEnd();
}

// Renders the scene
void render()
{
	glEnable(GL_DEPTH_TEST);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45,screenRatio,0.01f,80.0f);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();                       

	glTranslatef(0.0f,0.0f,-camPosZ);
	glRotatef(camAngleX,1.0f,0.0f,0.0f);
	glRotatef(camAngleY,0.0f,1.0f,0.0f);

	// Renders all the particle systems
	for (SystemList::const_iterator it = particleSystems.begin(); it != particleSystems.end(); ++it)
	{
		drawBoundingBox(**it);
		(*it)->renderParticles();
	}

	SDL_GL_SwapBuffers();
}

// Creates the base system and returns its ID
SPK::Ref<SPK::System> createParticleSystemBase(GLuint textureExplosion,GLuint textureFlash,GLuint textureSpark1,GLuint textureSpark2,GLuint textureWave)
{
	///////////////
	// Renderers //
	///////////////

	// smoke renderer
	SPK::Ref<SPK::GL::GLQuadRenderer> smokeRenderer = SPK::GL::GLQuadRenderer::create();
	smokeRenderer->setTexturingMode(SPK::TEXTURE_MODE_2D);
	smokeRenderer->setTexture(textureExplosion);
	smokeRenderer->setAtlasDimensions(2,2); // uses 4 different patterns in the texture
	smokeRenderer->setBlendMode(SPK::BLEND_MODE_ALPHA);
	smokeRenderer->enableRenderingOption(SPK::RENDERING_OPTION_DEPTH_WRITE,false);
	smokeRenderer->setShared(true);

	// flame renderer
	SPK::Ref<SPK::GL::GLQuadRenderer> flameRenderer = SPK::GL::GLQuadRenderer::create();
	flameRenderer->setTexturingMode(SPK::TEXTURE_MODE_2D);
	flameRenderer->setTexture(textureExplosion);
	flameRenderer->setAtlasDimensions(2,2);
	flameRenderer->setBlendMode(SPK::BLEND_MODE_ADD);
	flameRenderer->enableRenderingOption(SPK::RENDERING_OPTION_DEPTH_WRITE,false);
	flameRenderer->setShared(true);

	// flash renderer
	SPK::Ref<SPK::GL::GLQuadRenderer> flashRenderer = SPK::GL::GLQuadRenderer::create();
	flashRenderer->setTexturingMode(SPK::TEXTURE_MODE_2D);
	flashRenderer->setTexture(textureFlash);
	flashRenderer->setBlendMode(SPK::BLEND_MODE_ADD);
	flashRenderer->enableRenderingOption(SPK::RENDERING_OPTION_DEPTH_WRITE,false);
	flashRenderer->setShared(true);

	// spark 1 renderer
	SPK::Ref<SPK::GL::GLQuadRenderer> spark1Renderer = SPK::GL::GLQuadRenderer::create();
	spark1Renderer->setTexturingMode(SPK::TEXTURE_MODE_2D);
	spark1Renderer->setTexture(textureSpark1);
	spark1Renderer->setBlendMode(SPK::BLEND_MODE_ADD);
	spark1Renderer->enableRenderingOption(SPK::RENDERING_OPTION_DEPTH_WRITE,false);
	spark1Renderer->setOrientation(SPK::DIRECTION_ALIGNED); // sparks are oriented function of their velocity
	spark1Renderer->setScale(0.05f,1.0f); // thin rectangles
	spark1Renderer->setShared(true);

	// spark 2 renderer
	SPK::Ref<SPK::GL::GLRenderer> spark2Renderer = SPK_NULL_REF;
	if (SPK::GL::GLPointRenderer::isPointSpriteSupported() && SPK::GL::GLPointRenderer::isWorldSizeSupported())// uses point sprite if possible
	{
		SPK::GL::GLPointRenderer::setPixelPerUnit(45.0f * PI / 180.f,screenHeight);
		SPK::Ref<SPK::GL::GLPointRenderer> pointRenderer = SPK::GL::GLPointRenderer::create();
		pointRenderer->setType(SPK::POINT_TYPE_SPRITE);
		pointRenderer->setTexture(textureSpark2);
		pointRenderer->enableWorldSize(true);
		spark2Renderer = pointRenderer;
	}
	else
	{
		SPK::Ref<SPK::GL::GLQuadRenderer> quadRenderer = SPK::GL::GLQuadRenderer::create();
		quadRenderer->setTexturingMode(SPK::TEXTURE_MODE_2D);
		quadRenderer->setTexture(textureSpark2);
		spark2Renderer = quadRenderer;
	}
	spark2Renderer->setBlendMode(SPK::BLEND_MODE_ADD);
	spark2Renderer->enableRenderingOption(SPK::RENDERING_OPTION_DEPTH_WRITE,false);
	spark2Renderer->setShared(true);

	// wave renderer
	SPK::Ref<SPK::GL::GLQuadRenderer> waveRenderer = SPK::GL::GLQuadRenderer::create();
	waveRenderer->setTexturingMode(SPK::TEXTURE_MODE_2D);
	waveRenderer->setTexture(textureWave);
	waveRenderer->setBlendMode(SPK::BLEND_MODE_ALPHA);
	waveRenderer->enableRenderingOption(SPK::RENDERING_OPTION_DEPTH_WRITE,false);
	waveRenderer->enableRenderingOption(SPK::RENDERING_OPTION_ALPHA_TEST,true); // uses the alpha test
	waveRenderer->setAlphaTestThreshold(0.0f);
	waveRenderer->setOrientation(SPK::FIXED_ORIENTATION); // the orientation is fixed
	waveRenderer->lookVector.set(0.0f,1.0f,0.0f);
	waveRenderer->upVector.set(1.0f,0.0f,0.0f); // we dont really care about the up axis
	waveRenderer->setShared(true);

	//////////////
	// Emitters //
	//////////////

	// This zone will be used by several emitters
	SPK::Ref<SPK::Sphere> explosionSphere = SPK::Sphere::create(SPK::Vector3D(0.0f,0.0f,0.0f),0.4f);

	// smoke emitter
	SPK::Ref<SPK::RandomEmitter> smokeEmitter = SPK::RandomEmitter::create();
	smokeEmitter->setZone(SPK::Sphere::create(SPK::Vector3D(0.0f,0.0f,0.0f),0.6f),false);
	smokeEmitter->setTank(15);
	smokeEmitter->setFlow(-1);
	smokeEmitter->setForce(0.02f,0.04f);

	// flame emitter
	SPK::Ref<SPK::NormalEmitter> flameEmitter = SPK::NormalEmitter::create();
	flameEmitter->setZone(explosionSphere);
	flameEmitter->setTank(15);
	flameEmitter->setFlow(-1);
	flameEmitter->setForce(0.06f,0.1f);

	// flash emitter
	SPK::Ref<SPK::StaticEmitter> flashEmitter = SPK::StaticEmitter::create();
	flashEmitter->setZone(SPK::Sphere::create(SPK::Vector3D(0.0f,0.0f,0.0f),0.1f));
	flashEmitter->setTank(3);
	flashEmitter->setFlow(-1);

	// spark 1 emitter
	SPK::Ref<SPK::NormalEmitter> spark1Emitter = SPK::NormalEmitter::create();
	spark1Emitter->setZone(explosionSphere);
	spark1Emitter->setTank(20);
	spark1Emitter->setFlow(-1);	
	spark1Emitter->setForce(2.0f,3.0f);
	spark1Emitter->setInverted(true);

	// spark 2 emitter
	SPK::Ref<SPK::NormalEmitter> spark2Emitter = SPK::NormalEmitter::create();
	spark2Emitter->setZone(explosionSphere);
	spark2Emitter->setTank(400);
	spark2Emitter->setFlow(-1);
	spark2Emitter->setForce(0.4f,1.0f);
	spark2Emitter->setInverted(true);

	// wave emitter
	SPK::Ref<SPK::StaticEmitter> waveEmitter = SPK::StaticEmitter::create();
	waveEmitter->setZone(SPK::Point::create());
	waveEmitter->setTank(1);
	waveEmitter->setFlow(-1);
	
	////////////
	// Groups //
	////////////

	SPK::Ref<SPK::System> system = SPK::System::create(false); // not initialized as it is the base system
	system->setName("Explosion");

	SPK::Ref<SPK::ColorGraphInterpolator> colorInterpolator;
	SPK::Ref<SPK::FloatGraphInterpolator> paramInterpolator;

	// smoke group
	colorInterpolator = SPK::ColorGraphInterpolator::create();
	colorInterpolator->addEntry(0.0f,0x33333300);
	colorInterpolator->addEntry(0.4f,0x33333366,0x33333399);
	colorInterpolator->addEntry(0.6f,0x33333366,0x33333399);
	colorInterpolator->addEntry(1.0f,0x33333300);

	SPK::Ref<SPK::Group> smokeGroup = system->createGroup(15);
	smokeGroup->setName("Smoke");
	smokeGroup->setPhysicalRadius(0.0f);
	smokeGroup->setLifeTime(2.5f,3.0f);
	smokeGroup->setRenderer(smokeRenderer);
	smokeGroup->addEmitter(smokeEmitter);	
	smokeGroup->setColorInterpolator(colorInterpolator);
	smokeGroup->setParamInterpolator(SPK::PARAM_SCALE,SPK::FloatRandomInterpolator::create(0.3f,0.4f,0.5f,0.7f));
	smokeGroup->setParamInterpolator(SPK::PARAM_TEXTURE_INDEX,SPK::FloatRandomInitializer::create(0.0f,4.0f));
	smokeGroup->setParamInterpolator(SPK::PARAM_ANGLE,SPK::FloatRandomInterpolator::create(0.0f,PI * 0.5f,0.0f,PI * 0.5f));
	smokeGroup->addModifier(SPK::Gravity::create(SPK::Vector3D(0.0f,0.05f,0.0f)));
	
	// flame group
	colorInterpolator = SPK::ColorGraphInterpolator::create();
	colorInterpolator->addEntry(0.0f,0xFF8033FF);
	colorInterpolator->addEntry(0.5f,0x995933FF);
	colorInterpolator->addEntry(1.0f,0x33333300);

	paramInterpolator = SPK::FloatGraphInterpolator::create();
	paramInterpolator->addEntry(0.0f,0.125f);
	paramInterpolator->addEntry(0.02f,0.3f,0.4f);
	paramInterpolator->addEntry(1.0f,0.5f,0.7f);	

	SPK::Ref<SPK::Group> flameGroup = system->createGroup(15);
	flameGroup->setName("Flame");
	flameGroup->setLifeTime(1.5f,2.0f);
	flameGroup->setRenderer(flameRenderer);
	flameGroup->addEmitter(flameEmitter);
	flameGroup->setColorInterpolator(colorInterpolator);
	flameGroup->setParamInterpolator(SPK::PARAM_SCALE,paramInterpolator);
	flameGroup->setParamInterpolator(SPK::PARAM_TEXTURE_INDEX,SPK::FloatRandomInitializer::create(0.0f,4.0f));
	flameGroup->setParamInterpolator(SPK::PARAM_ANGLE,SPK::FloatRandomInterpolator::create(0.0f,PI * 0.5f,0.0f,PI * 0.5f));
	
	// flash group
	paramInterpolator = SPK::FloatGraphInterpolator::create();
	paramInterpolator->addEntry(0.0f,0.1f);
	paramInterpolator->addEntry(0.25f,0.5f,1.0f);

	SPK::Ref<SPK::Group> flashGroup = system->createGroup(3);
	flashGroup->setName("Flash");
	flashGroup->setLifeTime(0.2f,0.2f);
	flashGroup->addEmitter(flashEmitter);
	flashGroup->setRenderer(flashRenderer);
	flashGroup->setColorInterpolator(SPK::ColorSimpleInterpolator::create(0xFFFFFFFF,0xFFFFFF00));
	flashGroup->setParamInterpolator(SPK::PARAM_SCALE,paramInterpolator);
	flashGroup->setParamInterpolator(SPK::PARAM_ANGLE,SPK::FloatRandomInitializer::create(0.0f,2.0f * PI));

	// spark 1 group
	SPK::Ref<SPK::Group> spark1Group = system->createGroup(20);
	spark1Group->setName("Spark 1");
	spark1Group->setPhysicalRadius(0.0f);
	spark1Group->setLifeTime(0.2f,1.0f);
	spark1Group->addEmitter(spark1Emitter);
	spark1Group->setRenderer(spark1Renderer);
	spark1Group->setColorInterpolator(SPK::ColorSimpleInterpolator::create(0xFFFFFFFF,0xFFFFFF00));
	spark1Group->setParamInterpolator(SPK::PARAM_SCALE,SPK::FloatRandomInitializer::create(0.1f,0.2f));
	spark1Group->addModifier(SPK::Gravity::create(SPK::Vector3D(0.0f,-0.75f,0.0f)));

	// spark 2 group
	SPK::Ref<SPK::Group> spark2Group = system->createGroup(400);
	spark2Group->setName("Spark 2");
	spark2Group->setGraphicalRadius(0.01f);
	spark2Group->setLifeTime(1.0f,3.0f);
	spark2Group->addEmitter(spark2Emitter);
	spark2Group->setRenderer(spark2Renderer);
	spark2Group->setColorInterpolator(SPK::ColorRandomInterpolator::create(0xFFFFB2FF,0xFFFFB2FF,0xFF4C4C00,0xFFFF4C00));
	spark2Group->setParamInterpolator(SPK::PARAM_MASS,SPK::FloatRandomInitializer::create(0.5f,2.5f));
	spark2Group->addModifier(SPK::Gravity::create(SPK::Vector3D(0.0f,-0.1f,0.0f)));
	spark2Group->addModifier(SPK::Friction::create(0.4f));

	// wave group
	paramInterpolator = SPK::FloatGraphInterpolator::create();
	paramInterpolator->addEntry(0.0f,0.0f);
	paramInterpolator->addEntry(0.2f,0.0f);
	paramInterpolator->addEntry(1.0f,3.0f);

	SPK::Ref<SPK::Group> waveGroup = system->createGroup(1);
	waveGroup->setName("Wave");
	waveGroup->setLifeTime(0.8f,0.8f);
	waveGroup->addEmitter(waveEmitter);
	waveGroup->setRenderer(waveRenderer);
	waveGroup->setColorInterpolator(SPK::ColorSimpleInterpolator::create(0xFFFFFF20,0xFFFFFF00));
	waveGroup->setParamInterpolator(SPK::PARAM_SCALE,paramInterpolator);

	// Gets a pointer to the base
	return system;
}

// Creates a particle system from the base system
SPK::Ref<SPK::System> createParticleSystem(const SPK::Vector3D& pos)
{
	// Creates a copy of the base system
	SPK::Ref<SPK::System> system = SPK::SPKObject::copy(baseSystem);

	// Locates the system at the given position
	system->initialize();
	system->getTransform().setPosition(pos);
	system->updateTransform(); // updates the world transform of system and its children
	system->enableAABBComputation(enableBB);

	return system;
}

// This function is used to sort the systems from front to back
bool cmpDistToCam(const SPK::Ref<SPK::System>& system0,const SPK::Ref<SPK::System>& system1)
{
	return getSqrDist(system0->getTransform().getWorldPos(),camPos) > getSqrDist(system1->getTransform().getWorldPos(),camPos);
}

// Main function
int main(int argc, char *argv[])
{
	SDL_Event event;

	// inits SDL
	SDL_Init(SDL_INIT_VIDEO);
	SDL_WM_SetCaption("SPARK Explosion demo",NULL);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);

	// vsync
	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL,0);

	SDL_SetVideoMode(0,0,32,SDL_OPENGL | SDL_FULLSCREEN);
	SDL_ShowCursor(0);

	SDL_Surface& screen = *SDL_GetVideoSurface();

	// inits openGL
	screenWidth = screen.w;
	screenHeight = screen.h;
	screenRatio = (float)screen.w / (float)screen.h;
	
	glClearColor(0.0f,0.0f,0.0f,1.0f);
	glViewport(0,0,screen.w,screen.h);

	// Loads particle texture
	GLuint textureExplosion;
	if (!loadTexture(textureExplosion,"res/explosion.bmp",GL_ALPHA,GL_CLAMP,false))
		return 1;

	GLuint textureFlash;
	if (!loadTexture(textureFlash,"res/flash.bmp",GL_RGB,GL_CLAMP,false))
		return 1;

	GLuint textureSpark1;
	if (!loadTexture(textureSpark1,"res/spark1.bmp",GL_RGB,GL_CLAMP,false))
		return 1;

	GLuint textureSpark2;
	if (!loadTexture(textureSpark2,"res/point.bmp",GL_ALPHA,GL_CLAMP,false))
		return 1;

	GLuint textureWave;
	if (!loadTexture(textureWave,"res/wave.bmp",GL_RGBA,GL_CLAMP,false))
		return 1;

	// Sets the update step
	SPK::System::setClampStep(true,0.1f);			// clamp the step to 100 ms
	SPK::System::useAdaptiveStep(0.001f,0.01f);		// use an adaptive step from 1ms to 10ms (1000fps to 100fps)

	// creates the base system
	baseSystem = createParticleSystemBase(textureExplosion,textureFlash,textureSpark1,textureSpark2,textureWave);
	
	bool exit = false;
	bool paused = false;

	while (SDL_PollEvent(&event)){}
	
	std::deque<unsigned int> frameFPS;
	frameFPS.push_back(SDL_GetTicks());

	float spacePressed = -1.0f;

	while(!exit)
	{
		while (SDL_PollEvent(&event))
		{
			// if space is pressed, a new system is added
			if (((event.type == SDL_KEYDOWN)&&(event.key.keysym.sym == SDLK_SPACE))||
				(event.type == SDL_MOUSEBUTTONDOWN)&&
				((event.button.button == SDL_BUTTON_LEFT)||(event.button.button == SDL_BUTTON_RIGHT)))
			{
				spacePressed = 1000.0f;
			}

			if (((event.type == SDL_KEYUP)&&(event.key.keysym.sym == SDLK_SPACE))||
				(event.type == SDL_MOUSEBUTTONUP)&&
				((event.button.button == SDL_BUTTON_LEFT)||(event.button.button == SDL_BUTTON_RIGHT)))
			{
				spacePressed = -1.0f;
			}

			// if esc is pressed, exit
			if ((event.type == SDL_KEYDOWN)&&(event.key.keysym.sym == SDLK_ESCAPE))
				exit = true;

			// if pause is pressed, the system is paused
			if ((event.type == SDL_KEYDOWN)&&(event.key.keysym.sym == SDLK_PAUSE))
				paused = !paused;

			// Moves the camera with the mouse
			if (event.type == SDL_MOUSEMOTION)
			{
				camAngleY -= event.motion.xrel * 0.05f;
				camAngleX -= event.motion.yrel * 0.05f;
				if (camAngleX > 90.0f)
					camAngleX = 90.0f;
				if (camAngleX < -90.0f)
					camAngleX = -90.0f;
			}

			// Zoom in and out
			if (event.type == SDL_MOUSEBUTTONDOWN)
			{
				if (event.button.button == SDL_BUTTON_WHEELDOWN)
					camPosZ = min(10.0f,camPosZ + 0.5f);
				if (event.button.button == SDL_BUTTON_WHEELUP)
					camPosZ = max(0.5f,camPosZ - 0.5f);
			}

			if ((event.type == SDL_KEYDOWN)&&(event.key.keysym.sym == SDLK_F2))
			{
				enableBB = !enableBB;
				for (SystemList::const_iterator it = particleSystems.begin(); it != particleSystems.end(); ++it)
					(*it)->enableAABBComputation(enableBB);
			}
		}

		if (!paused)
		{
			// if space is pressed, a new system is added
			if (spacePressed >= 0.0f)
			{
				spacePressed += deltaTime;

				if (spacePressed >= 1000.0f)
				{
					SPK::Vector3D position(SPK_RANDOM(-2.0f,2.0f),SPK_RANDOM(-2.0f,2.0f),SPK_RANDOM(-2.0f,2.0f));
					particleSystems.push_back(createParticleSystem(position));

					spacePressed = 0.0f;
				}
			}
		}

		// Computes the camera position and sort the systems from back to front
		float cosX = cos(camAngleX * PI / 180.0f);
		float sinX = sin(camAngleX * PI / 180.0f);
		float cosY = cos(camAngleY * PI / 180.0f);
		float sinY = sin(camAngleY * PI / 180.0f);
		camPos.set(-camPosZ * sinY * cosX,camPosZ * sinX,camPosZ * cosY * cosX);
		particleSystems.sort(cmpDistToCam);

		if (!paused)
		{
			SystemList::iterator it = particleSystems.begin();
			while(it != particleSystems.end())
			{
				// Updates the particle systems
				if (!(*it)->updateParticles(deltaTime * 0.001f))
				{
					// And erases its entry in the container
					it = particleSystems.erase(it);
				}
				else
					++it;
			}
		}

		// Renders scene
		render();

		// Computes delta time
		int time = SDL_GetTicks();
		deltaTime = time - frameFPS.back();

		frameFPS.push_back(time);

		while((frameFPS.back() - frameFPS.front() > 1000)&&(frameFPS.size() > 2))
			frameFPS.pop_front();
	}

	SPK::IO::Manager::get().save("explosion.xml",baseSystem);
	SPK::IO::Manager::get().save("explosion.spk",baseSystem);
	
	SPK_DUMP_MEMORY
	particleSystems.clear();
	baseSystem = SPK_NULL_REF;
	SPK_DUMP_MEMORY

	SDL_Quit();

	system("pause"); // Waits for the user to close the console

	return 0;
}