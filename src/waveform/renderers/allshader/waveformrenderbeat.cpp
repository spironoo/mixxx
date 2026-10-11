#include "waveform/renderers/allshader/waveformrenderbeat.h"

#include <QDomNode>
#include <algorithm>
#include <cmath>

#include "engine/engine.h"
#include "moc_waveformrenderbeat.cpp"
#include "rendergraph/geometry.h"
#include "rendergraph/material/rgbamaterial.h"
#include "rendergraph/vertexupdaters/rgbavertexupdater.h"
#include "skin/legacy/skincontext.h"
#include "track/track.h"
#include "util/colorcomponents.h"
#include "waveform/renderers/waveformwidgetrenderer.h"
#include "waveform/waveform.h"
#include "waveform/waveformwidgetfactory.h"
#include "widget/wskincolor.h"

using namespace rendergraph;

namespace {

constexpr int kNumVerticesPerRectangle = 6; // 2 triangles

QVector4D toVector4D(const QColor& color) {
    float r, g, b, a;
    getRgbF(color, &r, &g, &b, &a);
    return {r, g, b, a};
}

} // namespace

namespace allshader {

WaveformRenderBeat::WaveformRenderBeat(WaveformWidgetRenderer* waveformWidget,
        ::WaveformRendererAbstract::PositionSource type)
        : ::WaveformRendererAbstract(waveformWidget),
          m_isSlipRenderer(type == ::WaveformRendererAbstract::Slip) {
    initForRectangles<RGBAMaterial>(0);
    setUsePreprocess(true);
}

void WaveformRenderBeat::setup(const QDomNode& node, const SkinContext& skinContext) {
    m_color = QColor(skinContext.selectString(node, QStringLiteral("BeatColor")));
    m_color = WSkinColor::getCorrectColor(m_color).toRgb();
    const QString barColorName = skinContext.selectString(node, QStringLiteral("BarColor"));
    m_barColor = barColorName.isEmpty()
            ? QColor()
            : WSkinColor::getCorrectColor(QColor(barColorName)).toRgb();
}

void WaveformRenderBeat::draw(QPainter* painter, QPaintEvent* event) {
    Q_UNUSED(painter);
    Q_UNUSED(event);
    DEBUG_ASSERT(false);
}

void WaveformRenderBeat::preprocess() {
    if (!preprocessInner()) {
        geometry().allocate(0);
        markDirtyGeometry();
    }
}

bool WaveformRenderBeat::preprocessInner() {
    const TrackPointer trackInfo = m_waveformRenderer->getTrackInfo();

    if (!trackInfo || (m_isSlipRenderer && !m_waveformRenderer->isSlipActive())) {
        return false;
    }

    const bool isStemTrack = trackInfo && trackInfo->hasStem() &&
            trackInfo->getWaveform() && trackInfo->getWaveform()->hasStem();
    const bool splitStemTracks = isStemTrack && WaveformWidgetFactory::isCreated() &&
            WaveformWidgetFactory::instance()->isStemSplitTracks();

    auto positionType = m_isSlipRenderer ? ::WaveformRendererAbstract::Slip
                                         : ::WaveformRendererAbstract::Play;

    mixxx::BeatsPointer trackBeats = trackInfo->getBeats();
    if (!trackBeats) {
        return false;
    }

#ifndef __SCENEGRAPH__
    int alpha = m_waveformRenderer->getBeatGridAlpha();
    if (alpha == 0) {
        return false;
    }
    m_color.setAlphaF(alpha / 100.0f);
#endif

    if (!m_color.alpha()) {
        // Don't render the beatgrid lines is there are fully transparent
        return false;
    }

    m_barMarkers.update(m_waveformRenderer->getGroup(), *trackBeats);
    QColor barColor = m_barColor.isValid()
            ? m_barColor
            : WaveformBarMarkers::defaultBarColor();
    barColor.setAlphaF(m_color.alphaF());
    QColor markOutlineColor = WaveformBarMarkers::markOutlineColor();
    markOutlineColor.setAlphaF(m_color.alphaF());
    const QVector4D beatRgba = toVector4D(m_color);
    const QVector4D barRgba = toVector4D(barColor);
    const QVector4D markOutlineRgba = toVector4D(markOutlineColor);

    const float devicePixelRatio = m_waveformRenderer->getDevicePixelRatio();

    const double trackSamples = m_waveformRenderer->getTrackSamples();
    if (trackSamples <= 0.0) {
        return false;
    }

    const double firstDisplayedPosition =
            m_waveformRenderer->getFirstDisplayedPosition(positionType);
    const double lastDisplayedPosition =
            m_waveformRenderer->getLastDisplayedPosition(positionType);

    const auto startPosition = mixxx::audio::FramePos::fromEngineSamplePos(
            firstDisplayedPosition * trackSamples);
    const auto endPosition = mixxx::audio::FramePos::fromEngineSamplePos(
            lastDisplayedPosition * trackSamples);

    if (!startPosition.isValid() || !endPosition.isValid()) {
        return false;
    }

    const float rendererBreadth = m_waveformRenderer->getBreadth();

    const auto firstBeatIt = trackBeats->iteratorFrom(startPosition);
    if (firstBeatIt == trackBeats->cbegin() || firstBeatIt == trackBeats->cend()) {
        return false;
    }
    // Beat indices are only needed for bar and phrase markers. Looking them up
    // can be expensive for beat maps, so it's only done for the first beat.
    const int firstBeatIndex = m_barMarkers.isEnabled()
            ? trackBeats->beatIndexOf(firstBeatIt)
            : 0;
    auto markerType = [&](int beatIndex) {
        return m_barMarkers.isEnabled() ? m_barMarkers.typeOf(beatIndex)
                                        : WaveformBarMarkers::Type::Beat;
    };

    const bool splitStemBoxes = m_isSlipRenderer && splitStemTracks;
    const int numBoxesPerBeat = splitStemBoxes ? mixxx::kMaxSupportedStems : 1;
    // Like in rekordbox, the first beats of bars and phrases get marks at
    // both edges of the waveform, each drawn as an outline and a fill
    // rectangle. The slip renderer only draws the upper half of the waveform,
    // so it only gets the marks at the top.
    const int numMarksPerBar = m_isSlipRenderer ? 1 : 2;
    const int numVerticesPerMark = 2 * kNumVerticesPerRectangle;
    const float markBreadth = m_isSlipRenderer ? rendererBreadth / 2.f : rendererBreadth;

    // Count the vertices needed for the beats in the range to reserve space.
    // Note that we could also use
    //   int numBearsInRange = trackBeats->numBeatsInRange(startPosition, endPosition);
    // for this, but there have been reports of that method failing with a DEBUG_ASSERT.
    int reserved = 0;
    int beatIndex = firstBeatIndex;
    for (auto it = firstBeatIt;
            it != trackBeats->cend() && *it <= endPosition;
            ++it, ++beatIndex) {
        reserved += kNumVerticesPerRectangle * numBoxesPerBeat;
        if (markerType(beatIndex) != WaveformBarMarkers::Type::Beat) {
            reserved += numVerticesPerMark * numMarksPerBar;
        }
    }
    geometry().allocate(reserved);

    RGBAVertexUpdater vertexUpdater{geometry().vertexDataAs<Geometry::RGBAColoredPoint2D>()};

    const float boxBreadth = splitStemTracks
            ? rendererBreadth / static_cast<float>(mixxx::kMaxSupportedStems)
            : rendererBreadth;
    constexpr float markWidth = WaveformBarMarkers::kMarkWidthFactor;
    constexpr float markOutlineWidth = WaveformBarMarkers::kMarkOutlineWidthFactor;

    beatIndex = firstBeatIndex;
    for (auto it = firstBeatIt;
            it != trackBeats->cend() && *it <= endPosition;
            ++it, ++beatIndex) {
        double beatPosition = it->toEngineSamplePos();
        double xBeatPoint =
                m_waveformRenderer->transformSamplePositionInRendererWorld(
                        beatPosition, positionType);

        xBeatPoint = qRound(xBeatPoint * devicePixelRatio) / devicePixelRatio;

        const float x1 = static_cast<float>(xBeatPoint);
        const float x2 = x1 + 1.f;

        if (splitStemBoxes) {
            for (int stemIdx = 0; stemIdx < mixxx::kMaxSupportedStems; ++stemIdx) {
                const float posy1 = stemIdx * boxBreadth;
                const float posy2 = posy1 + boxBreadth / 2.f;
                vertexUpdater.addRectangle({x1, posy1}, {x2, posy2}, beatRgba);
            }
        } else {
            vertexUpdater.addRectangle({x1, 0.f},
                    {x2, m_isSlipRenderer ? rendererBreadth / 2 : rendererBreadth},
                    beatRgba);
        }

        const WaveformBarMarkers::Type type = markerType(beatIndex);
        if (type == WaveformBarMarkers::Type::Beat) {
            continue;
        }
        // Center the marks on the beat line and keep them on whole pixels
        const float markX1 = x1 - std::floor((markWidth - 1.f) / 2.f);
        const float markX2 = markX1 + markWidth;
        const float markLength = WaveformBarMarkers::markLength(type, markBreadth);
        vertexUpdater.addRectangle({markX1 - markOutlineWidth, 0.f},
                {markX2 + markOutlineWidth, markLength + markOutlineWidth},
                markOutlineRgba);
        vertexUpdater.addRectangle({markX1, 0.f}, {markX2, markLength}, barRgba);
        if (!m_isSlipRenderer) {
            vertexUpdater.addRectangle(
                    {markX1 - markOutlineWidth,
                            rendererBreadth - markLength - markOutlineWidth},
                    {markX2 + markOutlineWidth, rendererBreadth},
                    markOutlineRgba);
            vertexUpdater.addRectangle({markX1, rendererBreadth - markLength},
                    {markX2, rendererBreadth},
                    barRgba);
        }
    }
    markDirtyGeometry();

    DEBUG_ASSERT(reserved == vertexUpdater.index());

    markDirtyMaterial();

    return true;
}

} // namespace allshader
