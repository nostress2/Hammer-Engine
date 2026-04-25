#ifndef HAMMER_AUDIO_H
#define HAMMER_AUDIO_H

/*
MIT License

Copyright (c) [2025] [Денислав Тихомиров Цанков]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/*Edited example from https://www.youtube.com/watch?v=kWQM1iQ1W0E&ab_channel=Code%2CTech%2CandTutorials
Code Tech and Tutorials.
*/

// OpenAL library
#include <AL/al.h>
#include <AL/alc.h>
// GLM library
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
// Other
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <unordered_map>
#include <map>
#include <sndfile.h>
#include <inttypes.h>
#include <malloc.h>

static std::string getAudioPath(const std::string& assetRelativePath) {
    return std::filesystem::current_path().string() + "/Assets/" + assetRelativePath;
}

class SoundDevice
{
public:
	static SoundDevice* get();
	void AddListener(const glm::vec3& position, const glm::vec3& orientation);

private:
	SoundDevice();
	~SoundDevice();

	ALCdevice* p_ALCDevice;
	ALCcontext* p_ALCContext;

};

class SoundSource
{
public:
	SoundSource();
	~SoundSource();

	void Play(const ALuint buffer_to_play);
	void PlayBackgroundMusic(const ALuint buffer_to_play);
	void PlayAtPosition(const ALuint buffer_to_play, const glm::vec3& position);
	void UpdateVolumeBasedOnDistance(const glm::vec3& listenerPosition, float BaseVolume);
	void Stop();
	void Pause();
	void Resume();
	void ResumeMusic();
	bool isPlaying();

	void SetVolume(float Volume);

private:
	ALuint p_Source;
	float p_Pitch = 1.f;
	float p_Gain = 1.f;
	float p_Position[3] = { 0,0,0 };
	float p_Velocity[3] = { 0,0,0 };
	bool p_LoopSound = false;
	ALuint p_Buffer = 0;
};

class SoundBuffer
{
public:
	static SoundBuffer* get();

	ALuint addSoundEffect(const std::string& AudioName);
	bool removeSoundEffect(const ALuint& buffer);

private:
	SoundBuffer();
	~SoundBuffer();

	std::vector<ALuint> p_SoundEffectBuffers;
};

class Listener
{
public:
    static void SetPosition(const glm::vec3& position) {
        alListener3f(AL_POSITION, position.x, position.y, position.z);
    }

    static void SetOrientation(const glm::vec3& forward, const glm::vec3& up) {
    float orientation[] = {
        forward.x, forward.y, forward.z,  // Listener's forward vector
        up.x, up.y, up.z                  // Listener's up vector
    };
    alListenerfv(AL_ORIENTATION, orientation);
}

    static void SetVelocity(const glm::vec3& velocity) {
        alListener3f(AL_VELOCITY, velocity.x, velocity.y, velocity.z);
    }
};

uint32_t GetRandomSound(const std::vector<uint32_t>& sounds);

#endif // HAMMER_AUDIO_H
