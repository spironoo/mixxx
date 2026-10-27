#include "waveform/renderers/waveformbarmarkers.h"

#include "audio/frame.h"
#include "track/bars.h"
#include "waveform/waveformwidgetfactory.h"

void WaveformBarMarkers::update(const QString& group, const mixxx::Beats& beats) {
    if (WaveformWidgetFactory::isCreated()) {
        const WaveformWidgetFactory* pFactory = WaveformWidgetFactory::instance();
        m_showBars = pFactory->showBarMarkers();
        m_showPhrases = pFactory->showPhraseMarkers();
        m_shape = pFactory->barMarkerShape();
        m_scale = pFactory->barMarkerSize() / 100.f;
    }

    // The downbeat is also needed without markers, to thin out dense beat
    // grids by bars and phrases.
    if (!m_pIntroStartPosition || group != m_group) {
        m_group = group;
        m_pIntroStartPosition.emplace(group,
                QStringLiteral("intro_start_position"),
                ControlFlag::AllowMissingOrInvalid);
    }
    // The intro start is used as the downbeat if none has been set, like in
    // BpmControl.
    const auto introStartPosition = m_pIntroStartPosition->valid()
            ? mixxx::audio::FramePos::fromEngineSamplePosMaybeInvalid(
                      m_pIntroStartPosition->get())
            : mixxx::audio::kInvalidFramePos;
    m_downbeatIndex = beats.resolveDownbeatIndex(introStartPosition);
}

WaveformBarMarkers::Type WaveformBarMarkers::typeOf(int beatIndex) const {
    if (m_showPhrases && mixxx::bars::isPhraseStart(beatIndex, m_downbeatIndex)) {
        return Type::Phrase;
    }
    if (m_showBars && mixxx::bars::isBarStart(beatIndex, m_downbeatIndex)) {
        return Type::Bar;
    }
    return Type::Beat;
}

namespace {

// The opacity drops fastest when the spacing falls below fullSpacing, so the
// lines get out of the way quickly once they get dense, and then trail off
// towards hiddenSpacing.
float fade(float spacing, float hiddenSpacing, float fullSpacing) {
    const float linear = std::clamp(
            (spacing - hiddenSpacing) / (fullSpacing - hiddenSpacing), 0.f, 1.f);
    return linear * linear;
}

} // namespace

float WaveformBarMarkers::lineOpacity(int beatIndex, float beatSpacing) const {
    float spacing = beatSpacing;
    if (mixxx::bars::isPhraseStart(beatIndex, m_downbeatIndex)) {
        spacing *= mixxx::bars::kBeatsPerPhrase;
    } else if (mixxx::bars::isBarStart(beatIndex, m_downbeatIndex)) {
        spacing *= mixxx::bars::kBeatsPerBar;
    }
    return fade(spacing, kLineHiddenSpacing, kLineFullSpacing);
}

float WaveformBarMarkers::markOpacity(Type type, float beatSpacing) const {
    const float hiddenSpacing = m_scale * kMarkHiddenSpacing;
    const float fullSpacing = m_scale * kMarkFullSpacing;
    switch (type) {
    case Type::Phrase:
        return fade(beatSpacing * mixxx::bars::kBeatsPerPhrase, hiddenSpacing, fullSpacing);
    case Type::Bar:
        return fade(beatSpacing * mixxx::bars::kBeatsPerBar, hiddenSpacing, fullSpacing);
    case Type::Beat:
        break;
    }
    return 0.f;
}

WaveformBarMarkers::TriangleMark WaveformBarMarkers::triangleMark(
        Type type, float breadth, float outlineWidth) const {
    // Equilateral triangles have a 60 degree angle at the tip, so each side
    // is 30 degrees off the vertical.
    constexpr float kSin30 = 0.5f;
    constexpr float kCos30 = 0.8660254f;
    constexpr float kTan30 = kSin30 / kCos30;
    const float length = markLength(type, breadth);
    const float halfWidth = length * kTan30;
    // Moving the sides outwards by outlineWidth moves them sideways by
    // outlineWidth / cos(30) and the tip by outlineWidth / sin(30).
    return TriangleMark{
            halfWidth,
            length,
            halfWidth + outlineWidth / kCos30,
            length + outlineWidth / kSin30,
    };
}

// static
QColor WaveformBarMarkers::defaultBarColor() {
    return QColor(0xff, 0x3b, 0x30);
}
