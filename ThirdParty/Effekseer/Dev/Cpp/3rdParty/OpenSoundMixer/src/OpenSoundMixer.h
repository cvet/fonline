
#pragma once

#include <stdint.h>
#include <stdio.h>
#include <vector>
#include <functional>

namespace osm {
struct Sample {
    int16_t Left;
    int16_t Right;
};

/**
@brief	参照カウンタのインターフェース
*/
class IReference {
public:
    IReference() {}

    virtual ~IReference() {}

    /**
    @brief	参照カウンタを加算する。
    @return	加算後の参照カウンタ
    */
    virtual int AddRef() = 0;

    /**
    @brief	参照カウンタを取得する。
    @return	参照カウンタ
    */
    virtual int GetRef() = 0;

    /**
    @brief	参照カウンタを減算する。0になった時、インスタンスを削除する。
    @return	減算後の参照カウンタ
    */
    virtual int Release() = 0;
};

class Sound : public IReference {
protected:
    Sound() {}
    virtual ~Sound() {}

public:
    virtual float GetLoopStartingPoint() const = 0;

    virtual void SetLoopStartingPoint(float startingPoint) = 0;

    virtual float GetLoopEndPoint() const = 0;

    virtual void SetLoopEndPoint(float endPoint) = 0;

    virtual bool GetIsLoopingMode() const = 0;

    virtual void SetIsLoopingMode(bool isLoopingMode) = 0;

    virtual float GetLength() const = 0;
};

// Window Function Type
enum class FFTWindow
{
    Rectangular,
    Triangle,
    Hamming,
    Hanning,
    Blackman,
    BlackmanHarris
};

class Manager : public IReference {
protected:
    Manager() {}
    ~Manager() {}

public:
    virtual bool Initialize() = 0;

    virtual void Finalize() = 0;

    virtual Sound* CreateSound(const void* data, int32_t size, bool isDecompressed) = 0;

    virtual int32_t Play(Sound* sound) = 0;

    virtual bool IsPlaying(int32_t id) = 0;

    virtual void StopAll() = 0;

    virtual void Stop(int32_t id) = 0;

    virtual void Pause(int32_t id) = 0;

    virtual void Resume(int32_t id) = 0;

    virtual void SetVolume(int32_t id, float volume) = 0;

    virtual void FadeIn(int32_t id, float second) = 0;

    virtual void FadeOut(int32_t id, float second) = 0;

    /**
            @brief	任意の音量に音量を一定時間かけて変更する。
            @param	id		音のID
            @param	second	変更にかかる時間(秒)
            @param	targetedVolume	変更先の音量(0.0〜1.0)
            @note
            この指定される音量はSetVolumeに指定される音量とは異なり、FadeIn、FadeOutに使用される音量と共通である。
            つまり、このメソッドはFadeIn、FadeOutの任意音量指定版と言える。
    */
    virtual void Fade(int32_t id, float second, float targetedVolume) = 0;

    virtual bool GetIsPlaybackSpeedEnabled(int32_t id) = 0;

    virtual void SetIsPlaybackSpeedEnabled(int32_t id, bool isPlaybackSpeedEnabled) = 0;

    virtual float GetPlaybackSpeed(int32_t id) = 0;

    virtual void SetPlaybackSpeed(int32_t id, float playbackSpeed) = 0;

    virtual float GetPanningPosition(int32_t id) = 0;

    virtual void SetPanningPosition(int32_t id, float panningPosition) = 0;

    virtual float GetPlaybackPosition(int32_t id) = 0;

    virtual void SetPlaybackPosition(int32_t id, float playbackPosition) = 0;

    virtual void GetSpectrum(int32_t id, std::vector<float> &spectrums, FFTWindow window) = 0;

    static Manager* Create();
};

enum class LogType {
    Info,
    Warning,
    Error,
    Debug,
};

void SetLogger(const std::function<void(LogType, const char*)>& logger);

void Log(LogType logType, const char* message);

}  // namespace osm
