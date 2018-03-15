#include "dialogprogresslistener.h"

DialogProgressListener::DialogProgressListener(QWidget *parent) : AbstractProgressListener(parent)
{
    dialog.reset(new QProgressDialog(parent));
}


int DialogProgressListener::value() const {
    return dialog->value();
}

int DialogProgressListener::maximum() const {
    return dialog->maximum();
}

void DialogProgressListener::setSubtaskName(QString label) {
    dialog->setLabelText(label);
}

void DialogProgressListener::setMaximum(int max) {
    dialog->setMaximum(max);
}

void DialogProgressListener::setValue(int value) {
    dialog->setValue(value);
}

void DialogProgressListener::setTaskName(QString title) {
    dialog->setWindowTitle(title);
}

void DialogProgressListener::reset() {
    dialog->reset();
}

void DialogProgressListener::show() {
    dialog->show();
}

void DialogProgressListener::hide() {
    dialog->hide();
}

bool DialogProgressListener::wasCanceled() const {
    return dialog->wasCanceled();
}

void DialogProgressListener::close() {
    dialog->close();
}

void DialogProgressListener::setRange(int min, int max) {
    dialog->setRange(min, max);
}
