#pragma once

/// Shape of the marks at the edges of the waveform that highlight the first
/// beats of bars and phrases. The values are stored in the user settings.
enum class BarMarkerShape : int {
    /// Narrow rectangles, like in rekordbox
    Rectangle = 0,
    /// Triangles pointing towards the center of the waveform, which are
    /// easier to see
    Triangle = 1,
};
