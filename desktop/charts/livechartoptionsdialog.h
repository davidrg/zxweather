#ifndef LIVECHARTOPTIONSDIALOG_H
#define LIVECHARTOPTIONSDIALOG_H

#include <QDialog>

namespace Ui {
class LiveChartOptionsDialog;
}

class LiveChartOptionsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LiveChartOptionsDialog(bool aggregate, int period, bool maxRainRate,
                                    bool stormRain, bool stormRainEnabled,
                                    int rangeMinutes, bool tags, bool multiRect,
                                    QWidget *parent = 0);
    ~LiveChartOptionsDialog();

    bool aggregate() const;
    int aggregatePeriod() const;
    bool maxRainRate() const;
    bool stormRain() const;
    int rangeMinutes() const;
    bool tagsEnabled() const;
    bool multipleAxisRectsEnabled() const;

private:
    Ui::LiveChartOptionsDialog *ui;
};

#endif // LIVECHARTOPTIONSDIALOG_H
