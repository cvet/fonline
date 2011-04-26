#include "StdAfx.h"
#include "Animation.h"
#include "Loader.h"
#include "Common.h"
#include "Text.h"
#include "ConstantsManager.h"
#include "Script.h"
#include "CritterType.h"

#define CONTOURS_EXTRA_SIZE     (20)

#define SKINNING_SOFTWARE       (0)
#define SKINNING_HLSL_SHADER    (1)

LPDIRECT3DDEVICE9 D3DDevice=NULL;
D3DCAPS9 D3DCaps;
int ModeWidth=0,ModeHeight=0;
float FixedZ=0.0f;
D3DLIGHT9 GlobalLight;
D3DXMATRIX MatrixProj,MatrixView,MatrixViewInv,MatrixEmpty,MatrixViewProj;
double MoveTransitionTime=0.25;
float GlobalSpeedAdjust=1.0f;
D3DVIEWPORT9 ViewPort;
D3DXMATRIX* BoneMatrices=NULL;
uint MaxBones=0;
int SkinningMethod=SKINNING_SOFTWARE;
EffectEx* EffectMain=NULL;
uint AnimDelay=0;

/************************************************************************/
/* Animation3d                                                          */
/************************************************************************/

Animation3dVec Animation3d::generalAnimations;

Animation3d::Animation3d():animEntity(NULL),animController(NULL),numAnimationSets(0),
currentTrack(0),lastTick(0),endTick(0),speedAdjustBase(1.0f),speedAdjustCur(1.0f),speedAdjustLink(1.0f),
shadowDisabled(false),dirAngle(GameOpt.MapHexagonal?150.0f:135.0f),sprId(0),
drawScale(0.0f),bordersDisabled(false),calcBordersTick(0),noDraw(true),parentAnimation(NULL),parentFrame(NULL),
childChecker(true),useGameTimer(true),animPosProc(0.0f),animPosTime(0.0f),animPosPeriod(0.0f)
{
	ZeroMemory(currentLayers,sizeof(currentLayers));
	groundPos.w=1.0f;
	D3DXMatrixRotationX(&matRot,-GameOpt.MapCameraAngle*D3DX_PI/180.0f);
	D3DXMatrixIdentity(&matScale);
	D3DXMatrixIdentity(&matScaleBase);
	D3DXMatrixIdentity(&matRotBase);
	D3DXMatrixIdentity(&matTransBase);
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
void Animation3d::SetAnimation(uint anim1, uint anim2, int* layers, int flags)
{
	// Get animation index
	float speed=1.0f;
	int index=0;
	double period_proc=0.0;
	if(!FLAG(flags,ANIMATION_INIT))
	{
		if(!anim1)
		{
			index=animEntity->renderAnim;
			period_proc=(double)anim2/10.0;
		}
		else
		{
			index=animEntity->GetAnimationIndex(anim1,anim2,&speed);
		}
	}

	if(FLAG(flags,ANIMATION_PERIOD(0))) period_proc=(flags>>16);
	period_proc=CLAMP(period_proc,0.0,99.9);

	// Check changes indices or not
	if(!layers) layers=currentLayers;
	bool layer_changed=false;
	if(!FLAG(flags,ANIMATION_INIT))
	{
		for(int i=0;i<LAYERS3D_COUNT;i++)
		{
			if(currentLayers[i]!=layers[i])
			{
				layer_changed=true;
				break;
			}
		}
	}
	else
	{
		layer_changed=true;
	}

	// Is not one time play and same action
	int action=(anim1<<16)|anim2;
	if(!FLAG(flags,ANIMATION_INIT|ANIMATION_ONE_TIME) && currentLayers[LAYERS3D_COUNT]==action && !layer_changed) return;

	memcpy(currentLayers,layers,sizeof(int)*LAYERS3D_COUNT);
	currentLayers[LAYERS3D_COUNT]=action;

	if(layer_changed)
	{
		// Set base data
		SetAnimData(this,animEntity->animDataDefault,true);

		// Append linked data
		if(parentFrame) SetAnimData(this,animLink,false);

		// Mark animations as unused
		for(Animation3dVecIt it=childAnimations.begin(),end=childAnimations.end();it!=end;++it) (*it)->childChecker=false;

		// Get unused layers and subsets
		bool unused_layers[LAYERS3D_COUNT]={0};
		for(int i=0;i<LAYERS3D_COUNT;i++)
		{
			if(layers[i]==0) continue;

			for(AnimParamsVecIt it=animEntity->animData.begin(),end=animEntity->animData.end();it!=end;++it)
			{
				AnimParams& link=*it;
				if(link.Layer==i && link.LayerValue==layers[i] && !link.ChildFName)
				{
					for(uint j=0;j<link.DisabledLayersCount;j++)
					{
						unused_layers[link.DisabledLayers[j]]=true;
					}
					for(uint j=0;j<link.DisabledSubsetsCount;j++)
					{
						uint mesh_ss=link.DisabledSubsets[j]%100;
						uint mesh_num=link.DisabledSubsets[j]/100;
						if(mesh_num<meshOpt.size() && mesh_ss<meshOpt[mesh_num].SubsetsCount)
							meshOpt[mesh_num].DisabledSubsets[mesh_ss]=true;
					}
				}
			}
		}

		if(parentFrame)
		{
			for(uint j=0;j<animLink.DisabledLayersCount;j++)
			{
				unused_layers[animLink.DisabledLayers[j]]=true;
			}
			for(uint j=0;j<animLink.DisabledSubsetsCount;j++)
			{
				uint mesh_ss=animLink.DisabledSubsets[j]%100;
				uint mesh_num=animLink.DisabledSubsets[j]/100;
				if(mesh_num<meshOpt.size() && mesh_ss<meshOpt[mesh_num].SubsetsCount)
					meshOpt[mesh_num].DisabledSubsets[mesh_ss]=true;
			}
		}

		// Append animations
		for(int i=0;i<LAYERS3D_COUNT;i++)
		{
			if(unused_layers[i] || layers[i]==0) continue;

			for(AnimParamsVecIt it=animEntity->animData.begin(),end=animEntity->animData.end();it!=end;++it)
			{
				AnimParams& link=*it;
				if(link.Layer==i && link.LayerValue==layers[i])
				{
					if(!link.ChildFName)
					{
						SetAnimData(this,link,false);
						continue;
					}

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
							FrameEx* to_frame=(FrameEx*)D3DXFrameFind(animEntity->xFile->frameRoot,link.LinkBone);
							if(to_frame)
							{
								anim3d=Animation3d::GetAnimation(link.ChildFName,true);
								if(anim3d)
								{
									anim3d->parentAnimation=this;
									anim3d->parentFrame=to_frame;
									anim3d->animLink=link;
									SetAnimData(anim3d,link,false);
									childAnimations.push_back(anim3d);
								}
							}
						}
						// Link all bones
						else
						{
							anim3d=Animation3d::GetAnimation(link.ChildFName,true);
							if(anim3d)
							{
								for(FrameVecIt it=anim3d->animEntity->xFile->framesSkinned.begin(),end=anim3d->animEntity->xFile->framesSkinned.end();it!=end;++it)
								{
									FrameEx* child_frame=*it;
									FrameEx* root_frame=(FrameEx*)D3DXFrameFind(animEntity->xFile->frameRoot,child_frame->Name);
									if(root_frame)
									{
										anim3d->linkFrames.push_back(root_frame);
										anim3d->linkFrames.push_back(child_frame);
									}
								}

								anim3d->parentAnimation=this;
								anim3d->parentFrame=(FrameEx*)animEntity->xFile->frameRoot;
								anim3d->animLink=link;
								SetAnimData(anim3d,link,false);
								childAnimations.push_back(anim3d);
							}
						}
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

	if(animController && index>=0)
	{
		// Get the animation set from the controller
		ID3DXAnimationSet* set;
		animController->GetAnimationSet(index,&set);

		// Alternate tracks
		uint new_track=(currentTrack==0?1:0);
		double period=set->GetPeriod();
		animPosPeriod=(float)period;
		if(FLAG(flags,ANIMATION_INIT)) period=0.0002;

		// Assign to our track
		animController->SetTrackAnimationSet(new_track,set);
		set->Release();

		// Clear any track events currently assigned to our two tracks
		animController->UnkeyAllTrackEvents(currentTrack);
		animController->UnkeyAllTrackEvents(new_track);
		animController->ResetTime();

		// Smooth time
		double smooth_time=(FLAG(flags,ANIMATION_NO_SMOOTH|ANIMATION_STAY|ANIMATION_INIT)?0.0001:MoveTransitionTime);
		double start_time=period*period_proc/100.0;
		if(FLAG(flags,ANIMATION_STAY)) period=start_time+0.0002;

		// Add an event key to disable the currently playing track smooth_time seconds in the future
		animController->KeyTrackEnable(currentTrack,FALSE,smooth_time);
		// Add an event key to change the speed right away so the animation completes in smooth_time seconds
		animController->KeyTrackSpeed(currentTrack,0.0f,0.0,smooth_time,D3DXTRANSITION_LINEAR);
		// Add an event to change the weighting of the current track (the effect it has blended with the second track)
		animController->KeyTrackWeight(currentTrack,0.0f,0.0,smooth_time,D3DXTRANSITION_LINEAR);

		// Enable the new track
		animController->SetTrackEnable(new_track,TRUE);
		animController->SetTrackPosition(new_track,0.0);
		// Add an event key to set the speed of the track
		animController->KeyTrackSpeed(new_track,1.0f,0.0,smooth_time,D3DXTRANSITION_LINEAR);
		if(FLAG(flags,ANIMATION_ONE_TIME|ANIMATION_STAY|ANIMATION_INIT))
			animController->KeyTrackSpeed(new_track,0.0f,period-0.0001,0.0,D3DXTRANSITION_LINEAR);
		// Add an event to change the weighting of the current track (the effect it has blended with the first track)
		// As you can see this will go from 0 effect to total effect(1.0f) in smooth_time seconds and the first track goes from 
		// total to 0.0f in the same time.
		animController->KeyTrackWeight(new_track,1.0f,0.0,smooth_time,D3DXTRANSITION_LINEAR);

		if(start_time!=0.0) animController->AdvanceTime(start_time,NULL);

		// Remember current track
		currentTrack=new_track;

		// Speed
		speedAdjustCur=speed;

		// End time
		uint tick=GetTick();
		if(FLAG(flags,ANIMATION_ONE_TIME)) endTick=tick+uint(period/GetSpeed()*1000.0);
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
			else calcBordersTick=tick+uint(smooth_time*1000.0);
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

bool Animation3d::IsAnimation(uint anim1, uint anim2)
{
	int ii=(anim1<<8)|anim2;
	IntMapIt it=animEntity->animIndexes.find(ii);
	return it!=animEntity->animIndexes.end();
}

bool Animation3d::CheckAnimation(uint& anim1, uint& anim2)
{
	if(animEntity->GetAnimationIndex(anim1,anim2,NULL)==-1)
	{
		anim1=ANIM1_UNARMED;
		anim2=ANIM2_IDLE;
		return false;
	}
	return true;
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
	return GetTick()<endTick;
}

bool Animation3d::IsIntersect(int x, int y)
{
	if(noDraw) return false;
	INTRECT borders=GetExtraBorders();
	if(x<borders.L || x>borders.R || y<borders.T || y>borders.B) return false;

	D3DXVECTOR3 ray_origin,ray_dir;
	D3DXVec3Unproject(&ray_origin,&D3DXVECTOR3((float)x,(float)y,0.0f),&ViewPort,&MatrixProj,&MatrixView,&MatrixEmpty);
	D3DXVec3Unproject(&ray_dir,&D3DXVECTOR3((float)x,(float)y,FixedZ),&ViewPort,&MatrixProj,&MatrixView,&MatrixEmpty);

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
		FrameEx* frame_ex=(FrameEx*)frame;
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

	FLTRECT borders(1000000.0f,1000000.0f,-1000000.0f,-1000000.0f);

	// Root
	FrameMove(0.0,drawXY.X,drawXY.Y,drawScale,true);
	SetupBordersFrame(animEntity->xFile->frameRoot,borders);
	baseBorders.L=(int)borders.L;
	baseBorders.R=(int)borders.R;
	baseBorders.T=(int)borders.T;
	baseBorders.B=(int)borders.B;

	// Childs
	for(Animation3dVecIt it=childAnimations.begin(),end=childAnimations.end();it!=end;++it)
	{
		Animation3d* child=*it;
		child->FrameMove(0.0,drawXY.X,drawXY.Y,1.0f,true);
		SetupBordersFrame(child->animEntity->xFile->frameRoot,borders);
	}

	// Store result
	fullBorders.L=(int)borders.L;
	fullBorders.R=(int)borders.R;
	fullBorders.T=(int)borders.T;
	fullBorders.B=(int)borders.B;
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
		FrameEx* frame_ex=(FrameEx*)frame;
		D3DXMESHCONTAINER_EXTENDED* mesh_container_ex=(D3DXMESHCONTAINER_EXTENDED*)mesh_container;
		LPD3DXMESH mesh=(mesh_container_ex->pSkinInfo?mesh_container_ex->exSkinMesh:mesh_container_ex->MeshData.pMesh);

		uint v_size=mesh->GetNumBytesPerVertex();
		uint count=mesh->GetNumVertices();
		if(bordersResult.size()<count) bordersResult.resize(count);

		LPDIRECT3DVERTEXBUFFER v;
		uchar* data;
		D3D_HR(mesh->GetVertexBuffer(&v));
		D3D_HR(v->Lock(0,0,(void**)&data,D3DLOCK_READONLY));
		D3DXVec3ProjectArray(&bordersResult[0],sizeof(D3DXVECTOR3),(D3DXVECTOR3*)data,v_size,
			&ViewPort,&MatrixProj,&MatrixView,mesh_container_ex->pSkinInfo?&MatrixEmpty:&frame_ex->exCombinedTransformationMatrix,count);
		D3D_HR(v->Unlock());

		for(uint i=0;i<count;i++)
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
	if(calcBordersTick && GetTick()>=calcBordersTick)
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
	result.L-=(int)((float)CONTOURS_EXTRA_SIZE*3.0f*drawScale);
	result.T-=(int)((float)CONTOURS_EXTRA_SIZE*4.0f*drawScale);
	result.R+=(int)((float)CONTOURS_EXTRA_SIZE*3.0f*drawScale);
	result.B+=(int)((float)CONTOURS_EXTRA_SIZE*2.0f*drawScale);
	return result;
}

void Animation3d::GetRenderFramesData(float& period, int& proc_from, int& proc_to)
{
	period=0.0f;
	proc_from=animEntity->renderAnimProcFrom;
	proc_to=animEntity->renderAnimProcTo;

	if(animController)
	{
		ID3DXAnimationSet* set;
		if(SUCCEEDED(animController->GetAnimationSet(animEntity->renderAnim,&set)))
		{
			period=(float)set->GetPeriod();
			set->Release();
		}
	}
}

double Animation3d::GetSpeed()
{
	return speedAdjustCur*speedAdjustBase*speedAdjustLink*GlobalSpeedAdjust;
}

uint Animation3d::GetTick()
{
	if(useGameTimer) return Timer::GameTick();
	return Timer::FastTick();
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

void Animation3d::SetAnimData(Animation3d* anim3d, AnimParams& data, bool clear)
{
	// Transformations
	if(clear)
	{
		D3DXMatrixIdentity(&anim3d->matScaleBase);
		D3DXMatrixIdentity(&anim3d->matRotBase);
		D3DXMatrixIdentity(&anim3d->matTransBase);
	}
	D3DXMATRIX mat_tmp;
	if(data.ScaleX!=0.0f) anim3d->matScaleBase=*D3DXMatrixScaling(&mat_tmp,data.ScaleX,1.0f,1.0f)*anim3d->matScaleBase;
	if(data.ScaleY!=0.0f) anim3d->matScaleBase=*D3DXMatrixScaling(&mat_tmp,1.0f,data.ScaleY,1.0f)*anim3d->matScaleBase;
	if(data.ScaleZ!=0.0f) anim3d->matScaleBase=*D3DXMatrixScaling(&mat_tmp,1.0f,1.0f,data.ScaleZ)*anim3d->matScaleBase;
	if(data.RotX!=0.0f) anim3d->matRotBase=*D3DXMatrixRotationX(&mat_tmp,data.RotX*D3DX_PI/180.0f)*anim3d->matRotBase;
	if(data.RotY!=0.0f) anim3d->matRotBase=*D3DXMatrixRotationY(&mat_tmp,data.RotY*D3DX_PI/180.0f)*anim3d->matRotBase;
	if(data.RotZ!=0.0f) anim3d->matRotBase=*D3DXMatrixRotationZ(&mat_tmp,data.RotZ*D3DX_PI/180.0f)*anim3d->matRotBase;
	if(data.MoveX!=0.0f) anim3d->matTransBase=*D3DXMatrixTranslation(&mat_tmp,data.MoveX,0.0f,0.0f)*anim3d->matTransBase;
	if(data.MoveY!=0.0f) anim3d->matTransBase=*D3DXMatrixTranslation(&mat_tmp,0.0f,data.MoveY,0.0f)*anim3d->matTransBase;
	if(data.MoveZ!=0.0f) anim3d->matTransBase=*D3DXMatrixTranslation(&mat_tmp,0.0f,0.0f,data.MoveZ)*anim3d->matTransBase;

	// Speed
	if(clear) anim3d->speedAdjustLink=1.0f;
	if(data.SpeedAjust!=0.0f) anim3d->speedAdjustLink*=data.SpeedAjust;

	// Textures
	if(clear)
	{
		// Enable all subsets, set default texture
		for(MeshOptionsVecIt it=anim3d->meshOpt.begin(),end=anim3d->meshOpt.end();it!=end;++it)
		{
			MeshOptions& mopt=*it;
			if(mopt.SubsetsCount)
			{
				memcpy(mopt.TexSubsets,mopt.DefaultTexSubsets,mopt.SubsetsCount*sizeof(TextureEx*)*EFFECT_TEXTURES);
				memset(mopt.DisabledSubsets,0,mopt.SubsetsCount*sizeof(bool));
			}
		}
	}

	if(data.TextureNamesCount)
	{
		for(uint i=0;i<data.TextureNamesCount;i++)
		{
			uint mesh_num=data.TextureSubsets[i]/100;
			uint mesh_ss=data.TextureSubsets[i]%100;
			TextureEx* texture=NULL;

			if(!_strnicmp(data.TextureNames[i],"Parent",6)) // ParentX-Y, X mesh number, Y mesh subset, optional
			{
				uint parent_mesh_num=0;
				uint parent_mesh_ss=0;

				const char* parent=data.TextureNames[i]+6;
				if(*parent && *parent!='-') parent_mesh_num=atoi(parent);
				while(*parent && *parent!='-') parent++;
				if(*parent=='-' && *(parent+1)) parent_mesh_ss=atoi(parent+1);

				if(anim3d->parentAnimation && parent_mesh_num<anim3d->parentAnimation->meshOpt.size())
				{
					MeshOptions& mopt=anim3d->parentAnimation->meshOpt[parent_mesh_num];
					if(parent_mesh_ss<mopt.SubsetsCount) texture=mopt.TexSubsets[parent_mesh_ss*EFFECT_TEXTURES+data.TextureNum[i]];
				}
			}
			else
			{
				texture=anim3d->animEntity->xFile->GetTexture(data.TextureNames[i]);
			}

			if(data.TextureSubsets[i]<0)
			{
				for(MeshOptionsVecIt it=anim3d->meshOpt.begin(),end=anim3d->meshOpt.end();it!=end;++it)
				{
					MeshOptions& mopt=*it;
					for(uint j=0;j<mopt.SubsetsCount;j++) mopt.TexSubsets[j*EFFECT_TEXTURES+data.TextureNum[i]]=texture;
				}
			}
			else if(mesh_num<anim3d->meshOpt.size())
			{
				MeshOptions& mopt=anim3d->meshOpt[mesh_num];
				if(mesh_ss<mopt.SubsetsCount) mopt.TexSubsets[mesh_ss*EFFECT_TEXTURES+data.TextureNum[i]]=texture;
			}
		}
	}

	// Effects
	if(clear)
	{
		for(MeshOptionsVecIt it=anim3d->meshOpt.begin(),end=anim3d->meshOpt.end();it!=end;++it)
		{
			MeshOptions& mopt=*it;
			memcpy(mopt.EffectSubsets,mopt.DefaultEffectSubsets,mopt.SubsetsCount*sizeof(EffectEx*));
		}
	}

	if(data.EffectInstSubsetsCount)
	{
		for(uint i=0;i<data.EffectInstSubsetsCount;i++)
		{
			uint mesh_ss=data.EffectInstSubsets[i]%100;
			uint mesh_num=data.EffectInstSubsets[i]/100;
			EffectEx* effect=NULL;

			if(!_stricmp(data.EffectInst[i].pEffectFilename,"Parent"))
			{
				uint parent_mesh_num=0;
				uint parent_mesh_ss=0;

				const char* parent=data.TextureNames[i]+6;
				if(*parent && *parent!='-') parent_mesh_num=atoi(parent);
				while(*parent && *parent!='-') parent++;
				if(*parent=='-' && *(parent+1)) parent_mesh_ss=atoi(parent+1);

				if(anim3d->parentAnimation && parent_mesh_num<anim3d->parentAnimation->meshOpt.size())
				{
					MeshOptions& mopt=anim3d->parentAnimation->meshOpt[parent_mesh_num];
					if(parent_mesh_ss<mopt.SubsetsCount) effect=mopt.EffectSubsets[parent_mesh_ss];
				}
			}
			else
			{
				effect=anim3d->animEntity->xFile->GetEffect(&data.EffectInst[i]);
			}

			if(data.EffectInstSubsets[i]<0)
			{
				for(MeshOptionsVecIt it=anim3d->meshOpt.begin(),end=anim3d->meshOpt.end();it!=end;++it)
				{
					MeshOptions& mopt=*it;
					for(uint i=0;i<mopt.SubsetsCount;i++) mopt.EffectSubsets[i]=effect;
				}
			}
			else if(mesh_num<anim3d->meshOpt.size())
			{
				MeshOptions& mopt=anim3d->meshOpt[mesh_num];
				if(mesh_ss<mopt.SubsetsCount) mopt.EffectSubsets[mesh_ss]=effect;
			}
		}
	}
}

void Animation3d::SetDir(int dir)
{
	float angle=(float)(GameOpt.MapHexagonal?150-dir*60:135-dir*45);
	if(angle!=dirAngle)
	{
		dirAngle=angle;
		SetupBorders();
	}
}

void Animation3d::SetDirAngle(int dir_angle)
{
	float angle=(float)dir_angle;
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
	speedAdjustBase=speed;
}

void Animation3d::SetTimer(bool use_game_timer)
{
	useGameTimer=use_game_timer;
}

bool Animation3d::Draw(int x, int y, float scale, FLTRECT* stencil, uint color)
{
	// Apply stencil
	if(stencil)
	{
		struct Vertex
		{
			FLOAT x,y,z,rhw;
			uint diffuse;
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
	uint tick=GetTick();
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

	bool shadow_disabled=(shadowDisabled || animEntity->shadowDisabled);

	FrameMove(elapsed,x,y,scale,false);
	DrawFrame(animEntity->xFile->frameRoot,!shadow_disabled);
	for(Animation3dVecIt it=childAnimations.begin(),end=childAnimations.end();it!=end;++it)
	{
		Animation3d* child=*it;
		child->FrameMove(elapsed,x,y,1.0f,false);
		child->DrawFrame(child->animEntity->xFile->frameRoot,!shadow_disabled);
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
		D3DXMATRIX mat_rot_y,mat_scale,mat_trans;
		D3DXMatrixScaling(&mat_scale,scale,scale,scale);
		D3DXMatrixRotationY(&mat_rot_y,-dirAngle*D3DX_PI/180.0f);
		D3DXMatrixTranslation(&mat_trans,p3d.X,p3d.Y,0.0f);
		parentMatrix=matScaleBase*matScale*mat_scale*matRotBase*mat_rot_y*matRot*matTransBase*mat_trans;
		groundPos.x=parentMatrix(3,0);
		groundPos.y=parentMatrix(3,1);
		groundPos.z=parentMatrix(3,2);
	}

	// Advance the time and set in the controller
	if(animController)
	{
		elapsed*=GetSpeed();
		animController->AdvanceTime(elapsed,NULL);
		if(animController->GetTime()>60.0) animController->ResetTime();

		D3DXTRACK_DESC tdesc;
		if(SUCCEEDED(animController->GetTrackDesc(currentTrack,&tdesc)) && animPosPeriod>0.0f)
		{
			animPosProc=(float)tdesc.Position/animPosPeriod;
			if(animPosProc>=1.0f) animPosProc=fmod(animPosProc,1.0f);
			animPosTime=(float)tdesc.Position;
			if(animPosTime>=animPosPeriod) animPosTime=fmod(animPosTime,animPosPeriod);
		}
	}

	// Now update the model matrices in the hierarchy
	UpdateFrameMatrices(animEntity->xFile->frameRoot,&parentMatrix);

	// Update linked matrices
	if(parentFrame && linkFrames.size())
	{
		for(uint i=0,j=(uint)linkFrames.size()/2;i<j;i++)
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
			for(uint i=0,j=mesh_container->pSkinInfo->GetNumBones();i<j;i++)
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
		child->parentMatrix=child->matScaleBase*child->matRotBase*child->matTransBase*child->parentMatrix;
	}
	return true;
}

// Called to update the frame_base matrices in the hierarchy to reflect current animation stage
void Animation3d::UpdateFrameMatrices(const D3DXFRAME* frame_base, const D3DXMATRIX* parent_matrix)
{
	FrameEx* cur_frame=(FrameEx*)frame_base;

	// If parent matrix exists multiply our frame matrix by it
	D3DXMatrixMultiply(&cur_frame->exCombinedTransformationMatrix,&cur_frame->TransformationMatrix,parent_matrix);

	// If we have a sibling recurse 
	if(cur_frame->pFrameSibling) UpdateFrameMatrices(cur_frame->pFrameSibling,parent_matrix);

	// If we have a child recurse 
	if(cur_frame->pFrameFirstChild) UpdateFrameMatrices(cur_frame->pFrameFirstChild,&cur_frame->exCombinedTransformationMatrix);
}

// Called to render a frame in the hierarchy
bool Animation3d::DrawFrame(LPD3DXFRAME frame, bool with_shadow)
{
	// Draw all mesh containers in this frame
	LPD3DXMESHCONTAINER mesh_container=frame->pMeshContainer;
	while(mesh_container && mesh_container->MeshData.pMesh)
	{
		// Cast to our extended frame type
		FrameEx* frame_ex=(FrameEx*)frame;
		// Cast to our extended mesh container
		D3DXMESHCONTAINER_EXTENDED* mesh_container_ex=(D3DXMESHCONTAINER_EXTENDED*)mesh_container;
		// Get mesh options for this animation instance
		MeshOptions* mopt=GetMeshOptions(mesh_container_ex);

		if(mesh_container_ex->exSkinMeshBlended)
		{
			LPD3DXBONECOMBINATION bone_comb=(LPD3DXBONECOMBINATION)mesh_container_ex->exBoneCombinationBuf->GetBufferPointer();

			for(uint i=0,j=mesh_container_ex->exNumAttributeGroups;i<j;i++)
			{
				uint attr_id=bone_comb[i].AttribId;
				if(mopt->DisabledSubsets[attr_id]) continue;

				// First calculate all the world matrices
				for(uint k=0,l=mesh_container_ex->exNumPaletteEntries;k<l;k++)
				{
					uint matrix_index=bone_comb[i].BoneId[k];
					if(matrix_index!=UINT_MAX)
						D3DXMatrixMultiply(&BoneMatrices[k],&mesh_container_ex->exBoneOffsets[matrix_index],mesh_container_ex->exFrameCombinedMatrixPointer[matrix_index]);
				}

				EffectEx* effect=(mopt->EffectSubsets[attr_id]?mopt->EffectSubsets[attr_id]:EffectMain);

				if(effect->EffectParams) D3D_HR(effect->Effect->ApplyParameterBlock(effect->EffectParams));
				if(effect->ViewProjMatrix) D3D_HR(effect->Effect->SetMatrix(effect->ViewProjMatrix,&MatrixViewProj));
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

				// Set NumBones to select the correct vertex shader for the number of bones
				if(effect->BonesInfluences) D3D_HR(effect->Effect->SetInt(effect->BonesInfluences,mesh_container_ex->exNumInfl-1));

				// Start the effect now all parameters have been updated
				DrawMeshEffect(mesh_container_ex->exSkinMeshBlended,i,effect,&mopt->TexSubsets[attr_id*EFFECT_TEXTURES],with_shadow?effect->TechniqueSkinnedWithShadow:effect->TechniqueSkinned);
			}
		}
		else
		{
			// Select the mesh to draw, if there is skin then use the skinned mesh else the normal one
			LPD3DXMESH draw_mesh=(mesh_container_ex->pSkinInfo?mesh_container_ex->exSkinMesh:mesh_container_ex->MeshData.pMesh);

			// Loop through all the materials in the mesh rendering each subset
			for(uint i=0;i<mesh_container_ex->NumMaterials;i++)
			{
				if(mopt->DisabledSubsets[i]) continue;

				EffectEx* effect=(mopt->EffectSubsets[i]?mopt->EffectSubsets[i]:EffectMain);

				if(effect)
				{
					if(effect->EffectParams) D3D_HR(effect->Effect->ApplyParameterBlock(effect->EffectParams));
					if(effect->ViewProjMatrix) D3D_HR(effect->Effect->SetMatrix(effect->ViewProjMatrix,&MatrixViewProj));
					if(effect->GroundPosition) D3D_HR(effect->Effect->SetVector(effect->GroundPosition,&groundPos));
					D3DXMATRIX wmatrix=(!mesh_container_ex->pSkinInfo?frame_ex->exCombinedTransformationMatrix:MatrixEmpty);
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
					DrawMeshEffect(draw_mesh,i,effect,&mopt->TexSubsets[i*EFFECT_TEXTURES],with_shadow?effect->TechniqueSimpleWithShadow:effect->TechniqueSimple);
				}
				else
				{
					D3D_HR(D3DDevice->SetMaterial(&mesh_container_ex->exMaterials[i]));
					D3D_HR(D3DDevice->SetTexture(0,mopt->TexSubsets[i*EFFECT_TEXTURES]?mopt->TexSubsets[i*EFFECT_TEXTURES]->Texture:NULL));
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

bool Animation3d::DrawMeshEffect(ID3DXMesh* mesh, uint subset, EffectEx* effect_ex, TextureEx** textures, D3DXHANDLE technique)
{
	D3D_HR(D3DDevice->SetTexture(0,textures && textures[0]?textures[0]->Texture:NULL));

	ID3DXEffect* effect=effect_ex->Effect;
	D3D_HR(effect->SetTechnique(technique));
	if(effect_ex->IsNeedProcess) Loader3d::EffectProcessVariables(effect_ex,-1,animPosProc,animPosTime,textures);

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
			WriteLogF(_FUNC_," - Fail to create effect, skinning switched to software.\n");
		}
	}

	// FPS & Smooth
	if(GameOpt.Animation3dSmoothTime)
	{
		// Smoothing
		AnimDelay=0;
		MoveTransitionTime=(double)GameOpt.Animation3dSmoothTime/1000.0;
		if(MoveTransitionTime<0.001) MoveTransitionTime=0.001;
	}
	else
	{
		// 2D emulation
		AnimDelay=1000/(GameOpt.Animation3dFPS?GameOpt.Animation3dFPS:10);
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
	D3DXMatrixOrthoLH(&MatrixProj,18.65f*k*(float)ModeWidth/ModeHeight,18.65f*k,0.0f,FixedZ*2.0f);
	D3D_HR(D3DDevice->SetTransform(D3DTS_PROJECTION,&MatrixProj));

	// View
	D3DXMatrixLookAtLH(&MatrixView,&D3DXVECTOR3(0.0f,0.0f,-FixedZ),&D3DXVECTOR3(0.0f,0.0f,0.0f),&D3DXVECTOR3(0.0f,1.0f,0.0f));
	D3D_HR(D3DDevice->SetTransform(D3DTS_VIEW,&MatrixView));
	D3DXMatrixInverse(&MatrixViewInv,NULL,&MatrixView);
	MatrixViewProj=MatrixView*MatrixProj;

	// View port
	D3DVIEWPORT9 vp={0,0,ModeWidth,ModeHeight,0.0f,1.0f};
	D3D_HR(D3DDevice->SetViewport(&vp));
	ViewPort=vp;

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
	char fname[MAX_FOPATH];
	Str::Copy(fname,FileManager::GetPath(path_type));
	Str::Append(fname,name);
	return GetAnimation(fname,is_child);
}

Animation3d* Animation3d::GetAnimation(const char* name, bool is_child)
{
	Animation3dEntity* entity=Animation3dEntity::GetEntity(name);
	if(!entity) return NULL;
	Animation3d* anim3d=entity->CloneAnimation();
	if(!anim3d) return NULL;

	// Set mesh options
	anim3d->meshOpt.resize(entity->xFile->allMeshes.size());
	for(uint i=0,j=(uint)entity->xFile->allMeshes.size();i<j;i++)
	{
		D3DXMESHCONTAINER_EXTENDED* mesh=entity->xFile->allMeshes[i];
		MeshOptions& mopt=anim3d->meshOpt[i];
		mopt.MeshPtr=mesh;
		mopt.SubsetsCount=mesh->NumMaterials;
		mopt.DisabledSubsets=new(nothrow) bool[mesh->NumMaterials];
		mopt.TexSubsets=new(nothrow) TextureEx*[mesh->NumMaterials*EFFECT_TEXTURES];
		mopt.DefaultTexSubsets=new(nothrow) TextureEx*[mesh->NumMaterials*EFFECT_TEXTURES];
		mopt.EffectSubsets=new(nothrow) EffectEx*[mesh->NumMaterials];
		mopt.DefaultEffectSubsets=new(nothrow) EffectEx*[mesh->NumMaterials];
		ZeroMemory(mopt.DisabledSubsets,mesh->NumMaterials*sizeof(bool));
		ZeroMemory(mopt.TexSubsets,mesh->NumMaterials*sizeof(TextureEx*)*EFFECT_TEXTURES);
		ZeroMemory(mopt.DefaultTexSubsets,mesh->NumMaterials*sizeof(TextureEx*)*EFFECT_TEXTURES);
		ZeroMemory(mopt.EffectSubsets,mesh->NumMaterials*sizeof(EffectEx*));
		ZeroMemory(mopt.DefaultEffectSubsets,mesh->NumMaterials*sizeof(EffectEx*));

		// Set default textures and effects
		for(uint k=0;k<mopt.SubsetsCount;k++)
		{
			uint tex_num=k*EFFECT_TEXTURES;
			const char* tex_name=mesh->exTexturesNames[k];
			mopt.DefaultTexSubsets[tex_num]=(tex_name?entity->xFile->GetTexture(tex_name):NULL);
			mopt.TexSubsets[tex_num]=mopt.DefaultTexSubsets[tex_num];

			mopt.DefaultEffectSubsets[k]=(mesh->exEffects[k].pEffectFilename?entity->xFile->GetEffect(&mesh->exEffects[k]):NULL);
			mopt.EffectSubsets[k]=mopt.DefaultEffectSubsets[k];
		}
	}

	// Set default data
	anim3d->SetAnimation(0,0,NULL,ANIMATION_INIT);

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
	D3DXVec3Unproject(&coords,&D3DXVECTOR3((float)x,(float)y,FixedZ),&ViewPort,&MatrixProj,&MatrixView,&MatrixEmpty);
	return FLTPOINT(coords.x,coords.y);
}

INTPOINT Animation3d::Convert3dTo2d(float x, float y)
{
	D3DXVECTOR3 coords;
	D3DXVec3Project(&coords,&D3DXVECTOR3(x,y,FixedZ),&ViewPort,&MatrixProj,&MatrixView,&MatrixEmpty);
	return INTPOINT((int)coords.x,(int)coords.y);
}

void Animation3d::SetDefaultEffect(EffectEx* effect)
{
	EffectMain=effect;
}

bool Animation3d::Is2dEmulation()
{
	return AnimDelay!=0;
}

/************************************************************************/
/* Animation3dEntity                                                    */
/************************************************************************/

Animation3dEntityVec Animation3dEntity::allEntities;

Animation3dEntity::Animation3dEntity():xFile(NULL),animController(NULL),
renderAnim(0),renderAnimProcFrom(0),renderAnimProcTo(100),
shadowDisabled(false),calcualteTangetSpace(false)
{
	ZeroMemory(&animDataDefault,sizeof(animDataDefault));
}

Animation3dEntity::~Animation3dEntity()
{
	animData.push_back(animDataDefault);
	ZeroMemory(&animDataDefault,sizeof(animDataDefault));
	for(AnimParamsVecIt it=animData.begin(),end=animData.end();it!=end;++it)
	{
		AnimParams& link=*it;
		SAFEDELA(link.LinkBone);
		SAFEDELA(link.ChildFName);
		SAFEDELA(link.DisabledLayers);
		SAFEDELA(link.DisabledSubsets);
		for(uint i=0;i<link.TextureNamesCount;i++)
			SAFEDELA(link.TextureNames[i]);
		SAFEDELA(link.TextureNames);
		SAFEDELA(link.TextureSubsets);
		SAFEDELA(link.TextureNum);
		for(uint i=0;i<link.EffectInstSubsetsCount;i++)
		{
			for(uint j=0;j<link.EffectInst[i].NumDefaults;j++)
			{
				SAFEDELA(link.EffectInst[i].pDefaults[j].pParamName);
				SAFEDELA(link.EffectInst[i].pDefaults[j].pValue);
				SAFEDELA(link.EffectInst[i].pDefaults);
			}
		}
		SAFEDELA(link.EffectInst);
	}
	animData.clear();
	SAFEREL(animController);
}

bool Animation3dEntity::Load(const char* name)
{
	const char* ext=FileManager::GetExtension(name);
	if(!ext) return false;

	// Load fonline 3d file
	if(!_stricmp(ext,"fo3d"))
	{
		// Load main fo3d file
		FileManager fo3d;
		if(!fo3d.LoadFile(name,PT_DATA)) return false;
		char* big_buf=Str::GetBigBuf();
		fo3d.CopyMem(big_buf,fo3d.GetFsize());
		big_buf[fo3d.GetFsize()]=0;

		// Extract file path
		char path[MAX_FOPATH];
		FileManager::ExtractPath(name,path);

		// Parse
		char model[MAX_FOPATH]={0};
		char render_fname[MAX_FOPATH]={0};
		char render_anim[MAX_FOPATH]={0};
		vector<size_t> anim_indexes;

		int mesh=0;
		int subset=-1;
		int layer=-1;
		int layer_val=0;
		D3DXEFFECTINSTANCE* cur_effect=NULL;

		AnimParams dummy_link;
		memzero(&dummy_link,sizeof(dummy_link));
		AnimParams* link=&animDataDefault;
		static uint link_id=0;

		istrstream* istr=new istrstream(big_buf);
		bool closed=false;
		char token[MAX_FOTEXT];
		char line[MAX_FOTEXT];
		char buf[MAX_FOTEXT];
		float valuei=0;
		float valuef=0.0f;
		while(!(*istr).eof())
		{
			(*istr) >> token;
			if((*istr).fail()) break;

			char* comment=strstr(token,";");
			if(!comment) comment=strstr(token,"#");
			if(comment)
			{
				*comment=0;
				(*istr).getline(line,MAX_FOTEXT);
				if(!strlen(token)) continue;
			}

			if(closed)
			{
				if(!_stricmp(token,"ContinueParsing")) closed=false;
				continue;
			}

			if(!_stricmp(token,"StopParsing")) closed=true;
			else if(!_stricmp(token,"Model"))
			{
				(*istr) >> buf;
				FileManager::MakeFilePath(buf,path,model);
			}
			else if(!_stricmp(token,"Include"))
			{
				// Get swapped words
				StrVec templates;
				(*istr).getline(line,MAX_FOTEXT);
				Str::EraseChars(line,'\r');
				Str::EraseChars(line,'\n');
				Str::ParseLine(line,' ',templates,Str::ParseLineDummy);
				if(templates.empty()) continue;
				for(uint i=1,j=(uint)templates.size();i<j-1;i+=2) templates[i]=string("%").append(templates[i]).append("%");

				// Include file path
				char fname[MAX_FOPATH];
				FileManager::MakeFilePath(templates[0].c_str(),path,fname);

				// Load file
				FileManager fo3d_ex;
				if(!fo3d_ex.LoadFile(fname,PT_DATA))
				{
					WriteLogF(_FUNC_," - Include file<%s> not found.\n",fname);
					continue;
				}

				// Words swapping
				uint len=fo3d_ex.GetFsize();
				char* pbuf=(char*)fo3d_ex.ReleaseBuffer();
				if(templates.size()>2)
				{
					// Grow buffer for longer swapped words
					char* tmp_pbuf=new char[len+MAX_FOTEXT];
					memcpy(tmp_pbuf,pbuf,len+1);
					delete[] pbuf;
					pbuf=tmp_pbuf;

					// Swap words
					for(int i=1,j=(int)templates.size();i<j-1;i+=2)
					{
						const char* from=templates[i].c_str();
						const char* to=templates[i+1].c_str();

						char* replace=strstr(pbuf,from);
						while(replace)
						{
							Str::EraseInterval(replace,Str::Length(from));
							Str::Insert(replace,to);
							replace=strstr(pbuf,from);
						}
					}
				}

				// Insert new buffer to main file
				int pos=(int)(*istr).tellg();
				Str::Insert(&big_buf[pos]," ");
				Str::Insert(&big_buf[pos+1],pbuf);
				Str::Insert(&big_buf[pos+1+strlen(pbuf)]," ");
				delete[] pbuf;

				// Reinitialize stream
				delete istr;
				istr=new istrstream(&big_buf[pos]);
			}
			else if(!_stricmp(token,"Root"))
			{
				if(layer<0) link=&animDataDefault;
				else if(layer_val<=0)
				{
					WriteLogF(_FUNC_,"Wrong layer<%d> value<%d>.\n",layer,layer_val);
					link=&dummy_link;
				}
				else
				{
					animData.push_back(AnimParams());
					link=&animData.back();
					ZeroMemory(link,sizeof(AnimParams));
					link->Id=++link_id;
					link->Layer=layer;
					link->LayerValue=layer_val;
				}

				mesh=0;
				subset=-1;
			}
			else if(!_stricmp(token,"Mesh"))
			{
				(*istr) >> buf;
				mesh=ConstantsManager::GetDefineValue(buf);
			}
			else if(!_stricmp(token,"Subset"))
			{
				(*istr) >> buf;
				subset=ConstantsManager::GetDefineValue(buf);
			}
			else if(!_stricmp(token,"Layer") || !_stricmp(token,"Value"))
			{
				(*istr) >> buf;
				if(!_stricmp(token,"Layer")) layer=ConstantsManager::GetDefineValue(buf);
				else layer_val=ConstantsManager::GetDefineValue(buf);

				link=&dummy_link;
				mesh=0;
				subset=-1;
			}
			else if(!_stricmp(token,"Attach"))
			{
				(*istr) >> buf;
				if(layer<0 || layer_val<=0) continue;

				animData.push_back(AnimParams());
				link=&animData.back();
				ZeroMemory(link,sizeof(AnimParams));
				link->Id=++link_id;
				link->Layer=layer;
				link->LayerValue=layer_val;

				char fname[MAX_FOPATH];
				FileManager::MakeFilePath(buf,path,fname);
				link->ChildFName=Str::Duplicate(fname);

				mesh=0;
				subset=-1;
			}
			else if(!_stricmp(token,"Link"))
			{
				(*istr) >> buf;
				if(link->Id) link->LinkBone=Str::Duplicate(buf);
			}
			else if(!_stricmp(token,"RotX")) (*istr) >> link->RotX;
			else if(!_stricmp(token,"RotY")) (*istr) >> link->RotY;
			else if(!_stricmp(token,"RotZ")) (*istr) >> link->RotZ;
			else if(!_stricmp(token,"MoveX")) (*istr) >> link->MoveX;
			else if(!_stricmp(token,"MoveY")) (*istr) >> link->MoveY;
			else if(!_stricmp(token,"MoveZ")) (*istr) >> link->MoveZ;
			else if(!_stricmp(token,"ScaleX")) (*istr) >> link->ScaleX;
			else if(!_stricmp(token,"ScaleY")) (*istr) >> link->ScaleY;
			else if(!_stricmp(token,"ScaleZ")) (*istr) >> link->ScaleZ;
			else if(!_stricmp(token,"Scale"))
			{
				(*istr) >> valuef;
				link->ScaleX=link->ScaleY=link->ScaleZ=valuef;
			}
			else if(!_stricmp(token,"Speed")) (*istr) >> link->SpeedAjust;
			else if(!_stricmp(token,"RotX+")) {(*istr) >> valuef; link->RotX=(link->RotX==0.0f?valuef:link->RotX+valuef);}
			else if(!_stricmp(token,"RotY+")) {(*istr) >> valuef; link->RotY=(link->RotY==0.0f?valuef:link->RotY+valuef);}
			else if(!_stricmp(token,"RotZ+")) {(*istr) >> valuef; link->RotZ=(link->RotZ==0.0f?valuef:link->RotZ+valuef);}
			else if(!_stricmp(token,"MoveX+")) {(*istr) >> valuef; link->MoveX=(link->MoveX==0.0f?valuef:link->MoveX+valuef);}
			else if(!_stricmp(token,"MoveY+")) {(*istr) >> valuef; link->MoveY=(link->MoveY==0.0f?valuef:link->MoveY+valuef);}
			else if(!_stricmp(token,"MoveZ+")) {(*istr) >> valuef; link->MoveZ=(link->MoveZ==0.0f?valuef:link->MoveZ+valuef);}
			else if(!_stricmp(token,"ScaleX+")) {(*istr) >> valuef; link->ScaleX=(link->ScaleX==0.0f?valuef:link->ScaleX+valuef);}
			else if(!_stricmp(token,"ScaleY+")) {(*istr) >> valuef; link->ScaleY=(link->ScaleY==0.0f?valuef:link->ScaleY+valuef);}
			else if(!_stricmp(token,"ScaleZ+")) {(*istr) >> valuef; link->ScaleZ=(link->ScaleZ==0.0f?valuef:link->ScaleZ+valuef);}
			else if(!_stricmp(token,"Scale+"))
			{
				(*istr) >> valuef;
				link->ScaleX=(link->ScaleX==0.0f?valuef:link->ScaleX+valuef);
				link->ScaleY=(link->ScaleY==0.0f?valuef:link->ScaleY+valuef);
				link->ScaleZ=(link->ScaleZ==0.0f?valuef:link->ScaleZ+valuef);
			}
			else if(!_stricmp(token,"Speed+")) {(*istr) >> valuef; link->SpeedAjust=(link->SpeedAjust==0.0f?valuef:link->SpeedAjust*valuef);}
			else if(!_stricmp(token,"RotX*")) {(*istr) >> valuef; link->RotX=(link->RotX==0.0f?valuef:link->RotX*valuef);}
			else if(!_stricmp(token,"RotY*")) {(*istr) >> valuef; link->RotY=(link->RotY==0.0f?valuef:link->RotY*valuef);}
			else if(!_stricmp(token,"RotZ*")) {(*istr) >> valuef; link->RotZ=(link->RotZ==0.0f?valuef:link->RotZ*valuef);}
			else if(!_stricmp(token,"MoveX*")) {(*istr) >> valuef; link->MoveX=(link->MoveX==0.0f?valuef:link->MoveX*valuef);}
			else if(!_stricmp(token,"MoveY*")) {(*istr) >> valuef; link->MoveY=(link->MoveY==0.0f?valuef:link->MoveY*valuef);}
			else if(!_stricmp(token,"MoveZ*")) {(*istr) >> valuef; link->MoveZ=(link->MoveZ==0.0f?valuef:link->MoveZ*valuef);}
			else if(!_stricmp(token,"ScaleX*")) {(*istr) >> valuef; link->ScaleX=(link->ScaleX==0.0f?valuef:link->ScaleX*valuef);}
			else if(!_stricmp(token,"ScaleY*")) {(*istr) >> valuef; link->ScaleY=(link->ScaleY==0.0f?valuef:link->ScaleY*valuef);}
			else if(!_stricmp(token,"ScaleZ*")) {(*istr) >> valuef; link->ScaleZ=(link->ScaleZ==0.0f?valuef:link->ScaleZ*valuef);}
			else if(!_stricmp(token,"Scale*"))
			{
				(*istr) >> valuef;
				link->ScaleX=(link->ScaleX==0.0f?valuef:link->ScaleX*valuef);
				link->ScaleY=(link->ScaleY==0.0f?valuef:link->ScaleY*valuef);
				link->ScaleZ=(link->ScaleZ==0.0f?valuef:link->ScaleZ*valuef);
			}
			else if(!_stricmp(token,"Speed*")) {(*istr) >> valuef; link->SpeedAjust=(link->SpeedAjust==0.0f?valuef:link->SpeedAjust*valuef);}
			else if(!_stricmp(token,"DisableLayer"))
			{
				(*istr) >> buf;
				StrVec layers;
				Str::ParseLine(buf,'-',layers,Str::ParseLineDummy);
				for(uint m=0,n=(uint)layers.size();m<n;m++)
				{
					int layer=ConstantsManager::GetDefineValue(layers[m].c_str());
					if(layer>=0 && layer<LAYERS3D_COUNT)
					{
						int* tmp=link->DisabledLayers;
						link->DisabledLayers=new int[link->DisabledLayersCount+1];
						for(uint h=0;h<link->DisabledLayersCount;h++) link->DisabledLayers[h]=tmp[h];
						if(tmp) delete[] tmp;
						link->DisabledLayers[link->DisabledLayersCount]=layer;
						link->DisabledLayersCount++;
					}
				}
			}
			else if(!_stricmp(token,"DisableSubset"))
			{
				(*istr) >> buf;
				StrVec subsets;
				Str::ParseLine(buf,'-',subsets,Str::ParseLineDummy);
				for(uint m=0,n=(uint)subsets.size();m<n;m++)
				{
					int ss=ConstantsManager::GetDefineValue(subsets[m].c_str());
					if(ss>=0)
					{
						int* tmp=link->DisabledSubsets;
						link->DisabledSubsets=new int[link->DisabledSubsetsCount+1];
						for(uint h=0;h<link->DisabledSubsetsCount;h++) link->DisabledSubsets[h]=tmp[h];
						if(tmp) delete[] tmp;
						link->DisabledSubsets[link->DisabledSubsetsCount]=mesh*100+ss;
						link->DisabledSubsetsCount++;
					}
				}
			}
			else if(!_stricmp(token,"Texture"))
			{
				(*istr) >> buf;
				int index=ConstantsManager::GetDefineValue(buf);
				(*istr) >> buf;
				if(index>=0 && index<EFFECT_TEXTURES)
				{
					char** tmp1=link->TextureNames;
					int* tmp2=link->TextureSubsets;
					int* tmp3=link->TextureNum;
					link->TextureNames=new char*[link->TextureNamesCount+1];
					link->TextureSubsets=new int[link->TextureNamesCount+1];
					link->TextureNum=new int[link->TextureNamesCount+1];
					for(uint h=0;h<link->TextureNamesCount;h++)
					{
						link->TextureNames[h]=tmp1[h];
						link->TextureSubsets[h]=tmp2[h];
						link->TextureNum[h]=tmp3[h];
					}
					if(tmp1) delete[] tmp1;
					if(tmp2) delete[] tmp2;
					if(tmp3) delete[] tmp3;

					link->TextureNames[link->TextureNamesCount]=Str::Duplicate(buf);
					link->TextureSubsets[link->TextureNamesCount]=mesh*100+subset;
					link->TextureNum[link->TextureNamesCount]=index;
					link->TextureNamesCount++;
				}
			}
			else if(!_stricmp(token,"Effect"))
			{
				(*istr) >> buf;
				D3DXEFFECTINSTANCE* effect_inst=new D3DXEFFECTINSTANCE;
				ZeroMemory(effect_inst,sizeof(D3DXEFFECTINSTANCE));
				effect_inst->pEffectFilename=Str::Duplicate(buf);

				D3DXEFFECTINSTANCE* tmp1=link->EffectInst;
				int* tmp2=link->EffectInstSubsets;
				link->EffectInst=new D3DXEFFECTINSTANCE[link->EffectInstSubsetsCount+1];
				link->EffectInstSubsets=new int[link->EffectInstSubsetsCount+1];
				for(uint h=0;h<link->EffectInstSubsetsCount;h++)
				{
					link->EffectInst[h]=tmp1[h];
					link->EffectInstSubsets[h]=tmp2[h];
				}
				if(tmp1) delete[] tmp1;
				if(tmp2) delete[] tmp2;

				link->EffectInst[link->EffectInstSubsetsCount]=*effect_inst;
				link->EffectInstSubsets[link->EffectInstSubsetsCount]=mesh*100+subset;
				link->EffectInstSubsetsCount++;

				cur_effect=&link->EffectInst[link->EffectInstSubsetsCount-1];
			}
			else if(!_stricmp(token,"EffDef"))
			{
				char def_name[MAX_FOTEXT];
				char def_value[MAX_FOTEXT];
				(*istr) >> buf;
				(*istr) >> def_name;
				(*istr) >> def_value;

				if(!cur_effect) continue;

				D3DXEFFECTDEFAULTTYPE type;
				char* data=NULL;
				uint data_len=0;
				if(!_stricmp(buf,"String"))
				{
					type=D3DXEDT_STRING;
					data=Str::Duplicate(def_value);
					data_len=Str::Length(data);
				}
				else if(!_stricmp(buf,"Floats"))
				{
					type=D3DXEDT_FLOATS;
					StrVec floats;
					Str::ParseLine(def_value,'-',floats,Str::ParseLineDummy);
					if(floats.empty()) continue;
					data_len=(uint)floats.size()*sizeof(float);
					data=new(nothrow) char[data_len];
					for(uint i=0,j=(uint)floats.size();i<j;i++) ((float*)data)[i]=(float)atof(floats[i].c_str());
				}
				else if(!_stricmp(buf,"Dword"))
				{
					type=D3DXEDT_DWORD;
					data_len=sizeof(uint);
					data=new char[data_len];
					*((uint*)data)=ConstantsManager::GetDefineValue(def_value);
				}
				else continue;

				LPD3DXEFFECTDEFAULT tmp=cur_effect->pDefaults;
				cur_effect->pDefaults=new D3DXEFFECTDEFAULT[cur_effect->NumDefaults+1];
				for(uint h=0;h<cur_effect->NumDefaults;h++) cur_effect->pDefaults[h]=tmp[h];
				if(tmp) delete[] tmp;
				cur_effect->pDefaults[cur_effect->NumDefaults].Type=type;
				cur_effect->pDefaults[cur_effect->NumDefaults].pParamName=Str::Duplicate(def_name);
				cur_effect->pDefaults[cur_effect->NumDefaults].NumBytes=data_len;
				cur_effect->pDefaults[cur_effect->NumDefaults].pValue=data;
				cur_effect->NumDefaults++;
			}
			else if(!_stricmp(token,"Anim") || !_stricmp(token,"AnimSpeed"))
			{
				// Index animation
				int ind1=0,ind2=0;
				(*istr) >> buf;
				ind1=ConstantsManager::GetDefineValue(buf);
				(*istr) >> buf;
				ind2=ConstantsManager::GetDefineValue(buf);

				if(!_stricmp(token,"Anim"))
				{
					// Deferred loading
					// Todo: Reverse play

					anim_indexes.push_back((ind1<<8)|ind2);
					(*istr) >> buf;
					anim_indexes.push_back((size_t)Str::Duplicate(buf));
					(*istr) >> buf;
					anim_indexes.push_back((size_t)Str::Duplicate(buf));
				}
				else
				{
					(*istr) >> valuef;
					animSpeed.insert(IntFloatMapVal((ind1<<8)|ind2,valuef));
				}
			}
			else if(!_stricmp(token,"AnimEqual"))
			{
				(*istr) >> valuei;

				int ind1=0,ind2=0;
				(*istr) >> buf;
				ind1=ConstantsManager::GetDefineValue(buf);
				(*istr) >> buf;
				ind2=ConstantsManager::GetDefineValue(buf);

				if(valuei==1) anim1Equals.insert(IntMapVal(ind1,ind2));
				else if(valuei==2) anim2Equals.insert(IntMapVal(ind1,ind2));
			}
			else if(!_stricmp(token,"RenderFrame") || !_stricmp(token,"RenderFrames"))
			{
				anim_indexes.push_back(0);
				(*istr) >> buf;
				Str::Copy(render_fname,buf);
				anim_indexes.push_back((size_t)Str::Duplicate(buf));
				(*istr) >> buf;
				Str::Copy(render_anim,buf);
				anim_indexes.push_back((size_t)Str::Duplicate(buf));

				(*istr) >> renderAnimProcFrom;

				// One frame
				renderAnimProcTo=renderAnimProcFrom;

				// Many frames
				if(!_stricmp(token,"RenderFrames")) (*istr) >> renderAnimProcTo;

				// Check
				renderAnimProcFrom=CLAMP(renderAnimProcFrom,0,100);
				renderAnimProcTo=CLAMP(renderAnimProcTo,0,100);
			}
			else if(!_stricmp(token,"DisableShadow")) shadowDisabled=true;
			else if(!_stricmp(token,"CalculateTangentSpace")) calcualteTangetSpace=true;
			else
			{
				WriteLogF(_FUNC_," - Unknown token<%s> in file<%s>.\n",token,name);
			}
		}

		// Process pathes
		if(!model[0])
		{
			WriteLogF(_FUNC_," - 'Model' section not found in file<%s>.\n",name);
			return false;
		}

		// Load x file
		Animation3dXFile* xfile=Animation3dXFile::GetXFile(model,calcualteTangetSpace);
		if(!xfile) return false;

		// Create animation
		fileName=name;
		xFile=xfile;

		// Parse animations
		AnimSetVec animations;
		uint max_matrices=0;
		for(uint i=0,j=(uint)anim_indexes.size();i<j;i+=3)
		{
			char* anim_fname=(char*)anim_indexes[i+1];
			char* anim_name=(char*)anim_indexes[i+2];

			char anim_path[MAX_FOPATH];
			if(!_stricmp(anim_fname,"ModelFile"))
				Str::Copy(anim_path,model);
			else
				FileManager::MakeFilePath(anim_fname,path,anim_path);

			AnimSet* set=Loader3d::LoadAnimation(D3DDevice,anim_path,anim_name);
			if(set)
			{
				animations.push_back(set);
				max_matrices=max(max_matrices,set->GetNumAnimations());
			}
		}

		if(animations.size())
		{
			if(SUCCEEDED(D3DXCreateAnimationController(max_matrices,(uint)animations.size(),2,10,&animController)))
			{
				numAnimationSets=(uint)animations.size();
				for(uint i=0,j=(uint)animations.size();i<j;i++)
					animController->RegisterAnimationSet(animations[i]);
			}
			else
			{
				WriteLogF(_FUNC_," - Unable to create animation controller, file<%s>.\n",name);
			}
		}

		for(uint i=0,j=(uint)anim_indexes.size();i<j;i+=3)
		{
			int anim_index=(int)anim_indexes[i+0];
			char* anim_fname=(char*)anim_indexes[i+1];
			char* anim_name=(char*)anim_indexes[i+2];

			if(anim_index && animController)
			{
				int anim_set=abs(atoi(anim_name));
				if(!Str::IsNumber(anim_name)) anim_set=GetAnimationIndex(anim_name[0]!='-'?anim_name:&anim_name[1]);
				if(anim_set>=0 && anim_set<(int)numAnimationSets) animIndexes.insert(IntMapVal(anim_index,anim_set));
			}

			delete[] anim_fname;
			delete[] anim_name;
		}

		if(render_fname[0] && render_anim[0] && animController)
		{
			int anim_set=abs(atoi(render_anim));
			if(!Str::IsNumber(render_anim)) anim_set=GetAnimationIndex(render_anim[0]!='-'?render_anim:&render_anim[1]);
			if(anim_set>=0 && anim_set<(int)numAnimationSets) renderAnim=anim_set;
		}

		if(animController) Animation3dXFile::SetupAnimationOutput(xFile->frameRoot,animController);
	}
	// Load just x file
	else
	{
		Animation3dXFile* xfile=Animation3dXFile::GetXFile(name,false);
		if(!xfile) return false;

		// Create animation
		fileName=name;
		xFile=xfile;

		// Todo: process default animations
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
			Str::EraseInterval(token,Str::Length(from));
			Str::Insert(token,to);
			token=strstr(str,from);
		}
	}
}

int Animation3dEntity::GetAnimationIndex(const char* anim_name)
{
	if(!animController) return -1;

	ID3DXAnimationSet* set;
	if(animController->GetAnimationSetByName(anim_name,&set)!=S_OK) return -1;

	int result=0;
	for(uint i=0;i<numAnimationSets;i++)
	{
		ID3DXAnimationSet* set_;
		animController->GetAnimationSet(i,&set_);
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

int Animation3dEntity::GetAnimationIndex(uint& anim1, uint& anim2, float* speed)
{
	// Find index
	int index=GetAnimationIndexEx(anim1,anim2,speed);
	if(index!=-1) return index;

	// Find substitute animation
	uint crtype=0;
	uint crtype_base=crtype,anim1_base=anim1,anim2_base=anim2;
#ifdef FONLINE_CLIENT
	while(index==-1 && Script::PrepareContext(ClientFunctions.CritterAnimationSubstitute,_FUNC_,"Anim"))
#else // FONLINE_MAPPER
	while(index==-1 && Script::PrepareContext(MapperFunctions.CritterAnimationSubstitute,_FUNC_,"Anim"))
#endif
	{
		uint crtype_=crtype,anim1_=anim1,anim2_=anim2;
		Script::SetArgUInt(ANIM_TYPE_3D);
		Script::SetArgUInt(crtype_base);
		Script::SetArgUInt(anim1_base);
		Script::SetArgUInt(anim2_base);
		Script::SetArgAddress(&crtype);
		Script::SetArgAddress(&anim1);
		Script::SetArgAddress(&anim2);
		if(Script::RunPrepared() && Script::GetReturnedBool() &&
			(crtype_!=crtype || anim1!=anim1_ || anim2!=anim2_)) index=GetAnimationIndexEx(anim1,anim2,speed);
		else break;
	}

	return index;
}

int Animation3dEntity::GetAnimationIndexEx(uint anim1, uint anim2, float* speed)
{
	// Check equals
	IntMapIt it1=anim1Equals.find(anim1);
	if(it1!=anim1Equals.end()) anim1=(*it1).second;
	IntMapIt it2=anim2Equals.find(anim2);
	if(it2!=anim2Equals.end()) anim2=(*it2).second;

	// Make index
	int ii=(anim1<<8)|anim2;

	// Speed
	if(speed)
	{
		IntFloatMapIt it=animSpeed.find(ii);
		if(it!=animSpeed.end()) *speed=(*it).second;
		else *speed=1.0f;
	}

	// Find number of animation
	IntMapIt it=animIndexes.find(ii);
	if(it!=animIndexes.end()) return (*it).second;

	return -1;
}

Animation3d* Animation3dEntity::CloneAnimation()
{
	// Create instance
	Animation3d* a3d=new(nothrow) Animation3d();
	if(!a3d) return NULL;

	a3d->animEntity=this;
	a3d->lastTick=Timer::GameTick();

	if(animController)
	{
		a3d->numAnimationSets=animController->GetMaxNumAnimationSets();

		// Clone the original AC.  This clone is what we will use to animate
		// this mesh; the original never gets used except to clone, since we
		// always need to be able to add another instance at any time.
		D3D_HR(animController->CloneAnimationController(
			animController->GetMaxNumAnimationOutputs(),
			animController->GetMaxNumAnimationSets(),
			animController->GetMaxNumTracks(),
			animController->GetMaxNumEvents(),
			&a3d->animController));
	}

	return a3d;
}

Animation3dEntity* Animation3dEntity::GetEntity(const char* name)
{
	// Try find instance
	Animation3dEntity* entity=NULL;
	for(Animation3dEntityVecIt it=allEntities.begin(),end=allEntities.end();it!=end;++it)
	{
		Animation3dEntity* e=*it;
		if(e->fileName==name)
		{
			entity=e;
			break;
		}
	}

	// Create new instance
	if(!entity)
	{
		entity=new(nothrow) Animation3dEntity();
		if(!entity || !entity->Load(name))
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

Animation3dXFile::Animation3dXFile():frameRoot(NULL),
facesCount(0),tangentsCalculated(false)
{
}

Animation3dXFile::~Animation3dXFile()
{
	Loader3d::Free(frameRoot);
	frameRoot=NULL;
}

Animation3dXFile* Animation3dXFile::GetXFile(const char* xname, bool calc_tangent)
{
	Animation3dXFile* xfile=NULL;

	for(Animation3dXFileVecIt it=xFiles.begin(),end=xFiles.end();it!=end;++it)
	{
		Animation3dXFile* x=*it;
		if(x->fileName==xname)
		{
			xfile=x;

			if(calc_tangent && !x->tangentsCalculated)
			{
				xfile->CalculateNormalTangent((FrameEx*)xfile->frameRoot);
				xfile->tangentsCalculated=true;
			}
			break;
		}
	}

	if(!xfile)
	{
		// Load
		D3DXFRAME* frame_root=Loader3d::LoadModel(D3DDevice,xname,calc_tangent);
		if(!frame_root)
		{
			WriteLogF(_FUNC_," - Unable to load 3d file<%s>.\n",xname);
			return NULL;
		}

		xfile=new(nothrow) Animation3dXFile();
		if(!xfile)
		{
			WriteLogF(_FUNC_," - Allocation fail, x file<%s>.\n",xname);
			return NULL;
		}

		xfile->fileName=xname;
		xfile->frameRoot=frame_root;
		xfile->tangentsCalculated=calc_tangent;

		// Skinning
		xfile->SetupSkinning(xfile,(FrameEx*)xfile->frameRoot,(FrameEx*)xfile->frameRoot);

		// Setup normals and tangent space
		if(calc_tangent)
		{
			const char* ext=FileManager::GetExtension(xname);
			if(ext && !_stricmp(ext,"x")) xfile->CalculateNormalTangent((FrameEx*)xfile->frameRoot);
		}

		// Faces count
		xfile->SetupFacesCount((FrameEx*)xfile->frameRoot,xfile->facesCount);

		xFiles.push_back(xfile);
	}

	return xfile;
}

// Need to go through the hierarchy and set the combined matrices calls itself recursively as it tareverses the hierarchy
bool Animation3dXFile::SetupSkinning(Animation3dXFile* xfile, FrameEx* frame, FrameEx* frame_root)
{
	// Cast to our extended structure first
	D3DXMESHCONTAINER_EXTENDED* mesh_container=(D3DXMESHCONTAINER_EXTENDED*)frame->pMeshContainer;

	// If this frame has a mesh
	while(mesh_container)
	{
		if(mesh_container->MeshData.pMesh) xfile->allMeshes.push_back(mesh_container);

		// Skinned data
		if(mesh_container->MeshData.pMesh && mesh_container->pSkinInfo)
		{
			if(SkinningMethod==SKINNING_HLSL_SHADER)
			{
				// Get palette size
				mesh_container->exNumPaletteEntries=mesh_container->pSkinInfo->GetNumBones();

				DWORD* new_adjency=new(nothrow) DWORD[mesh_container->MeshData.pMesh->GetNumFaces()*3];
				if(!new_adjency) return false;

				D3D_HR(mesh_container->pSkinInfo->ConvertToIndexedBlendedMesh(
					mesh_container->MeshData.pMesh,
					D3DXMESHOPT_VERTEXCACHE|D3DXMESH_MANAGED,
					mesh_container->exNumPaletteEntries,
					mesh_container->pAdjacency,new_adjency,
					NULL,NULL,
					&mesh_container->exNumInfl,
					&mesh_container->exNumAttributeGroups,
					&mesh_container->exBoneCombinationBuf,
					&mesh_container->exSkinMeshBlended));

				delete[] mesh_container->pAdjacency;
				mesh_container->pAdjacency=new_adjency;

				// FVF has to match our declarator. Vertex shaders are not as forgiving as FF pipeline
				uint fvf=(mesh_container->exSkinMeshBlended->GetFVF()&D3DFVF_POSITION_MASK)|D3DFVF_NORMAL|D3DFVF_TEX1|D3DFVF_LASTBETA_UBYTE4;
				if(fvf!=mesh_container->exSkinMeshBlended->GetFVF())
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
			for(uint i=0,j=mesh_container->pSkinInfo->GetNumBones();i<j;i++)
			{
				// Find the frame containing the bone
				FrameEx* tmp_frame=(FrameEx*)D3DXFrameFind(frame_root,mesh_container->pSkinInfo->GetBoneName(i));

				// Set the bone part - point it at the transformation matrix
				if(tmp_frame)
				{
					mesh_container->exFrameCombinedMatrixPointer[i]=&tmp_frame->exCombinedTransformationMatrix;
					FrameVec& frames=xfile->framesSkinned;
					if(std::find(frames.begin(),frames.end(),tmp_frame)==frames.end()) frames.push_back(tmp_frame);
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

	if(frame->pFrameSibling) SetupSkinning(xfile,(FrameEx*)frame->pFrameSibling,frame_root);
	if(frame->pFrameFirstChild) SetupSkinning(xfile,(FrameEx*)frame->pFrameFirstChild,frame_root);
	return true;
}

bool Animation3dXFile::CalculateNormalTangent(FrameEx* frame)
{
	D3DXMESHCONTAINER_EXTENDED* mesh_container=(D3DXMESHCONTAINER_EXTENDED*)frame->pMeshContainer;
	while(mesh_container)
	{
		if(mesh_container->MeshData.pMesh && (!mesh_container->pSkinInfo || mesh_container->exSkinMeshBlended))
		{
			LPD3DXMESH& mesh=(mesh_container->exSkinMeshBlended?mesh_container->exSkinMeshBlended:mesh_container->MeshData.pMesh);

			D3DVERTEXELEMENT9 declaration[MAX_FVF_DECL_SIZE];
			LPD3DVERTEXELEMENT9 declaration_ptr=declaration;
			D3D_HR(mesh->GetDeclaration(declaration));

			bool is_texcoord=false,is_normal=false,is_tangent=false,is_binormal=false;
			while(declaration_ptr->Stream!=0xFF)
			{
				if(declaration_ptr->Usage==D3DDECLUSAGE_TEXCOORD) is_texcoord=true;
				else if(declaration_ptr->Usage==D3DDECLUSAGE_NORMAL) is_normal=true;
				else if(declaration_ptr->Usage==D3DDECLUSAGE_TANGENT) is_tangent=true;
				else if(declaration_ptr->Usage==D3DDECLUSAGE_BINORMAL) is_binormal=true;
				declaration_ptr++;
			}

			if(!is_normal || (is_texcoord && (!is_tangent || !is_binormal)))
			{
#define ADD_FVF_DECL(d,s,o,t,m,u,ui) {d->Stream=s;d->Offset=o;d->Type=t;d->Method=m;d->Usage=u;d->UsageIndex=ui;d++; offs+=12;}
				int offs=(declaration_ptr-1)->Offset+(((int)(declaration_ptr-1)->Type+1)*4);
				if(!is_normal) ADD_FVF_DECL(declaration_ptr,0,offs,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_NORMAL,0);
				if(is_texcoord && !is_tangent) ADD_FVF_DECL(declaration_ptr,0,offs,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TANGENT,0);
				if(is_texcoord && !is_binormal) ADD_FVF_DECL(declaration_ptr,0,offs,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_BINORMAL,0);
				ADD_FVF_DECL(declaration_ptr,0xFF,0,D3DDECLTYPE_UNUSED,0,0,0);

				LPD3DXMESH tmp_mesh=NULL;
				D3D_HR(mesh->CloneMesh(D3DXMESH_MANAGED,declaration,D3DDevice,&tmp_mesh));
				mesh->Release();
				mesh=tmp_mesh;

				D3D_HR(D3DXComputeTangentFrameEx(mesh,
					is_texcoord?D3DDECLUSAGE_TEXCOORD:D3DX_DEFAULT,0,
					is_texcoord && !is_tangent?D3DDECLUSAGE_TANGENT:D3DX_DEFAULT,0,
					is_texcoord && !is_binormal?D3DDECLUSAGE_BINORMAL:D3DX_DEFAULT,0,
					!is_normal?D3DDECLUSAGE_NORMAL:D3DX_DEFAULT,0,
					(!is_normal?D3DXTANGENT_CALCULATE_NORMALS:0),
					mesh_container->pAdjacency,0.01f,0.25f,0.01f,&tmp_mesh,NULL));
				mesh->Release();
				mesh=tmp_mesh;
			}
		}

		mesh_container=(D3DXMESHCONTAINER_EXTENDED*)mesh_container->pNextMeshContainer;
	}

	if(frame->pFrameSibling) CalculateNormalTangent((FrameEx*)frame->pFrameSibling);
	if(frame->pFrameFirstChild) CalculateNormalTangent((FrameEx*)frame->pFrameFirstChild);
	return true;
}

void Animation3dXFile::SetupFacesCount(FrameEx* frame, uint& count)
{
	D3DXMESHCONTAINER_EXTENDED* mesh_container=(D3DXMESHCONTAINER_EXTENDED*)frame->pMeshContainer;
	while(mesh_container)
	{
		LPD3DXMESH mesh=(mesh_container->pSkinInfo?mesh_container->exSkinMesh:mesh_container->MeshData.pMesh);
		count+=mesh->GetNumFaces();
		mesh_container=(D3DXMESHCONTAINER_EXTENDED*)mesh_container->pNextMeshContainer;
	}

	if(frame->pFrameSibling) SetupFacesCount((FrameEx*)frame->pFrameSibling,count);
	if(frame->pFrameFirstChild) SetupFacesCount((FrameEx*)frame->pFrameFirstChild,count);
}

void Animation3dXFile::SetupAnimationOutput(D3DXFRAME* frame, ID3DXAnimationController* anim_controller)
{
	if(frame->Name && frame->Name[0]) anim_controller->RegisterAnimationOutput(frame->Name,&frame->TransformationMatrix,NULL,NULL,NULL);
	if(frame->pFrameSibling) SetupAnimationOutput(frame->pFrameSibling,anim_controller);
	if(frame->pFrameFirstChild) SetupAnimationOutput(frame->pFrameFirstChild,anim_controller);
}

TextureEx* Animation3dXFile::GetTexture(const char* tex_name)
{
	TextureEx* texture=Loader3d::LoadTexture(D3DDevice,tex_name,fileName.c_str());
	if(!texture) WriteLogF(_FUNC_," - Can't load texture<%s>.\n",tex_name?tex_name:"nullptr");
	return texture;
}

EffectEx* Animation3dXFile::GetEffect(D3DXEFFECTINSTANCE* effect_inst)
{
	EffectEx* effect=Loader3d::LoadEffect(D3DDevice,effect_inst,fileName.c_str());
	if(!effect) WriteLogF(_FUNC_," - Can't load effect<%s>.\n",effect_inst && effect_inst->pEffectFilename?effect_inst->pEffectFilename:"nullptr");
	return effect;
}

/************************************************************************/
/*                                                                      */
/************************************************************************/
