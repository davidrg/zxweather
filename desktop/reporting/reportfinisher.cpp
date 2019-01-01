#include "reportfinisher.h"
#include "report.h"

ReportFinisher::ReportFinisher(Report *report) : QObject(NULL) {
    r = report;
    finished = false;
}

void ReportFinisher::cachingFinished() {
    r->executeReport();
}

void ReportFinisher::finishReport() {
    finished = true;
    emit reportCompleted();
}

bool ReportFinisher::isFinished() {
    return finished;
}
