#pragma once

#include <QColor>
#include <QString>
#include <algorithm>
#include <cmath>
#include <optional>

#include "control/pollingcontrolproxy.h"
#include "track/beats.h"
#include "waveform/renderers/barmarkershape.h"

/// Shared logic for the beat renderers to highlight the first beat of each
/// bar and of each phrase.
///
/// The markers follow rekordbox: every beat, including the first beat of a
/// bar, is drawn as a regular beat line, and the first beat of each bar gets
/// red marks at both edges of the waveform. Phrase markers are bar markers
/// with longer marks.
///
/// Unlike in rekordbox, the waveform reaches the edges, so the marks get a
/// dark outline to stay visible on top of it. Instead of the narrow
/// rectangles, the marks can also be drawn as triangles, see BarMarkerShape.
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

    /// Width of the rectangle marks relative to the beat line width, at a
    /// scale of 1
    static constexpr float kMarkWidthFactor = 5.f;
    /// Width of the dark outline around the marks relative to the beat line
    /// width
    static constexpr float kMarkOutlineWidthFactor = 1.f;

    /// Spacing in pixels from which lines are drawn fully, and below which
    /// they are hidden.
    static constexpr float kLineFullSpacing = 11.f;
    static constexpr float kLineHiddenSpacing = 4.4f;
    /// Same for the marks at a scale of 1, which are wider than the lines.
    /// Larger marks fade out earlier.
    static constexpr float kMarkFullSpacing = 30.8f;
    static constexpr float kMarkHiddenSpacing = 15.4f;

    /// Size of the marks relative to their default size, see setScale()
    float scale() const {
        return m_scale;
    }
    /// Sets the size of the marks relative to their default size. update()
    /// sets it from the preferences.
    void setScale(float scale) {
        m_scale = scale;
    }
    /// Sets the shape of the marks. update() sets it from the preferences.
    void setShape(BarMarkerShape shape) {
        m_shape = shape;
    }

    /// Size of the triangle marks relative to the rectangle marks at the
    /// default size. Triangles are wider than rectangles, so they're drawn
    /// smaller.
    static constexpr float kTriangleScaleFactor = 0.75f;
    /// Largest size of the triangle marks relative to the rectangle marks at
    /// the default size
    static constexpr float kTriangleScaleMax = 2.f;

    /// Size of the triangle marks relative to the rectangle marks at the
    /// default size, for the current scale()
    float triangleScale() const {
        return std::min(m_scale * kTriangleScaleFactor, kTriangleScaleMax);
    }

    /// Length of the rectangle marks at each edge of the waveform, measured
    /// from the edge. `breadth` is the height of a horizontal waveform. The
    /// marks never reach past the center of the waveform.
    float markLength(Type type, float breadth) const {
        return markLengthAtScale(type, breadth, m_scale);
    }

    /// Width of the rectangle marks in whole multiples of `beatLineWidth`
    float rectangleMarkWidth(float beatLineWidth) const {
        return std::max(1.f, std::round(m_scale * kMarkWidthFactor)) * beatLineWidth;
    }

    /// A triangle mark at the top edge of a horizontal waveform. The base is
    /// on the edge and centered on the beat line, the tip points towards the
    /// center of the waveform. The outline is a larger triangle behind it.
    struct TriangleMark {
        /// Half the width of the base
        float halfWidth;
        /// Distance of the tip from the edge
        float length;
        float outlineHalfWidth;
        float outlineLength;
    };

    /// Returns the size of a triangle mark of type `type`, with an outline
    /// that is `outlineWidth` wide on all sides. The triangles are
    /// equilateral and as long as rectangle marks at triangleScale().
    TriangleMark triangleMark(Type type, float breadth, float outlineWidth) const;

    /// Reads the current preferences. Must be called once per frame before
    /// using `typeOf()`.
    void update(const QString& group, const mixxx::Beats& beats);

    bool isEnabled() const {
        return m_showBars || m_showPhrases;
    }

    BarMarkerShape shape() const {
        return m_shape;
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
    float markOpacity(Type type, float beatSpacing) const;

    /// Default color for skins that don't define a BarColor
    static QColor defaultBarColor();
    /// Color of the outline around the marks
    static QColor markOutlineColor() {
        return QColor(Qt::black);
    }

  private:
    static float markLengthAtScale(Type type, float breadth, float scale) {
        const float barMarkLength = std::min(
                std::round(scale * std::clamp(std::round(breadth / 8.f), 4.f, 14.f)),
                breadth / 2.f);
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

    QString m_group;
    std::optional<PollingControlProxy> m_pIntroStartPosition;
    bool m_showBars = true;
    bool m_showPhrases = true;
    BarMarkerShape m_shape = BarMarkerShape::Rectangle;
    float m_scale = 1.f;
    int m_downbeatIndex = 0;
};
