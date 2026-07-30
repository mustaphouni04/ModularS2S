#include "conversation/SentenceBuffer.hpp"
#include "test_util.hpp"

using assistant::conversation::SentenceBuffer;

namespace {

void test_basic_streaming() {
    SentenceBuffer buf;
    std::vector<std::string> emitted;

    for (const std::string& tok : {"The ", "weather ", "today ", "is ", "sunny", ". ", "It ",
                                    "will ", "remain ", "warm", "."}) {
        for (auto& s : buf.push(tok)) emitted.push_back(s);
    }
    for (auto& s : buf.flush()) emitted.push_back(s);

    CHECK_EQ(emitted.size(), 2u);
    if (emitted.size() == 2) {
        CHECK_EQ(emitted[0], std::string("The weather today is sunny."));
        CHECK_EQ(emitted[1], std::string("It will remain warm."));
    }
}

void test_multiple_sentences_in_one_delta() {
    SentenceBuffer buf;
    // Trailing "Maybe." has no whitespace after it yet, so a plain push()
    // correctly holds it back (more text could still arrive); only
    // flush() should release it, matching the streaming contract.
    auto sentences = buf.push("Yes. No. Maybe.");
    CHECK_EQ(sentences.size(), 2u);
    if (sentences.size() == 2) {
        CHECK_EQ(sentences[0], std::string("Yes."));
        CHECK_EQ(sentences[1], std::string("No."));
    }

    auto trailing = buf.flush();
    CHECK_EQ(trailing.size(), 1u);
    if (trailing.size() == 1) {
        CHECK_EQ(trailing[0], std::string("Maybe."));
    }
}

void test_abbreviation_guard() {
    SentenceBuffer buf;
    std::vector<std::string> emitted;
    for (auto& s : buf.push("Mr. Smith went home. He was tired.")) emitted.push_back(s);
    for (auto& s : buf.flush()) emitted.push_back(s);

    CHECK_EQ(emitted.size(), 2u);
    if (emitted.size() == 2) {
        CHECK_EQ(emitted[0], std::string("Mr. Smith went home."));
        CHECK_EQ(emitted[1], std::string("He was tired."));
    }
}

void test_decimal_guard() {
    SentenceBuffer buf;
    std::vector<std::string> emitted;
    for (auto& s : buf.push("Pi is about 3.14 and that's neat.")) emitted.push_back(s);
    for (auto& s : buf.flush()) emitted.push_back(s);

    CHECK_EQ(emitted.size(), 1u);
    if (emitted.size() == 1) {
        CHECK_EQ(emitted[0], std::string("Pi is about 3.14 and that's neat."));
    }
}

void test_flush_without_trailing_punctuation() {
    SentenceBuffer buf;
    auto mid = buf.push("This has no ending punctuation yet");
    CHECK_EQ(mid.size(), 0u);

    auto trailing = buf.flush();
    CHECK_EQ(trailing.size(), 1u);
    if (trailing.size() == 1) {
        CHECK_EQ(trailing[0], std::string("This has no ending punctuation yet"));
    }
}

}  // namespace

int main() {
    test_basic_streaming();
    test_multiple_sentences_in_one_delta();
    test_abbreviation_guard();
    test_decimal_guard();
    test_flush_without_trailing_punctuation();

    if (g_failures > 0) {
        std::cerr << g_failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "all sentence buffer checks passed\n";
    return 0;
}
