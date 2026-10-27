#pragma once

#include <QColor>
#include <QPolygonF>

#include "skin/legacy/skincontext.h"
#include "util/class.h"
#include "waveform/renderers/waveformbarmarkers.h"
#include "waveform/renderers/waveformrendererabstract.h"

class WaveformRenderBeat : public WaveformRendererAbstract {
  public:
    explicit WaveformRenderBeat(WaveformWidgetRenderer* waveformWidgetRenderer);
    virtual ~WaveformRenderBeat();

    virtual void setup(const QDomNode& node, const SkinContext& context);
    virtual void draw(QPainter* painter, QPaintEvent* event);

  private:
    QColor m_beatColor;
    // Used for the bar and phrase marks
    QColor m_barColor;
    QVector<QLineF> m_beats;
    // Marks at the edges of the first beats of bars and phrases
    QVector<QRectF> m_marks;
    // The same marks when drawn as triangles, with their outlines
    QVector<QPolygonF> m_triangleMarks;
    QVector<QPolygonF> m_triangleMarkOutlines;
    WaveformBarMarkers m_barMarkers;

    DISALLOW_COPY_AND_ASSIGN(WaveformRenderBeat);
};
