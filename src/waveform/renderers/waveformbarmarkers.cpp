#include "waveform/renderers/waveformbarmarkers.h"

#include "audio/frame.h"
#include "track/bars.h"
#include "waveform/waveformwidgetfactory.h"

void WaveformBarMarkers::update(const QString& group, const mixxx::Beats& beats) {
    if (WaveformWidgetFactory::isCreated()) {
        const WaveformWidgetFactory* pFactory = WaveformWidgetFactory::instance();
        m_showBars = pFactory->showBarMarkers();
        m_showPhrases = pFactory->showPhraseMarkers();
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

// static
float WaveformBarMarkers::markOpacity(Type type, float beatSpacing) {
    switch (type) {
    case Type::Phrase:
        return fade(beatSpacing * mixxx::bars::kBeatsPerPhrase,
                kMarkHiddenSpacing,
                kMarkFullSpacing);
    case Type::Bar:
        return fade(beatSpacing * mixxx::bars::kBeatsPerBar,
                kMarkHiddenSpacing,
                kMarkFullSpacing);
    case Type::Beat:
        break;
    }
    return 0.f;
}

// static
QColor WaveformBarMarkers::defaultBarColor() {
    return QColor(0xff, 0x3b, 0x30);
}
