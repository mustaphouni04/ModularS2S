#pragma once

namespace assistant::util {

// Reference-counted Pa_Initialize()/Pa_Terminate() so AudioInput and
// AudioOutput can each own an instance independently without double-init
// or terminating PortAudio out from under the other.
class PortAudioGuard {
public:
    PortAudioGuard();
    ~PortAudioGuard();

    PortAudioGuard(const PortAudioGuard&) = delete;
    PortAudioGuard& operator=(const PortAudioGuard&) = delete;
};

}  // namespace assistant::util
