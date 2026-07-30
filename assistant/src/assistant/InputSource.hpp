#pragma once

#include <functional>
#include <stop_token>
#include <string>

namespace assistant::core {

// Abstraction over how user utterances arrive: typed on stdin ("hands
// needed" mode, the mic is never opened) or spoken into the mic and
// transcribed via Wyoming STT ("hands-free" mode). Assistant only talks to
// this interface, so switching --mode typed|voice never touches anything
// below it (LLM, sentence buffering, TTS, playback).
//
// Push-based (rather than a blocking next_utterance() call) so that a new
// utterance can arrive and preempt a turn that's still being spoken -
// necessary for voice-mode barge-in, where the user starts talking again
// before the assistant finishes its answer.
class InputSource {
public:
    virtual ~InputSource() = default;

    // Called once by Assistant::run(). Must not block: implementations run
    // their capture/read loop on their own thread(s) and invoke
    // on_utterance for every recognized user turn. Call on_utterance with
    // an empty string to signal end-of-session (EOF on stdin, fatal STT
    // error, etc).
    virtual void start(std::stop_token stop_token,
                        std::function<void(std::string)> on_utterance) = 0;

    // Stops capture/read threads and blocks until they've exited. Safe to
    // call even if start() was never called or already stopped.
    virtual void stop() = 0;

    // Optional: lets Assistant tell voice-capable sources whether they
    // should currently be listening for new speech. Used to mute the mic's
    // VAD while the assistant's own TTS is playing when barge-in is
    // disabled (see Config::allow_barge_in). No-op for sources that don't
    // care (e.g. typed input). Must be called before start().
    virtual void set_activity_probe(std::function<bool()> should_listen) { (void)should_listen; }
};

}  // namespace assistant::core
