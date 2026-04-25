#include "Hammer_Audio.h"

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

//{ Sound Device
SoundDevice* SoundDevice::get()
{
	static SoundDevice* snd_device = new SoundDevice();
	return snd_device;
}

SoundDevice::SoundDevice()
{
	p_ALCDevice = alcOpenDevice(nullptr); // nullptr = get default device
	if (!p_ALCDevice)
		throw("failed to get sound device");

	p_ALCContext = alcCreateContext(p_ALCDevice, nullptr);  // create context
	if(!p_ALCContext)
		throw("Failed to set sound context");

	if (!alcMakeContextCurrent(p_ALCContext))   // make context current
		throw("failed to make context current");

	const ALCchar* name = nullptr;
	if (alcIsExtensionPresent(p_ALCDevice, "ALC_ENUMERATE_ALL_EXT"))
		name = alcGetString(p_ALCDevice, ALC_ALL_DEVICES_SPECIFIER);
	if (!name || alcGetError(p_ALCDevice) != AL_NO_ERROR)
		name = alcGetString(p_ALCDevice, ALC_DEVICE_SPECIFIER);
	printf("Opened \"%s\"\n", name);
}

SoundDevice::~SoundDevice()
{
	if (!alcMakeContextCurrent(nullptr))
		throw("failed to set context to nullptr");

	alcDestroyContext(p_ALCContext);
	if (p_ALCContext)
		throw("failed to unset during close");

	if (!alcCloseDevice(p_ALCDevice))
		throw("failed to close sound device");

}
void SoundDevice::AddListener(const glm::vec3& position, const glm::vec3& orientation) {
    alListener3f(AL_POSITION, position.x, position.y, position.z);

    // Forward vector (where the listener is "looking") and Up vector
    ALfloat orientationVec[6] = { orientation.x, orientation.y, orientation.z, 0.0f, 1.0f, 0.0f };
    alListenerfv(AL_ORIENTATION, orientationVec);
}

//}

//{ Sound source
SoundSource::SoundSource()
{
    alGenSources(1, &p_Source);
    alSourcef(p_Source, AL_PITCH, p_Pitch);
    alSourcef(p_Source, AL_GAIN, p_Gain);
    alSource3f(p_Source, AL_POSITION, p_Position[0], p_Position[1], p_Position[2]);
    alSource3f(p_Source, AL_VELOCITY, p_Velocity[0], p_Velocity[1], p_Velocity[2]);
    alSourcei(p_Source, AL_LOOPING, 0);
    alSourcei(p_Source, AL_BUFFER, 0);  // Start with no buffer
}

SoundSource::~SoundSource()
{
    alDeleteSources(1, &p_Source);
    if (p_Buffer != 0)
    {
        alDeleteBuffers(1, &p_Buffer);  // Ensure buffer is deleted if it's been loaded
    }
}

bool SoundSource::isPlaying()
{
    ALint state;
    alGetSourcei(p_Source, AL_SOURCE_STATE, &state);
    return state == AL_PLAYING;
}

void SoundSource::Play(const ALuint buffer_to_play)
{
    if (!isPlaying())  // Only play if not already playing
    {
            // Unload the previous buffer if necessary
            if (buffer_to_play != p_Buffer)
            {
                if (p_Buffer != 0) // Delete the previous buffer if it was used
                {
                    alDeleteBuffers(1, &p_Buffer);
                }
                p_Buffer = buffer_to_play;
                alSourcei(p_Source, AL_BUFFER, (ALint)p_Buffer);
            }

        alSourcePlay(p_Source);
    }
}

void SoundSource::PlayBackgroundMusic(const ALuint buffer_to_play)
{
    if (!isPlaying())  // Only play if not already playing
    {
        // Unload the previous buffer if necessary
        if (buffer_to_play != p_Buffer)
        {
            if (p_Buffer != 0) // Delete the previous buffer if it was used
            {
                alDeleteBuffers(1, &p_Buffer);
            }
            p_Buffer = buffer_to_play;
            alSourcei(p_Source, AL_BUFFER, (ALint)p_Buffer);
        }

        alSourcei(p_Source, AL_LOOPING, AL_TRUE);  // Enable looping for background music
        alSourcePlay(p_Source);
    }
}
void SoundSource::PlayAtPosition(const ALuint buffer_to_play, const glm::vec3& position) {
    alSourcei(p_Source, AL_BUFFER, buffer_to_play);
    alSource3f(p_Source, AL_POSITION, position.x, position.y, position.z);
    alSourcePlay(p_Source);
}

void SoundSource::UpdateVolumeBasedOnDistance(const glm::vec3& listenerPosition, float BaseVolume)
{
    glm::vec3 sourcePosition(p_Position[0], p_Position[1], p_Position[2]);
    float distance = glm::distance(listenerPosition, sourcePosition);

    // Adjust volume based on distance (clamp to avoid negative values)
    float volume = 1.0f / (1.0f + 0.1f * distance * distance) * BaseVolume;
    volume = glm::clamp(volume, 0.0f, 1.0f);

    alSourcef(p_Source, AL_GAIN, volume);
}

void SoundSource::Stop()
{
    if (isPlaying())  // Only play if not already playing
    {
        alSourceStop(p_Source);
    }
}

void SoundSource::Pause()
{
    ALint state;
    alGetSourcei(p_Source, AL_SOURCE_STATE, &state);

    if (state == AL_PLAYING)
        alSourcePause(p_Source);
}

void SoundSource::Resume()
{
    ALint state;
    alGetSourcei(p_Source, AL_SOURCE_STATE, &state);

    if (state == AL_PAUSED)
        alSourcePlay(p_Source);
}

void SoundSource::ResumeMusic()
{
    if (!isPlaying())  // Only play if not already playing
    {
        alSourcei(p_Source, AL_LOOPING, AL_TRUE);  // Enable looping for background music
        alSourcePlay(p_Source);
    }
}

void SoundSource::SetVolume(float Volume)
{
    p_Gain = Volume;

    alSourcef(p_Source, AL_GAIN, p_Gain);
}

//}


//{sound buffer

SoundBuffer* SoundBuffer::get()
{
	static SoundBuffer* sndbuf = new SoundBuffer();
	return sndbuf;
}

ALuint SoundBuffer::addSoundEffect(const std::string& AudioName)
{

    std::string Path = getAudioPath(AudioName);
	ALenum err, format;
	ALuint buffer;
	SNDFILE* sndfile;
	SF_INFO sfinfo;
	short* membuf;
	sf_count_t num_frames;
	ALsizei num_bytes;

	/* Open the audio file and check that it's usable. */
	sndfile = sf_open(Path.c_str(), SFM_READ, &sfinfo);
	if (!sndfile)
	{
		fprintf(stderr, "Could not open audio in %s: %s\n", Path.c_str(), sf_strerror(sndfile));
		return 0;
	}
	if (sfinfo.frames < 1 || sfinfo.frames >(sf_count_t)(INT_MAX / sizeof(short)) / sfinfo.channels)
	{
		fprintf(stderr, "Bad sample count in %s (%" PRId64 ")\n", Path.c_str(), sfinfo.frames);
		sf_close(sndfile);
		return 0;
	}

	/* Get the sound format, and figure out the OpenAL format */
	format = AL_NONE;
	if (sfinfo.channels == 1)
		format = AL_FORMAT_MONO16;
	else if (sfinfo.channels == 2)
		format = AL_FORMAT_STEREO16;
	if (!format)
	{
		fprintf(stderr, "Unsupported channel count: %d\n", sfinfo.channels);
		sf_close(sndfile);
		return 0;
	}

	/* Decode the whole audio file to a buffer. */
	membuf = static_cast<short*>(malloc((size_t)(sfinfo.frames * sfinfo.channels) * sizeof(short)));

	num_frames = sf_readf_short(sndfile, membuf, sfinfo.frames);
	if (num_frames < 1)
	{
		free(membuf);
		sf_close(sndfile);
		fprintf(stderr, "Failed to read samples in %s (%" PRId64 ")\n", Path, num_frames);
		return 0;
	}
	num_bytes = (ALsizei)(num_frames * sfinfo.channels) * (ALsizei)sizeof(short);

	/* Buffer the audio data into a new buffer object, then free the data and
	 * close the file.
	 */
	buffer = 0;
	alGenBuffers(1, &buffer);
	alBufferData(buffer, format, membuf, num_bytes, sfinfo.samplerate);

	free(membuf);
	sf_close(sndfile);

	/* Check if an error occured, and clean up if so. */
	err = alGetError();
	if (err != AL_NO_ERROR)
	{
		fprintf(stderr, "OpenAL Error: %s\n", alGetString(err));
		if (buffer && alIsBuffer(buffer))
			alDeleteBuffers(1, &buffer);
		return 0;
	}

	p_SoundEffectBuffers.push_back(buffer);  // add to the list of known buffers

	return buffer;
}

bool SoundBuffer::removeSoundEffect(const ALuint& buffer)
{
	auto it = p_SoundEffectBuffers.begin();
	while (it != p_SoundEffectBuffers.end())
	{
		if (*it == buffer)
		{
			alDeleteBuffers(1, &*it);

			it = p_SoundEffectBuffers.erase(it);

			return true;
		}
		else {
			++it;
		}
	}
	return false;  // couldn't find to remove
}


SoundBuffer::SoundBuffer()
{
	p_SoundEffectBuffers.clear();

}

SoundBuffer::~SoundBuffer()
{
	alDeleteBuffers(p_SoundEffectBuffers.size(), p_SoundEffectBuffers.data());

	p_SoundEffectBuffers.clear();
}

uint32_t GetRandomSound(const std::vector<uint32_t>& sounds)
{
    return sounds[rand() % sounds.size()];
}

//}
