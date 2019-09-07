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

    device->setWindowCaption(L"SPARK Irrlicht test - Animator on CSPKIrrlichtObject");
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

	SPK::Ref<SPK::Emitter> emitter1 = SPK::RandomEmitter::create(SPK::Point::create());
	emitter1->setForce(0.4f,0.6f);
	emitter1->setFlow(200);

	/*SPK::Ref<SPK::Emitter> emitter1 = SPK::RandomEmitter::create(SPK::Point::create());
	emitter1->setForce(0.4f,0.6f);
	emitter1->setFlow(200);*/

	SPK::Ref<SPK::Emitter> emitter2 = SPK::SPKObject::copy(emitter1);

	SPK::Ref<SPK::ColorGraphInterpolator> graphInterpolator = SPK::ColorGraphInterpolator::create();
	graphInterpolator->addEntry(0.0f,0xFF000088);
	graphInterpolator->addEntry(0.5f,0x00FF0088);
	graphInterpolator->addEntry(1.0f,0x0000FF88);

	SPK::Ref<SPK::Group> group = system->createSPKGroup(400);
	group->setRadius(0.15f);
	group->setLifeTime(0.5f,1.0f);
	group->setColorInterpolator(graphInterpolator);
	group->setParamInterpolator(SPK::PARAM_SCALE,SPK::FloatRandomInterpolator::create(0.8f,1.2f,0.0f,0.0f));
	group->setParamInterpolator(SPK::PARAM_ANGLE,SPK::FloatRandomInitializer::create(0.0f,2 * 3.14159f));
	group->addEmitter(emitter1);
	group->addEmitter(emitter2);
	group->addModifier(SPK::Gravity::create(SPK::Vector3D(0.0f,-0.5f,0.0f)));
	group->addModifier(SPK::Friction::create(0.2f));
	group->setRenderer(quadRenderer);

	// Animates the emitters using Irrlicht
	irr::scene::CSPKEmitterNode* emitter1Node = new irr::scene::CSPKEmitterNode(emitter1,smgr->getRootSceneNode(),smgr);
	emitter1Node->drop();

	irr::scene::ISceneNodeAnimator* animator1 = smgr->createFlyCircleAnimator(
		irr::core::vector3df(0.0f,0.0f,0.0f),
		0.6f,
		0.005f,
		irr::core::vector3df(0.0f,0.0f,1.0f),
		0.0f);
	emitter1Node->addAnimator(animator1);
	animator1->drop();

	irr::scene::CSPKEmitterNode* emitter2Node = new irr::scene::CSPKEmitterNode(emitter2,smgr->getRootSceneNode(),smgr);
	emitter2Node->drop();

	irr::scene::ISceneNodeAnimator* animator2 = smgr->createFlyCircleAnimator(
		irr::core::vector3df(0.0f,0.0f,0.0f),
		0.6f,
		0.005f,
		irr::core::vector3df(0.0f,0.0f,1.0f),
		0.5f);
	emitter2Node->addAnimator(animator2);
	animator2->drop();


	while(device->run())
	{
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