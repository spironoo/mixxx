#pragma once

#include <QColor>

#include "rendergraph/geometrynode.h"
#include "util/class.h"
#include "waveform/renderers/waveformbarmarkers.h"
#include "waveform/renderers/waveformrendererabstract.h"

class QDomNode;
class SkinContext;

namespace allshader {
class WaveformRenderBeat;
} // namespace allshader

class allshader::WaveformRenderBeat final
        : public QObject,
          public ::WaveformRendererAbstract,
          public rendergraph::GeometryNode {
    Q_OBJECT
  public:
    explicit WaveformRenderBeat(WaveformWidgetRenderer* waveformWidget,
            ::WaveformRendererAbstract::PositionSource type =
                    ::WaveformRendererAbstract::Play);

    // Pure virtual from WaveformRendererAbstract, not used
    void draw(QPainter* painter, QPaintEvent* event) override final;

    void setup(const QDomNode& node, const SkinContext& skinContext) override;

    // Virtuals for rendergraph::Node
    void preprocess() override;

  public slots:
    void setColor(const QColor& color) {
        m_color = color;
    }
    void setBarColor(const QColor& color) {
        m_barColor = color;
    }

  private:
    QColor m_color;
    // Used for bar and phrase markers. If invalid, the default color is used.
    QColor m_barColor;
    bool m_isSlipRenderer;
    WaveformBarMarkers m_barMarkers;

    bool preprocessInner();

    DISALLOW_COPY_AND_ASSIGN(WaveformRenderBeat);
};
