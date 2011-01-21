#include "StdAfx.h"
#include "ResourceManager.h"
#include "Common.h"
#include "FileManager.h"
#include "DataFile.h"
#include "CritterType.h"

ResourceManager ResMngr;

void ResourceManager::AddNamesHash(StrVec& names)
{
	for(StrVecIt it=names.begin(),end=names.end();it!=end;++it)
	{
		const string& fname=*it;
		DWORD hash=Str::GetHash(fname.c_str());

		DwordStrMapIt it_=namesHash.find(hash);
		if(it_==namesHash.end())
			namesHash.insert(DwordStrMapVal(hash,fname));
		else if(_stricmp(fname.c_str(),(*it_).second.c_str()))
			WriteLog(__FUNCTION__" - Found equal hash for different names, name1<%s>, name2<%s>, hash<%u>.\n",fname.c_str(),(*it_).second.c_str(),hash);
	}
}

const char* ResourceManager::GetName(DWORD name_hash)
{
	if(!name_hash) return NULL;
	DwordStrMapIt it=namesHash.find(name_hash);
	return it!=namesHash.end()?(*it).second.c_str():NULL;
}

void ResourceManager::Refresh()
{
	// Folders, unpacked data
	static bool folders_done=false;
	if(!folders_done)
	{
		// All names
		StrVec file_names;
		FileManager::GetFolderFileNames(PT_DATA,NULL,file_names);
		AddNamesHash(file_names);

		// Splashes
		StrVec splashes;
		FileManager::GetFolderFileNames(PT_ART_SPLASH,"rix",splashes);
		FileManager::GetFolderFileNames(PT_ART_SPLASH,"png",splashes);
		FileManager::GetFolderFileNames(PT_ART_SPLASH,"jpg",splashes);
		for(StrVecIt it=splashes.begin(),end=splashes.end();it!=end;++it)
			if(std::find(splashNames.begin(),splashNames.end(),*it)==splashNames.end())
				splashNames.push_back(*it);

		// Sound names
		StrVec sounds;
		FileManager::GetFolderFileNames(PT_SND_SFX,"wav",sounds);
		FileManager::GetFolderFileNames(PT_SND_SFX,"acm",sounds);
		FileManager::GetFolderFileNames(PT_SND_SFX,"ogg",sounds);
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
			pfile->GetFileNames(FileManager::GetPath(PT_DATA),NULL,file_names);
			AddNamesHash(file_names);

			// Splashes
			StrVec splashes;
			pfile->GetFileNames(FileManager::GetPath(PT_ART_SPLASH),"rix",splashes);
			pfile->GetFileNames(FileManager::GetPath(PT_ART_SPLASH),"png",splashes);
			pfile->GetFileNames(FileManager::GetPath(PT_ART_SPLASH),"jpg",splashes);
			for(StrVecIt it=splashes.begin(),end=splashes.end();it!=end;++it)
				if(std::find(splashNames.begin(),splashNames.end(),*it)==splashNames.end())
					splashNames.push_back(*it);

			// Sound names
			StrVec sounds;
			pfile->GetFileNames(FileManager::GetPath(PT_SND_SFX),"wav",sounds);
			pfile->GetFileNames(FileManager::GetPath(PT_SND_SFX),"acm",sounds);
			pfile->GetFileNames(FileManager::GetPath(PT_SND_SFX),"ogg",sounds);
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
	const char* fname=GetName(name_hash);
	if(!fname) return NULL;

	SprMngr.SurfType=res_type;
	AnyFrames* anim=SprMngr.LoadAnimation(fname,PT_DATA,ANIM_DIR(dir)|ANIM_FRM_ANIM_PIX);
	SprMngr.SurfType=RES_NONE;

	loadedAnims.insert(LoadedAnimMapVal(id,LoadedAnim(res_type,anim)));
	return anim;
}

AnyFrames* ResourceManager::GetCrit2dAnim(DWORD crtype, DWORD anim1, DWORD anim2, int dir)
{
	if(CritType::IsAnim3d(crtype)) return NULL;

	dir=CLAMP(dir,0,5);
	if(!CritType::IsCanRotate(crtype)) dir=0;
	DWORD id=(crtype<<19)|(anim1<<11)|(anim2<<3)|(dir&7);

	AnimMapIt it=critterFrames.find(id);
	if(it!=critterFrames.end()) return (*it).second;

	static char frm_ind[]="_ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	char spr_name[64];
	SprMngr.SurfType=RES_CRITTERS;

	// Try load fofrm
	const char* name=CritType::GetName(crtype);
	sprintf(spr_name,"%s%c%c.fofrm",name,frm_ind[anim1],frm_ind[anim2]);
	AnyFrames* cr_frm=SprMngr.LoadAnimation(spr_name,PT_ART_CRITTERS,ANIM_DIR(dir));

	// Try load fallout frames
	if(!cr_frm)
	{
		sprintf(spr_name,"%s%c%c.frm",name,frm_ind[anim1],frm_ind[anim2]);
		cr_frm=SprMngr.LoadAnimation(spr_name,PT_ART_CRITTERS,ANIM_DIR(dir));
		if(!cr_frm)
		{
			sprintf(spr_name,"%s%c%c.fr%u",name,frm_ind[anim1],frm_ind[anim2],dir);
			cr_frm=SprMngr.LoadAnimation(spr_name,PT_ART_CRITTERS,ANIM_DIR(0));
			if(!cr_frm)
			{
				SprMngr.SurfType=RES_NONE;
				return NULL;
			}
		}
	}

	SprMngr.SurfType=RES_NONE;
	critterFrames.insert(AnimMapVal(id,cr_frm));
	return cr_frm;
}

Animation3d* ResourceManager::GetCrit3dAnim(DWORD crtype, DWORD anim1, DWORD anim2, int dir)
{
	if(!CritType::IsAnim3d(crtype)) return NULL;

	if(!CritType::IsCanRotate(crtype)) dir=0;

	if(crtype<critter3d.size() && critter3d[crtype])
	{
		critter3d[crtype]->SetAnimation(anim1,anim2,NULL,ANIMATION_STAY|ANIMATION_NO_SMOOTH);
		critter3d[crtype]->SetDir(dir);
		return critter3d[crtype];
	}

	char name[64];
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
	if(!CritType::IsAnim3d(crtype))
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