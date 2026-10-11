#pragma once

/// Helpers for deriving bars and phrases from beat indices.
///
/// Bars are assumed to have 4 beats (4/4 time) and phrases to have 4 bars
/// (16 beats). Both are aligned to a downbeat, see
/// `mixxx::Beats::resolveDownbeatIndex()`.
namespace mixxx::bars {

constexpr int kBeatsPerBar = 4;
constexpr int kBarsPerPhrase = 4;
constexpr int kBeatsPerPhrase = kBeatsPerBar * kBarsPerPhrase;

/// Returns `value` modulo `divisor`, always in the range [0, divisor).
constexpr int positiveModulo(int value, int divisor) {
    const int remainder = value % divisor;
    return remainder < 0 ? remainder + divisor : remainder;
}

/// Returns `value` divided by `divisor`, rounded towards negative infinity.
constexpr int floorDivide(int value, int divisor) {
    return (value - positiveModulo(value, divisor)) / divisor;
}

/// Returns the 0-based position (0 to kBeatsPerBar - 1) of the beat with index
/// `beatIndex` within its bar.
constexpr int beatInBar(int beatIndex, int downbeatIndex) {
    return positiveModulo(beatIndex - downbeatIndex, kBeatsPerBar);
}

/// Returns the 0-based position (0 to kBarsPerPhrase - 1) of the bar
/// containing the beat with index `beatIndex` within its phrase.
constexpr int barInPhrase(int beatIndex, int downbeatIndex) {
    return positiveModulo(
            floorDivide(beatIndex - downbeatIndex, kBeatsPerBar), kBarsPerPhrase);
}

constexpr bool isBarStart(int beatIndex, int downbeatIndex) {
    return beatInBar(beatIndex, downbeatIndex) == 0;
}

constexpr bool isPhraseStart(int beatIndex, int downbeatIndex) {
    return positiveModulo(beatIndex - downbeatIndex, kBeatsPerPhrase) == 0;
}

} // namespace mixxx::bars
