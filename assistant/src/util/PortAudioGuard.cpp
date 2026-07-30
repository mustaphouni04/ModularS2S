#include "util/PortAudioGuard.hpp"

#include <portaudio.h>
#include <spdlog/spdlog.h>

#include <mutex>

namespace assistant::util {

namespace {
std::mutex g_mutex;
int g_refcount = 0;
}  // namespace

PortAudioGuard::PortAudioGuard() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_refcount == 0) {
        PaError err = Pa_Initialize();
        if (err != paNoError) {
            spdlog::error("PortAudio init failed: {}", Pa_GetErrorText(err));
        }
    }
    ++g_refcount;
}

PortAudioGuard::~PortAudioGuard() {
    std::lock_guard<std::mutex> lock(g_mutex);
    --g_refcount;
    if (g_refcount == 0) {
        Pa_Terminate();
    }
}

}  // namespace assistant::util
