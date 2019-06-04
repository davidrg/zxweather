#include "nonaggregatingliveaggregator.h"

#include <QtDebug>

NonAggregatingLiveAggregator::NonAggregatingLiveAggregator(bool runningTotalRain, QObject *parent)
    : AbstractLiveAggregator(parent)
{
    this->runningTotalRain = runningTotalRain;
    reset();
}


void NonAggregatingLiveAggregator::reset() {
    lastStormRain = -1;
}

void NonAggregatingLiveAggregator::incomingLiveData(LiveDataSet data) {
    // If rain isn't to be a running total, figure out the difference from the last
    // update.
    if (data.hw_type == HW_DAVIS) {
        if (data.davisHw.stormDateValid) {
            if (!runningTotalRain && lastStormRain != -1) {
                data.davisHw.stormRain = data.davisHw.stormRain - lastStormRain;
            }
            lastStormRain = data.davisHw.stormRain;
        } else {
            lastStormRain = 0;
        }
    }


    emit liveData(data);
}
