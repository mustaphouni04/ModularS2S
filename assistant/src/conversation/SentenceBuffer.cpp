#include "conversation/SentenceBuffer.hpp"

#include <cctype>
#include <unordered_set>

namespace assistant::conversation {

namespace {

const std::unordered_set<std::string>& abbreviations() {
    static const std::unordered_set<std::string> kAbbrevs = {
        "mr", "mrs", "ms", "dr", "prof", "sr", "jr", "st", "vs", "etc", "eg", "ie",
    };
    return kAbbrevs;
}

bool is_sentence_ender(char c) { return c == '.' || c == '!' || c == '?'; }

// Returns the lowercased word immediately preceding pending[end_idx]
// (the sentence-ender), used for the abbreviation guard.
std::string word_before(const std::string& s, size_t end_idx) {
    size_t start = end_idx;
    while (start > 0 && std::isalpha(static_cast<unsigned char>(s[start - 1]))) {
        --start;
    }
    std::string word = s.substr(start, end_idx - start);
    for (char& c : word) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return word;
}

std::string trim(const std::string& s) {
    size_t begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

}  // namespace

std::vector<std::string> SentenceBuffer::push(const std::string& delta) {
    pending_ += delta;
    return scan(/*at_end=*/false);
}

std::vector<std::string> SentenceBuffer::flush() {
    auto sentences = scan(/*at_end=*/true);
    std::string remainder = trim(pending_);
    if (!remainder.empty()) {
        sentences.push_back(remainder);
    }
    pending_.clear();
    return sentences;
}

void SentenceBuffer::reset() { pending_.clear(); }

std::vector<std::string> SentenceBuffer::scan(bool at_end) {
    std::vector<std::string> sentences;

    size_t i = 0;
    size_t consumed = 0;  // how much of pending_ has been emitted so far

    while (i < pending_.size()) {
        char c = pending_[i];
        if (!is_sentence_ender(c)) {
            ++i;
            continue;
        }

        bool have_next = (i + 1) < pending_.size();
        if (!have_next && !at_end) {
            // Might be followed by more punctuation/whitespace in a future
            // delta (e.g. "..." or ". "); wait for more data.
            break;
        }

        char next_char = have_next ? pending_[i + 1] : '\0';
        bool boundary_by_whitespace = !have_next || std::isspace(static_cast<unsigned char>(next_char));

        // Decimal guard: "3.14" - a digit immediately after the dot means
        // it's not sentence-final (covered by boundary_by_whitespace above
        // already being false), nothing extra needed here.

        // Abbreviation guard: "Mr. Smith", "e.g. this"
        bool is_abbrev = false;
        if (c == '.') {
            std::string word = word_before(pending_, i);
            if (!word.empty() && abbreviations().count(word)) {
                is_abbrev = true;
            }
        }

        if (boundary_by_whitespace && !is_abbrev) {
            std::string sentence = trim(pending_.substr(consumed, i + 1 - consumed));
            if (!sentence.empty()) {
                sentences.push_back(sentence);
            }
            consumed = i + 1;
            // skip the single trailing space, if present, so the next
            // sentence doesn't start with a leading space
            if (have_next && std::isspace(static_cast<unsigned char>(next_char))) {
                consumed = i + 2;
                ++i;
            }
        }

        ++i;
    }

    if (consumed > 0) {
        pending_.erase(0, consumed);
    }

    return sentences;
}

}  // namespace assistant::conversation
