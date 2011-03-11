#include "StdAfx.h"
#include "ResourceManager.h"
#include "Common.h"
#include "FileManager.h"
#include "DataFile.h"
#include "CritterType.h"
#include "Script.h"

ResourceManager ResMngr;

void ResourceManager::Refresh()
{
	// Folders, unpacked data
	static bool folders_done=false;
	if(!folders_done)
	{
		// All names
		StrVec file_names;
		FileManager::GetFolderFileNames(FileManager::GetPath(PT_DATA),true,NULL,file_names);
		for(StrVecIt it=file_names.begin(),end=file_names.end();it!=end;++it) Str::AddNameHash((*it).c_str());

		// Splashes
		StrVec splashes;
		FileManager::GetFolderFileNames(FileManager::GetPath(PT_ART_SPLASH),true,"rix",splashes);
		FileManager::GetFolderFileNames(FileManager::GetPath(PT_ART_SPLASH),true,"png",splashes);
		FileManager::GetFolderFileNames(FileManager::GetPath(PT_ART_SPLASH),true,"jpg",splashes);
		for(StrVecIt it=splashes.begin(),end=splashes.end();it!=end;++it)
			if(std::find(splashNames.begin(),splashNames.end(),*it)==splashNames.end())
				splashNames.push_back(*it);

		// Sound names
		StrVec sounds;
		FileManager::GetFolderFileNames(FileManager::GetPath(PT_SND_SFX),true,"wav",sounds);
		FileManager::GetFolderFileNames(FileManager::GetPath(PT_SND_SFX),true,"acm",sounds);
		FileManager::GetFolderFileNames(FileManager::GetPath(PT_SND_SFX),true,"ogg",sounds);
		char fname[MAX_FOPATH];
		char name[MAX_FOPATH];
		for(StrVecIt it=sounds.begin(),end=sounds.end();it!=end;++it)
		{
			FileManager::ExtractFileName((*it).c_str(),fname);
			StringCopy(name,fname);
			Str::Upr(name);
			char* ext=(char*)FileManager::GetExtension(name);
			if(!ext) continue;
			*(--ext)=0;
			if(name[0]) soundNames.insert(StrMapVal(name,fname));
		}

		folders_done=true;
	}

	// Dat files, packed data
	DataFileVec& pfiles=FileManager::GetDataFiles();
	for(DataFileVecIt it=pfiles.begin(),end=pfiles.end();it!=end;++it)
	{
		DataFile* pfile=*it;
		if(std::find(processedDats.begin(),processedDats.end(),pfile)==processedDats.end())
		{
			// All names
			StrVec file_names;
			pfile->GetFileNames(FileManager::GetPath(PT_DATA),true,NULL,file_names);
			for(StrVecIt it=file_names.begin(),end=file_names.end();it!=end;++it) Str::AddNameHash((*it).c_str());

			// Splashes
			StrVec splashes;
			pfile->GetFileNames(FileManager::GetPath(PT_ART_SPLASH),true,"rix",splashes);
			pfile->GetFileNames(FileManager::GetPath(PT_ART_SPLASH),true,"png",splashes);
			pfile->GetFileNames(FileManager::GetPath(PT_ART_SPLASH),true,"jpg",splashes);
			for(StrVecIt it=splashes.begin(),end=splashes.end();it!=end;++it)
				if(std::find(splashNames.begin(),splashNames.end(),*it)==splashNames.end())
					splashNames.push_back(*it);

			// Sound names
			StrVec sounds;
			pfile->GetFileNames(FileManager::GetPath(PT_SND_SFX),true,"wav",sounds);
			pfile->GetFileNames(FileManager::GetPath(PT_SND_SFX),true,"acm",sounds);
			pfile->GetFileNames(FileManager::GetPath(PT_SND_SFX),true,"ogg",sounds);
			char fname[MAX_FOPATH];
			char name[MAX_FOPATH];
			for(StrVecIt it=sounds.begin(),end=sounds.end();it!=end;++it)
			{
				FileManager::ExtractFileName((*it).c_str(),fname);
				StringCopy(name,fname);
				Str::Upr(name);
				char* ext=(char*)FileManager::GetExtension(name);
				if(!ext) continue;
				*(--ext)=0;
				if(name[0]) soundNames.insert(StrMapVal(name,fname));
			}

			processedDats.push_back(pfile);
		}
	}
}

void ResourceManager::Finish()
{
	WriteLog("Resource manager finish...\n");
	loadedAnims.clear();
	WriteLog("Resource manager finish complete.\n");
}

void ResourceManager::FreeResources(int type)
{
	if(type==RES_IFACE)
	{
		SprMngr.FreeSurfaces(RES_IFACE);
		for(LoadedAnimMapIt it=loadedAnims.begin();it!=loadedAnims.end();)
		{
			if((*it).second.ResType==RES_IFACE) it=loadedAnims.erase(it);
			else ++it;
		}
	}
	else if(type==RES_IFACE_EXT)
	{
		SprMngr.FreeSurfaces(RES_IFACE_EXT);
		for(LoadedAnimMapIt it=loadedAnims.begin();it!=loadedAnims.end();)
		{
			if((*it).second.ResType==RES_IFACE_EXT) it=loadedAnims.erase(it);
			else ++it;
		}
	}
	else if(type==RES_CRITTERS)
	{
		for(AnimMapIt it=critterFrames.begin(),end=critterFrames.end();it!=end;++it) SAFEDEL((*it).second);
		critterFrames.clear();
		SprMngr.FreeSurfaces(RES_CRITTERS);
		SprMngr.ClearSpriteContours();
	}
	else if(type==RES_ITEMS)
	{
		SprMngr.FreeSurfaces(RES_ITEMS);
		for(LoadedAnimMapIt it=loadedAnims.begin();it!=loadedAnims.end();)
		{
			if((*it).second.ResType==RES_ITEMS) it=loadedAnims.erase(it);
			else ++it;
		}
	}
	else if(type==RES_SCRIPT)
	{
		SprMngr.FreeSurfaces(RES_SCRIPT);
		for(LoadedAnimMapIt it=loadedAnims.begin();it!=loadedAnims.end();)
		{
			if((*it).second.ResType==RES_SCRIPT) it=loadedAnims.erase(it);
			else ++it;
		}
	}
}

AnyFrames* ResourceManager::GetAnim(DWORD name_hash, int dir, int res_type)
{
	// Find already loaded
	DWORD id=name_hash+dir;
	LoadedAnimMapIt it=loadedAnims.find(id);
	if(it!=loadedAnims.end()) return (*it).second.Anim;

	// Load new animation
	const char* fname=Str::GetName(name_hash);
	if(!fname) return NULL;

	SprMngr.SurfType=res_type;
	AnyFrames* anim=SprMngr.LoadAnimation(fname,PT_DATA,ANIM_DIR(dir)|ANIM_FRM_ANIM_PIX);
	SprMngr.SurfType=RES_NONE;

	loadedAnims.insert(LoadedAnimMapVal(id,LoadedAnim(res_type,anim)));
	return anim;
}

DWORD AnimMapId(DWORD crtype, DWORD anim1, DWORD anim2, int dir, bool is_fallout)
{
	DWORD dw[5]={crtype,anim1,anim2,dir,is_fallout?-1:1};
	return Crypt.Crc32((BYTE*)&dw[0],sizeof(dw));
}

AnyFrames* ResourceManager::GetCrit2dAnim(DWORD crtype, DWORD anim1, DWORD anim2, int dir)
{
	// Check for 3d
	DWORD anim_type=CritType::GetAnimType(crtype);
	if(anim_type==ANIM_TYPE_3D) return NULL;

	// Process dir
	dir=CLAMP(dir,0,DIRS_COUNT-1);
	if(!CritType::IsCanRotate(crtype)) dir=0;

	// Make animation id
	DWORD id=AnimMapId(crtype,anim1,anim2,dir,false);

	// Check already loaded
	AnimMapIt it=critterFrames.find(id);
	if(it!=critterFrames.end()) return (*it).second;

	// Process loading
	DWORD crtype_base=crtype,anim1_base=anim1,anim2_base=anim2;
	AnyFrames* anim=NULL;
	while(true)
	{
		// Load
		if(anim_type==ANIM_TYPE_FALLOUT)
		{
			// Hardcoded
			anim=LoadFalloutAnim(crtype,anim1,anim2,dir);
		}
		else
		{
			// Script specific
			DWORD pass_base=0;
#ifdef FONLINE_CLIENT
			while(Script::PrepareContext(ClientFunctions.CritterAnimation,CALL_FUNC_STR,"Anim"))
#else // FONLINE_MAPPER
			while(Script::PrepareContext(MapperFunctions.CritterAnimation,CALL_FUNC_STR,"Anim"))
#endif
			{
#define ANIM_FLAG_FIRST_FRAME        (0x01)
#define ANIM_FLAG_LAST_FRAME         (0x02)

				DWORD pass=pass_base;
				DWORD flags=0;
				int ox=0,oy=0;
				Script::SetArgDword(anim_type);
				Script::SetArgDword(crtype);
				Script::SetArgDword(anim1);
				Script::SetArgDword(anim2);
				Script::SetArgAddress(&pass);
				Script::SetArgAddress(&flags);
				Script::SetArgAddress(&ox);
				Script::SetArgAddress(&oy);
				if(Script::RunPrepared())
				{
					CScriptString* str=(CScriptString*)Script::GetReturnedObject();
					if(str)
					{
						if(str->length())
						{
							SprMngr.SurfType=RES_CRITTERS;
							anim=SprMngr.LoadAnimation(str->c_str(),PT_DATA,ANIM_DIR(dir));
							if(!anim) anim=SprMngr.LoadAnimation(str->c_str(),PT_DATA,ANIM_DIR(0));
							SprMngr.SurfType=RES_NONE;

							// Process flags
							if(anim && flags)
							{
								if(FLAG(flags,ANIM_FLAG_FIRST_FRAME) || FLAG(flags,ANIM_FLAG_LAST_FRAME))
								{
									bool first=FLAG(flags,ANIM_FLAG_FIRST_FRAME);

									// Append offsets
									if(!first)
									{
										for(DWORD i=0;i<anim->CntFrm-1;i++)
										{
											anim->NextX[anim->CntFrm-1]+=anim->NextX[i];
											anim->NextY[anim->CntFrm-1]+=anim->NextY[i];
										}
									}

									// Change size
									DWORD spr_id=(first?anim->Ind[0]:anim->Ind[anim->CntFrm-1]);
									short nx=(first?anim->NextX[0]:anim->NextX[anim->CntFrm-1]);
									short ny=(first?anim->NextY[0]:anim->NextY[anim->CntFrm-1]);
									delete[] anim->Ind;
									delete[] anim->NextX;
									delete[] anim->NextY;
									anim->Ind=new DWORD[1];
									anim->NextX=new short[1];
									anim->NextY=new short[1];
									anim->Ind[0]=spr_id;
									anim->NextX[0]=nx;
									anim->NextY[0]=ny;
									anim->CntFrm=1;
								}
							}

							// Add offsets
							if(anim && (ox || oy) && false)
							{
								for(DWORD i=0;i<anim->CntFrm;i++)
								{
									DWORD spr_id=anim->Ind[i];
									bool fixed=false;
									for(DWORD j=0;j<i;j++)
									{
										if(anim->Ind[j]==spr_id)
										{
											fixed=true;
											break;
										}
									}
									if(!fixed)
									{
										SpriteInfo* si=SprMngr.GetSpriteInfo(spr_id);
										si->OffsX+=ox;
										si->OffsY+=oy;
									}
								}
							}
						}
						str->Release();
					}

					// If pass changed and animation not loaded than try again
					if(!anim && pass!=pass_base)
					{
						pass_base=pass;
						continue;
					}

					// Done
					break;
				}
			}
		}

		// Find substitute animation
#ifdef FONLINE_CLIENT
		if(!anim && Script::PrepareContext(ClientFunctions.CritterAnimationSubstitute,CALL_FUNC_STR,"Anim"))
#else // FONLINE_MAPPER
		if(!anim && Script::PrepareContext(MapperFunctions.CritterAnimationSubstitute,CALL_FUNC_STR,"Anim"))
#endif
		{
			DWORD crtype_=crtype,anim1_=anim1,anim2_=anim2;
			Script::SetArgDword(anim_type);
			Script::SetArgDword(crtype_base);
			Script::SetArgDword(anim1_base);
			Script::SetArgDword(anim2_base);
			Script::SetArgAddress(&crtype);
			Script::SetArgAddress(&anim1);
			Script::SetArgAddress(&anim2);
			if(Script::RunPrepared() && Script::GetReturnedBool() &&
				(crtype_!=crtype || anim1!=anim1_ || anim2!=anim2_)) continue;
		}

		// Exit from loop
		break;
	}

	// Store resulted animation indices
	if(anim)
	{
		anim->Anim1=anim1;
		anim->Anim2=anim2;
	}

	// Store
	critterFrames.insert(AnimMapVal(id,anim));
	return anim;
}

AnyFrames* ResourceManager::LoadFalloutAnim(DWORD crtype, DWORD anim1, DWORD anim2, int dir)
{
	// Convert from common to fallout specific
#ifdef FONLINE_CLIENT
	if(Script::PrepareContext(ClientFunctions.CritterAnimationFallout,CALL_FUNC_STR,"Anim"))
#else // FONLINE_MAPPER
	if(Script::PrepareContext(MapperFunctions.CritterAnimationFallout,CALL_FUNC_STR,"Anim"))
#endif
	{
		DWORD anim1ex=0,anim2ex=0,flags=0;
		Script::SetArgDword(crtype);
		Script::SetArgAddress(&anim1);
		Script::SetArgAddress(&anim2);
		Script::SetArgAddress(&anim1ex);
		Script::SetArgAddress(&anim2ex);
		Script::SetArgAddress(&flags);
		if(Script::RunPrepared() && Script::GetReturnedBool())
		{
			// Load
			AnyFrames* anim=LoadFalloutAnimSpr(crtype,anim1,anim2,dir);
			if(!anim) return NULL;

			// Merge
			if(anim1ex && anim2ex)
			{
				AnyFrames* animex=LoadFalloutAnimSpr(crtype,anim1ex,anim2ex,dir);
				if(!animex) return NULL;

				AnyFrames* anim_=new AnyFrames();
				anim_->CntFrm=anim->CntFrm+animex->CntFrm;
				anim_->Ticks=anim->Ticks+animex->Ticks;
				anim_->Ind=new DWORD[anim_->CntFrm];
				anim_->NextX=new short[anim_->CntFrm];
				anim_->NextY=new short[anim_->CntFrm];
				memcpy(anim_->Ind,anim->Ind,anim->CntFrm*sizeof(DWORD));
				memcpy(anim_->NextX,anim->NextX,anim->CntFrm*sizeof(short));
				memcpy(anim_->NextY,anim->NextY,anim->CntFrm*sizeof(short));
				memcpy(anim_->Ind+anim->CntFrm,animex->Ind,animex->CntFrm*sizeof(DWORD));
				memcpy(anim_->NextX+anim->CntFrm,animex->NextX,animex->CntFrm*sizeof(short));
				memcpy(anim_->NextY+anim->CntFrm,animex->NextY,animex->CntFrm*sizeof(short));
				short ox=0,oy=0;
				for(DWORD i=0;i<anim->CntFrm;i++)
				{
					ox+=anim->NextX[i];
					oy+=anim->NextY[i];
				}
				anim_->NextX[anim->CntFrm]-=ox;
				anim_->NextY[anim->CntFrm]-=oy;
				return anim_;
			}

			// Clone
			if(anim)
			{
				AnyFrames* anim_=new AnyFrames();
				anim_->CntFrm=(!FLAG(flags,ANIM_FLAG_FIRST_FRAME|ANIM_FLAG_LAST_FRAME)?anim->CntFrm:1);
				anim_->Ticks=anim->Ticks;
				anim_->Ind=new DWORD[anim_->CntFrm];
				anim_->NextX=new short[anim_->CntFrm];
				anim_->NextY=new short[anim_->CntFrm];
				if(!FLAG(flags,ANIM_FLAG_FIRST_FRAME|ANIM_FLAG_LAST_FRAME))
				{
					memcpy(anim_->Ind,anim->Ind,anim->CntFrm*sizeof(DWORD));
					memcpy(anim_->NextX,anim->NextX,anim->CntFrm*sizeof(short));
					memcpy(anim_->NextY,anim->NextY,anim->CntFrm*sizeof(short));
				}
				else
				{
					anim_->Ind[0]=anim->Ind[FLAG(flags,ANIM_FLAG_FIRST_FRAME)?0:anim->CntFrm-1];
					anim_->NextX[0]=anim->NextX[FLAG(flags,ANIM_FLAG_FIRST_FRAME)?0:anim->CntFrm-1];
					anim_->NextY[0]=anim->NextY[FLAG(flags,ANIM_FLAG_FIRST_FRAME)?0:anim->CntFrm-1];

					// Append offsets
					if(FLAG(flags,ANIM_FLAG_LAST_FRAME))
					{
						for(DWORD i=0;i<anim->CntFrm-1;i++)
						{
							anim_->NextX[0]+=anim->NextX[i];
							anim_->NextY[0]+=anim->NextY[i];
						}
					}
				}
				anim=anim_;
			}
			return anim;
		}
	}
	return NULL;
}

AnyFrames* ResourceManager::LoadFalloutAnimSpr(DWORD crtype, DWORD anim1, DWORD anim2, int dir)
{
	AnimMapIt it=critterFrames.find(AnimMapId(crtype,anim1,anim2,dir,true));
	if(it!=critterFrames.end()) return (*it).second;

	// Load file
	static char frm_ind[]="_ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	char spr_name[MAX_FOPATH];
	SprMngr.SurfType=RES_CRITTERS;

	// Try load fofrm
	const char* name=CritType::GetName(crtype);
	sprintf(spr_name,"%s%c%c.fofrm",name,frm_ind[anim1],frm_ind[anim2]);
	AnyFrames* frames=SprMngr.LoadAnimation(spr_name,PT_ART_CRITTERS,ANIM_DIR(dir));

	// Try load fallout frames
	if(!frames)
	{
		sprintf(spr_name,"%s%c%c.frm",name,frm_ind[anim1],frm_ind[anim2]);
		frames=SprMngr.LoadAnimation(spr_name,PT_ART_CRITTERS,ANIM_DIR(dir));
		if(!frames)
		{
			sprintf(spr_name,"%s%c%c.fr%u",name,frm_ind[anim1],frm_ind[anim2],dir);
			frames=SprMngr.LoadAnimation(spr_name,PT_ART_CRITTERS,ANIM_DIR(0));
		}
	}
	SprMngr.SurfType=RES_NONE;

	critterFrames.insert(AnimMapVal(AnimMapId(crtype,anim1,anim2,dir,true),frames));
	if(!frames) return NULL;

//////////////////////////////////////////////////////////////////////////
#define LOADSPR_ADDOFFS(a1,a2) \
	do{\
		AnyFrames* stay_frm=LoadFalloutAnimSpr(crtype,a1,a2,dir);\
		if(!stay_frm) break;\
		AnyFrames* frm=frames;\
		SpriteInfo* stay_si=SprMngr.GetSpriteInfo(stay_frm->Ind[0]);\
		if(!stay_si) break;\
		for(int i=0;i<frm->CntFrm;i++)\
		{\
			SpriteInfo* si=SprMngr.GetSpriteInfo(frm->Ind[i]);\
			if(!si) continue;\
			si->OffsX+=stay_si->OffsX;\
			si->OffsY+=stay_si->OffsY;\
		}\
	}while(0)
#define LOADSPR_ADDOFFS_NEXT(a1,a2) \
	do{\
		AnyFrames* stay_frm=LoadFalloutAnimSpr(crtype,a1,a2,dir);\
		if(!stay_frm) break;\
		AnyFrames* frm=frames;\
		SpriteInfo* stay_si=SprMngr.GetSpriteInfo(stay_frm->Ind[0]);\
		if(!stay_si) break;\
		short ox=0;\
		short oy=0;\
		for(int i=0;i<stay_frm->CntFrm;i++)\
		{\
			ox+=stay_frm->NextX[i];\
			oy+=stay_frm->NextY[i];\
		}\
		for(int i=0;i<frm->CntFrm;i++)\
		{\
			SpriteInfo* si=SprMngr.GetSpriteInfo(frm->Ind[i]);\
			if(!si) continue;\
			si->OffsX+=ox;\
			si->OffsY+=oy;\
		}\
	}while(0)
//////////////////////////////////////////////////////////////////////////
	// Fallout animations
#define ANIM1_FALLOUT_UNARMED            (1)
#define ANIM1_FALLOUT_DEAD               (2)
#define ANIM1_FALLOUT_KNOCKOUT           (3)
#define ANIM1_FALLOUT_KNIFE              (4)
#define ANIM1_FALLOUT_CLUB               (5)
#define ANIM1_FALLOUT_HAMMER             (6)
#define ANIM1_FALLOUT_SPEAR              (7)
#define ANIM1_FALLOUT_PISTOL             (8)
#define ANIM1_FALLOUT_UZI                (9)
#define ANIM1_FALLOUT_SHOOTGUN           (10)
#define ANIM1_FALLOUT_RIFLE              (11)
#define ANIM1_FALLOUT_MINIGUN            (12)
#define ANIM1_FALLOUT_ROCKET_LAUNCHER    (13)
#define ANIM1_FALLOUT_AIM                (14)
#define ANIM2_FALLOUT_STAY               (1)
#define ANIM2_FALLOUT_WALK               (2)
#define ANIM2_FALLOUT_SHOW               (3)
#define ANIM2_FALLOUT_HIDE               (4)
#define ANIM2_FALLOUT_DODGE_WEAPON       (5)
#define ANIM2_FALLOUT_THRUST             (6)
#define ANIM2_FALLOUT_SWING              (7)
#define ANIM2_FALLOUT_PREPARE_WEAPON     (8)
#define ANIM2_FALLOUT_TURNOFF_WEAPON     (9)
#define ANIM2_FALLOUT_SHOOT              (10)
#define ANIM2_FALLOUT_BURST              (11)
#define ANIM2_FALLOUT_FLAME              (12)
#define ANIM2_FALLOUT_THROW_WEAPON       (13)
#define ANIM2_FALLOUT_DAMAGE_FRONT       (15)
#define ANIM2_FALLOUT_DAMAGE_BACK        (16)
#define ANIM2_FALLOUT_KNOCK_FRONT        (1)  // Only with ANIM1_FALLOUT_DEAD
#define ANIM2_FALLOUT_KNOCK_BACK         (2)
#define ANIM2_FALLOUT_STANDUP_BACK       (8)  // Only with ANIM1_FALLOUT_KNOCKOUT
#define ANIM2_FALLOUT_STANDUP_FRONT      (10)
#define ANIM2_FALLOUT_PICKUP             (11) // Only with ANIM1_FALLOUT_UNARMED
#define ANIM2_FALLOUT_USE                (12)
#define ANIM2_FALLOUT_DODGE_EMPTY        (14)
#define ANIM2_FALLOUT_PUNCH              (17)
#define ANIM2_FALLOUT_KICK               (18)
#define ANIM2_FALLOUT_THROW_EMPTY        (19)
#define ANIM2_FALLOUT_RUN                (20)
#define ANIM2_FALLOUT_DEAD_FRONT         (1)  // Only with ANIM1_FALLOUT_DEAD
#define ANIM2_FALLOUT_DEAD_BACK          (2)
#define ANIM2_FALLOUT_DEAD_BLOODY_SINGLE (4)
#define ANIM2_FALLOUT_DEAD_BURN          (5)
#define ANIM2_FALLOUT_DEAD_BLOODY_BURST  (6)
#define ANIM2_FALLOUT_DEAD_BURST         (7)
#define ANIM2_FALLOUT_DEAD_PULSE         (8)
#define ANIM2_FALLOUT_DEAD_LASER         (9)
#define ANIM2_FALLOUT_DEAD_BURN2         (10)
#define ANIM2_FALLOUT_DEAD_PULSE_DUST    (11)
#define ANIM2_FALLOUT_DEAD_EXPLODE       (12)
#define ANIM2_FALLOUT_DEAD_FUSED         (13)
#define ANIM2_FALLOUT_DEAD_BURN_RUN      (14)
#define ANIM2_FALLOUT_DEAD_FRONT2        (15)
#define ANIM2_FALLOUT_DEAD_BACK2         (16)
//////////////////////////////////////////////////////////////////////////

	if(anim1==ANIM1_FALLOUT_AIM) return frames; // Aim, 'N'

	// Empty offsets
	if(anim1==ANIM1_FALLOUT_UNARMED)
	{
		if(anim2==ANIM2_FALLOUT_STAY || anim2==ANIM2_FALLOUT_WALK || anim2==ANIM2_FALLOUT_RUN) return frames;
		LOADSPR_ADDOFFS(ANIM1_FALLOUT_UNARMED,ANIM2_FALLOUT_STAY);
	}

	// Weapon offsets
	if(anim1>=ANIM1_FALLOUT_KNIFE && anim1<=ANIM1_FALLOUT_ROCKET_LAUNCHER)
	{
		if(anim2==ANIM2_FALLOUT_SHOW) LOADSPR_ADDOFFS(ANIM1_FALLOUT_UNARMED,ANIM2_FALLOUT_STAY);
		else if(anim2==ANIM2_FALLOUT_WALK) return frames;
		else if(anim2==ANIM2_FALLOUT_STAY)
		{
			LOADSPR_ADDOFFS(ANIM1_FALLOUT_UNARMED,ANIM2_FALLOUT_STAY);
			LOADSPR_ADDOFFS_NEXT(anim1,ANIM2_FALLOUT_SHOW);
		}
		else if(anim2==ANIM2_FALLOUT_SHOOT || anim2==ANIM2_FALLOUT_BURST || anim2==ANIM2_FALLOUT_FLAME)
		{
			LOADSPR_ADDOFFS(anim1,ANIM2_FALLOUT_PREPARE_WEAPON);
			LOADSPR_ADDOFFS_NEXT(anim1,ANIM2_FALLOUT_PREPARE_WEAPON);
		}
		else if(anim2==ANIM2_FALLOUT_TURNOFF_WEAPON)
		{
			if(anim1==ANIM1_FALLOUT_MINIGUN)
			{
				LOADSPR_ADDOFFS(anim1,ANIM2_FALLOUT_BURST);
				LOADSPR_ADDOFFS_NEXT(anim1,ANIM2_FALLOUT_BURST);
			}
			else
			{
				LOADSPR_ADDOFFS(anim1,ANIM2_FALLOUT_SHOOT);
				LOADSPR_ADDOFFS_NEXT(anim1,ANIM2_FALLOUT_SHOOT);
			}
		}
		else LOADSPR_ADDOFFS(anim1,ANIM2_FALLOUT_STAY);
	}

	// Dead & Ko offsets
	if(anim1==ANIM1_FALLOUT_DEAD)
	{
		if(anim2==ANIM2_FALLOUT_DEAD_FRONT2)
		{
			LOADSPR_ADDOFFS(ANIM1_FALLOUT_DEAD,ANIM2_FALLOUT_DEAD_FRONT);
			LOADSPR_ADDOFFS_NEXT(ANIM1_FALLOUT_DEAD,ANIM2_FALLOUT_DEAD_FRONT);
		}
		else if(anim2==ANIM2_FALLOUT_DEAD_BACK2)
		{
			LOADSPR_ADDOFFS(ANIM1_FALLOUT_DEAD,ANIM2_FALLOUT_DEAD_BACK);
			LOADSPR_ADDOFFS_NEXT(ANIM1_FALLOUT_DEAD,ANIM2_FALLOUT_DEAD_BACK);
		}
		else
		{
			LOADSPR_ADDOFFS(ANIM1_FALLOUT_UNARMED,ANIM2_FALLOUT_STAY);
		}
	}

	// Ko rise offsets
	if(anim1==ANIM1_FALLOUT_KNOCKOUT)
	{
		BYTE anim2_=ANIM2_FALLOUT_KNOCK_FRONT;
		if(anim2==ANIM2_FALLOUT_STANDUP_BACK) anim2_=ANIM2_FALLOUT_KNOCK_BACK;
		LOADSPR_ADDOFFS(ANIM1_FALLOUT_DEAD,anim2_);
		LOADSPR_ADDOFFS_NEXT(ANIM1_FALLOUT_DEAD,anim2_);
	}

	return frames;
}

Animation3d* ResourceManager::GetCrit3dAnim(DWORD crtype, DWORD anim1, DWORD anim2, int dir)
{
	if(CritType::GetAnimType(crtype)!=ANIM_TYPE_3D) return NULL;

	if(!CritType::IsCanRotate(crtype)) dir=0;

	if(crtype<critter3d.size() && critter3d[crtype])
	{
		critter3d[crtype]->SetAnimation(anim1,anim2,NULL,ANIMATION_STAY|ANIMATION_NO_SMOOTH);
		critter3d[crtype]->SetDir(dir);
		return critter3d[crtype];
	}

	char name[MAX_FOPATH];
	sprintf(name,"%s.fo3d",CritType::GetName(crtype));
	Animation3d* anim3d=SprMngr.LoadPure3dAnimation(name,PT_ART_CRITTERS);
	if(!anim3d) return NULL;

	if(crtype>=critter3d.size()) critter3d.resize(crtype+1);
	critter3d[crtype]=anim3d;

	anim3d->SetAnimation(anim1,anim2,NULL,ANIMATION_STAY|ANIMATION_NO_SMOOTH);
	anim3d->SetDir(dir);
	return anim3d;
}

DWORD ResourceManager::GetCritSprId(DWORD crtype, DWORD anim1, DWORD anim2, int dir)
{
	DWORD spr_id=0;
	if(CritType::GetAnimType(crtype)!=ANIM_TYPE_3D)
	{
		AnyFrames* anim=GetCrit2dAnim(crtype,anim1,anim2,dir);
		spr_id=(anim?anim->GetSprId(0):0);
	}
	else
	{
		Animation3d* anim=GetCrit3dAnim(crtype,anim1,anim2,dir);
		spr_id=(anim?anim->GetSprId():0);
	}
	return spr_id;
}

AnyFrames* ResourceManager::GetRandomSplash()
{
	if(splashNames.empty()) return 0;
	int rnd=Random(0,splashNames.size()-1);
	static AnyFrames* splash=NULL;
	SprMngr.SurfType=RES_SPLASH;
	splash=SprMngr.ReloadAnimation(splash,splashNames[rnd].c_str(),PT_DATA);
	SprMngr.SurfType=RES_NONE;
	return splash;
}