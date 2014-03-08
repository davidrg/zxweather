#ifndef INTROPAGE_H
#define INTROPAGE_H

#include <QWizardPage>
#include <QScopedPointer>

class QLabel;

class IntroPage : public QWizardPage {
    Q_OBJECT

public:
    IntroPage(QWidget *parent = 0);

    int nextId() const;
};



#endif // INTROPAGE_H
