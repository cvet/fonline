#include "StdAfx.h"
#include "ResourceManager.h"
#include "Common.h"
#include "FileManager.h"
#include "CritterType.h"
#include "Crypt.h"

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

void ResourceManager::Refresh(SpriteManager* spr_mngr)
{
	if(spr_mngr) sprMngr=spr_mngr;

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
	TDatFilePtrVec& dats=FileManager::GetDatFiles();
	for(TDatFilePtrVecIt it=dats.begin(),end=dats.end();it!=end;++it)
	{
		TDatFile* dat=*it;
		if(std::find(processedDats.begin(),processedDats.end(),dat)==processedDats.end())
		{
			// All names
			StrVec file_names;
			dat->GetFileNames(FileManager::GetPath(PT_DATA),NULL,file_names);
			AddNamesHash(file_names);

			// Splashes
			StrVec splashes;
			dat->GetFileNames(FileManager::GetPath(PT_ART_SPLASH),"rix",splashes);
			dat->GetFileNames(FileManager::GetPath(PT_ART_SPLASH),"png",splashes);
			dat->GetFileNames(FileManager::GetPath(PT_ART_SPLASH),"jpg",splashes);
			for(StrVecIt it=splashes.begin(),end=splashes.end();it!=end;++it)
				if(std::find(splashNames.begin(),splashNames.end(),*it)==splashNames.end())
					splashNames.push_back(*it);

			// Sound names
			StrVec sounds;
			dat->GetFileNames(FileManager::GetPath(PT_SND_SFX),"wav",sounds);
			dat->GetFileNames(FileManager::GetPath(PT_SND_SFX),"acm",sounds);
			dat->GetFileNames(FileManager::GetPath(PT_SND_SFX),"ogg",sounds);
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

			processedDats.push_back(dat);
		}
	}
}

void ResourceManager::Finish()
{
	WriteLog("Resource manager finish...\n");
	loadedAnims.clear();
	sprMngr=NULL;
	WriteLog("Resource manager finish complete.\n");
}

void ResourceManager::FreeResources(int type)
{
	if(type==RES_IFACE)
	{
		sprMngr->FreeSurfaces(RES_IFACE);
		for(LoadedAnimMapIt it=loadedAnims.begin();it!=loadedAnims.end();)
		{
			if((*it).second.ResType==RES_IFACE) it=loadedAnims.erase(it);
			else ++it;
		}
	}
	else if(type==RES_IFACE_EXT)
	{
		sprMngr->FreeSurfaces(RES_IFACE_EXT);
		for(LoadedAnimMapIt it=loadedAnims.begin();it!=loadedAnims.end();)
		{
			if((*it).second.ResType==RES_IFACE_EXT) it=loadedAnims.erase(it);
			else ++it;
		}
	}
	else if(type==RES_CRITTERS)
	{
		critterFrames.clear();
		sprMngr->FreeSurfaces(RES_CRITTERS);
		sprMngr->ClearSpriteContours();
	}
	else if(type==RES_ITEMS)
	{
		sprMngr->FreeSurfaces(RES_ITEMS);
		for(LoadedAnimMapIt it=loadedAnims.begin();it!=loadedAnims.end();)
		{
			if((*it).second.ResType==RES_ITEMS) it=loadedAnims.erase(it);
			else ++it;
		}
	}
	else if(type==RES_SCRIPT)
	{
		sprMngr->FreeSurfaces(RES_SCRIPT);
		for(LoadedAnimMapIt it=loadedAnims.begin();it!=loadedAnims.end();)
		{
			if((*it).second.ResType==RES_SCRIPT) it=loadedAnims.erase(it);
			else ++it;
		}
	}
}

DWORD ResourceManager::GetSprId(DWORD name_hash, int dir)
{
	if(!name_hash) return 0;
	AnyFrames* anim=GetAnim(name_hash,dir);
	if(!anim) return 0;
	return anim->GetSprId(0);
}

SpriteInfo* ResourceManager::GetSprInfo(DWORD name_hash, int dir)
{
	DWORD id=GetSprId(name_hash,dir);
	return id?sprMngr->GetSpriteInfo(id):NULL;
}

AnyFrames* ResourceManager::GetAnim(DWORD name_hash, int dir)
{
	int res_type=(sprMngr->SurfType==RES_NONE?RES_ITEMS:sprMngr->SurfType);
	sprMngr->SurfType=RES_NONE;
	DWORD id=name_hash+dir;
	LoadedAnimMapIt it=loadedAnims.find(id);
	if(it!=loadedAnims.end()) return (*it).second.Anim;

	// Load new animation
	const char* fname=GetName(name_hash);
	if(!fname) return NULL;
	sprMngr->SurfType=res_type;
	AnyFrames* anim=sprMngr->LoadAnyAnimation(fname,PT_DATA,true,dir);
	sprMngr->SurfType=RES_NONE;
	if(!anim) return NULL;
	loadedAnims.insert(LoadedAnimMapVal(id,LoadedAnim(res_type,anim)));
	return anim;
}

DWORD ResourceManager::GetIfaceSprId(DWORD name_hash){sprMngr->SurfType=RES_IFACE; return GetSprId(name_hash,0);}
DWORD ResourceManager::GetInvSprId(DWORD name_hash){sprMngr->SurfType=RES_IFACE_EXT; return GetSprId(name_hash,0);}
DWORD ResourceManager::GetSkDxSprId(DWORD name_hash){sprMngr->SurfType=RES_IFACE_EXT; return GetSprId(name_hash,0);}
SpriteInfo* ResourceManager::GetIfaceSprInfo(DWORD name_hash){sprMngr->SurfType=RES_IFACE; return GetSprInfo(name_hash,0);}
SpriteInfo* ResourceManager::GetInvSprInfo(DWORD name_hash){sprMngr->SurfType=RES_IFACE_EXT; return GetSprInfo(name_hash,0);}
SpriteInfo* ResourceManager::GetSkDxSprInfo(DWORD name_hash){sprMngr->SurfType=RES_IFACE_EXT; return GetSprInfo(name_hash,0);}
AnyFrames* ResourceManager::GetIfaceAnim(DWORD name_hash){sprMngr->SurfType=RES_IFACE; return GetAnim(name_hash,0);}
AnyFrames* ResourceManager::GetInvAnim(DWORD name_hash){sprMngr->SurfType=RES_IFACE_EXT; return GetAnim(name_hash,0);}

AnyFrames* ResourceManager::GetCritAnim(DWORD crtype, BYTE anim1, BYTE anim2, BYTE dir)
{
	DWORD id=(int(crtype)<<19)|(int(anim1)<<11)|(int(anim2)<<3)|(dir&7);

	AnimMapIt it=critterFrames.find(id);
	if(it!=critterFrames.end()) return (*it).second;

	static char frm_ind[]="_ABCDEFGHIJKLMNOPQRST";
	char spr_name[64];
	sprMngr->SurfType=RES_CRITTERS;

	// Try load fofrm
	const char* name=CritType::GetName(crtype);
	sprintf(spr_name,"%s%c%c.fofrm",name,frm_ind[anim1],frm_ind[anim2]);
	AnyFrames* cr_frm=sprMngr->LoadAnyAnimation(spr_name,PT_ART_CRITTERS,false,dir);

	// Try load fallout frames
	if(!cr_frm)
	{
		sprintf(spr_name,"%s%c%c.frm",name,frm_ind[anim1],frm_ind[anim2]);
		cr_frm=sprMngr->LoadAnyAnimation(spr_name,PT_ART_CRITTERS,false,dir);
		if(!cr_frm)
		{
			sprintf(spr_name,"%s%c%c.fr%u",name,frm_ind[anim1],frm_ind[anim2],dir);
			cr_frm=sprMngr->LoadAnyAnimation(spr_name,PT_ART_CRITTERS,false,0);
			if(!cr_frm)
			{
				sprMngr->SurfType=RES_NONE;
				return NULL;
			}
		}
	}

	sprMngr->SurfType=RES_NONE;
	critterFrames.insert(AnimMapVal(id,cr_frm));
	return cr_frm;
}

DWORD ResourceManager::GetAvatarSprId(const char* fname)
{
	static DWORD avatar_id=0;
	static string avatar_name;
	if(avatar_name==fname) return avatar_id;
	avatar_name=fname;
	sprMngr->SurfType=RES_AVATAR;
	avatar_id=sprMngr->ReloadSprite(avatar_id,fname,PT_ART_INTRFACE);
	sprMngr->SurfType=RES_NONE;
	return avatar_id;
}

DWORD ResourceManager::GetRandomSplash()
{
	if(splashNames.empty()) return 0;
	int rnd=Random(0,splashNames.size()-1);
	static DWORD splash=0;
	sprMngr->SurfType=RES_SPLASH;
	splash=sprMngr->ReloadSprite(splash,splashNames[rnd].c_str(),PT_DATA);
	sprMngr->SurfType=RES_NONE;
	return splash;
}