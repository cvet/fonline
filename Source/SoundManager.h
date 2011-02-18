#ifndef __SOUND_MANAGER__
#define __SOUND_MANAGER__

#include "Common.h"
#include "FileManager.h"
#include "Ogg/vorbisfile.h"

/************************************************************************/
/* MANAGER                                                              */
/************************************************************************/

#define SOUND_DEFAULT_EXT			".acm"
#define MUSIC_PROCESS_TIME          (200)
#define MUSIC_PLAY_PAUSE            (Random(240,360)*1000) // 4-6 minutes
#define STREAMING_PORTION           (0x40000)
#define SOUND_FILE_NONE             (0)
#define SOUND_FILE_WAV              (1)
#define SOUND_FILE_ACM              (2)
#define SOUND_FILE_OGG              (3)

/************************************************************************/
/* SOUND TYPE WEAPON                                                    */
/************************************************************************/

#define SOUND_WEAPON_USE			'A'
#define SOUND_WEAPON_FLY			'F'
#define SOUND_WEAPON_MISS			'H'
#define SOUND_WEAPON_EMPTY			'O'
#define SOUND_WEAPON_RELOAD			'R'

/************************************************************************/
/* SOUNDS                                                               */
/************************************************************************/

#define SND_BUTTON1_IN				"BUTIN1"
#define SND_BUTTON2_IN				"BUTIN2"
#define SND_BUTTON3_IN				"BUTIN3"
#define SND_BUTTON4_IN				"BUTIN4"
#define SND_BUTTON1_OUT				"BUTOUT1"
#define SND_BUTTON2_OUT				"BUTOUT2"
#define SND_BUTTON3_OUT				"BUTOUT3"
#define SND_BUTTON4_OUT				"BUTOUT4"
#define SND_LMENU					"IACCUXX1"
#define SND_COMBAT_MODE_ON          "ICIBOXX1"
#define SND_COMBAT_MODE_OFF         "ICIBCXX1"

/************************************************************************/
/*                                                                      */
/************************************************************************/

#define MUSIC_PIECE					(10000)

struct Sound
{
	IDirectSoundBuffer* SndBuf;
	DWORD BufSize;
	bool IsMusic;
	DWORD NextPlay;
	DWORD RepeatTime;

	// Streaming data
	bool IsNeedStreaming;
	DWORD BufOffs;
	DWORD Decoded;
	int MediaType;
	OggVorbis_File OggDescriptor;

	// Restore buffer
	int PathType;
	string FileName;

	Sound():SndBuf(NULL),BufSize(0),IsMusic(false),NextPlay(0),RepeatTime(0),IsNeedStreaming(false),BufOffs(0),Decoded(0),
		MediaType(SOUND_FILE_NONE),PathType(0){}
	~Sound(){SAFEREL(SndBuf); if(MediaType==SOUND_FILE_OGG) ov_clear(&OggDescriptor);}
};
typedef vector<Sound*> SoundVec;
typedef vector<Sound*>::iterator SoundVecIt;

class SoundManager
{
public:
	SoundManager():soundDevice(NULL),isActive(false),soundVolDb(DSBVOLUME_MAX),musicVolDb(DSBVOLUME_MAX){}

	bool IsActive(){return isActive;}
	bool Init(HWND wnd);
	void Clear();
	void ClearSounds();
	void Process();

	DWORD GetSoundVolume();
	DWORD GetMusicVolume();
	DWORD GetSoundVolumeDb();
	DWORD GetMusicVolumeDb();
	void SetSoundVolume(int vol_proc);
	void SetMusicVolume(int vol_proc);

	bool PlaySound(const char* name);
	bool PlaySoundType(BYTE sound_type, BYTE sound_type_ext, BYTE sound_id, BYTE sound_id_ext);
	bool PlayMusic(const char* fname, DWORD pos = 0, DWORD repeat = MUSIC_PLAY_PAUSE);
	void StopMusic();
	void PlayAmbient(const char* str);

private:
	void Play(Sound* sound, int vol_db, DWORD flags);
	Sound* Load(const char* fname, int path_type);
	bool LoadWAV(Sound* sound, WAVEFORMATEX& fformat, BYTE*& sample_data);
	bool LoadACM(Sound* sound, WAVEFORMATEX& fformat, BYTE*& sample_data, bool mono);
	bool LoadOGG(Sound* sound, WAVEFORMATEX& fformat, BYTE*& sample_data);
	bool Streaming(Sound* sound);
	bool StreamingWAV(Sound* sound, BYTE*& sample_data, DWORD& size_data);
	bool StreamingACM(Sound* sound, BYTE*& sample_data, DWORD& size_data);
	bool StreamingOGG(Sound* sound, BYTE*& sample_data, DWORD& size_data);

	bool isActive;
	int soundVolDb;
	int musicVolDb;
	IDirectSound8* soundDevice;

	SoundVec soundsActive;
};

extern SoundManager SndMngr;

#endif // __SOUND_MANAGER__