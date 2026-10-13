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
///
/// When the waveform is zoomed out so far that the beats get too close to
/// each other, the beat lines would cover the waveform. The lines then fade
/// out, starting with the lines of beats within a bar, followed by the lines
/// of the first beats of bars, so that only the first beats of phrases are
/// left. The marks fade out the same way.
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

    /// Spacing in pixels from which lines are drawn fully, and below which
    /// they are hidden.
    static constexpr float kLineFullSpacing = 11.f;
    static constexpr float kLineHiddenSpacing = 4.4f;
    /// Same for the marks, which are wider than the lines.
    static constexpr float kMarkFullSpacing = 30.8f;
    static constexpr float kMarkHiddenSpacing = 15.4f;

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

    /// Whether the beat indices are needed for `typeOf()` or `lineOpacity()`
    /// when beats are `beatSpacing` pixels apart. Looking them up can be
    /// expensive for beat maps.
    bool needsBeatIndices(float beatSpacing) const {
        return isEnabled() || beatSpacing < kLineFullSpacing;
    }

    /// Opacity from 0 to 1 of the beat line of beat `beatIndex`, when beats
    /// are `beatSpacing` pixels apart. This doesn't depend on the bar and
    /// phrase preferences, so dense beat grids are thinned out by bars and
    /// phrases even without markers.
    float lineOpacity(int beatIndex, float beatSpacing) const;

    /// Opacity from 0 to 1 of the marks of a beat of type `type`, when beats
    /// are `beatSpacing` pixels apart.
    static float markOpacity(Type type, float beatSpacing);

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
