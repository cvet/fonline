#include "StdAfx.h"
#include "SoundManager.h"
#include "Text.h"

SoundManager SndMngr;

/************************************************************************/
/* LIBS                                                                 */
/************************************************************************/

//for ACM
#include "Acm/acmstrm.h"

//for OGG
#include "Ogg/codec.h"
#include "Ogg/vorbisfile.h"
#pragma comment (lib, "ogg_static.lib")
#pragma comment (lib, "vorbisfile_static.lib")
#pragma comment (lib, "vorbisenc_static.lib")
#pragma comment (lib, "vorbis_static.lib")

// Auto ptr
#include <common.h>

/************************************************************************/
/* Volume correction                                                    */
/************************************************************************/
// From gd.net http://www.gamedev.net/community/forums/viewreply.asp?ID=1975621
// The first function converts the default DirectSound range (0 - -10000) to a floating
// point value between 0.0f and 1.0f. The second converts back. The third converts the
// converted value to decibels. Enjoy.
// Modify for long arg and result.

long VolumeToDb(long vol_level)
{
	double level=double(vol_level)/100.0;
	if(level<=0.0) return DSBVOLUME_MIN;
	else if(level>=1.0) return DSBVOLUME_MAX;
	return (long)(-2000.0*log10(1.0/level));
}

DWORD DbToVolume(long vol_db)
{
	if(vol_db<=-9600) return 0;
	else if(vol_db>=0) return 100;
	return (DWORD)(pow(10,double(vol_db+2000)/2000.0)*10.0);
}

/*int VolumeToDecibels(float vol) 
{
	if (vol>=1.0F) 
		return 0;
	if (vol<=0.0F) 
		return DSBVOLUME_MIN;
	static const float adj=3321.928094887;  // 1000/log10(2)
	return int(float(log10(vol)) * adj);
}*/

/************************************************************************/
/* Initialization / Deinitialization / Process                          */
/************************************************************************/

bool SoundManager::Init(HWND wnd)
{
	if(isActive) return true;

	WriteLog("Sound manager initialization...\n");
	if(DirectSoundCreate8(0,&soundDevice,0)!=DS_OK)
	{
		WriteLog("Create DirectSound fail!\n");
		return false;
	}

	if(soundDevice->SetCooperativeLevel(wnd,DSSCL_NORMAL)!=DS_OK)
	{
		WriteLog("SetCooperativeLevel fail.\n");
		return false;
	}

	isActive=true;
	WriteLog("Sound manager initialization complete.\n");
	return true;
}

void SoundManager::Clear()
{
	WriteLog("Sound manager finish.\n");
	ClearSounds();
	SAFEREL(soundDevice);
	soundInfo.clear();
	isActive=false;
	WriteLog("Sound manager finish complete.\n");
}

void SoundManager::ClearSounds()
{
	for(SoundVecIt it=soundsActive.begin(),end=soundsActive.end();it!=end;++it) delete *it;
	soundsActive.clear();
}

void SoundManager::Process()
{
	if(!isActive) return;

	DWORD tick=Timer::FastTick();
	static DWORD call_tick=0;
	if(tick<call_tick) return;
	call_tick=tick+MUSIC_PROCESS_TIME;

	for(SoundVecIt it=soundsActive.begin();it!=soundsActive.end();)
	{
		Sound* sound=*it;
		bool erase=false;
		DWORD status;
		if(sound->SndBuf->GetStatus(&status)!=DS_OK || status&DSBSTATUS_BUFFERLOST) // Restore buffer
		{
			erase=true;
		}
		else if(!(status&DSBSTATUS_PLAYING)) // End of play
		{
			if(sound->NextPlay)
			{
				if(tick>=sound->NextPlay)
				{
					if(sound->IsNeedStreaming)
					{
						sound->Decoded=0;
						sound->BufOffs=0;
						if(sound->MediaType==SOUND_FILE_OGG && (ov_raw_seek(&sound->OggDescriptor,0) || !Streaming(sound))) erase=true;
					}
					if(!erase) Play(sound,sound->IsMusic?musicVolDb:soundVolDb,0);
					sound->NextPlay=0;
				}
			}
			else if(sound->RepeatTime) sound->NextPlay=tick+sound->RepeatTime;
			else erase=true;
		}
		else if(sound->IsNeedStreaming) // Streaming
		{
			DWORD play_cur;
			if(sound->SndBuf->GetCurrentPosition(&play_cur,NULL)==DS_OK)
			{
				DWORD free_buf=0;
				if(sound->BufOffs>=play_cur) free_buf=sound->BufSize-sound->BufOffs+play_cur;
				else free_buf=play_cur-sound->BufOffs;
				if(free_buf>=sound->BufSize*75/100 && !Streaming(sound))
				{
					DWORD status_;
					if(sound->SndBuf->GetStatus(&status_)!=DS_OK || status_&DSBSTATUS_PLAYING) erase=true;
				}
			}
			else erase=true;
		}

		if(erase)
		{
			delete sound;
			it=soundsActive.erase(it);
		}
		else ++it;
	}
}

DWORD SoundManager::GetSoundVolume()
{
	return DbToVolume(soundVolDb);
}

DWORD SoundManager::GetMusicVolume()
{
	return DbToVolume(musicVolDb);
}

DWORD SoundManager::GetSoundVolumeDb()
{
	return soundVolDb;
}

DWORD SoundManager::GetMusicVolumeDb()
{
	return musicVolDb;
}

void SoundManager::SetSoundVolume(int vol_proc)
{
	soundVolDb=VolumeToDb(vol_proc);

	for(SoundVecIt it=soundsActive.begin(),end=soundsActive.end();it!=end;++it)
	{
		Sound* sound=*it;
		if(!sound->IsMusic) sound->SndBuf->SetVolume(soundVolDb);
	}
}

void SoundManager::SetMusicVolume(int vol_proc)
{
	musicVolDb=VolumeToDb(vol_proc);

	for(SoundVecIt it=soundsActive.begin(),end=soundsActive.end();it!=end;++it)
	{
		Sound* sound=*it;
		if(sound->IsMusic) sound->SndBuf->SetVolume(musicVolDb);
	}
}

/************************************************************************/
/* Loader                                                               */
/************************************************************************/

void SoundManager::Play(Sound* sound, long vol_db, DWORD flags)
{
	if(!sound) return;
	if(sound->IsNeedStreaming) flags|=DSBPLAY_LOOPING;
	sound->SndBuf->Stop();
	sound->SndBuf->SetCurrentPosition(0);
	sound->SndBuf->SetVolume(vol_db);
	sound->SndBuf->Play(0,0,flags);
}

Sound* SoundManager::Load(const char* fname, int path_type)
{
	char full_name[MAX_FOPATH];
	StringCopy(full_name,fname);

	const char* ext=FileManager::GetExtension(fname);
	if(!ext)
	{
		// Default ext
		ext=full_name+strlen(full_name);
		StringAppend(full_name,SOUND_DEFAULT_EXT);
	}
	else
	{
		--ext;
	}

	Sound* sound=new Sound();
	if(!sound)
	{
		WriteLog(__FUNCTION__" - Allocation error.\n");
		return NULL;
	}

	sound->FileName=full_name;
	sound->PathType=path_type;

	WAVEFORMATEX fmt;
	ZeroMemory(&fmt,sizeof(WAVEFORMATEX));
	unsigned char* sample_data=NULL;

	if(!((!_stricmp(ext,".wav") && LoadWAV(sound,fmt,sample_data))
	|| (!_stricmp(ext,".acm") && LoadACM(sound,fmt,sample_data,path_type==PT_SND_MUSIC?false:true))
	|| (!_stricmp(ext,".ogg") && LoadOGG(sound,fmt,sample_data))))
	{
		WriteLog(__FUNCTION__" - Unable to load sound<%s>.\n",fname);
		delete sound;
		return NULL;
	}

	DSBUFFERDESC dsbd;
	ZeroMemory(&dsbd,sizeof(DSBUFFERDESC));
	dsbd.dwBufferBytes=sound->BufSize;
	dsbd.dwFlags=(DSBCAPS_CTRLVOLUME|DSBCAPS_GETCURRENTPOSITION2|(OptGlobalSound?DSBCAPS_GLOBALFOCUS:0)|(sound->IsNeedStreaming?0:DSBCAPS_STATIC));
	dsbd.dwSize=sizeof(DSBUFFERDESC);
	dsbd.lpwfxFormat=&fmt;
	dsbd.dwReserved=0;

	if(soundDevice->CreateSoundBuffer(&dsbd,&sound->SndBuf,NULL)!=DS_OK)
	{
		WriteLog(__FUNCTION__" - CreateSoundBuffer error.\n");
		delete sound;
		delete[] sample_data;
		return NULL;
	}

	void* dst=NULL;
	DWORD size=0;
	if(sound->SndBuf->Lock(0,sound->Decoded,&dst,&size,NULL,NULL,sound->IsNeedStreaming?0:DSBLOCK_ENTIREBUFFER)!=DS_OK)
	{
		WriteLog(__FUNCTION__" - Lock error.\n");
		delete sound;
		delete[] sample_data;
		return NULL;
	}

	memcpy(dst,sample_data,size);
	sound->SndBuf->Unlock(dst,size,0,0);
	delete[] sample_data;
	soundsActive.push_back(sound);
	return sound;
}

bool SoundManager::LoadWAV(Sound* sound, WAVEFORMATEX& fformat, BYTE*& sample_data)
{
	if(!fileMngr.LoadFile(sound->FileName.c_str(),sound->PathType))
	{
		WriteLog(__FUNCTION__" - File<%s> not found.\n",sound->FileName.c_str());	
		return NULL;
	}

	DWORD dw_buf=fileMngr.GetLEDWord();
	if(dw_buf!=MAKEFOURCC('R','I','F','F'))
	{
		WriteLog(__FUNCTION__" - <RIFF> not found.\n");
		return false;
	}

	fileMngr.GoForward(4);

	dw_buf=fileMngr.GetLEDWord();
	if(dw_buf!=MAKEFOURCC('W','A','V','E'))
	{
		WriteLog(__FUNCTION__" - <WAVE> not found.\n");
		return false;
	}

	dw_buf=fileMngr.GetLEDWord();
	if(dw_buf!=MAKEFOURCC('f','m','t',' '))
	{
		WriteLog(__FUNCTION__" - <fmt > not found.\n");
		return false;
	}

	dw_buf=fileMngr.GetLEDWord();
	if(!dw_buf)
	{
		WriteLog(__FUNCTION__" - Unknown format.\n");
		return false;
	}

	fileMngr.CopyMem(&fformat,16);
	fformat.cbSize=0;

	if(fformat.wFormatTag!=1)
	{
		WriteLog(__FUNCTION__" - Compressed files not supported.\n");
		return false;
	}

	fileMngr.GoForward(dw_buf-16);

	dw_buf=fileMngr.GetLEDWord();
	if(dw_buf==MAKEFOURCC('f','a','c','t'))
	{
		dw_buf=fileMngr.GetLEDWord();
		fileMngr.GoForward(dw_buf);
		dw_buf=fileMngr.GetLEDWord();
	}

	if(dw_buf!=MAKEFOURCC('d','a','t','a'))
	{
		WriteLog(__FUNCTION__" - Unknown format2.\n");
		return false;
	}

	dw_buf=fileMngr.GetLEDWord();
	sound->BufSize=dw_buf;
	sample_data=new unsigned char[dw_buf];

	if(!fileMngr.CopyMem(sample_data,dw_buf))
	{
		WriteLog(__FUNCTION__" - File truncated.\n");
		delete[] sample_data;
		return false;
	}

	return true;
}

bool SoundManager::LoadACM(Sound* sound, WAVEFORMATEX& fformat, BYTE*& sample_data, bool mono)
{
	if(!fileMngr.LoadFile(sound->FileName.c_str(),sound->PathType))
	{
		WriteLog(__FUNCTION__" - File<%s> not found.\n",sound->FileName.c_str());	
		return NULL;
	}

	int channels=0;
	int freq=0;
	int samples=0;

	AutoPtr<CACMUnpacker> acm(new CACMUnpacker(fileMngr.GetBuf(),(int)fileMngr.GetFsize(),channels,freq,samples));
	samples*=2;

	if(!acm.IsValid())
	{
		WriteLog(__FUNCTION__" - ACMUnpacker init fail.\n");
		return false;
	}

	// Mono
	// nChannels			2
	// nSamplesPerSec	22050
	// nAvgBytesPerSec	22050*4
	// nBlockAlign		4
	// wBitsPerSample	16

	// Stereo
	// nChannels			1
	// nSamplesPerSec	22050
	// nAvgBytesPerSec	22050*2
	// nBlockAlign		2
	// wBitsPerSample	16

	//mono=(channels==1?TRUE:FALSE);

	fformat.wFormatTag=WAVE_FORMAT_PCM;
	fformat.nChannels=mono?1:2;
	fformat.nSamplesPerSec=22050; //freq;
	fformat.nBlockAlign=mono?2:4;
	fformat.nAvgBytesPerSec=(mono?2:4)*22050; //freq*4;
	fformat.wBitsPerSample=16;
	fformat.cbSize=0;

	sound->BufSize=samples;
	sample_data=new unsigned char[samples];
	int dec_data=acm->readAndDecompress((WORD*)sample_data,samples);
	if(dec_data!=samples)
	{
		WriteLog(__FUNCTION__" - Decode Acm error.\n");
		delete[] sample_data;
		return false;
	}

	return true;
}

bool SoundManager::LoadOGG(Sound* sound, WAVEFORMATEX& fformat, BYTE*& sample_data)
{
	char full_path[MAX_FOPATH];
	fileMngr.GetFullPath(sound->FileName.c_str(),sound->PathType,full_path);

	FILE* fs=fopen(full_path,"rb");
	if(!fs)
	{
		WriteLog(__FUNCTION__" - File<%s> not found.\n",full_path);
		return false;
	}

	int error;
	if(error=ov_open(fs,&sound->OggDescriptor,NULL,0))
	{
		WriteLog(__FUNCTION__" - ov_open error<%d>.\n",error);
		return false;
	}

    vorbis_info* vi=ov_info(&sound->OggDescriptor,-1);
	if(!vi)
	{
		WriteLog(__FUNCTION__" - ov_info error.\n");
		return false;
	}

	fformat.wFormatTag=WAVE_FORMAT_PCM;
	fformat.nChannels=vi->channels;
	fformat.nSamplesPerSec=vi->rate;
	fformat.wBitsPerSample=16;
	fformat.nBlockAlign=vi->channels*fformat.wBitsPerSample/8;
	fformat.nAvgBytesPerSec=fformat.nBlockAlign*vi->rate;
	fformat.cbSize=0;

	sample_data=new unsigned char[STREAMING_PORTION];
	if(!sample_data)
	{
		ov_clear(&sound->OggDescriptor);
		delete[] sample_data;
		return false;
	}

	int result=0;
	while(true)
	{
		result=ov_read(&sound->OggDescriptor,(char*)(sample_data)+sound->Decoded,4096,0,2,1,NULL);
		if(result<=0) break;
		sound->Decoded+=result;
		if(sound->Decoded>=STREAMING_PORTION*75/100) break;
	}

	if(result<=0) // No need streaming
	{
		sound->IsNeedStreaming=false;
		sound->BufSize=sound->Decoded;
		ov_clear(&sound->OggDescriptor);
	}
	else
	{
		sound->IsNeedStreaming=true;
		sound->BufSize=STREAMING_PORTION*2;
		sound->BufOffs=sound->Decoded;
		sound->MediaType=SOUND_FILE_OGG; // Allow ov_clear on Sound destructor
	}
	return true;
}

bool SoundManager::Streaming(Sound* sound)
{
	unsigned char* smpl_data=NULL;
	DWORD size_data=0;
	DWORD decoded=0;

	if(!((sound->MediaType==SOUND_FILE_WAV && StreamingWAV(sound,smpl_data,size_data))
	|| (sound->MediaType==SOUND_FILE_ACM && StreamingACM(sound,smpl_data,size_data))
	|| (sound->MediaType==SOUND_FILE_OGG && StreamingOGG(sound,smpl_data,size_data))))
	{
		WriteLog(__FUNCTION__" - Unable to stream sound.\n");
		SAFEDELA(smpl_data);
		return false;
	}

	void* dst=NULL;
	void* dst_=NULL;
	DWORD size=0;
	DWORD size_=0;
	HRESULT hr;
	if((hr=sound->SndBuf->Lock(sound->BufOffs,size_data,&dst,&size,&dst_,&size_,0))!=DS_OK)
	{
		WriteLog(__FUNCTION__" - Lock error<%u>.\n",hr);
		delete[] smpl_data;
		return false;
	}

	memcpy(dst,smpl_data,size);
	if(dst_) memcpy(dst_,smpl_data+size,size_);
	if((hr=sound->SndBuf->Unlock(dst,size,dst_,size_))!=DS_OK)
	{
		WriteLog(__FUNCTION__" - Unlock error<%u>.\n",hr);
		delete[] smpl_data;
		return false;
	}
	sound->BufOffs+=size+size_;
	if(sound->BufOffs>=sound->BufSize) sound->BufOffs-=sound->BufSize;
	SAFEDELA(smpl_data);
	return true;
}

bool SoundManager::StreamingWAV(Sound* sound, BYTE*& sample_data, DWORD& size_data)
{
	return false;
}

bool SoundManager::StreamingACM(Sound* sound, BYTE*& sample_data, DWORD& size_data)
{
	return false;
}

bool SoundManager::StreamingOGG(Sound* sound, BYTE*& sample_data, DWORD& size_data)
{
	sample_data=new unsigned char[sound->BufSize];
	if(!sample_data) return false;

	int result=0;
	DWORD decoded=0;
	while(true)
	{
		result=ov_read(&sound->OggDescriptor,(char*)(sample_data)+decoded,4096,0,2,1,NULL);
		if(result<=0) break;
		decoded+=result;
		sound->Decoded+=result;
		if(decoded>=sound->BufSize*75/100) break;
	}

	if(result<=0)
	{
		// No need streaming
		if(decoded<=0) // Erase
		{
			sound->SndBuf->Stop();
			return false;
		}
		else // Fill silence
		{
			if(decoded<sound->BufSize*75/100) ZeroMemory(sample_data+decoded,sound->BufSize*75/100-decoded);
		}
	}
	size_data=decoded;
	return true;
}

/************************************************************************/
/* FOnline API                                                          */
/************************************************************************/

bool SoundManager::LoadSoundList(const char* lst_name, int path_type)
{
	if(!isActive) return false;

	if(!fileMngr.LoadFile(lst_name,path_type))
	{
		WriteLog(__FUNCTION__" - Sound list<%s> load fail.\n",lst_name);
		return false;
	}
	char str[256];
	char name[256];
	char fname[256];
	istrstream istr((char*)fileMngr.GetBuf());
	while(!istr.eof())
	{
		if(istr.getline(str,256).fail()) break;
		Str::EraseChars(str,'\r');
		Str::EraseChars(str,'\n');
		_strupr(str);

		StringCopy(fname,str);
		StringCopy(name,str);
		char* ext=(char*)FileManager::GetExtension(name);
		if(!ext) continue;
		*(--ext)='\0';

		soundInfo.insert(StrStrMapVal(name,fname));
	}

	return true;
}

void SoundManager::PlaySound(const char* name)
{
	if(!isActive || !GetSoundVolume()) return;
	Sound* sound=Load(name,PT_SND_SFX);
	if(sound) Play(sound,soundVolDb,0);
}

void SoundManager::PlayAction(const char* body_type, BYTE anim1, BYTE anim2)
{
	if(!isActive || !GetSoundVolume() || !body_type) return;

	const char abc[]="_ABCDEFGHIJKLMNOPQRST";
	char name[64];
	StringCopy(name,body_type);
	size_t len=strlen(name);
	name[len+0]=abc[anim1];
	name[len+1]=abc[anim2];
	name[len+2]='\0';
	Str::Upr(name);

	StrStrMapIt it=soundInfo.find(name);
	if(it==soundInfo.end())
	{
		name[2]='X';
		name[3]='X';
		name[4]='X';
		name[5]='X';

		it=soundInfo.find(name);
		if(it==soundInfo.end()) return;
	}

	PlaySound((*it).second.c_str());
}

void SoundManager::PlaySoundType(BYTE sound_type, BYTE sound_type_ext, BYTE sound_id, BYTE sound_id_ext)
{
	if(!isActive || !GetSoundVolume()) return;

	// Generate name of the sound
	char name[9];
	if(sound_type=='W') // Weapon, W123XXXR
	{
		name[0]='W';
		name[1]=sound_type_ext;
		name[2]=sound_id;
		name[3]=sound_id_ext;
		name[4]='X';
		name[5]='X';
		name[6]='X';
		name[7]='1';
		name[8]='\0';

		// Try count dublier
		if(!Random(0,1))
		{
			name[7]='2';
			_strupr(name);
			if(!soundInfo.count(name)) name[7]='1';
		}
	}
	else if(sound_type=='S') // Door
	{
		name[0]='S';
		name[1]=sound_type_ext;
		name[2]='D';
		name[3]='O';
		name[4]='O';
		name[5]='R';
		name[6]='S';
		name[7]=sound_id;
		name[8]='\0';
	}
	else // Other
	{
		name[0]=sound_type;
		name[1]=sound_type_ext;
		name[2]=sound_id;
		name[3]=sound_id_ext;
		name[4]='X';
		name[5]='X';
		name[6]='X';
		name[7]='X';
		name[8]='\0';
	}
	_strupr(name);

	StrStrMapIt it=soundInfo.find(name);
	if(it==soundInfo.end()) return;

	// Play
	PlaySound((*it).second.c_str());
}

void SoundManager::PlayMusic(const char* fname, DWORD pos, DWORD repeat)
{
	if(!isActive || !GetMusicVolume()) return;

	StopMusic();

	// Load new
	Sound* sound=Load(fname,PT_SND_MUSIC);
	if(sound)
	{
		sound->IsMusic=true;
		sound->RepeatTime=repeat;
		Play(sound,musicVolDb,0);
	}
}

void SoundManager::StopMusic()
{
	// Find and erase old music
	for(SoundVecIt it=soundsActive.begin();it!=soundsActive.end();)
	{
		Sound* sound=*it;
		if(sound->IsMusic)
		{
			delete sound;
			it=soundsActive.erase(it);
		}
		else ++it;
	}
}

void SoundManager::PlayAmbient(const char* str)
{
	if(!isActive || !GetSoundVolume()) return;
	
	int rnd=Random(1,100);

	char name[32];
	char num[32];

	for(int i=0;*str;++i,++str)
	{
		// Read name
		name[i]=*str;

		if(*str==':')
		{
			if(!i) return;
			name[i]='\0';
			str++;

			// Read number
			int j;
			for(j=0;*str && *str!=',';++j,++str) num[j]=*str;
			if(!j) return;
			num[j]='\0';

			// Check
			int k=atoi(num);
			if(rnd<=k)
			{
				_strupr(name);
				PlaySound(name);
				return;
			}

			rnd-=k;
			i=-1;

			if(*str!=',') return;
			str++;
		}
	}
}

/************************************************************************/
/*                                                                      */
/************************************************************************/