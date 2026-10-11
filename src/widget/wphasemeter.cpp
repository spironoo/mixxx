#include "widget/wphasemeter.h"

#include <QPainter>
#include <QPainterPath>
#include <QStyleOption>
#include <QStylePainter>
#include <algorithm>
#include <cmath>

#include "control/controlproxy.h"
#include "engine/sync/syncable.h"
#include "mixer/playermanager.h"
#include "moc_wphasemeter.cpp"
#include "skin/legacy/skincontext.h"
#include "track/bars.h"
#include "widget/wskincolor.h"

namespace {

const QString kAppGroup = QStringLiteral("[App]");

// Offsets below this (in beats) are shown as "in phase".
constexpr double kInPhaseThresholdBeats = 1.0 / 16;

// The ring around the reference deck's phrase dot
constexpr double kRingWidth = 1.0;
// Distance from the dot's edge to the ring's outer edge
constexpr double kRingMargin = 1.5;

QColor colorOrDefault(const SkinContext& context,
        const QDomNode& node,
        const QString& name,
        const QColor& defaultColor) {
    const QColor color = context.selectColor(node, name);
    if (!color.isValid()) {
        return defaultColor;
    }
    return WSkinColor::getCorrectColor(color);
}

} // namespace

WPhaseMeter::DeckControls::DeckControls(const QString& group)
        : group(group),
          barDistance(group, QStringLiteral("bar_distance"), ControlFlag::AllowMissingOrInvalid),
          beatInBar(group, QStringLiteral("beat_in_bar"), ControlFlag::AllowMissingOrInvalid),
          barInPhrase(group, QStringLiteral("bar_in_phrase"), ControlFlag::AllowMissingOrInvalid),
          syncMode(group, QStringLiteral("sync_mode"), ControlFlag::AllowMissingOrInvalid),
          play(group, QStringLiteral("play"), ControlFlag::AllowMissingOrInvalid) {
}

bool WPhaseMeter::DeckControls::hasBars() const {
    return beatInBar.get() > 0;
}

double WPhaseMeter::DeckControls::barPosition() const {
    return std::clamp(barDistance.get(), 0.0, 1.0) * mixxx::bars::kBeatsPerBar;
}

WPhaseMeter::WPhaseMeter(QWidget* pParent, const QString& group)
        : WWidget(pParent),
          m_group(group),
          m_deck(group),
          m_numDecks(kAppGroup, QStringLiteral("num_decks"), ControlFlag::AllowMissingOrInvalid),
          m_pGuiTick(new ControlProxy(
                  kAppGroup, QStringLiteral("gui_tick_full_period_s"), this)),
          m_segmentColor(0x33, 0x33, 0x33),
          m_beatColor(0x99, 0x99, 0x99),
          m_downbeatColor(0xff, 0x3b, 0x30),
          m_phraseColor(0xff, 0x3b, 0x30),
          m_referenceColor(0x00, 0xc6, 0xff),
          m_inPhaseColor(0x34, 0xc7, 0x59) {
    m_pGuiTick->connectValueChanged(this, &WPhaseMeter::slotGuiTick);
    updateDecks();
}

void WPhaseMeter::setup(const QDomNode& node, const SkinContext& context) {
    m_segmentColor = colorOrDefault(context, node, QStringLiteral("SegmentColor"), m_segmentColor);
    m_beatColor = colorOrDefault(context, node, QStringLiteral("BeatColor"), m_beatColor);
    m_downbeatColor = colorOrDefault(
            context, node, QStringLiteral("DownbeatColor"), m_downbeatColor);
    m_phraseColor = colorOrDefault(context, node, QStringLiteral("PhraseColor"), m_downbeatColor);
    m_referenceColor = colorOrDefault(
            context, node, QStringLiteral("ReferenceColor"), m_referenceColor);
    m_inPhaseColor = colorOrDefault(context, node, QStringLiteral("InPhaseColor"), m_inPhaseColor);
    setFocusPolicy(Qt::NoFocus);
}

// static
double WPhaseMeter::barOffsetBeats(double position, double reference) {
    constexpr double kBeatsPerBar = mixxx::bars::kBeatsPerBar;
    double offset = std::fmod(position - reference, kBeatsPerBar);
    if (offset < -kBeatsPerBar / 2) {
        offset += kBeatsPerBar;
    } else if (offset >= kBeatsPerBar / 2) {
        offset -= kBeatsPerBar;
    }
    return offset;
}

void WPhaseMeter::updateDecks() {
    const int numDecks = std::max(0, static_cast<int>(m_numDecks.get()));
    if (static_cast<int>(m_allDecks.size()) == numDecks) {
        return;
    }
    m_allDecks.clear();
    for (int i = 0; i < numDecks; ++i) {
        const QString group = PlayerManager::groupForDeck(i);
        m_allDecks.push_back(std::make_unique<DeckControls>(group));
    }
}

const WPhaseMeter::DeckControls* WPhaseMeter::findReference() const {
    // The sync leader is the reference for all other decks.
    if (isLeader(syncModeFromDouble(m_deck.syncMode.get()))) {
        return nullptr;
    }
    for (const auto& pDeck : m_allDecks) {
        if (pDeck->group != m_group && pDeck->hasBars() &&
                isLeader(syncModeFromDouble(pDeck->syncMode.get()))) {
            return pDeck.get();
        }
    }
    // Without a leader, use the first other playing deck.
    for (const auto& pDeck : m_allDecks) {
        if (pDeck->group != m_group && pDeck->hasBars() && pDeck->play.toBool()) {
            return pDeck.get();
        }
    }
    return nullptr;
}

WPhaseMeter::State WPhaseMeter::currentState() const {
    State state;
    state.valid = m_deck.hasBars();
    if (!state.valid) {
        return state;
    }
    state.barPosition = m_deck.barPosition();
    state.barInPhrase = static_cast<int>(m_deck.barInPhrase.get());

    const DeckControls* pReference = findReference();
    if (pReference) {
        state.hasReference = true;
        state.referenceBarPosition = pReference->barPosition();
        state.referenceBarInPhrase = static_cast<int>(pReference->barInPhrase.get());
    }
    return state;
}

void WPhaseMeter::slotGuiTick() {
    if (!isVisible()) {
        return;
    }
    updateDecks();
    const State state = currentState();
    if (state == m_state) {
        return;
    }
    m_state = state;
    update();
}

void WPhaseMeter::paintEvent(QPaintEvent* /*unused*/) {
    QStyleOption option;
    option.initFrom(this);
    QStylePainter painter(this);
    painter.drawPrimitive(QStyle::PE_Widget, option);

    const QRectF area = contentsRect();
    constexpr int kSegments = mixxx::bars::kBeatsPerBar;
    if (area.width() < 2 * kSegments || area.height() < 3) {
        return;
    }
    painter.setRenderHint(QPainter::Antialiasing);

    // One segment per beat of the bar, one dot per bar of the phrase below.
    const double gap = std::max(1.0, std::round(area.width() * 0.02));
    const bool drawDots = area.height() >= 12;
    const double dotDiameter = drawDots ? std::clamp(area.height() * 0.25, 3.0, 6.0) : 0.0;
    // Room for the ring around the reference deck's dot
    const double dotsHeight = drawDots ? dotDiameter + 2 * kRingMargin : 0.0;
    const double dotGap = drawDots ? 1.0 : 0.0;
    const double segmentHeight = area.height() - dotsHeight - dotGap;
    const double segmentWidth = (area.width() - gap * (kSegments - 1)) / kSegments;
    auto segmentLeft = [&](int segment) {
        return area.left() + segment * (segmentWidth + gap);
    };
    // Maps a position in the bar (in beats) to an x coordinate
    auto xForBarPosition = [&](double barPosition) {
        const int segment = std::clamp(static_cast<int>(barPosition), 0, kSegments - 1);
        const double fraction = std::clamp(barPosition - segment, 0.0, 1.0);
        return segmentLeft(segment) + fraction * segmentWidth;
    };

    const State& state = m_state;
    bool inPhase = false;
    if (state.valid && state.hasReference) {
        inPhase = std::abs(barOffsetBeats(state.barPosition, state.referenceBarPosition)) <
                kInPhaseThresholdBeats;
    }

    for (int segment = 0; segment < kSegments; ++segment) {
        const QRectF segmentRect(segmentLeft(segment), area.top(), segmentWidth, segmentHeight);
        painter.fillRect(segmentRect, m_segmentColor);
        if (!state.valid) {
            continue;
        }
        // Fill up to the current position in the bar
        const double fill = std::clamp(state.barPosition - segment, 0.0, 1.0);
        if (fill > 0.0) {
            QRectF fillRect = segmentRect;
            fillRect.setWidth(segmentRect.width() * fill);
            painter.fillRect(fillRect, segment == 0 ? m_downbeatColor : m_beatColor);
        }
    }

    if (state.valid && state.hasReference) {
        // Marker showing the position of the reference deck in its bar
        const QColor markerColor = inPhase ? m_inPhaseColor : m_referenceColor;
        const double x = xForBarPosition(state.referenceBarPosition);
        const double markerWidth = std::max(2.0, std::round(segmentWidth * 0.04));
        painter.fillRect(QRectF(x - markerWidth / 2, area.top(), markerWidth, segmentHeight),
                markerColor);
        const double triangleSize = std::clamp(segmentHeight * 0.4, 3.0, 7.0);
        QPainterPath triangle;
        triangle.moveTo(x - triangleSize, area.top());
        triangle.lineTo(x + triangleSize, area.top());
        triangle.lineTo(x, area.top() + triangleSize);
        triangle.closeSubpath();
        painter.fillPath(triangle, markerColor);
    }

    if (!drawDots) {
        return;
    }
    static_assert(mixxx::bars::kBarsPerPhrase == kSegments,
            "Phrase dots are drawn below the beat segments");
    const double dotRadius = dotDiameter / 2;
    const double dotCenterY = area.bottom() - kRingMargin - dotRadius;
    for (int bar = 0; bar < mixxx::bars::kBarsPerPhrase; ++bar) {
        const QPointF center(segmentLeft(bar) + segmentWidth / 2, dotCenterY);
        const bool isCurrentBar = state.valid && state.barInPhrase == bar + 1;
        painter.setPen(Qt::NoPen);
        painter.setBrush(isCurrentBar ? m_phraseColor : m_segmentColor);
        painter.drawEllipse(center, dotRadius, dotRadius);
        if (state.valid && state.hasReference && state.referenceBarInPhrase == bar + 1) {
            const bool phraseAligned = inPhase && state.barInPhrase == state.referenceBarInPhrase;
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(phraseAligned ? m_inPhaseColor : m_referenceColor, kRingWidth));
            const double ringRadius = dotRadius + kRingMargin - kRingWidth / 2;
            painter.drawEllipse(center, ringRadius, ringRadius);
        }
    }
}
