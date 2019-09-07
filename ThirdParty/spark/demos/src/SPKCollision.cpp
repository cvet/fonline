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

float angleX = 0.0f;
float angleZ = 0.0f;
const float CAM_POS_Z = 2.75f;

const float PI = 3.14159265358979323846f;

const size_t NB_PARTICLES = 750;
const float RADIUS = 0.06f;

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

void drawBox(float r,float g,float b,const SPK::Vector3D& AABBMin,const SPK::Vector3D& AABBMax)
{
	glDisable(GL_TEXTURE_2D);
	glBegin(GL_LINES);
	glColor3f(r,g,b);

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

void drawCell(const SPK::Octree& octree,const SPK::Octree::Cell& cell,const SPK::Vector3D& offset,const SPK::Vector3D& dim)
{
	if (!cell.particles.empty())
	{
		drawBox(0.0f,1.0f,0.0f,offset,offset + dim);				
		if (cell.hasChildren)
			std::cout << "ANORMAL BEHAVIOR" << std::endl;
	}
	else if (cell.hasChildren)
	{
		SPK::Vector3D childDim = dim * 0.5f;
		for (size_t i = 0; i < 8; ++i)
		{
			SPK::Vector3D childOffset(((i >> 2) & 1) * childDim.x,((i >> 1) & 1) * childDim.y,(i & 1) * childDim.z);
			drawCell(octree,octree.getCell(cell.children[i]),childOffset + offset,childDim);
		}
	}
}

// Main function
int main(int argc, char *argv[])
{
	// Inits SDL
	SDL_Init(SDL_INIT_VIDEO);
	SDL_WM_SetCaption("SPARK Collision 2 Demo",NULL);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL,0); // vsync

	SDL_SetVideoMode(0,0,32,SDL_OPENGL | SDL_FULLSCREEN);
	SDL_ShowCursor(0);

	SDL_Surface& screen = *SDL_GetVideoSurface();

	// Inits openGL
	int screenWidth = screen.w;
	int screenHeight = screen.h;
	float screenRatio = (float)screen.w / (float)screen.h;
	
	glClearColor(0.0f,0.0f,0.0f,1.0f);
	glViewport(0,0,screen.w,screen.h);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45,screenRatio,0.01f,20.0f);
	glEnable(GL_DEPTH_TEST);

	// Loads particle texture
	GLuint textureParticle;
	if (!loadTexture(textureParticle,"res/ball.bmp",GL_RGBA,GL_CLAMP,false))
	{}//return 1;

	// Inits Particle Engine
	SPK::System::setClampStep(true,0.005f);			// clamp the step to 10 ms
	SPK::System::useAdaptiveStep(0.001f,0.005f);	// use an adaptive step from 1ms to 10ms (1000fps to 100fps)

	{
	// Renderers
	SPK::Ref<SPK::GL::GLPointRenderer> basicRenderer = SPK::GL::GLPointRenderer::create();
	SPK::Ref<SPK::GL::GLRenderer> particleRenderer = SPK_NULL_REF;
	// We use pointSprites only if it is available and if the GL extension point parameter is available
	if (SPK::GL::GLPointRenderer::isPointSpriteSupported() && SPK::GL::GLPointRenderer::isWorldSizeSupported())
	{
		SPK::Ref<SPK::GL::GLPointRenderer> pointRenderer = SPK::GL::GLPointRenderer::create();
		pointRenderer->setType(SPK::POINT_TYPE_SPRITE);
		pointRenderer->enableWorldSize(true);
		pointRenderer->setTexture(textureParticle);
		SPK::GL::GLPointRenderer::setPixelPerUnit(45.0f * PI / 180.f,screenHeight);
		particleRenderer = pointRenderer;
	}
	else // we use quads
	{
		SPK::Ref<SPK::GL::GLQuadRenderer> quadRenderer = SPK::GL::GLQuadRenderer::create();
		quadRenderer->setTexturingMode(SPK::TEXTURE_MODE_2D);
		quadRenderer->setTexture(textureParticle);
		particleRenderer = quadRenderer;
	}

	particleRenderer->setBlendMode(SPK::BLEND_MODE_NONE);
	particleRenderer->enableRenderingOption(SPK::RENDERING_OPTION_ALPHA_TEST,true);
	particleRenderer->setAlphaTestThreshold(0.8f);

	// Zone
	SPK::Ref<SPK::Sphere> sphere = SPK::Sphere::create();
	SPK::Ref<SPK::Box> cube = SPK::Box::create(SPK::Vector3D(),SPK::Vector3D(1.4f,1.4f,1.4f));

	// Gravity
	SPK::Ref<SPK::Gravity> gravity = SPK::Gravity::create();

	// System
	SPK::Ref<SPK::System> particleSystem = SPK::System::create(true);

	SPK::Ref<SPK::Collider> collider = SPK::Collider::create(0.8f);

	// Obstacle
	SPK::Ref<SPK::Obstacle> obstacle = SPK::Obstacle::create(cube,0.8f,0.9f,SPK::ZONE_TEST_INTERSECT);
	
	// Group
	SPK::Ref<SPK::Group> particleGroup = particleSystem->createGroup(NB_PARTICLES);
	particleGroup->setImmortal(true);
	particleGroup->setRadius(RADIUS);
	particleGroup->setRenderer(particleRenderer);
	particleGroup->addModifier(gravity);
	particleGroup->addModifier(obstacle);
	particleGroup->addModifier(collider);
	particleGroup->addModifier(SPK::Friction::create(0.2f));
	
	float deltaTime = 0.0f;

	SDL_Event event;
	bool exit = false;
	bool paused = false;

	// renderValue :
	// 0 : normal
	// 1 : basic render
	// 2 : no render
	unsigned int renderValue = 0;

	while (SDL_PollEvent(&event)){}
	
	std::deque<unsigned int> frameFPS;
	frameFPS.push_back(clock());

	while(!exit)
	{
		while (SDL_PollEvent(&event))
		{
			// if esc is pressed, exit
			if ((event.type == SDL_KEYDOWN)&&(event.key.keysym.sym == SDLK_ESCAPE))
				exit = true;

			// if del is pressed, reinit the system
			if ((event.type == SDL_KEYDOWN)&&(event.key.keysym.sym == SDLK_DELETE))
				particleGroup->empty();

			// if F2 is pressed, we display or not the bounding boxes
			if ((event.type == SDL_KEYDOWN)&&(event.key.keysym.sym == SDLK_F2))
			{
				particleSystem->enableAABBComputation(!particleSystem->isAABBComputationEnabled());

//				if (paused)
//					particleSystem->computeAABB();
			}

			// if F4 is pressed, the renderers are changed
			if ((event.type == SDL_KEYDOWN)&&(event.key.keysym.sym == SDLK_F4))
			{
				renderValue = (renderValue + 1) % 3;

				switch (renderValue)
				{
				case 0 :
					particleGroup->setRenderer(particleRenderer);
					break;

				case 1 :
					particleGroup->setRenderer(basicRenderer);
					break;

				case 2 :
					particleGroup->setRenderer(SPK_NULL_REF);
					break;
				}
			}

			if ((event.type == SDL_KEYDOWN)&&(event.key.keysym.sym == SDLK_SPACE))
			{
				// TODO Fix Box zone
				if (obstacle->getZone() == sphere)
					obstacle->setZone(cube);
				else if (obstacle->getZone() == cube)
					obstacle->setZone(sphere);
				particleGroup->empty();
			}

			// if pause is pressed, the system is paused
			if ((event.type == SDL_KEYDOWN)&&(event.key.keysym.sym == SDLK_PAUSE))
				paused = !paused;

			// Moves the camera with the mouse
			if (event.type == SDL_MOUSEMOTION)
			{
				angleZ += event.motion.xrel * 0.05f;
				angleX += event.motion.yrel * 0.05f;
			}
		}

		if (!paused)
		{
			// Updates particle system
			particleSystem->updateParticles(deltaTime);

			float cosX = cos(angleX * PI / 180.0f);
			float sinX = sin(angleX * PI / 180.0f);
			float cosZ = cos(angleZ * PI / 180.0f);
			float sinZ = sin(angleZ * PI / 180.0f);
			gravity->setValue(SPK::Vector3D(-1.5f * sinZ * cosX,-1.5f * cosZ * cosX,1.5f * sinX));

			// if the particles were deleted, we refill the system
			if (particleSystem->getNbParticles() == 0)
				particleGroup->addParticles(NB_PARTICLES,obstacle->getZone(),SPK::Vector3D());
		}

		// Renders scene
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();                       

		glTranslatef(0.0f,0.0f,-CAM_POS_Z);
		glRotatef(angleX,1.0f,0.0f,0.0f);
		glRotatef(angleZ,0.0f,0.0f,1.0f);

		//drawBox(0.0f,0.0f,1.0f,SPK::Vector3D(-0.25f),SPK::Vector3D(0.25f));

		if (particleSystem->isAABBComputationEnabled() && particleGroup->getOctree() != NULL)
		{
			const SPK::Octree& octree = *particleGroup->getOctree();
			drawCell(octree,octree.getCell(0),octree.getAABBMin(),octree.getAABBMax() - octree.getAABBMin());
		}

		drawBoundingBox(*particleSystem);
		SPK::GL::GLRenderer::saveGLStates();
		particleSystem->renderParticles();
		SPK::GL::GLRenderer::restoreGLStates();

		SDL_GL_SwapBuffers();

		// Computes delta time
		clock_t currentTick = clock();
		deltaTime = (float)(currentTick - frameFPS.back()) / CLOCKS_PER_SEC;
		frameFPS.push_back(currentTick);
		while((frameFPS.back() - frameFPS.front() > 1000)&&(frameFPS.size() > 2))
			frameFPS.pop_front();
	}

	SPK_DUMP_MEMORY
	}
	SPK_DUMP_MEMORY

	SDL_Quit();

	system("pause");

	return 0;
}


