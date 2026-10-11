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
    if (!isEnabled()) {
        return;
    }

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

// static
QColor WaveformBarMarkers::defaultBarColor() {
    return QColor(0xff, 0x3b, 0x30);
}
