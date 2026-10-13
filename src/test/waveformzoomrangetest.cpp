#include <gtest/gtest.h>

#include "waveform/renderers/waveformwidgetrenderer.h"

namespace {

class WaveformZoomRangeTest : public testing::Test {
  protected:
    void TearDown() override {
        WaveformWidgetRenderer::setZoomRange(
                WaveformWidgetRenderer::s_waveformMinZoom,
                WaveformWidgetRenderer::s_waveformMaxZoom);
    }
};

TEST_F(WaveformZoomRangeTest, DefaultRange) {
    EXPECT_DOUBLE_EQ(1.0, WaveformWidgetRenderer::getZoomRangeMin());
    EXPECT_DOUBLE_EQ(10.0, WaveformWidgetRenderer::getZoomRangeMax());
    EXPECT_DOUBLE_EQ(1.0, WaveformWidgetRenderer::clampZoom(0.5));
    EXPECT_DOUBLE_EQ(10.0, WaveformWidgetRenderer::clampZoom(50.0));
}

TEST_F(WaveformZoomRangeTest, CustomRange) {
    WaveformWidgetRenderer::setZoomRange(0.5, 40.0);
    EXPECT_DOUBLE_EQ(0.5, WaveformWidgetRenderer::getZoomRangeMin());
    EXPECT_DOUBLE_EQ(40.0, WaveformWidgetRenderer::getZoomRangeMax());
    EXPECT_DOUBLE_EQ(0.5, WaveformWidgetRenderer::clampZoom(0.1));
    EXPECT_DOUBLE_EQ(25.0, WaveformWidgetRenderer::clampZoom(25.0));
    EXPECT_DOUBLE_EQ(40.0, WaveformWidgetRenderer::clampZoom(100.0));
}

TEST_F(WaveformZoomRangeTest, SwappedRange) {
    WaveformWidgetRenderer::setZoomRange(20.0, 2.0);
    EXPECT_DOUBLE_EQ(2.0, WaveformWidgetRenderer::getZoomRangeMin());
    EXPECT_DOUBLE_EQ(20.0, WaveformWidgetRenderer::getZoomRangeMax());
}

TEST_F(WaveformZoomRangeTest, RangeIsLimited) {
    WaveformWidgetRenderer::setZoomRange(0.0001, 1e6);
    EXPECT_DOUBLE_EQ(WaveformWidgetRenderer::s_waveformZoomLimitMin,
            WaveformWidgetRenderer::getZoomRangeMin());
    EXPECT_DOUBLE_EQ(WaveformWidgetRenderer::s_waveformZoomLimitMax,
            WaveformWidgetRenderer::getZoomRangeMax());
}

} // namespace
