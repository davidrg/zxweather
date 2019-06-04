#ifndef NONAGGREGATINGLIVEAGGREGATOR_H
#define NONAGGREGATINGLIVEAGGREGATOR_H

#include "abstractliveaggregator.h"

#include <QTimer>

/* This class just passes through live data without any aggegating.
 * It can optionally turn storm rain into rainfall clicks.
 */
class NonAggregatingLiveAggregator : public AbstractLiveAggregator
{
    Q_OBJECT

public:
    NonAggregatingLiveAggregator(bool runningTotalRain, QObject *parent=0);

public slots:
   void incomingLiveData(LiveDataSet data);
   void reset();

private:
   bool runningTotalRain;
   float lastStormRain;
};

#endif // NONAGGREGATINGLIVEAGGREGATOR_H
