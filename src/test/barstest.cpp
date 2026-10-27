#include <gtest/gtest.h>

#include <QTest>
#include <cmath>
#include <memory>

#include "control/controlproxy.h"
#include "test/mockedenginebackendtest.h"
#include "track/bars.h"
#include "track/beats.h"
#include "track/cue.h"
#include "waveform/renderers/waveformbarmarkers.h"

using namespace mixxx;

namespace {

constexpr auto kSampleRate = audio::SampleRate(48000);
// 120 BPM at 48 kHz = 24000 frames per beat
constexpr auto kBpm = Bpm(120.0);
constexpr double kBeatLength = 24000.0;
constexpr auto kFirstBeat = audio::FramePos(1000);

audio::FramePos beatPosition(int beatIndex) {
    return kFirstBeat + beatIndex * kBeatLength;
}

BeatsPointer makeConstTempoBeats() {
    return Beats::fromConstTempo(kSampleRate, kFirstBeat, kBpm);
}

BeatsPointer makeBeatMap() {
    // 8 beats at 120 BPM, then 8 beats at 60 BPM
    QVector<audio::FramePos> positions;
    for (int i = 0; i <= 8; i++) {
        positions.append(beatPosition(i));
    }
    for (int i = 1; i <= 8; i++) {
        positions.append(beatPosition(8) + i * 2 * kBeatLength);
    }
    return Beats::fromBeatPositions(kSampleRate, positions);
}

BeatsPointer roundTrip(const BeatsPointer& pBeats) {
    return Beats::fromByteArray(kSampleRate,
            pBeats->getVersion(),
            pBeats->getSubVersion(),
            pBeats->toByteArray());
}

} // namespace

TEST(BarsTest, BeatInBar) {
    EXPECT_EQ(0, bars::beatInBar(0, 0));
    EXPECT_EQ(1, bars::beatInBar(1, 0));
    EXPECT_EQ(3, bars::beatInBar(7, 0));
    EXPECT_EQ(0, bars::beatInBar(8, 0));
    // Beats before the downbeat
    EXPECT_EQ(3, bars::beatInBar(-1, 0));
    EXPECT_EQ(0, bars::beatInBar(-4, 0));
    // Shifted downbeat
    EXPECT_EQ(0, bars::beatInBar(5, 1));
    EXPECT_EQ(3, bars::beatInBar(4, 1));
    EXPECT_EQ(2, bars::beatInBar(0, -6));
}

TEST(BarsTest, BarInPhrase) {
    EXPECT_EQ(0, bars::barInPhrase(0, 0));
    EXPECT_EQ(0, bars::barInPhrase(3, 0));
    EXPECT_EQ(1, bars::barInPhrase(4, 0));
    EXPECT_EQ(3, bars::barInPhrase(15, 0));
    EXPECT_EQ(0, bars::barInPhrase(16, 0));
    EXPECT_EQ(3, bars::barInPhrase(-1, 0));
    EXPECT_EQ(3, bars::barInPhrase(-4, 0));
    EXPECT_EQ(2, bars::barInPhrase(-5, 0));
    EXPECT_EQ(0, bars::barInPhrase(18, 2));
}

TEST(BarsTest, BarAndPhraseStarts) {
    EXPECT_TRUE(bars::isBarStart(4, 0));
    EXPECT_FALSE(bars::isBarStart(5, 0));
    EXPECT_TRUE(bars::isBarStart(-3, 1));
    EXPECT_TRUE(bars::isPhraseStart(16, 0));
    EXPECT_TRUE(bars::isPhraseStart(-13, 3));
    EXPECT_FALSE(bars::isPhraseStart(4, 0));
}

TEST(BarsTest, LinesFadeOutByBarsAndPhrases) {
    // Without a beat grid update, the downbeat is beat 0.
    const WaveformBarMarkers markers;
    using Markers = WaveformBarMarkers;
    constexpr int kBeat = 1;
    constexpr int kBar = 4;
    constexpr int kPhrase = 16;

    // Far enough apart, all lines are drawn fully.
    EXPECT_FLOAT_EQ(1.f, markers.lineOpacity(kBeat, Markers::kLineFullSpacing));
    EXPECT_FLOAT_EQ(1.f, markers.lineOpacity(kBar, Markers::kLineFullSpacing));
    EXPECT_FLOAT_EQ(1.f, markers.lineOpacity(kPhrase, Markers::kLineFullSpacing));

    // Lines of beats within a bar fade out first, fastest at the start: half
    // way between the thresholds, only a quarter of the opacity is left.
    const float halfFaded = (Markers::kLineFullSpacing + Markers::kLineHiddenSpacing) / 2;
    EXPECT_FLOAT_EQ(0.25f, markers.lineOpacity(kBeat, halfFaded));
    EXPECT_FLOAT_EQ(1.f, markers.lineOpacity(kBar, halfFaded));

    // Then the lines of bars, leaving the lines of phrases.
    const float barsHidden = Markers::kLineHiddenSpacing / 4;
    EXPECT_FLOAT_EQ(0.f, markers.lineOpacity(kBeat, barsHidden));
    EXPECT_FLOAT_EQ(0.f, markers.lineOpacity(kBar, barsHidden));
    EXPECT_FLOAT_EQ(1.f, markers.lineOpacity(kPhrase, barsHidden));
    EXPECT_FLOAT_EQ(0.f, markers.lineOpacity(kPhrase, Markers::kLineHiddenSpacing / 16));
}

TEST(BarsTest, MarksFadeOutByBarsAndPhrases) {
    using Markers = WaveformBarMarkers;
    using Type = Markers::Type;
    EXPECT_FLOAT_EQ(0.f, Markers::markOpacity(Type::Beat, 100.f));

    const float barsFull = Markers::kMarkFullSpacing / 4;
    EXPECT_FLOAT_EQ(1.f, Markers::markOpacity(Type::Bar, barsFull));
    EXPECT_FLOAT_EQ(1.f, Markers::markOpacity(Type::Phrase, barsFull));

    const float barsHidden = Markers::kMarkHiddenSpacing / 4;
    EXPECT_FLOAT_EQ(0.f, Markers::markOpacity(Type::Bar, barsHidden));
    EXPECT_FLOAT_EQ(1.f, Markers::markOpacity(Type::Phrase, barsHidden));
    EXPECT_FLOAT_EQ(0.f, Markers::markOpacity(Type::Phrase, Markers::kMarkHiddenSpacing / 16));
}

TEST(BarsTest, TriangleMarks) {
    using Markers = WaveformBarMarkers;
    using Type = Markers::Type;
    constexpr float kBreadth = 96.f;
    constexpr float kOutlineWidth = 1.f;

    // The triangles are as long as the rectangle marks and equilateral, so
    // all sides are as long as the base.
    const auto bar = Markers::triangleMark(Type::Bar, kBreadth, kOutlineWidth);
    EXPECT_FLOAT_EQ(Markers::markLength(Type::Bar, kBreadth), bar.length);
    const float barSide = std::hypot(bar.halfWidth, bar.length);
    EXPECT_NEAR(2 * bar.halfWidth, barSide, 1e-4f);

    // The outline is as wide on the sides as at the tip.
    EXPECT_FLOAT_EQ(bar.length + 2 * kOutlineWidth, bar.outlineLength);
    EXPECT_NEAR(bar.halfWidth + kOutlineWidth / std::cos(std::atan(1.f / std::sqrt(3.f))),
            bar.outlineHalfWidth,
            1e-4f);

    // Phrase marks are larger.
    const auto phrase = Markers::triangleMark(Type::Phrase, kBreadth, kOutlineWidth);
    EXPECT_FLOAT_EQ(Markers::markLength(Type::Phrase, kBreadth), phrase.length);
    EXPECT_GT(phrase.halfWidth, bar.halfWidth);
}

TEST(BarsTest, DownbeatIsUnsetByDefault) {
    const auto pBeats = makeConstTempoBeats();
    EXPECT_FALSE(pBeats->getDownbeatIndex().has_value());
    // Without a fallback, the first marker is used
    EXPECT_EQ(0, pBeats->resolveDownbeatIndex(audio::kInvalidFramePos));
    // Otherwise the beat closest to the fallback position
    EXPECT_EQ(3, pBeats->resolveDownbeatIndex(beatPosition(3) + 100));
    EXPECT_EQ(3, pBeats->resolveDownbeatIndex(beatPosition(3) - 100));
    EXPECT_EQ(-2, pBeats->resolveDownbeatIndex(beatPosition(-2) + kBeatLength * 0.4));
}

TEST(BarsTest, SetDownbeatOverridesFallback) {
    const auto pBeats = makeConstTempoBeats()->trySetDownbeatIndex(5);
    ASSERT_TRUE(pBeats.has_value());
    EXPECT_EQ(5, (*pBeats)->getDownbeatIndex());
    EXPECT_EQ(5, (*pBeats)->resolveDownbeatIndex(beatPosition(3)));
    EXPECT_NE(*makeConstTempoBeats(), **pBeats);

    const auto pUnset = (*pBeats)->trySetDownbeatIndex(std::nullopt);
    ASSERT_TRUE(pUnset.has_value());
    EXPECT_FALSE((*pUnset)->getDownbeatIndex().has_value());
}

TEST(BarsTest, FindClosestBeatIndex) {
    const auto pBeats = makeConstTempoBeats();
    EXPECT_EQ(0, pBeats->findClosestBeatIndex(kFirstBeat));
    EXPECT_EQ(1, pBeats->findClosestBeatIndex(beatPosition(1) - kBeatLength * 0.4));
    EXPECT_EQ(1, pBeats->findClosestBeatIndex(beatPosition(1) + kBeatLength * 0.4));
    EXPECT_EQ(-1, pBeats->findClosestBeatIndex(beatPosition(-1) + 10));
    EXPECT_FALSE(pBeats->findClosestBeatIndex(audio::kInvalidFramePos).has_value());

    const auto pBeatMap = makeBeatMap();
    EXPECT_EQ(8, pBeatMap->findClosestBeatIndex(beatPosition(8)));
    // After the tempo change, beats are twice as long
    EXPECT_EQ(9, pBeatMap->findClosestBeatIndex(beatPosition(8) + 2 * kBeatLength + 10));
    EXPECT_EQ(10, pBeatMap->findClosestBeatIndex(beatPosition(8) + 4 * kBeatLength - 10));
}

TEST(BarsTest, SerializeConstTempoDownbeat) {
    const auto pBeats = *makeConstTempoBeats()->trySetDownbeatIndex(-3);
    const auto pDeserialized = roundTrip(pBeats);
    ASSERT_TRUE(pDeserialized);
    EXPECT_EQ(-3, pDeserialized->getDownbeatIndex());
    EXPECT_EQ(*pBeats, *pDeserialized);

    // Beats without a downbeat stay without one
    const auto pDeserializedUnset = roundTrip(makeConstTempoBeats());
    ASSERT_TRUE(pDeserializedUnset);
    EXPECT_FALSE(pDeserializedUnset->getDownbeatIndex().has_value());
}

TEST(BarsTest, SerializeBeatMapDownbeat) {
    const auto pBeats = *makeBeatMap()->trySetDownbeatIndex(10);
    ASSERT_FALSE(pBeats->hasConstantTempo());
    const auto pDeserialized = roundTrip(pBeats);
    ASSERT_TRUE(pDeserialized);
    EXPECT_EQ(10, pDeserialized->getDownbeatIndex());
    // The index must still refer to the same beat
    const auto downbeat = *(pBeats->cfirstmarker() + 10);
    const auto deserializedDownbeat = *(pDeserialized->cfirstmarker() + 10);
    EXPECT_NEAR(downbeat.value(), deserializedDownbeat.value(), 1.0);
}

TEST(BarsTest, TranslateKeepsDownbeatIndex) {
    const auto pBeats = *makeConstTempoBeats()->trySetDownbeatIndex(2);
    const auto pTranslated = pBeats->tryTranslate(500);
    ASSERT_TRUE(pTranslated.has_value());
    EXPECT_EQ(2, (*pTranslated)->getDownbeatIndex());

    const auto pHalfTranslated = pBeats->tryTranslateBeats(0.5);
    ASSERT_TRUE(pHalfTranslated.has_value());
    EXPECT_EQ(2, (*pHalfTranslated)->getDownbeatIndex());
}

TEST(BarsTest, ScaleKeepsDownbeatPosition) {
    const auto pBeats = *makeConstTempoBeats()->trySetDownbeatIndex(3);
    const auto downbeat = beatPosition(3);

    const auto pDoubled = pBeats->tryScale(Beats::BpmScale::Double);
    ASSERT_TRUE(pDoubled.has_value());
    ASSERT_TRUE((*pDoubled)->getDownbeatIndex().has_value());
    EXPECT_EQ(6, (*pDoubled)->getDownbeatIndex());
    EXPECT_NEAR(downbeat.value(),
            (*((*pDoubled)->cfirstmarker() + *(*pDoubled)->getDownbeatIndex())).value(),
            1.0);

    const auto pHalved = (*pDoubled)->tryScale(Beats::BpmScale::Halve);
    ASSERT_TRUE(pHalved.has_value());
    EXPECT_EQ(3, (*pHalved)->getDownbeatIndex());
}

TEST(BarsTest, SetBpmKeepsDownbeatPosition) {
    const auto pBeats = *makeConstTempoBeats()->trySetDownbeatIndex(4);
    const auto downbeat = beatPosition(4);
    // 120 -> 121 BPM: the downbeat is still closest to the 5th beat
    const auto pAdjusted = pBeats->trySetBpm(Bpm(121.0));
    ASSERT_TRUE(pAdjusted.has_value());
    ASSERT_TRUE((*pAdjusted)->getDownbeatIndex().has_value());
    EXPECT_EQ(4, (*pAdjusted)->getDownbeatIndex());
    // 120 -> 240 BPM: twice as many beats
    const auto pDoubled = pBeats->trySetBpm(Bpm(240.0));
    ASSERT_TRUE(pDoubled.has_value());
    EXPECT_EQ(8, (*pDoubled)->getDownbeatIndex());
    EXPECT_NEAR(downbeat.value(),
            (*((*pDoubled)->cfirstmarker() + 8)).value(),
            1.0);
}

class BarsEngineTest : public MockedEngineBackendTest {
  protected:
    void SetUp() override {
        MockedEngineBackendTest::SetUp();
        // Two beats per second. Note that the fake track is only 5 seconds
        // (10 beats) long.
        m_beatLength = m_pTrack1->getSampleRate().value() / 2.0;
        m_pTrack1->trySetBeats(Beats::fromConstTempo(
                m_pTrack1->getSampleRate(), audio::kStartFramePos, Bpm(120.0)));
        m_pBeatInBar = std::make_unique<ControlProxy>(m_sGroup1, "beat_in_bar");
        m_pBarInPhrase = std::make_unique<ControlProxy>(m_sGroup1, "bar_in_phrase");
        m_pBarDistance = std::make_unique<ControlProxy>(m_sGroup1, "bar_distance");
    }

    void seekToBeat(double beats) {
        m_pChannel1->getEngineBuffer()->seekAbs(
                audio::kStartFramePos + beats * m_beatLength);
        ProcessBuffer(); // schedule the seek in a stopped deck
        ProcessBuffer(); // then seek
    }

    double m_beatLength = 0;
    std::unique_ptr<ControlProxy> m_pBeatInBar;
    std::unique_ptr<ControlProxy> m_pBarInPhrase;
    std::unique_ptr<ControlProxy> m_pBarDistance;
};

TEST_F(BarsEngineTest, BarPositionControls) {
    seekToBeat(2.5);
    EXPECT_EQ(3, m_pBeatInBar->get());
    EXPECT_EQ(1, m_pBarInPhrase->get());
    EXPECT_NEAR(2.5 / 4, m_pBarDistance->get(), 0.01);

    seekToBeat(5.25);
    EXPECT_EQ(2, m_pBeatInBar->get());
    EXPECT_EQ(2, m_pBarInPhrase->get());
    EXPECT_NEAR(1.25 / 4, m_pBarDistance->get(), 0.01);

    // Moving the downbeat moves the bars
    m_pTrack1->trySetBeats(*m_pTrack1->getBeats()->trySetDownbeatIndex(1));
    ProcessBuffer();
    EXPECT_EQ(1, m_pBeatInBar->get());
    EXPECT_EQ(2, m_pBarInPhrase->get());
}

TEST_F(BarsEngineTest, NoBeatsNoBarPosition) {
    seekToBeat(2.5);
    EXPECT_EQ(3, m_pBeatInBar->get());
    m_pTrack1->trySetBeats(nullptr);
    ProcessBuffer();
    EXPECT_EQ(0, m_pBeatInBar->get());
    EXPECT_EQ(0, m_pBarInPhrase->get());
    EXPECT_EQ(0, m_pBarDistance->get());
}

TEST_F(BarsEngineTest, IntroStartIsDefaultDownbeat) {
    m_pTrack1->createAndAddCue(CueType::Intro,
            Cue::kNoHotCue,
            audio::kStartFramePos + 3.1 * m_beatLength,
            audio::kInvalidFramePos);
    seekToBeat(3.5);
    EXPECT_EQ(1, m_pBeatInBar->get());
    seekToBeat(2.5);
    EXPECT_EQ(4, m_pBeatInBar->get());
    EXPECT_EQ(4, m_pBarInPhrase->get());
}

TEST_F(BarsEngineTest, DownbeatControls) {
    auto pDownbeatSet = std::make_unique<ControlProxy>(m_sGroup1, "downbeat_set");
    auto pMoveEarlier = std::make_unique<ControlProxy>(m_sGroup1, "downbeat_move_earlier");
    auto pMoveLater = std::make_unique<ControlProxy>(m_sGroup1, "downbeat_move_later");
    auto pMove = std::make_unique<ControlProxy>(m_sGroup1, "downbeat_move");

    seekToBeat(6.2);
    pDownbeatSet->set(1.0);
    pDownbeatSet->set(0.0);
    EXPECT_EQ(6, m_pTrack1->getBeats()->getDownbeatIndex());
    ProcessBuffer();
    EXPECT_EQ(1, m_pBeatInBar->get());

    pMoveLater->set(1.0);
    pMoveLater->set(0.0);
    EXPECT_EQ(7, m_pTrack1->getBeats()->getDownbeatIndex());

    pMoveEarlier->set(1.0);
    pMoveEarlier->set(0.0);
    pMoveEarlier->set(1.0);
    pMoveEarlier->set(0.0);
    EXPECT_EQ(5, m_pTrack1->getBeats()->getDownbeatIndex());

    // Move by a whole bar
    pMove->set(-4.0);
    EXPECT_EQ(1, m_pTrack1->getBeats()->getDownbeatIndex());
    ProcessBuffer();
    // 6.2 beats is the 2nd beat of the 2nd bar if the downbeat is beat 1
    EXPECT_EQ(2, m_pBeatInBar->get());
    EXPECT_EQ(2, m_pBarInPhrase->get());
}

TEST_F(BarsEngineTest, AdjustBeatgridSetsDownbeat) {
    auto pBeatsTranslateCurPos =
            std::make_unique<ControlProxy>(m_sGroup1, "beats_translate_curpos");
    auto pBeatsUndo = std::make_unique<ControlProxy>(m_sGroup1, "beats_undo_adjustment");

    seekToBeat(6.2);
    const auto playPosition = m_pChannel1->getEngineBuffer()->getExactPlayPos();
    // Beats changes in quick succession are combined into a single undo step,
    // so wait until the change done in SetUp() is old enough.
    QTest::qSleep(810);

    pBeatsTranslateCurPos->set(1.0);
    pBeatsTranslateCurPos->set(0.0);
    const BeatsPointer pBeats = m_pTrack1->getBeats();
    // The closest beat is moved to the play position...
    EXPECT_FRAMEPOS_EQ(playPosition, pBeats->findClosestBeat(playPosition));
    // ...and becomes the first beat of a bar and a phrase
    EXPECT_EQ(6, pBeats->getDownbeatIndex());
    ProcessBuffer();
    EXPECT_EQ(1, m_pBeatInBar->get());
    EXPECT_EQ(1, m_pBarInPhrase->get());

    // A single undo reverts both changes
    pBeatsUndo->set(1.0);
    pBeatsUndo->set(0.0);
    const BeatsPointer pRevertedBeats = m_pTrack1->getBeats();
    EXPECT_FALSE(pRevertedBeats->getDownbeatIndex().has_value());
    EXPECT_FRAMEPOS_EQ(audio::kStartFramePos + 6 * m_beatLength,
            pRevertedBeats->findClosestBeat(playPosition));
}

TEST_F(BarsEngineTest, DownbeatControlsRespectBpmLock) {
    m_pTrack1->setBpmLocked(true);
    seekToBeat(6.2);
    auto pDownbeatSet = std::make_unique<ControlProxy>(m_sGroup1, "downbeat_set");
    pDownbeatSet->set(1.0);
    EXPECT_FALSE(m_pTrack1->getBeats()->getDownbeatIndex().has_value());
}
