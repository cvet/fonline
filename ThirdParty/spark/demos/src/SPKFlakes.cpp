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

const unsigned int NB_PARTICLES_SIZE = 6;
const unsigned int NB_PARTICLES[NB_PARTICLES_SIZE] =
{
	10000,
	25000,
	50000,
	100000,
	200000,
	500000,
};

unsigned int nbParticlesIndex = 2;

// Draws the bounding box for a particle system
void drawBoundingBox(const SPK::System& system)
{
	if (!system.isAABBComputationEnabled())
		return;

	SPK::Vector3D AABBMin = system.getAABBMin();
	SPK::Vector3D AABBMax = system.getAABBMax();

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

// Main function
int main(int argc, char *argv[])
{
	// Inits SDL
	SDL_Init(SDL_INIT_VIDEO);
	SDL_WM_SetCaption("SPARK Flakes Demo",NULL);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);

	// vsync
	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL,0);

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

	// Inits Particle Engine
	SPK::System::setClampStep(true,0.01f); // clamp the step to 10 ms
	SPK::System::useRealStep();

	{
	// Renderers
	SPK::Ref<SPK::GL::GLPointRenderer> basicRenderer = SPK::GL::GLPointRenderer::create();

	SPK::Ref<SPK::GL::GLPointRenderer> particleRenderer = SPK::GL::GLPointRenderer::create(1.0f);
	particleRenderer->setBlendMode(SPK::BLEND_MODE_ADD);

	// Zone
	SPK::Ref<SPK::Sphere> sphere = SPK::Sphere::create(SPK::Vector3D(),1.0f);

	// Gravity
	SPK::Ref<SPK::Gravity> gravity = SPK::Gravity::create(SPK::Vector3D(0.0f,-0.5f,0.0f));

	// System
	SPK::Ref<SPK::System> particleSystem = SPK::System::create(true); 

	// Group
	SPK::Ref<SPK::Group> particleGroup = particleSystem->createGroup(NB_PARTICLES[NB_PARTICLES_SIZE - 1]);
	particleGroup->setRadius(0.0f);
	particleGroup->setRenderer(particleRenderer);
	particleGroup->setColorInterpolator(SPK::ColorDefaultInitializer::create(0xFFCC4C66));
	particleGroup->addModifier(gravity);	
	particleGroup->addModifier(SPK::Friction::create(0.2f));
	particleGroup->addModifier(SPK::Obstacle::create(sphere,0.9f,0.9f));
	particleGroup->setImmortal(true);

	// Particles are added to the group
	particleGroup->addParticles(NB_PARTICLES[nbParticlesIndex],sphere,SPK::Vector3D());
	particleGroup->flushBufferedParticles();
	
	float deltaTime = 0.0f;

	std::deque<clock_t> frameFPS;
	frameFPS.push_back(clock());

	SDL_Event event;
	bool exit = false;
	bool paused = false;

	// renderValue :
	// 0 : normal
	// 1 : basic render
	// 2 : no render
	unsigned int renderValue = 0;

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

			// if pause is pressed, the system is paused
			if ((event.type == SDL_KEYDOWN)&&(event.key.keysym.sym == SDLK_PAUSE))
				paused = !paused;

			if (!paused)
			{
				// if + is pressed, some particles are added
				if ((event.type == SDL_KEYDOWN)&&((event.key.keysym.sym == SDLK_PLUS)||(event.key.keysym.sym == SDLK_KP_PLUS)))
				{
					if (nbParticlesIndex < NB_PARTICLES_SIZE - 1)
					{
						++nbParticlesIndex;
						particleGroup->addParticles(NB_PARTICLES[nbParticlesIndex] - NB_PARTICLES[nbParticlesIndex - 1],sphere,SPK::Vector3D());
					}
				}

				// if - is pressed, some particles are removed
				if ((event.type == SDL_KEYDOWN)&&((event.key.keysym.sym == SDLK_MINUS)||(event.key.keysym.sym == SDLK_KP_MINUS)))
				{
					if (nbParticlesIndex > 0)
					{
						--nbParticlesIndex;
						for (unsigned int i = 0; i < NB_PARTICLES[nbParticlesIndex + 1] - NB_PARTICLES[nbParticlesIndex]; ++i)
							particleGroup->getParticle(i).kill();
					}
				}
			}

			// if space is pressed
			if ((event.type == SDL_KEYDOWN)&&(event.key.keysym.sym == SDLK_SPACE))
				for (size_t i = 0; i < particleGroup->getNbParticles(); ++i)
					particleGroup->getParticle(i).velocity() = SPK::Vector3D(SPK_RANDOM(-0.5f,0.5f),SPK_RANDOM(-0.5f,0.5f),SPK_RANDOM(-0.5f,0.5f));

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

			// sets the gravity so that it is always down the screen
			float cosX = cos(angleX * PI / 180.0f);
			float sinX = sin(angleX * PI / 180.0f);
			float cosZ = cos(angleZ * PI / 180.0f);
			float sinZ = sin(angleZ * PI / 180.0f);
			gravity->setValue(SPK::Vector3D(-0.5f * sinZ * cosX,-0.5f * cosZ * cosX,0.5f * sinX));

			// if the particles were deleted, we refill the system
			if (particleSystem->getNbParticles() == 0)
				particleGroup->addParticles(NB_PARTICLES[nbParticlesIndex],sphere,SPK::Vector3D());
		}

		// Renders scene
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();                       

		glTranslatef(0.0f,0.0f,-CAM_POS_Z);
		glRotatef(angleX,1.0f,0.0f,0.0f);
		glRotatef(angleZ,0.0f,0.0f,1.0f);

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
	
	return 0;
}