//////////////////////////////////////////////////////////////////////////////////
// SPARK Irrlicht Rendering library												//
// Copyright (C) 2008-2013														//
// Julien Fryer - julienfryer@gmail.com											//
// Thibault Lescoat -  info-tibo <at> orange <dot> fr							//
//																				//
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

// external libs
#include <irrlicht.h>

// SPARK lib
#include <SPARK.h>
#include <SPARK_IRR.h>

// windows only
#ifdef _WIN32
#include <windows.h>
#endif

float angleX = 0.0f;
float angleY = 0.0f;
const float CAM_POS_Z = 3.0f;

using namespace irr;
IrrlichtDevice* device = NULL;

// Input Receiver
class MyEventReceiver : public IEventReceiver
{
	public:
		virtual bool OnEvent(const SEvent& event)
		{
			if(event.EventType==EET_KEY_INPUT_EVENT)
			{
				if(event.KeyInput.Key == KEY_ESCAPE && event.KeyInput.PressedDown==false)
				{
					device->closeDevice();
				}
			}
			return false;
		}

		int oldMouseX,oldMouseY;
};

class SinController : public SPK::Controller
{
public:
	SinController() : period(1), alpha(1), phi(0), time(0) {}
	void setPhase(float p) { phi = p; }
	float getPhase() const { return phi; }
	void setAmplitude(float a) { alpha = a; }
	float getAmplitude() const { return alpha; }
	void setPeriod(float p) { period = p; }
	float getPeriod() const { return period; }

	spark_description(SinController, SPK::Controller)
	(
		spk_attribute(float, period, setPeriod, getPeriod);
		spk_attribute(float, phase, setPhase, getPhase);
		spk_attribute(float, amplitude, setAmplitude, getAmplitude);
		spk_control(float, value);
	);

protected:
	virtual void updateValues(float deltaTime)
	{
		time = fmod(time + deltaTime, 2 * period);
		float result = sin(phi + 2 * irr::core::PI * time / period) * alpha;
		if(!description::value::set(this, result))
			printf("failed to set control\n");
	}

private:
	float time, period, phi, alpha;
};

class Multiplexer : public SPK::Controller
{
public:
	Multiplexer() : x(0), y(0), z(0) {}
	void setX(float v) { x = v; }
	float getX() const { return x; }
	void setY(float v) { y = v; }
	float getY() const { return y; }
	void setZ(float v) { z = v; }
	float getZ() const { return z; }

	spark_description(Multiplexer, SPK::Controller)
	(
		spk_attribute(float, x, setX, getX);
		spk_attribute(float, y, setY, getY);
		spk_attribute(float, z, setZ, getZ);
		spk_control(SPK::Vector3D, value);
	);

protected:
	virtual void updateValues(float deltaTime)
	{
		if(!description::value::set(this, SPK::Vector3D(x, y, z)))
			printf("failed to set Multiplexer::value\n");
	}

private:
	float x, y, z;
};

// Main function
int main(int argc, char *argv[])
{
	//!IRRLICHT
	video::E_DRIVER_TYPE chosenDriver = video::EDT_OPENGL;
#ifdef _WIN32
	if(MessageBoxA(0,"Do you want to use DirectX 9 ? (else OpenGL)","SPARK Irrlicht test",MB_YESNO | MB_ICONQUESTION) == IDYES)
		chosenDriver = video::EDT_DIRECT3D9;
#endif

	//!IRRLICHT
	device = createDevice(chosenDriver,
		core::dimension2d<u32>(800,600),
		32,
		false,
		false,
		false,
		new MyEventReceiver);

	video::IVideoDriver* driver = device->getVideoDriver();
	scene::ISceneManager* smgr = device->getSceneManager();
	gui::IGUIEnvironment* guienv = device->getGUIEnvironment();

	device->setWindowCaption(L"SPARK Irrlicht test - Sin controllers");
	device->getCursorControl()->setVisible(false);
	irr::scene::ICameraSceneNode* cam = smgr->addCameraSceneNodeFPS(smgr->getRootSceneNode(),100.0f,0.0005f);
	cam->setPosition(irr::core::vector3df(0.0f,0.0f,1.5f));
	cam->setTarget(irr::core::vector3df(0.0f,-0.2f,0.0f));
	cam->setNearValue(0.05f);

	// Inits Particle Engine
	// Sets the update step
	SPK::System::setClampStep(true,0.1f);			// clamp the step to 100 ms
	SPK::System::useAdaptiveStep(0.001f,0.01f);		// use an adaptive step from 1ms to 10ms (1000fps to 100fps)
	
	irr::scene::CSPKParticleSystemNode* system = new irr::scene::CSPKParticleSystemNode(smgr->getRootSceneNode(),smgr);
	system->drop(); // We let the scene manager take care of the system life time

	{
	SPK::Ref<SPK::IRR::IRRQuadRenderer> quadRenderer = SPK::IRR::IRRQuadRenderer::create(device);
	quadRenderer->setBlendMode(SPK::BLEND_MODE_ADD);
	quadRenderer->enableRenderingOption(SPK::RENDERING_OPTION_DEPTH_WRITE,false);
	quadRenderer->setTexture(driver->getTexture("res\\flare.bmp"));
	quadRenderer->setTexturingMode(SPK::TEXTURE_MODE_2D);

	SPK::Ref<SPK::Point> emitpt = SPK::Point::create();
	SPK::Ref<SPK::RandomEmitter> emitter = SPK::RandomEmitter::create(emitpt);
	emitter->setForce(0.4f,0.6f);
	emitter->setFlow(200);

	SPK::Ref<SPK::ColorGraphInterpolator> graphInterpolator = SPK::ColorGraphInterpolator::create();
	graphInterpolator->addEntry(0.0f,0xFF000088);
	graphInterpolator->addEntry(0.5f,0x00FF0088);
	graphInterpolator->addEntry(1.0f,0x0000FF88);

	SPK::Ref<SPK::Group> group = system->createSPKGroup(400);
	group->setRadius(0.15f);
	group->setLifeTime(1.0f,2.0f);
	group->setColorInterpolator(graphInterpolator);
	group->setParamInterpolator(SPK::PARAM_SCALE,SPK::FloatRandomInterpolator::create(0.8f,1.2f,0.0f,0.0f));
	group->setParamInterpolator(SPK::PARAM_ANGLE,SPK::FloatRandomInitializer::create(0.0f,2 * 3.14159f));
	group->addEmitter(emitter);
	group->addModifier(SPK::Gravity::create(SPK::Vector3D(0.0f,-0.5f,0.0f)));
	group->addModifier(SPK::Friction::create(0.2f));
	group->setRenderer(quadRenderer);

	// Controllers
	/// ---------------------------------------------
	SPK::Ref<SinController> sin1 = new SinController();
	system->getSPKSystem()->addController(sin1);
	sin1->setPeriod(4);
	sin1->setAmplitude(2);

	SPK::Ref<SinController> sin2 = new SinController();
	system->getSPKSystem()->addController(sin2);
	sin2->setPeriod(4);
	sin2->setAmplitude(2);
	sin2->setPhase(irr::core::PI / 2);

	SPK::Ref<Multiplexer> multi = new Multiplexer();
	system->getSPKSystem()->addController(multi);

	SPK::connect(sin1, "value", multi, "x");
	SPK::connect(sin2, "value", multi, "z");
	SPK::connect(multi, "value", emitpt, "position");

	// IO test
	/// ---------------------------------------------
	SPK::IO::Manager::get().save("test.xml", system->getSPKSystem());
	SPK::IO::Manager::get().save("test.spk", system->getSPKSystem());
	/// ---------------------------------------------
	SPK::Factory::getInstance().registerType<SinController>();
	SPK::Factory::getInstance().registerType<Multiplexer>();
	SPK_DUMP_MEMORY
	SPK::Ref<SPK::System> loadedSystem = SPK::IO::Manager::get().load("test.spk");
	if(loadedSystem && loadedSystem->getGroup(0))
	{
		printf("Renderer set\n");
		loadedSystem->getGroup(0)->setRenderer(quadRenderer);
	}
	else
		 std::cout << "Renderer not set\n";
	system->setVisible(false);
	irr::scene::CSPKParticleSystemNode* irrLoadedSystem = new irr::scene::CSPKParticleSystemNode(loadedSystem, smgr->getRootSceneNode(), smgr);
	irrLoadedSystem->drop(); // We let the scene manager take care of the system life time
	/// ---------------------------------------------
	SPK::IO::Manager::get().save("test2.xml", loadedSystem);
	SPK::IO::Manager::get().save("test2.spk", loadedSystem);
	/// ---------------------------------------------

	irr::ITimer* timer = device->getTimer();
	float oldTime=0.0f, deltaTime=0.0f, time = timer->getTime() / 1000.0f;

	while(device->run())
	{
		// Update time
		oldTime = time;
		time = timer->getTime() / 1000.0f;
		deltaTime = time - oldTime;

		driver->beginScene(true, true, irr::video::SColor(0,0,0,0));

		// Renders scene
		smgr->drawAll();

		irr::core::stringw infos; infos+="FPS: "; infos+=driver->getFPS(); infos+=" - Nb Particles: "; infos+=system->getNbParticles();
		guienv->getBuiltInFont()->draw(infos.c_str(),irr::core::rect<irr::s32>(0,0,170,20),irr::video::SColor(255,255,255,255));

		driver->endScene();
	}

	SPK_DUMP_MEMORY
	}
	device->drop();
	SPK_DUMP_MEMORY

	return 0;
}
