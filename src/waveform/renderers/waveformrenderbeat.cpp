#include "waveform/renderers/waveformrenderbeat.h"

#include <QPainter>
#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <utility>

#include "track/track.h"
#include "util/painterscope.h"
#include "waveform/renderers/waveformwidgetrenderer.h"
#include "widget/wskincolor.h"

class QPaintEvent;

WaveformRenderBeat::WaveformRenderBeat(WaveformWidgetRenderer* waveformWidgetRenderer)
        : WaveformRendererAbstract(waveformWidgetRenderer) {
    m_beats.reserve(128);
    m_marks.reserve(64);
}

WaveformRenderBeat::~WaveformRenderBeat() {
}

void WaveformRenderBeat::setup(const QDomNode& node, const SkinContext& context) {
    m_beatColor = QColor(context.selectString(node, "BeatColor"));
    m_beatColor = WSkinColor::getCorrectColor(m_beatColor).toRgb();
    const QString barColorName = context.selectString(node, QStringLiteral("BarColor"));
    m_barColor = barColorName.isEmpty()
            ? WaveformBarMarkers::defaultBarColor()
            : WSkinColor::getCorrectColor(QColor(barColorName)).toRgb();
}

void WaveformRenderBeat::draw(QPainter* painter, QPaintEvent* /*event*/) {
    TrackPointer pTrackInfo = m_waveformRenderer->getTrackInfo();

    if (!pTrackInfo) {
        return;
    }

    mixxx::BeatsPointer trackBeats = pTrackInfo->getBeats();
    if (!trackBeats) {
        return;
    }

    int alpha = m_waveformRenderer->getBeatGridAlpha();
    if (alpha == 0) {
        return;
    }
#ifdef MIXXX_USE_QOPENGL
    // Using alpha transparency with drawLines causes a graphical issue when
    // drawing with QPainter on the QOpenGLWindow: instead of individual lines
    // a large rectangle encompassing all beatlines is drawn.
    m_beatColor.setAlphaF(1.f);
#else
    m_beatColor.setAlphaF(alpha/100.0);
#endif
    m_barColor.setAlphaF(m_beatColor.alphaF());

    const double trackSamples = m_waveformRenderer->getTrackSamples();
    if (trackSamples <= 0) {
        return;
    }

    const float devicePixelRatio = m_waveformRenderer->getDevicePixelRatio();

    const double firstDisplayedPosition =
            m_waveformRenderer->getFirstDisplayedPosition();
    const double lastDisplayedPosition =
            m_waveformRenderer->getLastDisplayedPosition();

    // qDebug() << "trackSamples" << trackSamples
    //          << "firstDisplayedPosition" << firstDisplayedPosition
    //          << "lastDisplayedPosition" << lastDisplayedPosition;

    const auto startPosition = mixxx::audio::FramePos::fromEngineSamplePos(
            firstDisplayedPosition * trackSamples);
    const auto endPosition = mixxx::audio::FramePos::fromEngineSamplePos(
            lastDisplayedPosition * trackSamples);
    auto it = trackBeats->iteratorFrom(startPosition);

    // if no beat do not waste time saving/restoring painter
    if (it == trackBeats->cend() || *it > endPosition) {
        return;
    }

    // Only happens for positions far outside of the track. Beat indices
    // can't be looked up for this iterator.
    if (it == trackBeats->cbegin()) {
        return;
    }

    PainterScope PainterScope(painter);

    painter->setRenderHint(QPainter::Antialiasing);

    const double beatLineWidth = std::max(1.0, scaleFactor());
    QPen beatPen(m_beatColor);
    beatPen.setWidthF(beatLineWidth);
    painter->setPen(beatPen);

    const Qt::Orientation orientation = m_waveformRenderer->getOrientation();
    const float rendererWidth = m_waveformRenderer->getWidth();
    const float rendererHeight = m_waveformRenderer->getHeight();
    const float breadth = orientation == Qt::Horizontal ? rendererHeight : rendererWidth;
    m_barMarkers.update(m_waveformRenderer->getGroup(), *trackBeats);
    const double markWidth = m_barMarkers.rectangleMarkWidth(static_cast<float>(beatLineWidth));

    // When zoomed out, the lines and marks of beats that are too close to
    // each other are hidden. Unlike the allshader renderer, this renderer
    // can't always draw transparent lines (see above), so they aren't faded.
    float beatSpacing = std::numeric_limits<float>::max();
    if (const auto nextIt = std::next(it); nextIt != trackBeats->cend()) {
        beatSpacing = static_cast<float>(std::abs(
                m_waveformRenderer->transformSamplePositionInRendererWorld(
                        nextIt->toEngineSamplePos()) -
                m_waveformRenderer->transformSamplePositionInRendererWorld(
                        it->toEngineSamplePos())));
    }
    const bool thinOutLines = beatSpacing < WaveformBarMarkers::kLineFullSpacing;
    constexpr float kMinOpacity = 0.5f;

    // Looking up beat indices can be expensive for beat maps, so it's only
    // done for the first beat.
    int beatIndex = m_barMarkers.needsBeatIndices(beatSpacing) ? trackBeats->beatIndexOf(it) : 0;

    const bool triangleMarks = m_barMarkers.shape() == BarMarkerShape::Triangle;
    const double outlineWidth = WaveformBarMarkers::kMarkOutlineWidthFactor * beatLineWidth;
    // Maps a position along the waveform and a distance from its top (or
    // left) edge to a point on the screen.
    auto toPoint = [orientation](double along, double across) {
        return orientation == Qt::Horizontal ? QPointF(along, across)
                                             : QPointF(across, along);
    };
    // A triangle with its base on the edge at `edge`, pointing towards the
    // center of the waveform, which is in the direction of `sign`.
    auto triangle = [&](double center, double edge, double sign, double halfWidth, double length) {
        return QPolygonF({toPoint(center - halfWidth, edge),
                toPoint(center + halfWidth, edge),
                toPoint(center, edge + sign * length)});
    };

    m_beats.clear();
    m_marks.clear();
    m_triangleMarks.clear();
    m_triangleMarkOutlines.clear();
    for (; it != trackBeats->cend() && *it <= endPosition; ++it, ++beatIndex) {
        double beatPosition = it->toEngineSamplePos();
        double xBeatPoint =
                m_waveformRenderer->transformSamplePositionInRendererWorld(beatPosition);

        xBeatPoint = qRound(xBeatPoint * devicePixelRatio) / devicePixelRatio;

        if (!thinOutLines || m_barMarkers.lineOpacity(beatIndex, beatSpacing) >= kMinOpacity) {
            if (orientation == Qt::Horizontal) {
                m_beats.append(QLineF(xBeatPoint, 0.0f, xBeatPoint, rendererHeight));
            } else {
                m_beats.append(QLineF(0.0f, xBeatPoint, rendererWidth, xBeatPoint));
            }
        }

        // Like in rekordbox, the first beats of bars and phrases get marks
        // at both edges of the waveform.
        const WaveformBarMarkers::Type type = m_barMarkers.isEnabled()
                ? m_barMarkers.typeOf(beatIndex)
                : WaveformBarMarkers::Type::Beat;
        if (m_barMarkers.markOpacity(type, beatSpacing) < kMinOpacity) {
            continue;
        }
        if (triangleMarks) {
            const auto mark = m_barMarkers.triangleMark(
                    type, breadth, static_cast<float>(outlineWidth));
            for (const auto [edge, sign] : {std::pair{0.0, 1.0}, std::pair{double{breadth}, -1.0}}) {
                m_triangleMarkOutlines.append(triangle(
                        xBeatPoint, edge, sign, mark.outlineHalfWidth, mark.outlineLength));
                m_triangleMarks.append(
                        triangle(xBeatPoint, edge, sign, mark.halfWidth, mark.length));
            }
            continue;
        }
        const double markLength = m_barMarkers.markLength(type, breadth);
        const double markStart = xBeatPoint - markWidth / 2;
        if (orientation == Qt::Horizontal) {
            m_marks.append(QRectF(markStart, 0.0, markWidth, markLength));
            m_marks.append(QRectF(markStart, breadth - markLength, markWidth, markLength));
        } else {
            m_marks.append(QRectF(0.0, markStart, markLength, markWidth));
            m_marks.append(QRectF(breadth - markLength, markStart, markLength, markWidth));
        }
    }

    // Make sure to use constData to prevent detaches!
    painter->drawLines(m_beats.constData(), static_cast<int>(m_beats.size()));

    // The outline keeps the marks visible on top of the waveform
    QColor markOutlineColor = WaveformBarMarkers::markOutlineColor();
    markOutlineColor.setAlphaF(m_beatColor.alphaF());
    if (!m_triangleMarks.isEmpty()) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(markOutlineColor);
        for (const QPolygonF& outline : std::as_const(m_triangleMarkOutlines)) {
            painter->drawPolygon(outline);
        }
        painter->setBrush(m_barColor);
        for (const QPolygonF& mark : std::as_const(m_triangleMarks)) {
            painter->drawPolygon(mark);
        }
    }
    if (!m_marks.isEmpty()) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(markOutlineColor);
        for (const QRectF& mark : std::as_const(m_marks)) {
            painter->drawRect(mark.adjusted(
                    -outlineWidth, -outlineWidth, outlineWidth, outlineWidth));
        }
        painter->setBrush(m_barColor);
        painter->drawRects(m_marks.constData(), static_cast<int>(m_marks.size()));
    }
}
