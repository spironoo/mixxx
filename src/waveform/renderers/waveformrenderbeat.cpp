#include "waveform/renderers/waveformrenderbeat.h"

#include <QPainter>
#include <algorithm>

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
    const double markWidth = WaveformBarMarkers::kMarkWidthFactor * beatLineWidth;

    m_barMarkers.update(m_waveformRenderer->getGroup(), *trackBeats);
    // Looking up beat indices can be expensive for beat maps, so it's only
    // done for the first beat.
    int beatIndex = m_barMarkers.isEnabled() ? trackBeats->beatIndexOf(it) : 0;

    m_beats.clear();
    m_marks.clear();
    for (; it != trackBeats->cend() && *it <= endPosition; ++it, ++beatIndex) {
        double beatPosition = it->toEngineSamplePos();
        double xBeatPoint =
                m_waveformRenderer->transformSamplePositionInRendererWorld(beatPosition);

        xBeatPoint = qRound(xBeatPoint * devicePixelRatio) / devicePixelRatio;

        if (orientation == Qt::Horizontal) {
            m_beats.append(QLineF(xBeatPoint, 0.0f, xBeatPoint, rendererHeight));
        } else {
            m_beats.append(QLineF(0.0f, xBeatPoint, rendererWidth, xBeatPoint));
        }

        // Like in rekordbox, the first beats of bars and phrases get marks
        // at both edges of the waveform.
        const WaveformBarMarkers::Type type = m_barMarkers.isEnabled()
                ? m_barMarkers.typeOf(beatIndex)
                : WaveformBarMarkers::Type::Beat;
        if (type == WaveformBarMarkers::Type::Beat) {
            continue;
        }
        const double markLength = WaveformBarMarkers::markLength(type, breadth);
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

    if (!m_marks.isEmpty()) {
        painter->setPen(Qt::NoPen);
        // The outline keeps the marks visible on top of the waveform
        QColor markOutlineColor = WaveformBarMarkers::markOutlineColor();
        markOutlineColor.setAlphaF(m_beatColor.alphaF());
        painter->setBrush(markOutlineColor);
        const qreal outlineWidth = WaveformBarMarkers::kMarkOutlineWidthFactor * beatLineWidth;
        for (const QRectF& mark : std::as_const(m_marks)) {
            painter->drawRect(mark.adjusted(
                    -outlineWidth, -outlineWidth, outlineWidth, outlineWidth));
        }
        painter->setBrush(m_barColor);
        painter->drawRects(m_marks.constData(), static_cast<int>(m_marks.size()));
    }
}
