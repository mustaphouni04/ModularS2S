#pragma once

#include <string>
#include <vector>

namespace assistant::conversation {

// Accumulates streamed LLM token deltas and emits complete sentences as
// soon as a boundary is found, so TTS can start speaking sentence 1 while
// the LLM is still generating sentence 2. Never buffers the whole reply.
class SentenceBuffer {
public:
    // Feed the next streamed delta. Returns zero or more sentences that
    // became complete as a result (a single delta can complete more than
    // one sentence, e.g. "Yes. No.").
    std::vector<std::string> push(const std::string& delta);

    // Call once the LLM stream has finished. Returns the trailing partial
    // text as a final sentence, if any content is left pending.
    std::vector<std::string> flush();

    void reset();

private:
    std::vector<std::string> scan(bool at_end);

    std::string pending_;
};

}  // namespace assistant::conversation
