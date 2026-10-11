#pragma once

#include <QColor>
#include <QString>
#include <algorithm>
#include <cmath>
#include <optional>

#include "control/pollingcontrolproxy.h"
#include "track/beats.h"

/// Shared logic for the beat renderers to highlight the first beat of each
/// bar and of each phrase.
///
/// The markers follow rekordbox: every beat, including the first beat of a
/// bar, is drawn as a regular beat line, and the first beat of each bar gets
/// red marks at both edges of the waveform. Phrase markers are bar markers
/// with longer marks.
///
/// Unlike in rekordbox, the waveform reaches the edges, so the marks get a
/// dark outline to stay visible on top of it.
class WaveformBarMarkers {
  public:
    enum class Type {
        Beat,
        Bar,
        Phrase,
    };

    /// Width of the marks relative to the beat line width
    static constexpr float kMarkWidthFactor = 5.f;
    /// Width of the dark outline around the marks relative to the beat line
    /// width
    static constexpr float kMarkOutlineWidthFactor = 1.f;

    /// Length of the marks at each edge of the waveform, measured from the
    /// edge. `breadth` is the height of a horizontal waveform.
    static float markLength(Type type, float breadth) {
        const float barMarkLength = std::clamp(std::round(breadth / 8.f), 4.f, 14.f);
        switch (type) {
        case Type::Phrase:
            return std::min(std::round(1.6f * barMarkLength), breadth / 2.f);
        case Type::Bar:
            return barMarkLength;
        case Type::Beat:
            break;
        }
        return 0.f;
    }

    /// Reads the current preferences. Must be called once per frame before
    /// using `typeOf()`.
    void update(const QString& group, const mixxx::Beats& beats);

    bool isEnabled() const {
        return m_showBars || m_showPhrases;
    }

    /// Returns how the beat with index `beatIndex` should be drawn.
    Type typeOf(int beatIndex) const;

    /// Default color for skins that don't define a BarColor
    static QColor defaultBarColor();
    /// Color of the outline around the marks
    static QColor markOutlineColor() {
        return QColor(Qt::black);
    }

  private:
    QString m_group;
    std::optional<PollingControlProxy> m_pIntroStartPosition;
    bool m_showBars = true;
    bool m_showPhrases = true;
    int m_downbeatIndex = 0;
};
