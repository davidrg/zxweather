#ifndef CONFIGWIZARD_H
#define CONFIGWIZARD_H

#include <QWizard>

class QPushButton;

class ConfigWizard : public QWizard
{
    Q_OBJECT
public:
    enum {
        Page_None = -1,

        /* Common pages */
        Page_Intro,
        Page_AccessType,
        Page_SelectStation,
        Page_ConfirmDetails,

        /* Database access type */
        Page_DatabaseDetails,

        /* Internet access type */
        Page_InternetSiteInfo,
        Page_ServerDetails  // TODO: page to enter internet weather server details
    };

    explicit ConfigWizard(QWidget *parent = 0);
    
    void accept();

private:
    QPushButton *subpageBack;
};

#endif // CONFIGWIZARD_H
