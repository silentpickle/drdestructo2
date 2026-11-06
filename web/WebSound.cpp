#include "stdafx.h"
#include "ISound.h"
#include <emscripten.h>

namespace
{
class WebSample : public ISample
{
public:
    explicit WebSample(EType type) : m_type(type) {}
    ~WebSample() override
    {
        EM_ASM({ Module.audio.remove($0); }, m_id);
    }

    bool Load(const std::string &path) override
    {
        EM_ASM({ Module.audio.remove($0); }, m_id);
        m_id = EM_ASM_INT({ return Module.audio.create(UTF8ToString($0), $1); },
            path.c_str(), m_type);
        m_volume = 1;
        m_pan = 0;
        m_pitch = 1;
        m_loop = LOOPMODE_ONCE;
        return m_id != 0;
    }

    bool Play() override
    {
        return EM_ASM_INT({ return Module.audio.play($0); }, m_id);
    }

    bool Stop() override
    {
        return EM_ASM_INT({ return Module.audio.stop($0); }, m_id);
    }

    void SetVolume(float value) override
    {
        m_volume = value;
        EM_ASM({ Module.audio.set($0, 'volume', $1); }, m_id, value);
    }
    float GetVolume() const override { return m_volume; }

    void SetPan(float value) override
    {
        m_pan = value;
        EM_ASM({ Module.audio.set($0, 'pan', $1); }, m_id, value);
    }
    float GetPan() const override { return m_pan; }

    void SetPitch(float value) override
    {
        m_pitch = value;
        EM_ASM({ Module.audio.set($0, 'rate', $1); }, m_id, value);
    }
    float GetPitch() const override { return m_pitch; }

    bool IsPlaying() const override
    {
        return EM_ASM_INT({ return Module.audio.playing($0); }, m_id);
    }

    void SetLoopMode(ELoopMode mode) override
    {
        m_loop = mode;
        EM_ASM({ Module.audio.set($0, 'loop', !!$1); }, m_id, mode == LOOPMODE_LOOP);
    }
    ELoopMode GetLoopMode() const override { return m_loop; }

private:
    const EType m_type;
    int m_id = 0;
    float m_volume = 1;
    float m_pan = 0;
    float m_pitch = 1;
    ELoopMode m_loop = LOOPMODE_ONCE;
};

class WebSound : public ISound
{
public:
    bool Init() override { return true; }

    void SetMasterVolume(float volume) override
    {
        m_master = volume;
        EM_ASM({ Module.audio.volume(-1, $0); }, volume);
    }
    float GetMasterVolume() const override { return m_master; }

    void SetVolumeByType(ISample::EType type, float volume) override
    {
        m_volumes[type] = volume;
        EM_ASM({ Module.audio.volume($0, $1); }, type, volume);
    }
    float GetVolumeByType(ISample::EType type) const override { return m_volumes[type]; }

private:
    ISamplePtr CreateSample(const std::string &path, ISample::EType type) override
    {
        auto sample = std::make_shared<WebSample>(type);
        return sample->Load(path) ? sample : nullptr;
    }

    float m_master = 1;
    float m_volumes[ISample::TYPES_COUNT] = {1, 1};
};
}

ISound & GetSound()
{
    static WebSound sound;
    return sound;
}
