#include "core/audio.hpp"

#include "core/engine.hpp"
#include "core/fileio.hpp"
#include "tools/log.hpp"
#include <fmod/fmod.h>
#include <fmod/fmod_studio.hpp>

using namespace bee;

Audio::Audio()
{
    Log::Info("Audio Engine: FMOD Studio by Firelight Technologies Pty Ltd.");

    // Create the Studio System object
    FMOD_RESULT result = FMOD::Studio::System::create(&m_system);
    if (result != FMOD_OK)
    {
        Log::Error("Failed to create the FMOD Studio System!");
        return;
    }

    // Initialize FMOD Studio, which will also initialize FMOD Core
    result = m_system->initialize(512, FMOD_STUDIO_INIT_NORMAL, FMOD_INIT_NORMAL, nullptr);
    if (result != FMOD_OK)
    {
        Log::Error("Failed to initialize the FMOD Studio System!");
        return;
    }

    // Get the Core System pointer from the Studio System object
    result = m_system->getCoreSystem(&m_core_system);
    if (result != FMOD_OK)
    {
        Log::Error("Failed to get the FMOD Studio System after initialization!");
        return;
    }

    m_core_system->createSoundGroup("SFX", &m_soundGroupSFX);
    m_core_system->createSoundGroup("Music", &m_soundGroupMusic);
}

Audio::~Audio()
{
    m_soundGroupSFX->release();
    m_soundGroupMusic->release();

    // TODO
    // for (auto sound : sounds) sound.second->release();
    for (auto bank : m_banks) bank.second->unload();

    m_system->release();
}

void Audio::Update()
{
    m_system->update();

    // remove released events from the hashmap?
    std::vector<int> eventsToRemove;
    for (auto eventInstance : m_events)
    {
        FMOD_STUDIO_PLAYBACK_STATE state = FMOD_STUDIO_PLAYBACK_STOPPED;
        eventInstance.second->getPlaybackState(&state);
        if (state == FMOD_STUDIO_PLAYBACK_STOPPED) eventsToRemove.push_back(eventInstance.first);
    }

    for (auto id : eventsToRemove) m_events.erase(id);
}

void Audio::LoadBank(const std::string& filename)
{
    // check if the bank already exists
    int hash = static_cast<int>(std::hash<std::string>{}(filename));
    if (m_banks.find(hash) != m_banks.end()) return;

    // try to load the bank
    FMOD::Studio::Bank* bank = nullptr;
    const std::string& real_filename = Engine.FileIO().GetPath(FileIO::Directory::Assets, filename);
    auto result = m_system->loadBankFile(real_filename.c_str(), FMOD_STUDIO_LOAD_BANK_NORMAL, &bank);
    if (result != FMOD_OK)
    {
        Log::Error("FMOD bank with filename {} could not be loaded!", filename);
        return;
    }

    // load all of the bank's sample data immediately
    bank->loadSampleData();
    m_system->flushSampleLoading();  // enable this to wait for loading to finish

    // store the bank by its ID
    m_banks[hash] = bank;
}

void Audio::UnloadBank(const std::string& filename)
{
    int hash = static_cast<int>(std::hash<std::string>{}(filename));
    if (m_banks.find(hash) == m_banks.end())
    {
        Log::Error("FMOD bank with filename {} could not be unloaded!", filename);
        return;
    }

    m_banks[hash]->unload();
    m_banks.erase(hash);
}

int Audio::StartEvent(const std::string& name)
{
    // get the event description
    FMOD::Studio::EventDescription* evd = nullptr;
    auto result = m_system->getEvent(("event:/" + name).c_str(), &evd);
    if (result != FMOD_OK)
    {
        Log::Error("FMOD event with name {} does not exist!", name);
        return -1;
    }

    // create an event instance
    FMOD::Studio::EventInstance* evi = nullptr;
    result = evd->createInstance(&evi);
    if (result != FMOD_OK)
    {
        Log::Error("FMOD event instance with name {} could not be created!", name);
        return -1;
    }

    int eventID = m_nextEventID;
    m_events[eventID] = evi;
    ++m_nextEventID;

    // trigger the event
    result = evi->start();

    // mark it for release immediately
    result = evi->release();

    return eventID;
}

void Audio::SetParameter(const std::string& name, float value, int eventInstanceID)
{
    if (eventInstanceID < 0)
    {
        m_system->setParameterByName(name.c_str(), (float)value);
    }
    else
    {
        auto it = m_events.find(eventInstanceID);
        if (it == m_events.end())
        {
            Log::Error("FMOD event with ID {} does not exist!", eventInstanceID);
            return;
        }

        it->second->setParameterByName(name.c_str(), (float)value);
    }
}

void Audio::SetParameter(const std::string& name, const std::string& value, int eventInstanceID)
{
    if (eventInstanceID < 0)
    {
        m_system->setParameterByNameWithLabel(name.c_str(), value.c_str());
    }
    else
    {
        auto it = m_events.find(eventInstanceID);
        if (it == m_events.end())
        {
            Log::Error("FMOD event with ID {} does not exist!", eventInstanceID);
            return;
        }

        it->second->setParameterByNameWithLabel(name.c_str(), value.c_str());
    }
}

void Audio::LoadSound(const std::string& filename, bool isMusic)
{
    // check if the sound already exists
    int hash = static_cast<int>(std::hash<std::string>{}(filename));
    if (m_sounds.find(hash) != m_sounds.end()) return;

    // try to load the sound file
    FMOD_MODE mode = isMusic ? (FMOD_CREATESTREAM | FMOD_LOOP_NORMAL) : FMOD_DEFAULT;
    FMOD::Sound* sound = nullptr;
    const std::string& real_filename = Engine.FileIO().GetPath(FileIO::Directory::Assets, filename);
    FMOD_RESULT result = m_core_system->createSound(real_filename.c_str(), mode, nullptr, &sound);
    if (result != FMOD_OK)
    {
        Log::Error("Sound with filename {} could not be loaded!", filename);
        return;
    }

    // attach the sound to the right group, and store it by its ID
    sound->setSoundGroup(isMusic ? m_soundGroupMusic : m_soundGroupSFX);
    m_sounds[hash] = sound;
}

int Audio::PlaySound(const std::string& filename)
{
    // check if the sound exists
    int hash = static_cast<int>(std::hash<std::string>{}(filename));
    auto sound = m_sounds.find(hash);
    if (sound == m_sounds.end())
    {
        Log::Error("Sound with filename {} has not been loaded!", filename);
        return -1;
    }

    // play it
    FMOD::Channel* channel = nullptr;
    m_core_system->playSound(sound->second, nullptr, false, &channel);

    // return the index of the channel on which it plays
    int channel_index = 0;
    channel->getIndex(&channel_index);
    return channel_index;
}

void Audio::SetChannelPaused(int channelID, bool paused)
{
    FMOD::Channel* channel = nullptr;
    FMOD_RESULT result = m_core_system->getChannel(channelID, &channel);
    if (result != FMOD_OK)
    {
        Log::Error("Sound channel with ID {} does not exist!", channelID);
        return;
    }
    channel->setPaused(paused);
};