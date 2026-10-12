#pragma once

#include <QColor>
#include <QString>
#include <memory>
#include <vector>

#include "control/pollingcontrolproxy.h"
#include "widget/wwidget.h"

class ControlProxy;
class QDomNode;
class QPainter;
class SkinContext;

/// Shows where a deck is within the current bar and phrase. If another deck is
/// the sync leader, or is playing while this deck is not the leader, its
/// position is shown as well, so bars and phrases of both decks can be
/// aligned.
///
/// The `Bar` style (default) shows one segment per beat of the bar, filling
/// up as the bar progresses, and one dot per bar of the phrase below. The
/// `Phrase` style shows a single row of one cell per beat of the phrase,
/// grouped by bar, with the current beat lit. Several `Phrase` style meters
/// can be stacked to compare decks at a glance.
class WPhaseMeter : public WWidget {
    Q_OBJECT
  public:
    WPhaseMeter(QWidget* pParent, const QString& group);

    void setup(const QDomNode& node, const SkinContext& context);

    /// Returns the offset of `position` relative to `reference` in beats,
    /// wrapped to the range [-beatsPerBar / 2, beatsPerBar / 2).
    static double barOffsetBeats(double position, double reference);

  protected:
    void paintEvent(QPaintEvent* pEvent) override;

  private slots:
    void slotGuiTick();

  private:
    enum class Style {
        Bar,
        Phrase,
    };

    struct DeckControls {
        explicit DeckControls(const QString& group);

        bool hasBars() const;
        /// Position in the bar in beats, 0 to beatsPerBar
        double barPosition() const;

        QString group;
        PollingControlProxy barDistance;
        PollingControlProxy beatInBar;
        PollingControlProxy barInPhrase;
        PollingControlProxy syncMode;
        PollingControlProxy play;
    };

    struct State {
        bool valid = false;
        double barPosition = 0.0;
        int barInPhrase = 0;
        bool hasReference = false;
        double referenceBarPosition = 0.0;
        int referenceBarInPhrase = 0;

        bool operator==(const State& other) const = default;
    };

    void updateDecks();
    const DeckControls* findReference() const;
    State currentState() const;
    bool isInPhase() const;
    void paintBarStyle(QPainter* pPainter, const QRectF& area);
    void paintPhraseStyle(QPainter* pPainter, const QRectF& area);

    const QString m_group;
    DeckControls m_deck;
    PollingControlProxy m_numDecks;
    // Controls of all decks, including this one
    std::vector<std::unique_ptr<DeckControls>> m_allDecks;
    ControlProxy* m_pGuiTick;
    State m_state;
    Style m_style;

    QColor m_segmentColor;
    QColor m_beatColor;
    QColor m_downbeatColor;
    QColor m_phraseColor;
    QColor m_referenceColor;
    QColor m_inPhaseColor;
};
