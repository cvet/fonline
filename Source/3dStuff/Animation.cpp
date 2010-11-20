#include "StdAfx.h"
#include "Animation.h"
#include "Loader.h"
#include "ShadowVolume.h"
#include "Common.h"
#include "Text.h"

#define CONTOURS_EXTRA_SIZE     (20)

#define SKINNING_SOFTWARE       (0)
#define SKINNING_HLSL_SHADER    (1)

LPDIRECT3DDEVICE9 D3DDevice=NULL;
D3DCAPS9 D3DCaps;
int ModeWidth=0,ModeHeight=0;
float FixedZ=0.0f;
D3DLIGHT9 GlobalLight;
D3DXMATRIX MatrixProj,MatrixView,MatrixViewInv,MatrixEmpty;
double MoveTransitionTime=0.25;
float GlobalSpeedAdjust=1.0f;
D3DVIEWPORT9 ViewPort;
ShadowVolumeCreator ShadowVolume;
D3DXMATRIX* BoneMatrices=NULL;
DWORD MaxBones=0;
int SkinningMethod=SKINNING_SOFTWARE;
EffectEx* EffectMain=NULL;
DWORD AnimDelay=0;

/************************************************************************/
/* Animation3d                                                          */
/************************************************************************/

Animation3dVec Animation3d::generalAnimations;

Animation3d::Animation3d():animEntity(NULL),animController(NULL),numAnimationSets(0),
currentTrack(0),lastTick(0),endTick(0),speedAdjust(1.0f),shadowDisabled(false),dirAngle(150.0f),sprId(0),
drawScale(0.0f),bordersDisabled(false),calcBordersTick(0),noDraw(true),parentAnimation(NULL),parentFrame(NULL),
childChecker(true)
{
	ZeroMemory(currentLayers,sizeof(currentLayers));
	groundPos.w=1.0f;
	D3DXMatrixRotationX(&matRot,-25.7f*D3DX_PI/180.0f);
	D3DXMatrixIdentity(&matScale);
}

Animation3d::~Animation3d()
{
	SAFEREL(animController);
	for(Animation3dVecIt it=childAnimations.begin(),end=childAnimations.end();it!=end;++it) delete *it;
	childAnimations.clear();

	Animation3dVecIt it=std::find(generalAnimations.begin(),generalAnimations.end(),this);
	if(it!=generalAnimations.end()) generalAnimations.erase(it);

	for(MeshOptionsVecIt it=meshOpt.begin(),end=meshOpt.end();it!=end;++it)
	{
		MeshOptions& mopt=*it;
		SAFEDELA(mopt.DisabledSubsets);
		SAFEDELA(mopt.TexSubsets);
		SAFEDELA(mopt.DefaultTexSubsets);
	}
	meshOpt.clear();
}

// Handles transitions between animations to make it smooth and not a sudden jerk to a new position
void Animation3d::SetAnimation(int anim1, int anim2, int* layers, int flags)
{
	// Get animation index
	int index=animEntity->GetAnimationIndex(anim1,anim2);
	if(index<0 && anim1>=ANIM1_KNIFE && anim1<=ANIM1_ROCKET_LAUNCHER) index=animEntity->GetAnimationIndex(ANIM1_UNARMED,anim2);
	if(index<0) return;

	// Check changes indices or not
	if(!layers) layers=currentLayers;
	bool layer_changed=false;
	for(int i=0;i<LAYERS3D_COUNT;i++)
	{
		if(currentLayers[i]!=layers[i])
		{
			layer_changed=true;
			break;
		}
	}

	// Is not one time play and same action
	int action=(anim1<<16)|anim2;
	if(!FLAG(flags,ANIMATION_ONE_TIME) && currentLayers[LAYERS3D_COUNT]==action && !layer_changed) return;

	memcpy(currentLayers,layers,sizeof(int)*LAYERS3D_COUNT);
	currentLayers[LAYERS3D_COUNT]=action;

	if(layer_changed)
	{
		// Mark animations as unused
		for(Animation3dVecIt it=childAnimations.begin(),end=childAnimations.end();it!=end;++it) (*it)->childChecker=false;

		// Enable all subsets, set default texture
		for(MeshOptionsVecIt it=meshOpt.begin(),end=meshOpt.end();it!=end;++it)
		{
			MeshOptions& mopt=*it;
			for(DWORD i=0;i<mopt.SubsetsCount;i++)
			{
				mopt.DisabledSubsets[i]=false;
				mopt.TexSubsets[i]=mopt.DefaultTexSubsets[i];
			}
		}

		// Get unused layers and subsets
		bool unused_layers[LAYERS3D_COUNT]={0};
		for(int i=0;i<LAYERS3D_COUNT;i++)
		{
			if(layers[i]!=0)
			{
				for(AnimLinkVecIt it=animEntity->animBones.begin(),end=animEntity->animBones.end();it!=end;++it)
				{
					AnimLink& link=*it;
					if(link.Layer==i && link.LayerValue==layers[i])
					{
						for(int j=0;j<link.DisabledLayersCount;j++) unused_layers[link.DisabledLayers[j]]=true;
						for(int j=0;j<link.DisabledSubsetsCount;j++)
						{
							int mesh_ss=link.DisabledSubsets[j]%100;
							int mesh_num=link.DisabledSubsets[j]/100;
							if(mesh_num<meshOpt.size() && mesh_ss<meshOpt[mesh_num].SubsetsCount) meshOpt[mesh_num].DisabledSubsets[mesh_ss]=true;
						}
					}
				}
			}
		}

		// Append animations
		for(int i=0;i<LAYERS3D_COUNT;i++)
		{
			if(unused_layers[i] || layers[i]==0) continue;

			for(AnimLinkVecIt it=animEntity->animBones.begin(),end=animEntity->animBones.end();it!=end;++it)
			{
				AnimLink& link=*it;
				if(link.Layer==i && link.LayerValue==layers[i])
				{
					if(link.RootTextureName) SetTexture(link.RootTextureName,-1);
					if(link.RootEffectInst) SetEffect(link.RootEffectInst,-1);

					if(!link.ChildFName) continue;

					bool aviable=false;
					for(Animation3dVecIt it=childAnimations.begin(),end=childAnimations.end();it!=end;++it)
					{
						Animation3d* anim3d=*it;
						if(anim3d->animLink.Id==link.Id)
						{
							anim3d->childChecker=true;
							aviable=true;
							break;
						}
					}

					if(!aviable)
					{
						Animation3d* anim3d=NULL;

						// Link to main frame
						if(link.LinkBone)
						{
							D3DXFRAME_EXTENDED* to_frame=(D3DXFRAME_EXTENDED*)D3DXFrameFind(animEntity->xFile->frameRoot,link.LinkBone);
							if(to_frame)
							{
								anim3d=Animation3d::GetAnimation(link.ChildFName,animEntity->pathType,true);
								if(anim3d)
								{
									anim3d->parentAnimation=this;
									anim3d->parentFrame=to_frame;
									anim3d->animLink=link;
									childAnimations.push_back(anim3d);
								}
							}
						}
						// Link all bones
						else
						{
							anim3d=Animation3d::GetAnimation(link.ChildFName,animEntity->pathType,true);
							if(anim3d)
							{
								for(FrameVecIt it=anim3d->animEntity->xFile->framesSkinned.begin(),end=anim3d->animEntity->xFile->framesSkinned.end();it!=end;++it)
								{
									D3DXFRAME_EXTENDED* child_frame=*it;
									D3DXFRAME_EXTENDED* root_frame=(D3DXFRAME_EXTENDED*)D3DXFrameFind(animEntity->xFile->frameRoot,child_frame->Name);
									if(root_frame)
									{
										anim3d->linkFrames.push_back(root_frame);
										anim3d->linkFrames.push_back(child_frame);
									}
								}

								anim3d->parentAnimation=this;
								anim3d->parentFrame=(D3DXFRAME_EXTENDED*)animEntity->xFile->frameRoot;
								anim3d->animLink=link;
								childAnimations.push_back(anim3d);
							}
						}

						// Set textures
						if(anim3d && link.TextureName) anim3d->SetTexture(link.TextureName,link.TextureSubset);

						// Set effects
						if(anim3d && link.EffectInst) anim3d->SetEffect(link.EffectInst,link.EffectSubset);
					}
				}
			}
		}

		// Erase unused animations
		for(Animation3dVecIt it=childAnimations.begin();it!=childAnimations.end();)
		{
			Animation3d* anim3d=*it;
			if(!anim3d->childChecker)
			{
				delete anim3d;
				it=childAnimations.erase(it);
			}
			else ++it;
		}
	}

	if(animController)
	{
		// Get the animation set from the controller
		ID3DXAnimationSet* set;
		animController->GetAnimationSet(index,&set);

		// Alternate tracks
		DWORD new_track=(currentTrack==0?1:0);
		double period=set->GetPeriod();

		// Assign to our track
		animController->SetTrackAnimationSet(new_track,set);
		set->Release();

		// Clear any track events currently assigned to our two tracks
		animController->UnkeyAllTrackEvents(currentTrack);
		animController->UnkeyAllTrackEvents(new_track);
		animController->ResetTime();

		// Smooth time
		double stime=(FLAG(flags,ANIMATION_NO_SMOOTH)?0.001f:MoveTransitionTime);

		// Add an event key to disable the currently playing track stime seconds in the future
		animController->KeyTrackEnable(currentTrack,FALSE,stime);
		// Add an event key to change the speed right away so the animation completes in stime seconds
		animController->KeyTrackSpeed(currentTrack,0.0f,0.0,stime,D3DXTRANSITION_LINEAR);
		// Add an event to change the weighting of the current track (the effect it has blended with the second track)
		animController->KeyTrackWeight(currentTrack,0.0f,0.0,stime,D3DXTRANSITION_LINEAR);

		// Enable the new track
		animController->SetTrackEnable(new_track,TRUE);
		animController->SetTrackPosition(new_track,FLAG(flags,ANIMATION_LAST_FRAME)?period:0.0);
		// Add an event key to set the speed of the track
		animController->KeyTrackSpeed(new_track,1.0f,0.0,stime,D3DXTRANSITION_LINEAR);
		if(FLAG(flags,ANIMATION_ONE_TIME) || FLAG(flags,ANIMATION_STAY)) animController->KeyTrackSpeed(new_track,0.0f,period-0.001,0.0,D3DXTRANSITION_LINEAR);
		// Add an event to change the weighting of the current track (the effect it has blended with the first track)
		// As you can see this will go from 0 effect to total effect(1.0f) in stime seconds and the first track goes from 
		// total to 0.0f in the same time.
		animController->KeyTrackWeight(new_track,1.0f,0.0,stime,D3DXTRANSITION_LINEAR);

		if(FLAG(flags,ANIMATION_LAST_FRAME)) animController->AdvanceTime(period,NULL);

		// Remember current track
		currentTrack=new_track;

		// End time
		DWORD tick=Timer::GameTick();
		if(FLAG(flags,ANIMATION_ONE_TIME)) endTick=tick+DWORD(period/GetSpeed()*1000.0);
		else endTick=0;

		// FPS imitation
		if(AnimDelay)
		{
			lastTick=tick+1;
			animController->AdvanceTime(0.001,NULL);
		}

		// Borders
		if(!parentAnimation)
		{
			if(FLAG(flags,ANIMATION_ONE_TIME)) calcBordersTick=endTick;
			else calcBordersTick=tick+DWORD(stime*1000.0);
		}
	}

	// Set animation for childs
	for(Animation3dVecIt it=childAnimations.begin(),end=childAnimations.end();it!=end;++it)
	{
		Animation3d* child=*it;
		child->SetAnimation(anim1,anim2,layers,flags);
	}

	// Calculate borders
	SetupBorders();
}

bool Animation3d::IsAnimation(int anim1, int anim2)
{
	int ii=(anim1<<8)|anim2;
	IntMapIt it=animEntity->animIndexes.find(ii);
	return it!=animEntity->animIndexes.end();
}

int Animation3d::GetAnim1()
{
	return currentLayers[LAYERS3D_COUNT]>>16;
}

int Animation3d::GetAnim2()
{
	return currentLayers[LAYERS3D_COUNT]&0xFFFF;
}

bool Animation3d::IsAnimationPlaying()
{
	return Timer::GameTick()<endTick;
}

bool Animation3d::IsIntersect(int x, int y)
{
	if(noDraw) return false;
	INTRECT borders=GetExtraBorders();
	if(x<borders.L || x>borders.R || y<borders.T || y>borders.B) return false;

	D3DXVECTOR3 ray_origin,ray_dir;
	D3DXVec3Unproject(&ray_origin,&D3DXVECTOR3(x,y,0.0f),&ViewPort,&MatrixProj,&MatrixView,&MatrixEmpty);
	D3DXVec3Unproject(&ray_dir,&D3DXVECTOR3(x,y,FixedZ),&ViewPort,&MatrixProj,&MatrixView,&MatrixEmpty);

	// Main
	FrameMove(0.0,drawXY.X,drawXY.Y,drawScale,true);
	if(IsIntersectFrame(animEntity->xFile->frameRoot,ray_origin,ray_dir)) return true;
	// Childs
	for(Animation3dVecIt it=childAnimations.begin(),end=childAnimations.end();it!=end;++it)
	{
		Animation3d* child=*it;
		child->FrameMove(0.0,drawXY.X,drawXY.Y,1.0f,true);
		if(IsIntersectFrame(child->animEntity->xFile->frameRoot,ray_origin,ray_dir)) return true;
	}
	return false;
}

bool Animation3d::IsIntersectFrame(LPD3DXFRAME frame, const D3DXVECTOR3& ray_origin, const D3DXVECTOR3& ray_dir)
{
	// Draw all mesh containers in this frame
	LPD3DXMESHCONTAINER mesh_container=frame->pMeshContainer;
	while(mesh_container)
	{
		D3DXFRAME_EXTENDED* frame_ex=(D3DXFRAME_EXTENDED*)frame;		
		D3DXMESHCONTAINER_EXTENDED* mesh_container_ex=(D3DXMESHCONTAINER_EXTENDED*)mesh_container;
		LPD3DXMESH mesh=(mesh_container_ex->pSkinInfo?mesh_container_ex->exSkinMesh:mesh_container_ex->MeshData.pMesh);

		// Use inverse of matrix
		D3DXMATRIX mat_inverse;
		D3DXMatrixInverse(&mat_inverse,NULL,!mesh_container_ex->pSkinInfo?&frame_ex->exCombinedTransformationMatrix:&MatrixEmpty);
		// Transform ray origin and direction by inv matrix
		D3DXVECTOR3 ray_obj_origin,ray_obj_direction;
		D3DXVec3TransformCoord(&ray_obj_origin,&ray_origin,&mat_inverse);
		D3DXVec3TransformNormal(&ray_obj_direction,&ray_dir,&mat_inverse);
		D3DXVec3Normalize(&ray_obj_direction,&ray_obj_direction);

		BOOL hit;
		D3D_HR(D3DXIntersect(mesh,&ray_obj_origin,&ray_obj_direction,&hit,NULL,NULL,NULL,NULL,NULL,NULL));
		if(hit) return true;

		mesh_container=mesh_container->pNextMeshContainer;
	}

	// Recurse for siblings
	if(frame->pFrameSibling && IsIntersectFrame(frame->pFrameSibling,ray_origin,ray_dir)) return true;
	if(frame->pFrameFirstChild && IsIntersectFrame(frame->pFrameFirstChild,ray_origin,ray_dir)) return true;
	return false;
}

void Animation3d::SetupBorders()
{
	if(bordersDisabled) return;

	FLTRECT borders(FLT_MAX,FLT_MAX,FLT_MIN,FLT_MIN);

	// Root
	FrameMove(0.0,drawXY.X,drawXY.Y,drawScale,true);
	SetupBordersFrame(animEntity->xFile->frameRoot,borders);
	baseBorders.L=borders.L;
	baseBorders.R=borders.R;
	baseBorders.T=borders.T;
	baseBorders.B=borders.B;

	// Childs
	for(Animation3dVecIt it=childAnimations.begin(),end=childAnimations.end();it!=end;++it)
	{
		Animation3d* child=*it;
		child->FrameMove(0.0,drawXY.X,drawXY.Y,1.0f,true);
		SetupBordersFrame(child->animEntity->xFile->frameRoot,borders);
	}

	// Store result
	fullBorders.L=borders.L;
	fullBorders.R=borders.R;
	fullBorders.T=borders.T;
	fullBorders.B=borders.B;
	bordersXY=drawXY;

	// Grow borders
	baseBorders.L-=1;
	baseBorders.R+=2;
	baseBorders.T-=1;
	baseBorders.B+=2;
	fullBorders.L-=1;
	fullBorders.R+=2;
	fullBorders.T-=1;
	fullBorders.B+=2;
}

bool Animation3d::SetupBordersFrame(LPD3DXFRAME frame, FLTRECT& borders)
{
	// Draw all mesh containers in this frame
	LPD3DXMESHCONTAINER mesh_container=frame->pMeshContainer;
	while(mesh_container)
	{
		D3DXFRAME_EXTENDED* frame_ex=(D3DXFRAME_EXTENDED*)frame;		
		D3DXMESHCONTAINER_EXTENDED* mesh_container_ex=(D3DXMESHCONTAINER_EXTENDED*)mesh_container;
		LPD3DXMESH mesh=(mesh_container_ex->pSkinInfo?mesh_container_ex->exSkinMesh:mesh_container_ex->MeshData.pMesh);

		DWORD v_size=mesh->GetNumBytesPerVertex();
		DWORD count=mesh->GetNumVertices();
		if(bordersResult.size()<count) bordersResult.resize(count);

		LPDIRECT3DVERTEXBUFFER v;
		BYTE* data;
		D3D_HR(mesh->GetVertexBuffer(&v));
		D3D_HR(v->Lock(0,0,(void**)&data,D3DLOCK_READONLY));
		D3DXVec3ProjectArray(&bordersResult[0],sizeof(D3DXVECTOR3),(D3DXVECTOR3*)data,v_size,
			&ViewPort,&MatrixProj,&MatrixView,mesh_container_ex->pSkinInfo?&MatrixEmpty:&frame_ex->exCombinedTransformationMatrix,
			count);
		D3D_HR(v->Unlock());

		for(DWORD i=0;i<count;i++)
		{
			D3DXVECTOR3& vec=bordersResult[i];
			if(vec.x<borders.L) borders.L=vec.x;
			if(vec.x>borders.R) borders.R=vec.x;
			if(vec.y<borders.T) borders.T=vec.y;
			if(vec.y>borders.B) borders.B=vec.y;
		}

		// Go to next mesh
		mesh_container=mesh_container->pNextMeshContainer;
	}

	// Recurse for siblings
	if(frame->pFrameSibling) SetupBordersFrame(frame->pFrameSibling,borders);
	if(frame->pFrameFirstChild) SetupBordersFrame(frame->pFrameFirstChild,borders);
	return true;
}

void Animation3d::ProcessBorders()
{
	if(calcBordersTick && Timer::GameTick()>=calcBordersTick)
	{
		SetupBorders();
		calcBordersTick=0;
	}
}

INTPOINT Animation3d::GetBaseBordersPivot()
{
	ProcessBorders();
	return INTPOINT(bordersXY.X-baseBorders.L,bordersXY.Y-baseBorders.T);
}

INTPOINT Animation3d::GetFullBordersPivot()
{
	ProcessBorders();
	return INTPOINT(bordersXY.X-fullBorders.L,bordersXY.Y-fullBorders.T);
}

INTRECT Animation3d::GetBaseBorders()
{
	ProcessBorders();
	return INTRECT(baseBorders,drawXY.X-bordersXY.X,drawXY.Y-bordersXY.Y);
}

INTRECT Animation3d::GetFullBorders()
{
	ProcessBorders();
	return INTRECT(fullBorders,drawXY.X-bordersXY.X,drawXY.Y-bordersXY.Y);
}

INTRECT Animation3d::GetExtraBorders()
{
	ProcessBorders();
	INTRECT result(fullBorders,drawXY.X-bordersXY.X,drawXY.Y-bordersXY.Y);
	result.L-=CONTOURS_EXTRA_SIZE*3*drawScale;
	result.T-=CONTOURS_EXTRA_SIZE*4*drawScale;
	result.R+=CONTOURS_EXTRA_SIZE*3*drawScale;
	result.B+=CONTOURS_EXTRA_SIZE*2*drawScale;
	return result;
}

double Animation3d::GetSpeed()
{
	double speed=speedAdjust;
	if(animEntity->speedAdjust!=1.0f) speed*=animEntity->speedAdjust;
	if(GlobalSpeedAdjust!=1.0f)  speed*=GlobalSpeedAdjust;
	return speed;
}

MeshOptions* Animation3d::GetMeshOptions(D3DXMESHCONTAINER_EXTENDED* mesh)
{
	for(MeshOptionsVecIt it=meshOpt.begin(),end=meshOpt.end();it!=end;++it)
	{
		MeshOptions& mopt=*it;
		if(mopt.MeshPtr==mesh) return &mopt;
	}
	return NULL;
}

void Animation3d::SetTexture(const char* texture_name, int subset)
{
	TextureEx* texture=NULL;
	if(!_stricmp(texture_name,"root"))
	{
		if(parentAnimation && parentAnimation->meshOpt.size())
		{
			MeshOptions& mopt=*parentAnimation->meshOpt.begin();
			if(mopt.SubsetsCount) texture=mopt.TexSubsets[0];
		}
	}
	else
	{
		texture=animEntity->xFile->GetTexture(texture_name);
	}

	if(texture)
	{
		if(subset>=0)
		{
			DWORD mesh_ss=subset%100;
			DWORD mesh_num=subset/100;
			if(mesh_num<meshOpt.size() && mesh_ss<meshOpt[mesh_num].SubsetsCount) meshOpt[mesh_num].TexSubsets[mesh_ss]=texture;
		}
		else
		{
			for(MeshOptionsVecIt it=meshOpt.begin(),end=meshOpt.end();it!=end;++it)
			{
				MeshOptions& mopt=*it;
				for(DWORD i=0;i<mopt.SubsetsCount;i++) mopt.TexSubsets[i]=texture;
			}
		}
	}
}

void Animation3d::SetEffect(D3DXEFFECTINSTANCE* effect_inst, int subset)
{
	EffectEx* effect=NULL;
	if(!_stricmp(effect_inst->pEffectFilename,"root"))
	{
		if(parentAnimation && parentAnimation->meshOpt.size())
		{
			MeshOptions& mopt=*parentAnimation->meshOpt.begin();
			if(mopt.SubsetsCount) effect=mopt.EffectSubsets[0];
		}
	}
	else
	{
		effect=animEntity->xFile->GetEffect(effect_inst);
	}

	if(effect)
	{
		if(subset>=0)
		{
			DWORD mesh_ss=subset%100;
			DWORD mesh_num=subset/100;
			if(mesh_num<meshOpt.size() && mesh_ss<meshOpt[mesh_num].SubsetsCount) meshOpt[mesh_num].EffectSubsets[mesh_ss]=effect;
		}
		else
		{
			for(MeshOptionsVecIt it=meshOpt.begin(),end=meshOpt.end();it!=end;++it)
			{
				MeshOptions& mopt=*it;
				for(DWORD i=0;i<mopt.SubsetsCount;i++) mopt.EffectSubsets[i]=effect;
			}
		}
	}
}

void Animation3d::SetDir(int dir)
{
	float angle=150-dir*60;
	if(angle!=dirAngle)
	{
		dirAngle=angle;
		SetupBorders();
	}
}

void Animation3d::SetDirAngle(int dir_angle)
{
	float angle=dir_angle;
	if(angle!=dirAngle)
	{
		dirAngle=angle;
		SetupBorders();
	}
}

void Animation3d::SetRotation(float rx, float ry, float rz)
{
	D3DXMatrixRotationYawPitchRoll(&matRot,ry,rx,rz);
}

void Animation3d::SetScale(float sx, float sy, float sz)
{
	D3DXMatrixScaling(&matScale,sx,sy,sz);
}

void Animation3d::SetSpeed(float speed)
{
	speedAdjust=speed;
}

bool Animation3d::Draw(int x, int y, float scale, FLTRECT* stencil, DWORD color)
{
	// Apply stencil
	if(stencil)
	{
		struct Vertex
		{
			FLOAT x,y,z,rhw;
			DWORD diffuse;
		} vb[6]=
		{
			{stencil->L-0.5f,stencil->B-0.5f,1.0f,1.0f,-1},
			{stencil->L-0.5f,stencil->T-0.5f,1.0f,1.0f,-1},
			{stencil->R-0.5f,stencil->B-0.5f,1.0f,1.0f,-1},
			{stencil->L-0.5f,stencil->T-0.5f,1.0f,1.0f,-1},
			{stencil->R-0.5f,stencil->T-0.5f,1.0f,1.0f,-1},
			{stencil->R-0.5f,stencil->B-0.5f,1.0f,1.0f,-1},
		};

		D3D_HR(D3DDevice->SetRenderState(D3DRS_STENCILENABLE,TRUE));
		D3D_HR(D3DDevice->SetRenderState(D3DRS_STENCILFUNC,D3DCMP_NEVER));
		D3D_HR(D3DDevice->SetRenderState(D3DRS_STENCILFAIL,D3DSTENCILOP_REPLACE));
		D3D_HR(D3DDevice->SetRenderState(D3DRS_STENCILREF,1));
		D3D_HR(D3DDevice->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_DISABLE));

		D3D_HR(D3DDevice->SetVertexShader(NULL));
		D3D_HR(D3DDevice->SetPixelShader(NULL));
		D3D_HR(D3DDevice->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE));

		D3D_HR(D3DDevice->Clear(0,NULL,D3DCLEAR_STENCIL,0,1.0f,0));
		D3D_HR(D3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST,2,(void*)vb,sizeof(Vertex)));

		D3D_HR(D3DDevice->SetRenderState(D3DRS_STENCILFUNC,D3DCMP_NOTEQUAL));
		D3D_HR(D3DDevice->SetRenderState(D3DRS_STENCILREF,0));
		D3D_HR(D3DDevice->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE2X));
	}

	// Lighting
	//int alpha=(color>>24)&0xFF;
	GlobalLight.Diffuse.r=0.3f+((color>>16)&0xFF)/255.0f;
	GlobalLight.Diffuse.r=CLAMP(GlobalLight.Diffuse.r,0.0f,1.0f);
	GlobalLight.Diffuse.g=0.3f+((color>>8)&0xFF)/255.0f;
	GlobalLight.Diffuse.g=CLAMP(GlobalLight.Diffuse.g,0.0f,1.0f);
	GlobalLight.Diffuse.b=0.3f+((color)&0xFF)/255.0f;
	GlobalLight.Diffuse.b=CLAMP(GlobalLight.Diffuse.b,0.0f,1.0f);

	// Move & Draw
	//D3D_HR(D3DDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE));
	D3D_HR(D3DDevice->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE));
	D3D_HR(D3DDevice->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_CURRENT));
	D3D_HR(D3DDevice->SetRenderState(D3DRS_ZENABLE,TRUE));
	D3D_HR(D3DDevice->Clear(0,NULL,D3DCLEAR_ZBUFFER,0,0.99999f,0));

	double elapsed=0.0f;
	DWORD tick=Timer::GameTick();
	if(AnimDelay && animController)
	{
		while(lastTick+AnimDelay<=tick)
		{
			elapsed+=0.001*(double)AnimDelay;
			lastTick+=AnimDelay;
		}
	}
	else
	{
		elapsed=0.001*(double)(tick-lastTick);
		lastTick=tick;
	}

	FrameMove(elapsed,x,y,scale,false);
	DrawFrame(animEntity->xFile->frameRoot,shadowDisabled?false:true);
	for(Animation3dVecIt it=childAnimations.begin(),end=childAnimations.end();it!=end;++it)
	{
		Animation3d* child=*it;
		child->FrameMove(elapsed,x,y,1.0f,false);
		child->DrawFrame(child->animEntity->xFile->frameRoot,shadowDisabled?false:true);
	}

	if(stencil) D3D_HR(D3DDevice->SetRenderState(D3DRS_STENCILENABLE,FALSE));
	D3D_HR(D3DDevice->SetRenderState(D3DRS_ZENABLE,FALSE));
	D3D_HR(D3DDevice->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE2X));
	D3D_HR(D3DDevice->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE));

	float old_scale=drawScale;
	noDraw=false;
	drawScale=scale;
	drawXY.X=x;
	drawXY.Y=y;

	if(scale!=old_scale) SetupBorders();
	return true;
}

// Called each frame_base update with the time and the current world matrix
bool Animation3d::FrameMove(double elapsed, int x, int y, float scale, bool software_skinning)
{
	// Update world matrix, only for root
	if(!parentFrame)
	{
		FLTPOINT p3d=Convert2dTo3d(x,y);
		D3DXMATRIX mat_rot_y,mat_scale,mat_scale2,mat_trans;
		D3DXMatrixRotationY(&mat_rot_y,-dirAngle*D3DX_PI/180.0f);
		D3DXMatrixTranslation(&mat_trans,p3d.X,p3d.Y,0.0f);
		D3DXMatrixScaling(&mat_scale,animEntity->scaleValue,animEntity->scaleValue,animEntity->scaleValue);
		D3DXMatrixScaling(&mat_scale2,scale,scale,scale);
		parentMatrix=mat_rot_y*matRot*mat_scale*matScale*mat_scale2*mat_trans;
		groundPos.x=p3d.X;
		groundPos.y=p3d.Y;
		groundPos.z=FixedZ;
	}

	// Advance the time and set in the controller
	if(animController)
	{
		elapsed*=GetSpeed();
		animController->AdvanceTime(elapsed,NULL);
		if(animController->GetTime()>60.0) animController->ResetTime();
	}

	// Store linked matrices
//	if(parentFrame && linkFrames.size())
//	{
//		linkMatricles.clear();
//		for(size_t i=0,j=linkFrames.size()/2;i<j;i++)
//			linkMatricles.push_back(linkFrames[i*2]->exCombinedTransformationMatrix);
//	}

	// Now update the model matrices in the hierarchy
	UpdateFrameMatrices(animEntity->xFile->frameRoot,&parentMatrix);

	// Update linked matrices
	if(parentFrame && linkFrames.size())
	{
		for(size_t i=0,j=linkFrames.size()/2;i<j;i++)
			//UpdateFrameMatrices(linkFrames[i*2+1],&linkMatricles[i]);
			//linkFrames[i*2+1]->exCombinedTransformationMatrix=linkMatricles[i];
			linkFrames[i*2+1]->exCombinedTransformationMatrix=linkFrames[i*2]->exCombinedTransformationMatrix;
	}

	// If the model contains a skinned mesh update the vertices
	for(MeshContainerVecIt it=animEntity->xFile->allMeshes.begin(),end=animEntity->xFile->allMeshes.end();it!=end;++it)
	{
		D3DXMESHCONTAINER_EXTENDED* mesh_container=*it;
		if(!mesh_container->pSkinInfo) continue;

		if(!mesh_container->exSkinMeshBlended || software_skinning)
		{
			// Create the bone matrices that transform each bone from bone space into character space
			// (via exFrameCombinedMatrixPointer) and also wraps the mesh around the bones using the bone offsets
			// in exBoneOffsetsArray
			for(DWORD i=0,j=mesh_container->pSkinInfo->GetNumBones();i<j;i++)
			{
				if(mesh_container->exFrameCombinedMatrixPointer[i])
					D3DXMatrixMultiply(&BoneMatrices[i],&mesh_container->exBoneOffsets[i],mesh_container->exFrameCombinedMatrixPointer[i]);
			}

			// We need to modify the vertex positions based on the new bone matrices. This is achieved
			// by locking the vertex buffers and then calling UpdateSkinnedMesh. UpdateSkinnedMesh takes the
			// original vertex data (in mesh->MeshData.mesh), applies the matrices and writes the new vertices
			// out to skin mesh (mesh->exSkinMesh).

			// UpdateSkinnedMesh uses software skinning which is the slowest way of carrying out skinning 
			// but is easiest to describe and works on the majority of graphic devices. 
			// Other methods exist that use hardware to do this skinning - see the notes and the 
			// DirectX SDK skinned mesh sample for more details
			void* src=0;
			D3D_HR(mesh_container->MeshData.pMesh->LockVertexBuffer(D3DLOCK_READONLY,(void**)&src));
			void* dest=0;
			D3D_HR(mesh_container->exSkinMesh->LockVertexBuffer(0,(void**)&dest));

			// Update the skinned mesh
			D3D_HR(mesh_container->pSkinInfo->UpdateSkinnedMesh(BoneMatrices,NULL,src,dest));

			// Unlock the meshes vertex buffers
			D3D_HR(mesh_container->exSkinMesh->UnlockVertexBuffer());
			D3D_HR(mesh_container->MeshData.pMesh->UnlockVertexBuffer());
		}
	}

	// Update world matricles for childs
	for(Animation3dVecIt it=childAnimations.begin(),end=childAnimations.end();it!=end;++it)
	{
		Animation3d* child=*it;
		child->groundPos=groundPos;
		child->parentMatrix=child->parentFrame->exCombinedTransformationMatrix;

		if(child->animLink.RotX!=0.0f) child->parentMatrix=*D3DXMatrixRotationX(&D3DXMATRIX(),child->animLink.RotX*D3DX_PI/180.0f)*child->parentMatrix;
		if(child->animLink.RotY!=0.0f) child->parentMatrix=*D3DXMatrixRotationY(&D3DXMATRIX(),child->animLink.RotY*D3DX_PI/180.0f)*child->parentMatrix;
		if(child->animLink.RotZ!=0.0f) child->parentMatrix=*D3DXMatrixRotationZ(&D3DXMATRIX(),child->animLink.RotZ*D3DX_PI/180.0f)*child->parentMatrix;
		if(child->animLink.MoveX!=0.0f) child->parentMatrix=*D3DXMatrixTranslation(&D3DXMATRIX(),child->animLink.MoveX,0.0f,0.0f)*child->parentMatrix;
		if(child->animLink.MoveY!=0.0f) child->parentMatrix=*D3DXMatrixTranslation(&D3DXMATRIX(),0.0f,child->animLink.MoveY,0.0f)*child->parentMatrix;
		if(child->animLink.MoveZ!=0.0f) child->parentMatrix=*D3DXMatrixTranslation(&D3DXMATRIX(),0.0f,0.0f,child->animLink.MoveZ)*child->parentMatrix;
	}
	return true;
}

// Called to update the frame_base matrices in the hierarchy to reflect current animation stage
void Animation3d::UpdateFrameMatrices(const D3DXFRAME* frame_base, const D3DXMATRIX* parent_matrix)
{
	D3DXFRAME_EXTENDED* cur_frame=(D3DXFRAME_EXTENDED*)frame_base;

	// If parent matrix exists multiply our frame matrix by it
	D3DXMatrixMultiply(&cur_frame->exCombinedTransformationMatrix,&cur_frame->TransformationMatrix,parent_matrix);

	// If we have a sibling recurse 
	if(cur_frame->pFrameSibling) UpdateFrameMatrices(cur_frame->pFrameSibling,parent_matrix);

	// If we have a child recurse 
	if(cur_frame->pFrameFirstChild) UpdateFrameMatrices(cur_frame->pFrameFirstChild,&cur_frame->exCombinedTransformationMatrix);
}

void Animation3d::BuildShadowVolume(D3DXFRAME_EXTENDED* frame)
{
	D3DXMESHCONTAINER_EXTENDED* mesh_container=(D3DXMESHCONTAINER_EXTENDED*)frame->pMeshContainer;
	while(mesh_container)
	{
		LPD3DXMESH mesh=(mesh_container->pSkinInfo?mesh_container->exSkinMesh:mesh_container->MeshData.pMesh);
//		ShadowVolume.BuildShadowVolume(mesh,D3DXVECTOR3(0.0f,sinf(75.7f)*FixedZ,-cosf(75.7f)*FixedZ),!mesh_container->pSkinInfo?frame->exCombinedTransformationMatrix:MatrixEmpty);
//		D3DXVECTOR3 look(0.0f,sinf(25.7f)*FixedZ,-cosf(25.7f)*FixedZ);
//		D3DXVec3Normalize(&look,&look);
		ShadowVolume.BuildShadowVolume(mesh,D3DXVECTOR3(0.0f,0.0f,1.0f),!mesh_container->pSkinInfo?frame->exCombinedTransformationMatrix:MatrixEmpty);
		mesh_container=(D3DXMESHCONTAINER_EXTENDED*)mesh_container->pNextMeshContainer;
	}

	if(frame->pFrameSibling) BuildShadowVolume((D3DXFRAME_EXTENDED*)frame->pFrameSibling);
	if(frame->pFrameFirstChild) BuildShadowVolume((D3DXFRAME_EXTENDED*)frame->pFrameFirstChild);
}

// Called to render a frame in the hierarchy
bool Animation3d::DrawFrame(LPD3DXFRAME frame, bool with_shadow)
{
	// Draw all mesh containers in this frame
	LPD3DXMESHCONTAINER mesh_container=frame->pMeshContainer;
	while(mesh_container && mesh_container->MeshData.pMesh)
	{
		// Cast to our extended frame type
		D3DXFRAME_EXTENDED* frame_ex=(D3DXFRAME_EXTENDED*)frame;		
		// Cast to our extended mesh container
		D3DXMESHCONTAINER_EXTENDED* mesh_container_ex=(D3DXMESHCONTAINER_EXTENDED*)mesh_container;
		// Get mesh options for this animation instance
		MeshOptions* mopt=GetMeshOptions(mesh_container_ex);

		if(mesh_container_ex->exSkinMeshBlended)
		{
			LPD3DXBONECOMBINATION bone_comb=(LPD3DXBONECOMBINATION)mesh_container_ex->exBoneCombinationBuf->GetBufferPointer();
			D3DXMATRIX matrix;

			for(DWORD i=0,j=mesh_container_ex->exNumAttributeGroups;i<j;i++)
			{
				DWORD attr_id=bone_comb[i].AttribId;
				if(mopt->DisabledSubsets[attr_id]) continue;

				// First calculate all the world matrices
				for(DWORD k=0,l=mesh_container_ex->exNumPaletteEntries;k<l;k++)
				{
					DWORD matrix_index=bone_comb[i].BoneId[k];
					if(matrix_index!=UINT_MAX)
					{
						D3DXMatrixMultiply(&matrix,&mesh_container_ex->exBoneOffsets[matrix_index],mesh_container_ex->exFrameCombinedMatrixPointer[matrix_index]);
						D3DXMatrixMultiply(&BoneMatrices[k],&matrix,&MatrixView);
					}
				}

				EffectEx* effect=(mopt->EffectSubsets[attr_id]?mopt->EffectSubsets[attr_id]:EffectMain);

				if(effect->EffectParams) D3D_HR(effect->Effect->ApplyParameterBlock(effect->EffectParams));
				if(effect->ProjectionMatrix) D3D_HR(effect->Effect->SetMatrix(effect->ProjectionMatrix,&MatrixProj));
				if(effect->GroundPosition) D3D_HR(effect->Effect->SetVector(effect->GroundPosition,&groundPos));
				if(effect->LightDiffuse) D3D_HR(effect->Effect->SetVector(effect->LightDiffuse,&D3DXVECTOR4(GlobalLight.Diffuse.r,GlobalLight.Diffuse.g,GlobalLight.Diffuse.b,GlobalLight.Diffuse.a)));
				if(effect->WorldMatrices) D3D_HR(effect->Effect->SetMatrixArray(effect->WorldMatrices,BoneMatrices,mesh_container_ex->exNumPaletteEntries));

				// Sum of all ambient and emissive contribution
				D3DMATERIAL9& material=mesh_container_ex->exMaterials[attr_id];
				//D3DXCOLOR amb_emm;
				//D3DXColorModulate(&amb_emm,&D3DXCOLOR(material.Ambient),&D3DXCOLOR(0.25f,0.25f,0.25f,1.0f));
				//amb_emm+=D3DXCOLOR(material.Emissive);
				//D3D_HR(effect->SetVector("MaterialAmbient",(D3DXVECTOR4*)&amb_emm));
				if(effect->MaterialDiffuse) D3D_HR(effect->Effect->SetVector(effect->MaterialDiffuse,(D3DXVECTOR4*)&material.Diffuse));

				// Setup the material of the mesh subset - REMEMBER to use the original pre-skinning attribute id to get the correct material id
				D3D_HR(D3DDevice->SetTexture(0,mopt->TexSubsets[attr_id]?mopt->TexSubsets[attr_id]->Texture:NULL));

				// Set NumBones to select the correct vertex shader for the number of bones
				if(effect->BonesInfluences) D3D_HR(effect->Effect->SetInt(effect->BonesInfluences,mesh_container_ex->exNumInfl-1));

				// Start the effect now all parameters have been updated
				DrawMeshEffect(mesh_container_ex->exSkinMeshBlended,i,effect,with_shadow?effect->TechniqueSkinnedWithShadow:effect->TechniqueSkinned);
			}
		}
		else
		{
			// Select the mesh to draw, if there is skin then use the skinned mesh else the normal one
			LPD3DXMESH draw_mesh=(mesh_container_ex->pSkinInfo?mesh_container_ex->exSkinMesh:mesh_container_ex->MeshData.pMesh);

			// Loop through all the materials in the mesh rendering each subset
			for(DWORD i=0;i<mesh_container_ex->NumMaterials;i++)
			{
				if(mopt->DisabledSubsets[i]) continue;

				EffectEx* effect=(mopt->EffectSubsets[i]?mopt->EffectSubsets[i]:EffectMain);

				if(effect)
				{
					if(effect->EffectParams) D3D_HR(effect->Effect->ApplyParameterBlock(effect->EffectParams));
					if(effect->ProjectionMatrix) D3D_HR(effect->Effect->SetMatrix(effect->ProjectionMatrix,&MatrixProj));
					if(effect->GroundPosition) D3D_HR(effect->Effect->SetVector(effect->GroundPosition,&groundPos));
					D3DXMATRIX wmatrix=(!mesh_container_ex->pSkinInfo?frame_ex->exCombinedTransformationMatrix:MatrixEmpty)*MatrixView;
					if(effect->WorldMatrices) D3D_HR(effect->Effect->SetMatrixArray(effect->WorldMatrices,&wmatrix,1));
					if(effect->LightDiffuse) D3D_HR(effect->Effect->SetVector(effect->LightDiffuse,&D3DXVECTOR4(GlobalLight.Diffuse.r,GlobalLight.Diffuse.g,GlobalLight.Diffuse.b,GlobalLight.Diffuse.a)));
				}
				else
				{
					D3DXMATRIX& wmatrix=(!mesh_container_ex->pSkinInfo?frame_ex->exCombinedTransformationMatrix:MatrixEmpty);
					D3D_HR(D3DDevice->SetTransform(D3DTS_WORLD,&wmatrix));
					D3D_HR(D3DDevice->SetLight(0,&GlobalLight));
				}

				if(effect)
				{
					if(effect->MaterialDiffuse) D3D_HR(effect->Effect->SetVector(effect->MaterialDiffuse,(D3DXVECTOR4*)&mesh_container_ex->exMaterials[i].Diffuse));
					D3D_HR(D3DDevice->SetTexture(0,mopt->TexSubsets[i]?mopt->TexSubsets[i]->Texture:NULL));
					DrawMeshEffect(draw_mesh,i,effect,with_shadow?effect->TechniqueSimpleWithShadow:effect->TechniqueSimple);
				}
				else
				{
					D3D_HR(D3DDevice->SetMaterial(&mesh_container_ex->exMaterials[i]));
					D3D_HR(D3DDevice->SetTexture(0,mopt->TexSubsets[i]?mopt->TexSubsets[i]->Texture:NULL));
					D3D_HR(D3DDevice->SetVertexShader(NULL));
					D3D_HR(D3DDevice->SetPixelShader(NULL));
					D3D_HR(draw_mesh->DrawSubset(i));
				}
			}
		}

		mesh_container=mesh_container->pNextMeshContainer;
	}

	// Recurse for siblings
	if(frame->pFrameSibling) DrawFrame(frame->pFrameSibling,with_shadow);
	// Recurse for children
	if(frame->pFrameFirstChild) DrawFrame(frame->pFrameFirstChild,with_shadow);
	return true;
}

bool Animation3d::DrawMeshEffect(ID3DXMesh* mesh, DWORD subset, EffectEx* effect_ex, D3DXHANDLE technique)
{
	ID3DXEffect* effect=effect_ex->Effect;
	D3D_HR(effect->SetTechnique(technique));
	if(effect_ex->IsNeedProcess) Loader3d::EffectProcessVariables(effect_ex,-1);

	UINT passes;
	D3D_HR(effect->Begin(&passes,effect_ex->EffectFlags));
	for(UINT pass=0;pass<passes;pass++)
	{
		if(effect_ex->IsNeedProcess) Loader3d::EffectProcessVariables(effect_ex,pass);

		D3D_HR(effect->BeginPass(pass));
		D3D_HR(mesh->DrawSubset(subset));
		D3D_HR(effect->EndPass());
	}
	D3D_HR(effect->End());
	return true;
}

bool Animation3d::StartUp(LPDIRECT3DDEVICE9 device, bool software_skinning)
{
	D3DDevice=device;

	// Get caps
	ZeroMemory(&D3DCaps,sizeof(D3DCaps));
	D3D_HR(D3DDevice->GetDeviceCaps(&D3DCaps));

	// Get size of draw area
	LPDIRECT3DSURFACE backbuf;
	D3DSURFACE_DESC backbuf_desc;
	D3D_HR(D3DDevice->GetRenderTarget(0,&backbuf));
	D3D_HR(backbuf->GetDesc(&backbuf_desc));
	SAFEREL(backbuf);
	if(!SetScreenSize(backbuf_desc.Width,backbuf_desc.Height)) return false;

	// Get skinning method
	SkinningMethod=SKINNING_SOFTWARE;
	if(!software_skinning && D3DCaps.VertexShaderVersion>=D3DVS_VERSION(2,0) && D3DCaps.MaxVertexBlendMatrices>=2) SkinningMethod=SKINNING_HLSL_SHADER;

	// Identity matrix
	D3DXMatrixIdentity(&MatrixEmpty);

	// Create a directional light
	ZeroMemory(&GlobalLight,sizeof(D3DLIGHT9));
	GlobalLight.Type=D3DLIGHT_DIRECTIONAL;
	GlobalLight.Diffuse.a=1.0f;
	// Direction for our light - it must be normalized - pointing down and along z
	D3DXVec3Normalize((D3DXVECTOR3*)&GlobalLight.Direction,&D3DXVECTOR3(0.0f,0.0f,1.0f));
	//GlobalLight.Position=D3DXVECTOR3(0.0f,sinf(25.7f)*FixedZ,-cosf(25.7f)*FixedZ);
	D3D_HR(D3DDevice->LightEnable(0,TRUE));
	// Plus some non directional ambient lighting
	//D3D_HR(D3DDevice->SetRenderState(D3DRS_AMBIENT,D3DCOLOR_XRGB(80,80,80)));

	// Create skinning effect
	if(SkinningMethod==SKINNING_HLSL_SHADER)
	{
		EffectMain=Loader3d::LoadEffect(D3DDevice,"3D_Default.fx");
		if(!EffectMain)
		{
			SkinningMethod=SKINNING_SOFTWARE;
			WriteLog(__FUNCTION__" - Fail to create effect, skinning switched to software.\n");
		}
	}

	// FPS & Smooth
	if(!GameOpt.Animation3dFPS)
	{
		AnimDelay=0;
		MoveTransitionTime=(double)GameOpt.Animation3dSmoothTime/1000.0;
		if(MoveTransitionTime<0.001) MoveTransitionTime=0.001;
	}
	else
	{
		AnimDelay=1000/GameOpt.Animation3dFPS;
		MoveTransitionTime=0.001;
	}

	return true;
}

bool Animation3d::SetScreenSize(int width, int height)
{
	// Build orthogonal projection
	ModeWidth=width;
	ModeHeight=height;
	FixedZ=500.0f;

	// Projection
	float k=(float)ModeHeight/768.0f;
	D3DXMatrixOrthoLH(&MatrixProj,18.65f*k*(float)ModeWidth/ModeHeight,18.65f*k,0.0f,FixedZ*2);
	D3D_HR(D3DDevice->SetTransform(D3DTS_PROJECTION,&MatrixProj));

	// View
	D3DXMatrixLookAtLH(&MatrixView,&D3DXVECTOR3(0.0f,0.0f,-FixedZ),&D3DXVECTOR3(0.0f,0.0f,0.0f),&D3DXVECTOR3(0.0f,1.0f,0.0f));
	D3D_HR(D3DDevice->SetTransform(D3DTS_VIEW,&MatrixView));
	D3DXMatrixInverse(&MatrixViewInv,NULL,&MatrixView);

	// View port
	D3D_HR(D3DDevice->GetViewport(&ViewPort));

	return true;
}

void Animation3d::Finish()
{
	for(Animation3dEntityVecIt it=Animation3dEntity::allEntities.begin(),end=Animation3dEntity::allEntities.end();it!=end;++it) delete *it;
	Animation3dEntity::allEntities.clear();
	for(Animation3dXFileVecIt it=Animation3dXFile::xFiles.begin(),end=Animation3dXFile::xFiles.end();it!=end;++it) delete *it;
	Animation3dXFile::xFiles.clear();

	SAFEDEL(EffectMain);
	SAFEDELA(BoneMatrices);
	MaxBones=0;

	Loader3d::FreeTexture(NULL);
}

void Animation3d::BeginScene()
{
	for(Animation3dVecIt it=generalAnimations.begin(),end=generalAnimations.end();it!=end;++it) (*it)->noDraw=true;
}

#pragma MESSAGE("Release all need stuff.")
void Animation3d::PreRestore()
{
}

Animation3d* Animation3d::GetAnimation(const char* name, int path_type, bool is_child)
{
	Animation3dEntity* entity=Animation3dEntity::GetEntity(name,path_type);
	if(!entity) return NULL;
	Animation3d* anim3d=entity->CloneAnimation();
	if(!anim3d) return NULL;

	// Set mesh options
	anim3d->meshOpt.resize(entity->xFile->allMeshes.size());
	for(size_t i=0,j=entity->xFile->allMeshes.size();i<j;i++)
	{
		D3DXMESHCONTAINER_EXTENDED* mesh=entity->xFile->allMeshes[i];
		MeshOptions& mopt=anim3d->meshOpt[i];
		mopt.MeshPtr=mesh;
		mopt.SubsetsCount=mesh->NumMaterials;
		mopt.DisabledSubsets=new(nothrow) bool[mesh->NumMaterials];
		mopt.TexSubsets=new(nothrow) TextureEx*[mesh->NumMaterials];
		mopt.DefaultTexSubsets=new(nothrow) TextureEx*[mesh->NumMaterials];
		mopt.EffectSubsets=new(nothrow) EffectEx*[mesh->NumMaterials];
		mopt.DefaultEffectSubsets=new(nothrow) EffectEx*[mesh->NumMaterials];
		ZeroMemory(mopt.DisabledSubsets,mesh->NumMaterials*sizeof(bool));
		ZeroMemory(mopt.TexSubsets,mesh->NumMaterials*sizeof(TextureEx*));
		ZeroMemory(mopt.DefaultTexSubsets,mesh->NumMaterials*sizeof(TextureEx*));
		ZeroMemory(mopt.EffectSubsets,mesh->NumMaterials*sizeof(EffectEx*));
		ZeroMemory(mopt.DefaultEffectSubsets,mesh->NumMaterials*sizeof(EffectEx*));

		// Set default textures and effects
		for(DWORD k=0;k<mopt.SubsetsCount;k++)
		{
			const char* tex_name=(entity->defaultTexture!=""?entity->defaultTexture.c_str():mesh->exTexturesNames[k]);
			mopt.DefaultTexSubsets[k]=(tex_name?entity->xFile->GetTexture(tex_name):NULL);
			mopt.TexSubsets[k]=mopt.DefaultTexSubsets[k];

			D3DXEFFECTINSTANCE* effect_inst=(entity->defaultEffect.pEffectFilename?&entity->defaultEffect:&mesh->exEffects[k]);
			mopt.DefaultEffectSubsets[k]=(effect_inst->pEffectFilename?entity->xFile->GetEffect(effect_inst):NULL);
			mopt.EffectSubsets[k]=mopt.DefaultEffectSubsets[k];
		}
	}

	// Set default zero animation
	if(anim3d->animController)
	{
		ID3DXAnimationSet* set;
		anim3d->animController->GetAnimationSet(0,&set);
		anim3d->animController->SetTrackAnimationSet(0,set);
		set->Release();
		anim3d->animController->SetTrackEnable(0,TRUE);
		anim3d->animController->SetTrackSpeed(0,0.0f);
		anim3d->animController->SetTrackWeight(0,1.0f);
	}

	if(!is_child) generalAnimations.push_back(anim3d);
	return anim3d;
}

void Animation3d::AnimateSlower()
{
	if(GlobalSpeedAdjust>0.1f) GlobalSpeedAdjust-=0.1f;
}

void Animation3d::AnimateFaster()
{
	GlobalSpeedAdjust+=0.1f;
}

FLTPOINT Animation3d::Convert2dTo3d(int x, int y)
{
	D3DXVECTOR3 coords;
	D3DXVec3Unproject(&coords,&D3DXVECTOR3(x,y,FixedZ),&ViewPort,&MatrixProj,&MatrixView,&MatrixEmpty);
	return FLTPOINT(coords.x,coords.y);
}

INTPOINT Animation3d::Convert3dTo2d(float x, float y)
{
	D3DXVECTOR3 coords;
	D3DXVec3Project(&coords,&D3DXVECTOR3(x,y,FixedZ),&ViewPort,&MatrixProj,&MatrixView,&MatrixEmpty);
	return INTPOINT(coords.x,coords.y);
}

void Animation3d::SetDefaultEffect(EffectEx* effect)
{
	EffectMain=effect;
}

/************************************************************************/
/* Animation3dEntity                                                    */
/************************************************************************/

Animation3dEntityVec Animation3dEntity::allEntities;

Animation3dEntity::Animation3dEntity():pathType(0),xFile(NULL),scaleValue(1.0f),speedAdjust(1.0f)
{
	ZeroMemory(&defaultEffect,sizeof(defaultEffect));
}

Animation3dEntity::~Animation3dEntity()
{
	SAFEDELA(defaultEffect.pEffectFilename);

	for(AnimLinkVecIt it=animBones.begin(),end=animBones.end();it!=end;++it)
	{
		AnimLink& link=*it;
		SAFEDELA(link.LinkBone);
		SAFEDELA(link.ChildFName);
		SAFEDELA(link.DisabledLayers);
		SAFEDELA(link.DisabledSubsets);
		SAFEDELA(link.RootTextureName);
		SAFEDELA(link.TextureName);
		if(link.RootEffectInst)
		{
			for(DWORD i=0;i<link.RootEffectInst->NumDefaults;i++)
				SAFEDELA(link.RootEffectInst->pDefaults[i].pValue);
			SAFEDELA(link.RootEffectInst->pDefaults);
		}
		SAFEDELA(link.RootEffectInst);
		if(link.EffectInst)
		{
			for(DWORD i=0;i<link.EffectInst->NumDefaults;i++)
				SAFEDELA(link.EffectInst->pDefaults[i].pValue);
			SAFEDELA(link.EffectInst->pDefaults);
		}
		SAFEDELA(link.EffectInst);
	}
	animBones.clear();
}

bool Animation3dEntity::Load(const char* name, int path_type)
{
	const char* ext=FileManager::GetExtension(name);
	if(!ext) return false;

	// Load fonline 3d file
	if(!_stricmp(ext,"fo3d"))
	{
		char* big_buf=Str::GetBigBuf();

		// Load main fo3d file
		IniParser fo3d;
		if(!fo3d.LoadFile(name,path_type)) return false;

		// Load template and parse it
		StrVec def;
		char template_str[512]={0};
		if(fo3d.GetStr("template","",template_str))
		{
			Str::ParseLine(template_str,' ',def,Str::ParseLineDummy);
			fo3d.AppendToEnd(def[0].c_str(),path_type);
			for(size_t i=1,j=def.size();i<j-1;i+=2) def[i]=string("%").append(def[i]).append("%");
		}
		fo3d.CacheKeys();

		// Get x file name
		char xfname[MAX_FOPATH];
		if(!fo3d.GetStr("xfile","",xfname) || !xfname[0])
		{
			WriteLog(__FUNCTION__" - 'xfile' section not found in file<%s>.\n",name);
			return false;
		}
		ProcessTemplateDefines(xfname,def);

		// Get animation file name
		char anim_xfname[MAX_FOPATH];
		if(fo3d.GetStr("animation","",anim_xfname) && anim_xfname[0]) ProcessTemplateDefines(anim_xfname,def);

		// Process pathes
		char name_path[MAX_FOPATH];
		FileManager::ExtractPath(name,name_path);
		if(name_path[0])
		{
			char tmp[MAX_FOPATH];
			StringCopy(tmp,xfname);
			StringCopy(xfname,name_path);
			StringAppend(xfname,tmp);
			if(anim_xfname[0])
			{
				StringCopy(tmp,anim_xfname);
				StringCopy(anim_xfname,name_path);
				StringAppend(anim_xfname,tmp);
			}
		}

		// Load x file
		Animation3dXFile* xfile=Animation3dXFile::GetXFile(xfname,anim_xfname[0]?anim_xfname:NULL,path_type);
		if(!xfile) return false;

		// Create animation
		fileName=name;
		pathType=path_type;
		xFile=xfile;
		if(xfile->animController) numAnimationSets=xfile->animController->GetMaxNumAnimationSets();

		// Indexing frames                          ||||||
		static char frm_ind[]="_ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
		//                     0123456789012345678901234567590123456
		char key[32];
		for(int i1=1;i1<=ABC_SIZE;i1++)
		{
			char ind1=frm_ind[i1];

			// Check substitute
			sprintf(key,"anim_%c",ind1);
			if(fo3d.IsCachedKey(key) && fo3d.GetStr(key,"",big_buf)) ind1=big_buf[0];

			for(int i2=1;i2<0x100;i2++)
			{
				sprintf(key,"anim_%c%d",ind1,i2);
				if(fo3d.IsCachedKey(key) && fo3d.GetStr(key,"",big_buf))
				{
					if(def.size()>2) ProcessTemplateDefines(big_buf,def);

					int anim_ind=abs(atoi(big_buf));
					if(!Str::IsNumber(big_buf)) anim_ind=GetAnimationIndex(big_buf[0]!='-'?big_buf:&big_buf[1]);
					//if((DWORD)anim_ind<numAnimationSets) animIndexes.insert(IntMapVal((i1<<8)|i2,anim_ind*(big_buf[0]!='-'?1:-1)));
					if(anim_ind>=0 && anim_ind<numAnimationSets) animIndexes.insert(IntMapVal((i1<<8)|i2,anim_ind));
				}
			}
		}

		// Indexing bones
		bool last_effect_root=false;
		for(int i=0;i<LAYERS3D_COUNT;i++)
		{
			sprintf(key,"layer_%d",i);
			if(fo3d.IsCachedKey(key) && fo3d.GetStr(key,"",big_buf,';'))
			{
				if(def.size()>2) ProcessTemplateDefines(big_buf,def);

				StrVec lines;
				Str::ParseLine(big_buf,',',lines,Str::ParseLineDummy);
				for(int j=0;j<lines.size();j++)
				{
					StrVec params;
					Str::ParseLine(lines[j].c_str(),' ',params,Str::ParseLineDummy);
					if(params.size()<3) continue;

					// Initialize
					AnimLink link;
					ZeroMemory(&link,sizeof(link));

					static DWORD id=0;
					link.Id=++id;
					link.Layer=i;
					link.RotX=0.0f;
					link.RotY=0.0f;
					link.RotZ=0.0f;
					link.MoveX=0.0f;
					link.MoveY=0.0f;
					link.MoveZ=0.0f;
					link.LayerValue=atoi(params[0].c_str());

					// Parameters
					for(int k=1;k<params.size()-1;k+=2)
					{
						string& p1=params[k];
						string& p2=params[k+1];
						if(p1=="anim") link.ChildFName=StringDuplicate(p2.c_str());
						else if(p1=="link") link.LinkBone=StringDuplicate(p2.c_str());
						else if(p1=="rotx") link.RotX=atoi(p2.c_str());
						else if(p1=="roty") link.RotY=atoi(p2.c_str());
						else if(p1=="rotz") link.RotZ=atoi(p2.c_str());
						else if(p1=="movex") link.MoveX=float(atoi(p2.c_str()))/1000.0f;
						else if(p1=="movey") link.MoveY=float(atoi(p2.c_str()))/1000.0f;
						else if(p1=="movez") link.MoveZ=float(atoi(p2.c_str()))/1000.0f;
						else if(p1=="disable_layer")
						{
							StrVec layers;
							Str::ParseLine(p2.c_str(),'-',layers,Str::ParseLineDummy);
							for(size_t m=0,n=layers.size();m<n;m++)
							{
								int layer=atoi(layers[m].c_str());
								if(layer>=0 && layer<LAYERS3D_COUNT)
								{
									int* tmp=link.DisabledLayers;
									link.DisabledLayers=new int[link.DisabledLayersCount+1];
									for(int h=0;h<link.DisabledLayersCount;h++) link.DisabledLayers[h]=tmp[h];
									if(tmp) delete tmp;
									link.DisabledLayers[link.DisabledLayersCount]=layer;
									link.DisabledLayersCount++;
								}
							}
						}
						else if(p1=="disable_subset")
						{
							StrVec subsets;
							Str::ParseLine(p2.c_str(),'-',subsets,Str::ParseLineDummy);
							for(size_t m=0,n=subsets.size();m<n;m++)
							{
								int ss=atoi(subsets[m].c_str());
								if(ss>=0)
								{
									int* tmp=link.DisabledSubsets;
									link.DisabledSubsets=new int[link.DisabledSubsetsCount+1];
									for(int h=0;h<link.DisabledSubsetsCount;h++) link.DisabledSubsets[h]=tmp[h];
									if(tmp) delete tmp;
									link.DisabledSubsets[link.DisabledSubsetsCount]=ss;
									link.DisabledSubsetsCount++;
								}
							}
						}
						else if(p1=="root_texture")
						{
							link.RootTextureName=StringDuplicate(p2.c_str());
						}
						else if(p1=="texture")
						{
							link.TextureName=StringDuplicate(p2.c_str());
							link.TextureSubset=-1;
							if(k<params.size()-2 && Str::IsNumber(params[k+2].c_str())) link.TextureSubset=atoi(params[k+2].c_str());
						}
						else if(p1=="root_effect")
						{
							if(!link.RootEffectInst) link.RootEffectInst=new D3DXEFFECTINSTANCE;
							ZeroMemory(link.RootEffectInst,sizeof(D3DXEFFECTINSTANCE));
							link.RootEffectInst->pEffectFilename=StringDuplicate(p2.c_str());
							last_effect_root=true;
						}
						else if(p1=="effect")
						{
							if(!link.EffectInst) link.EffectInst=new D3DXEFFECTINSTANCE;
							ZeroMemory(link.EffectInst,sizeof(D3DXEFFECTINSTANCE));
							link.EffectInst->pEffectFilename=StringDuplicate(p2.c_str());
							link.EffectSubset=-1;
							if(k<params.size()-2 && Str::IsNumber(params[k+2].c_str())) link.EffectSubset=atoi(params[k+2].c_str());
							last_effect_root=false;
						}
						else if(p1=="def")
						{
							// Todo: parse defaults
							/*D3DXEFFECTINSTANCE* inst=(last_effect_root?link.RootEffectInst:link.EffectInst);
							if(!inst) continue;

							LPD3DXEFFECTDEFAULT* tmp=inst->pDefaults;
							link.DisabledSubsets=new int[link.DisabledSubsetsCount+1];
							for(int h=0;h<link.DisabledSubsetsCount;h++) link.DisabledSubsets[h]=tmp[h];
							if(tmp) delete tmp;
							link.DisabledSubsets[link.DisabledSubsetsCount]=ss;
							link.DisabledSubsetsCount++;

							inst->NumDefaults

							if(p2=="string")
							{
							}
							else if(p2=="floats")
							{
							}
							else if(p2=="dword")
							{
							}*/
						}
					}

					// Add to sequence
					animBones.push_back(link);
				}
			}
		}

		// Default texture
		if(fo3d.IsCachedKey("texture"))
		{
			char tex_name[MAX_FOPATH];
			fo3d.GetStr("texture","",tex_name);
			defaultTexture=tex_name;
		}

		// Default effect
		if(fo3d.IsCachedKey("effect"))
		{
			char effect_name[MAX_FOPATH];
			fo3d.GetStr("effect","",effect_name);
			defaultEffect.pEffectFilename=StringDuplicate(effect_name);
		}

		// Other
		if(fo3d.IsCachedKey("scale")) scaleValue=float(fo3d.GetInt("scale",1000))/1000.0f;
		if(fo3d.IsCachedKey("speed")) speedAdjust=float(fo3d.GetInt("speed",1000))/1000.0f;
	//	if(Str::IsCachedKey("rotate_x")) scaleValue=float(Str::GetInt("scale",1000))/1000.0f;
	//	if(Str::IsCachedKey("rotate_y")) scaleValue=float(Str::GetInt("scale",1000))/1000.0f;
	//	if(Str::IsCachedKey("rotate_z")) scaleValue=float(Str::GetInt("scale",1000))/1000.0f;
	}
	// Load just x file
	else
	{
		Animation3dXFile* xfile=Animation3dXFile::GetXFile(name,NULL,path_type);
		if(!xfile) return false;

		// Create animation
		fileName=name;
		pathType=path_type;
		xFile=xfile;
		if(xfile->animController) numAnimationSets=xfile->animController->GetMaxNumAnimationSets();
	}

	return true;
}

void Animation3dEntity::ProcessTemplateDefines(char* str, StrVec& def)
{
	for(int i=1,j=(int)def.size();i<j-1;i+=2)
	{
		const char* from=def[i].c_str();
		const char* to=def[i+1].c_str();

		char* token=strstr(str,from);
		while(token)
		{
			Str::EraseInterval(token,strlen(from));
			Str::Insert(token,to);
			token=strstr(str,from);
		}
	}
}

int Animation3dEntity::GetAnimationIndex(const char* anim_name)
{
	if(!xFile->animController) return -1;

	ID3DXAnimationSet* set;
	if(xFile->animController->GetAnimationSetByName(anim_name,&set)!=S_OK) return -1;

	int result=0;
	for(int i=0;i<numAnimationSets;i++)
	{
		ID3DXAnimationSet* set_;
		xFile->animController->GetAnimationSet(i,&set_);
		if(!strcmp(set->GetName(),set_->GetName()))
		{
			result=i;
			set_->Release();
			break;
		}
		set_->Release();
	}
	set->Release();
	return result;
}

int Animation3dEntity::GetAnimationIndex(int anim1, int anim2)
{
	int ii=(anim1<<8)|anim2;
	IntMapIt it=animIndexes.find(ii);
	if(it!=animIndexes.end()) return (*it).second;

	if(anim1==ANIM1_UNARMED)
	{
		bool swapped=true;
		switch(anim2)
		{
		case ANIM2_3D_IDLE_STUNNED: anim2=ANIM2_3D_IDLE; break;
		case ANIM2_3D_LIMP: anim2=ANIM2_3D_WALK; break;
		case ANIM2_3D_RUN: anim2=ANIM2_3D_WALK; break;
		case ANIM2_3D_PANIC_RUN: anim2=ANIM2_3D_RUN; break;
		case ANIM2_3D_PICKUP: anim2=ANIM2_3D_USE; break;
		case ANIM2_3D_SWITCH_ITEMS: anim2=ANIM2_3D_USE; break;
		default: swapped=false; break;
		}
		if(swapped) return GetAnimationIndex(anim1,anim2);
	}

	switch(anim2)
	{
	case ANIM2_3D_IDLE_COMBAT: anim2=ANIM2_3D_IDLE; break;
	case ANIM2_3D_FIDGET2: anim2=ANIM2_3D_FIDGET1; break;
	case ANIM2_3D_CLIMBING: anim2=ANIM2_3D_USE; break;
	case ANIM2_3D_PUNCH_LEFT: anim2=ANIM2_3D_PUNCH_RIGHT; break;
	case ANIM2_3D_KICK_LO: anim2=ANIM2_3D_KICK_HI; break;
	case ANIM2_3D_PUNCH_COMBO: anim2=ANIM2_3D_PUNCH_LEFT; break;
	case ANIM2_3D_KICK_COMBO: anim2=ANIM2_3D_KICK_HI; break;
	case ANIM2_3D_SLASH_1H: anim2=ANIM2_3D_THRUST_1H; break;
	case ANIM2_3D_THRUST_2H: anim2=ANIM2_3D_THRUST_1H; break;
	case ANIM2_3D_SLASH_2H: anim2=ANIM2_3D_SLASH_1H; break;
	case ANIM2_3D_SWEEP: anim2=ANIM2_3D_BURST; break;
	case ANIM2_3D_BURST: anim2=ANIM2_3D_SINGLE; break;
	case ANIM2_3D_BUTT: anim2=ANIM2_3D_PUNCH_RIGHT; break;
	case ANIM2_3D_FLAME: anim2=ANIM2_3D_SINGLE; break;
	case ANIM2_3D_NO_RECOIL: anim2=ANIM2_3D_USE; break;
	case ANIM2_3D_THROW: anim2=ANIM2_3D_PUNCH_RIGHT; break;
	case ANIM2_3D_RELOAD: anim2=ANIM2_3D_USE; break;
	case ANIM2_3D_REPAIR: anim2=ANIM2_3D_RELOAD; break;
	case ANIM2_3D_DODGE_BACK: anim2=ANIM2_3D_DODGE_FRONT; break;
	case ANIM2_3D_DAMAGE_BACK: anim2=ANIM2_3D_DAMAGE_FRONT; break;
	case ANIM2_3D_DAMAGE_MUL_BACK: anim2=ANIM2_3D_DAMAGE_MUL_FRONT; break;
	case ANIM2_3D_WALK_DAMAGE_BACK: anim2=ANIM2_3D_WALK_DAMAGE_FRONT; break;
	case ANIM2_3D_LIMP_DAMAGE_BACK: anim2=ANIM2_3D_LIMP_DAMAGE_FRONT; break;
	case ANIM2_3D_RUN_DAMAGE_BACK: anim2=ANIM2_3D_RUN_DAMAGE_FRONT; break;

	case ANIM2_3D_KNOCK_BACK: anim2=ANIM2_3D_KNOCK_FRONT; break;
	case ANIM2_3D_LAYDOWN_BACK: anim2=ANIM2_3D_LAYDOWN_FRONT; break;
	case ANIM2_3D_IDLE_PRONE_BACK: anim2=ANIM2_3D_IDLE_PRONE_FRONT; break;
	case ANIM2_3D_STANDUP_BACK: anim2=ANIM2_3D_STANDUP_FRONT; break;
	case ANIM2_3D_DEAD_PRONE_BACK: anim2=ANIM2_3D_DEAD_PRONE_FRONT; break;
	case ANIM2_3D_DAMAGE_PRONE_BACK: anim2=ANIM2_3D_DAMAGE_PRONE_FRONT; break;
	case ANIM2_3D_DAMAGE_MUL_PRONE_BACK: anim2=ANIM2_3D_DAMAGE_MUL_PRONE_FRONT; break;
	case ANIM2_3D_TWITCH_PRONE_BACK: anim2=ANIM2_3D_TWITCH_PRONE_FRONT; break;
	case ANIM2_3D_DEAD_BLOODY_SINGLE:
	case ANIM2_3D_DEAD_BLOODY_BURST:
	case ANIM2_3D_DEAD_BURST:
	case ANIM2_3D_DEAD_PULSE:
	case ANIM2_3D_DEAD_PULSE_DUST:
	case ANIM2_3D_DEAD_LASER:
	case ANIM2_3D_DEAD_FUSED:
	case ANIM2_3D_DEAD_EXPLODE:
	case ANIM2_3D_DEAD_BURN:
	case ANIM2_3D_DEAD_BURN_RUN: anim2=ANIM2_3D_DEAD_PRONE_FRONT; break;
	case ANIM2_3D_KNOCK_FRONT: anim2=ANIM2_3D_DEAD_PRONE_FRONT; break;
	// ANIM2_3D_DEAD_PRONE_FRONT to default -1

	default: return -1;
	}

	return GetAnimationIndex(anim1,anim2);
}

Animation3d* Animation3dEntity::CloneAnimation()
{
	// Create instance
	Animation3d* a3d=new(nothrow) Animation3d();
	if(!a3d) return NULL;

	a3d->animEntity=this;
	a3d->lastTick=Timer::GameTick();

	if(xFile->animController)
	{
		a3d->numAnimationSets=xFile->animController->GetMaxNumAnimationSets();

		// Clone the original AC.  This clone is what we will use to animate
		// this mesh; the original never gets used except to clone, since we
		// always need to be able to add another instance at any time.
		D3D_HR(xFile->animController->CloneAnimationController(
			xFile->animController->GetMaxNumAnimationOutputs(),
			xFile->animController->GetMaxNumAnimationSets(),
			xFile->animController->GetMaxNumTracks(),
			xFile->animController->GetMaxNumEvents(),
			&a3d->animController));
	}

	return a3d;
}

Animation3dEntity* Animation3dEntity::GetEntity(const char* name, int path_type)
{
	// Try find instance
	Animation3dEntity* entity=NULL;
	for(Animation3dEntityVecIt it=allEntities.begin(),end=allEntities.end();it!=end;++it)
	{
		Animation3dEntity* e=*it;
		if(e->fileName==name && e->pathType==path_type)
		{
			entity=e;
			break;
		}
	}

	// Create new instance
	if(!entity)
	{
		entity=new(nothrow) Animation3dEntity();
		if(!entity || !entity->Load(name,path_type))
		{
			SAFEDEL(entity);
			return NULL;
		}
		allEntities.push_back(entity);
	}

	return entity;
}

/************************************************************************/
/* Animation3dXFile                                                     */
/************************************************************************/

Animation3dXFileVec Animation3dXFile::xFiles;

Animation3dXFile::Animation3dXFile():pathType(0),frameRoot(NULL),
animController(NULL),facesCount(0)
{
}

Animation3dXFile::~Animation3dXFile()
{
	Loader3d::Free(frameRoot);
	frameRoot=NULL;
	SAFEREL(animController);
}

Animation3dXFile* Animation3dXFile::GetXFile(const char* xname, const char* anim_xname, int path_type)
{
	Animation3dXFile* xfile=NULL;

	for(Animation3dXFileVecIt it=xFiles.begin(),end=xFiles.end();it!=end;++it)
	{
		Animation3dXFile* x=*it;
		if(x->fileName==xname && x->pathType==path_type)
		{
			xfile=x;
			break;
		}
	}

	if(!xfile)
	{
		Animation3dXFile* anim_xfile=NULL;
		if(anim_xname) anim_xfile=Animation3dXFile::GetXFile(anim_xname,NULL,path_type);

		FileManager fm;
		if(!fm.LoadFile(xname,path_type))
		{
			WriteLog(__FUNCTION__" - X file not found, file<%s>.\n",xname);
			return NULL;
		}

		// Load
		ID3DXAnimationController* anim_controller=NULL;
		D3DXFRAME* frame_root=Loader3d::Load(D3DDevice,xname,path_type,anim_xfile?NULL:&anim_controller);
		if(!frame_root)
		{
			WriteLog(__FUNCTION__" - X file<%s> not contain any frames.\n",xname);
			SAFEDEL(anim_controller);
			return NULL;
		}

		xfile=new(nothrow) Animation3dXFile();
		if(!xfile)
		{
			WriteLog(__FUNCTION__" - Allocation fail, x file<%s>.\n",xname);
			SAFEDEL(anim_controller);
			return NULL;
		}

		// Copy animation from another x file
		if(anim_xfile && anim_xfile->animController)
		{
			// Create controller
			if(D3DXCreateAnimationController(
				anim_xfile->animController->GetMaxNumAnimationOutputs(),
				anim_xfile->animController->GetMaxNumAnimationSets(),
				anim_xfile->animController->GetMaxNumTracks(),
				anim_xfile->animController->GetMaxNumEvents(),
				&anim_controller)!=D3D_OK)
			{
				WriteLog(__FUNCTION__" - Can't create animation controller.\n");
				SAFEDEL(xfile);
				return NULL;
			}

			// Copy animation sets
			for(UINT i=0,j=anim_xfile->animController->GetMaxNumAnimationSets();i<j;i++)
			{
				ID3DXAnimationSet* set;
				if(anim_xfile->animController->GetAnimationSet(i,&set)!=D3D_OK)
				{
					WriteLog(__FUNCTION__" - Can't read animation set.\n");
					SAFEDEL(xfile);
					return NULL;
				}

				if(anim_controller->RegisterAnimationSet(set)!=D3D_OK)
				{
					WriteLog(__FUNCTION__" - Can't register copy of animation set.\n");
					SAFEDEL(xfile);
					return NULL;
				}
			}

			// Register animation outputs
			SetupAnimationOutput(frame_root,anim_controller);
		}

		xfile->fileName=xname;
		xfile->pathType=path_type;
		xfile->animController=anim_controller;
		xfile->frameRoot=frame_root;

		// Bones for skinning
		// Set the bones up
		xfile->SetupBoneMatrices((D3DXFRAME_EXTENDED*)xfile->frameRoot,(D3DXFRAME_EXTENDED*)xfile->frameRoot);

		// Faces count
		xfile->SetupFacesCount((D3DXFRAME_EXTENDED*)xfile->frameRoot,xfile->facesCount);

		xFiles.push_back(xfile);
	}

	return xfile;
}

// Need to go through the hierarchy and set the combined matrices calls itself recursively as it tareverses the hierarchy
bool Animation3dXFile::SetupBoneMatrices(D3DXFRAME_EXTENDED* frame, D3DXFRAME_EXTENDED* frame_root)
{
	// Cast to our extended structure first
	D3DXMESHCONTAINER_EXTENDED* mesh_container=(D3DXMESHCONTAINER_EXTENDED*)frame->pMeshContainer;

	// If this frame has a mesh
	while(mesh_container)
	{
		if(mesh_container->MeshData.pMesh) allMeshes.push_back(mesh_container);

		if(mesh_container->MeshData.pMesh && mesh_container->pSkinInfo)
		{
			if(SkinningMethod==SKINNING_HLSL_SHADER)
			{
				// Get palette size
				mesh_container->exNumPaletteEntries=mesh_container->pSkinInfo->GetNumBones();

				D3D_HR(mesh_container->pSkinInfo->ConvertToIndexedBlendedMesh(
					mesh_container->MeshData.pMesh,
					D3DXMESHOPT_VERTEXCACHE|D3DXMESH_MANAGED,
					mesh_container->exNumPaletteEntries,
					mesh_container->pAdjacency,
					NULL,NULL,NULL,
					&mesh_container->exNumInfl,
					&mesh_container->exNumAttributeGroups,
					&mesh_container->exBoneCombinationBuf,
					&mesh_container->exSkinMeshBlended));

				// FVF has to match our declarator. Vertex shaders are not as forgiving as FF pipeline
				DWORD fvf=(mesh_container->exSkinMeshBlended->GetFVF()&D3DFVF_POSITION_MASK)|D3DFVF_NORMAL|D3DFVF_TEX1|D3DFVF_LASTBETA_UBYTE4;
				if(fvf!=mesh_container->exSkinMeshBlended->GetFVF() )
				{
					LPD3DXMESH mesh;
					D3D_HR(mesh_container->exSkinMeshBlended->CloneMeshFVF(mesh_container->exSkinMeshBlended->GetOptions(),fvf,D3DDevice,&mesh));
					mesh_container->exSkinMeshBlended->Release();
					mesh_container->exSkinMeshBlended=mesh;
				}

				D3DVERTEXELEMENT9 declaration[MAX_FVF_DECL_SIZE];
				LPD3DVERTEXELEMENT9 declaration_ptr=declaration;
				D3D_HR(mesh_container->exSkinMeshBlended->GetDeclaration(declaration));

				// The vertex shader is expecting to interpret the UBYTE4 as a D3DCOLOR, so update the type 
				// NOTE: this cannot be done with CloneMesh, that would convert the UBYTE4 data to float and then to D3DCOLOR
				// this is more of a "cast" operation
				while(declaration_ptr->Stream!=0xFF)
				{
					if(declaration_ptr->Usage==D3DDECLUSAGE_BLENDINDICES && declaration_ptr->UsageIndex==0)
						declaration_ptr->Type=D3DDECLTYPE_D3DCOLOR;
					declaration_ptr++;
				}

				D3D_HR(mesh_container->exSkinMeshBlended->UpdateSemantics(declaration));
			}

			// For each bone work out its matrix
			for(DWORD i=0,j=mesh_container->pSkinInfo->GetNumBones();i<j;i++)
			{
				// Find the frame containing the bone
				D3DXFRAME_EXTENDED* tmp_frame=(D3DXFRAME_EXTENDED*)D3DXFrameFind(frame_root,mesh_container->pSkinInfo->GetBoneName(i));

				// Set the bone part - point it at the transformation matrix
				if(tmp_frame)
				{
					mesh_container->exFrameCombinedMatrixPointer[i]=&tmp_frame->exCombinedTransformationMatrix;
					if(std::find(framesSkinned.begin(),framesSkinned.end(),tmp_frame)==framesSkinned.end()) framesSkinned.push_back(tmp_frame);
				}
			}

			// Create a copy of the mesh to skin into later
			D3DVERTEXELEMENT9 declaration[MAX_FVF_DECL_SIZE];
			D3D_HR(mesh_container->MeshData.pMesh->GetDeclaration(declaration));
			D3D_HR(mesh_container->MeshData.pMesh->CloneMesh(D3DXMESH_MANAGED,declaration,D3DDevice,&mesh_container->exSkinMesh));

			// Allocate a buffer for bone matrices, but only if another mesh has not allocated one of the same size or larger
			if(MaxBones<mesh_container->pSkinInfo->GetNumBones())
			{
				MaxBones=mesh_container->pSkinInfo->GetNumBones();

				// Allocate space for blend matrices
				SAFEDELA(BoneMatrices);
				BoneMatrices=new(nothrow) D3DXMATRIX[MaxBones];
				if(!BoneMatrices) return false;
				ZeroMemory(BoneMatrices,sizeof(D3DXMATRIX)*MaxBones);
			}
		}

		mesh_container=(D3DXMESHCONTAINER_EXTENDED*)mesh_container->pNextMeshContainer;
	}

	// Pass on to siblings
	if(frame->pFrameSibling) SetupBoneMatrices((D3DXFRAME_EXTENDED*)frame->pFrameSibling,frame_root);
	// Pass on to children
	if(frame->pFrameFirstChild) SetupBoneMatrices((D3DXFRAME_EXTENDED*)frame->pFrameFirstChild,frame_root);
	return true;
}

void Animation3dXFile::SetupFacesCount(D3DXFRAME_EXTENDED* frame, DWORD& count)
{
	D3DXMESHCONTAINER_EXTENDED* mesh_container=(D3DXMESHCONTAINER_EXTENDED*)frame->pMeshContainer;
	while(mesh_container)
	{
		LPD3DXMESH mesh=(mesh_container->pSkinInfo?mesh_container->exSkinMesh:mesh_container->MeshData.pMesh);
		count+=mesh->GetNumFaces();
		mesh_container=(D3DXMESHCONTAINER_EXTENDED*)mesh_container->pNextMeshContainer;
	}

	if(frame->pFrameSibling) SetupFacesCount((D3DXFRAME_EXTENDED*)frame->pFrameSibling,count);
	if(frame->pFrameFirstChild) SetupFacesCount((D3DXFRAME_EXTENDED*)frame->pFrameFirstChild,count);
}

void Animation3dXFile::SetupAnimationOutput(D3DXFRAME* frame, ID3DXAnimationController* anim_controller)
{
	if(frame->Name && frame->Name[0]) anim_controller->RegisterAnimationOutput(frame->Name,&frame->TransformationMatrix,NULL,NULL,NULL);
	if(frame->pFrameSibling) SetupAnimationOutput(frame->pFrameSibling,anim_controller);
	if(frame->pFrameFirstChild) SetupAnimationOutput(frame->pFrameFirstChild,anim_controller);
}

TextureEx* Animation3dXFile::GetTexture(const char* tex_name)
{
	TextureEx* texture=Loader3d::LoadTexture(D3DDevice,tex_name,fileName.c_str(),pathType);
	if(!texture) WriteLog(__FUNCTION__" - Can't load texture<%s>.\n",tex_name?tex_name:"nullptr");
	return texture;
}

EffectEx* Animation3dXFile::GetEffect(D3DXEFFECTINSTANCE* effect_inst)
{
	EffectEx* effect=Loader3d::LoadEffect(D3DDevice,effect_inst,fileName.c_str(),pathType);
	if(!effect) WriteLog(__FUNCTION__" - Can't load effect<%s>.\n",effect_inst && effect_inst->pEffectFilename?effect_inst->pEffectFilename:"nullptr");
	return effect;
}

/************************************************************************/
/*                                                                      */
/************************************************************************/
