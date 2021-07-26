#include <QLabel>
#include <QHBoxLayout>
#include <QMenu>
#include <QApplication>
#include <QClipboard>
#include <QDebug>

#include "weathervaluewidget.h"
#include "unit_conversions.h"
#include "settings.h"

// Local units:
//   The widget can optionally display wind speed using units other than the globally
//   configured default
#define LU_SETTING "units" // Setting the local override is saved under
#define LU_MS "ms"
#define LU_KMH "kmh"
#define LU_MPH "mph"
#define LU_KNOT "knots"
#define LU_GLOBAL "default" // Use global setting (whatever that may be) - no local override

WeatherValueWidget::WeatherValueWidget(QWidget *parent) : QWidget(parent)
{
    column1 = SC_NoColumns;
    column2 = SC_NoColumns;
    column1ex = EC_NoColumns;
    insideOutside = false;
    doubleValue = false;
    name = parent == NULL ? objectName() : parent->objectName() + "." + objectName();

    label = new QLabel(this);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(label);
    layout->setSpacing(0);
    layout->setMargin(0);
    setLayout(layout);
    label->setText("--");

    unitsChanged(Settings::getInstance().imperial(),
                 Settings::getInstance().kmh());
    changeUnits();

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(&Settings::getInstance(), SIGNAL(unitsChanged(bool,bool)),
            this, SLOT(unitsChanged(bool,bool)));
    connect(this, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(showContextMenu(QPoint)));
}

void WeatherValueWidget::unitsChanged(bool imperial, bool kmh) {
    Q_UNUSED(imperial);
    Q_UNUSED(kmh);

    globalUnits = LU_MS;
    if (Settings::getInstance().kmh()) {
        globalUnits = LU_KMH;
    } else if (Settings::getInstance().imperial()) {
        globalUnits = LU_MPH;
    }

    updateDisplay();
}

void WeatherValueWidget::setValue(
        UnitConversions::UnitValue value, StandardColumn column) {
    value1 = value;
    column1 = column;
    updateDisplay();
}

void WeatherValueWidget::setValue(
        UnitConversions::UnitValue value, ExtraColumn column) {
    value1 = value;
    column1ex = column;
    updateDisplay();
}

void WeatherValueWidget::setOutdoorIndoorValue(
        UnitConversions::UnitValue outdoor, StandardColumn outdoorColumn,
        UnitConversions::UnitValue indoor, StandardColumn indoorColumn) {
    value1 = outdoor;
    column1 = outdoorColumn;
    value2 = indoor;
    column2 = indoorColumn;
    insideOutside = true;
    updateDisplay();
}

void WeatherValueWidget::setDoubleValue(
        UnitConversions::UnitValue value1, StandardColumn column1,
        UnitConversions::UnitValue value2, StandardColumn column2) {
    this->value1 = value1;
    this->column1 = column1;
    this->value2 = value2;
    this->column2 = column2;
    doubleValue = true;
    updateDisplay();
}

void WeatherValueWidget::clear() {
    value1 = UnitConversions::UnitValue();
    column1 = SC_NoColumns;
    value2 = UnitConversions::UnitValue();
    column2 = SC_NoColumns;
    column1ex = EC_NoColumns;
    insideOutside = false;
    doubleValue = false;
    label->setText("--");
    localUnits = "default";
}

void WeatherValueWidget::updateDisplay() {
    UnitConversions::UnitValue v1(value1);
    UnitConversions::UnitValue v2(value2);

    if (v1.unit == UnitConversions::U_METERS_PER_SECOND || 
            v2.unit == UnitConversions::U_METERS_PER_SECOND) {
    
    }
    
    QString displayUnits = localUnits == LU_GLOBAL ? globalUnits : localUnits;
    
    if (displayUnits == LU_MPH) {
        v1 = UnitConversions::toImperial(v1);
        v2 = UnitConversions::toImperial(v2);
    } else if (displayUnits == LU_KMH) {
        if (v1.unit == UnitConversions::U_METERS_PER_SECOND) {
            v1 = UnitConversions::metersPerSecondToKilometersPerHour(v1);
        }
        if (v2.unit == UnitConversions::U_METERS_PER_SECOND) {
            v2 = UnitConversions::metersPerSecondToKilometersPerHour(v2);
        }
    } else if (displayUnits == LU_KNOT) {
        if (v1.unit == UnitConversions::U_METERS_PER_SECOND) {
            v1 = UnitConversions::metersPerSecondToKnots(v1);
        }
        if (v2.unit == UnitConversions::U_METERS_PER_SECOND) {
            v2 = UnitConversions::metersPerSecondToKnots(v2);
        }
    } // else: ms

    if (insideOutside) {
        label->setText(QString(tr("%1 (%2 inside)"))
                       .arg(QString(v1))
                       .arg(QString(v2)));
    } else if (doubleValue) {
      QString v2s = v2;
      if (v2s.isEmpty()) {
          label->setText(value1);
      } else if (column1 == SC_WindDirection && column2 == SC_WindDirection) {
          // Degrees & compass point
          label->setText(QString(v1) + " " + v2s);
      } else {
          label->setText(QString(tr("%1 (%2)"))
                         .arg(QString(v1)).arg(v2s));
      }
    } else {
        if (column1 == SC_UV_Index) {
            /* US EPA UV Index categories
             *
             *   Index      Exposure Category
             *   --------   -----------------
             *    0-2.9      Low
             *    3-5.9      Moderate
             *    6-7.9      High
             *    7-10.9     Very High
             *    11+        Extreme
             */

            QString exposureCategory = "";
            if ((float)v1 < 3.0) {
                exposureCategory = tr("low");
            } else if ((float)v1 < 6.0) {
                exposureCategory = tr("moderate");
            } else if ((float)v1 < 8.0) {
                exposureCategory = tr("high");
            } else if ((float)v1 < 11.0) {
                exposureCategory = tr("very high");
            } else {
                exposureCategory = tr("extreme");
            }

            label->setText(QString(tr("%1 (%2)")).arg(QString(v1)).arg(exposureCategory));
        } else {
            label->setText(v1);
        }
    }
}

#define PT_TODAY 1
#define PT_24H 2
#define PT_WEEK 3
#define PT_7DAYS 4
#define PT_MONTH 5
#define PT_YEAR 6

void WeatherValueWidget::showContextMenu(QPoint point) {
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    QAction *action;

    menu->addAction(tr("Copy"), this, SLOT(copy()));

    if (column1 == SC_AverageWindSpeed) {
        QMenu *units = menu->addMenu(tr("Units"));

        action = units->addAction(globalUnits == LU_MS ? tr("m/s (default)") : tr("m/s"),
                                  this, SLOT(changeUnits()));
        action->setData(globalUnits == LU_MS ? LU_GLOBAL : LU_MS);
        action->setCheckable(true);
        action->setChecked(localUnits == LU_MS || (globalUnits == LU_MS && localUnits == LU_GLOBAL));

        action = units->addAction(globalUnits == LU_KMH ? tr("km/h (default)") : tr("km/h"),
                                  this, SLOT(changeUnits()));
        action->setData(globalUnits == LU_KMH ? LU_GLOBAL : LU_KMH);
        action->setCheckable(true);
        action->setChecked(localUnits == LU_KMH || (globalUnits == LU_KMH && localUnits == LU_GLOBAL));

        action = units->addAction(globalUnits == LU_MPH ? tr("mph (default)") : tr("mph"),
                                  this, SLOT(changeUnits()));
        action->setData(globalUnits == LU_MPH ? LU_GLOBAL : LU_MPH);
        action->setCheckable(true);
        action->setChecked(localUnits == LU_MPH || (globalUnits == LU_MPH && localUnits == LU_GLOBAL));

        action = units->addAction(tr("knots"), this, SLOT(changeUnits()));
        action->setData(LU_KNOT);
        action->setCheckable(true);
        action->setChecked(localUnits == LU_KNOT);
    }

    if (column1 != SC_NoColumns) {
        menu->addSeparator();

        QMenu *plot = menu->addMenu(tr("Plot"));
        action = plot->addAction(tr("Today"), this, SLOT(plot()));
        action->setData(PT_TODAY);

        action = plot->addAction(tr("24 Hours"), this, SLOT(plot()));
        action->setData(PT_24H);

        action = plot->addAction(tr("Week"), this, SLOT(plot()));
        action->setData(PT_WEEK);

        action = plot->addAction(tr("7 days"), this, SLOT(plot()));
        action->setData(PT_7DAYS);

        action = plot->addAction(tr("Month"), this, SLOT(plot()));
        action->setData(PT_MONTH);

        action = plot->addAction(tr("Year"), this, SLOT(plot()));
        action->setData(PT_YEAR);
    }

    menu->popup(this->mapToGlobal(point));
}

void WeatherValueWidget::changeUnits() {

    if (QAction* menuAction = qobject_cast<QAction*>(sender())) {
        // Changing units by context menu.
        localUnits = menuAction->data().toString();

        Settings::getInstance().setWeatherValueWidgetSetting(name, LU_SETTING, localUnits);
    } else {
        // Load units from settings. If no local override is saved then
        // just use the global setting instead (LU_GLOBAL)
        localUnits = Settings::getInstance().weatherValueWidgetSetting(
                    name, LU_SETTING, LU_GLOBAL).toString();
    }

    updateDisplay();

    qDebug() << "WVW Name" << name
             << "Global units:" << globalUnits
             << "Local units:" << localUnits;
}

void WeatherValueWidget::plot() {
    if (QAction* menuAction = qobject_cast<QAction*>(sender())) {
        int range = menuAction->data().toInt();

        QTime start(0,0,0);
        QTime end(23,59,59);
        QDate today = QDate::currentDate();

        DataSet ds;
        ds.columns.standard = column1;
        ds.columns.extra = column1ex;
        ds.aggregateFunction = AF_None;
        ds.groupType = AGT_None;
        ds.customGroupMinutes = 5;

        if (column1 != column2 && column2 != SC_NoColumns) {
            ds.columns.standard |= column2;
        }

        switch(range) {
        case PT_TODAY:
            ds.startTime = QDateTime(today, start);
            ds.endTime = QDateTime(today, end);
            break;
        case PT_24H:
            ds.endTime = QDateTime::currentDateTime();
            ds.startTime = ds.endTime.addDays(-1);
            break;
        case PT_WEEK: {
            int subtractDays = 1 - QDateTime::currentDateTime().date().dayOfWeek();
            int addDays = 7 - QDateTime::currentDateTime().date().dayOfWeek();

            ds.startTime = QDateTime::currentDateTime().addDays(subtractDays);
            ds.endTime = QDateTime::currentDateTime().addDays(addDays);
            break;
        }
        case PT_7DAYS:
            ds.endTime = QDateTime::currentDateTime();
            ds.startTime = ds.endTime.addDays(-7);
            break;
        case PT_MONTH:
            ds.startTime = QDateTime(QDate(today.year(), today.month(), 1), start);
            ds.endTime = ds.startTime;
            ds.endTime = ds.endTime.addMonths(1);
            ds.endTime = ds.endTime.addDays(-1);
            ds.endTime.setTime(end);
            break;
        case PT_YEAR:
            ds.startTime = QDateTime(QDate(today.year(), 1, 1), start);
            ds.endTime = QDateTime(QDate(today.year(), 12, 31), end);
            break;
        }

        emit plotRequested(ds);
    }
}

void WeatherValueWidget::copy() {
    QClipboard * clipboard = QApplication::clipboard();
    clipboard->setText(this->label->text());
}

