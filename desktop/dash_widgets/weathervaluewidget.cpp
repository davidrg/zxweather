#include <QLabel>
#include <QHBoxLayout>
#include <QMenu>
#include <QApplication>
#include <QClipboard>

#include "weathervaluewidget.h"
#include "unit_conversions.h"
#include "settings.h"

WeatherValueWidget::WeatherValueWidget(QWidget *parent) : QWidget(parent)
{
    label = new QLabel(this);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(label);
    layout->setSpacing(0);
    layout->setMargin(0);
    setLayout(layout);
    label->setText("--");

    insideOutside = false;
    doubleValue = false;
    connect(&Settings::getInstance(), SIGNAL(unitsChanged(bool,bool)),
            this, SLOT(unitsChanged(bool,bool)));

    imperial = Settings::getInstance().imperial();
    kmh = Settings::getInstance().kmh();

    column1 = SC_NoColumns;
    column2 = SC_NoColumns;

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(showContextMenu(QPoint)));
}

void WeatherValueWidget::unitsChanged(bool imperial, bool kmh) {
    this->imperial = imperial;
    this->kmh = kmh;
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
    insideOutside = false;
    doubleValue = false;
    label->setText("--");
}

void WeatherValueWidget::updateDisplay() {
    UnitConversions::UnitValue v1(value1);
    UnitConversions::UnitValue v2(value2);

    if (imperial) {
        v1 = UnitConversions::toImperial(v1);
        v2 = UnitConversions::toImperial(v2);
    } else if (kmh) {
        if (v1.unit == UnitConversions::U_METERS_PER_SECOND) {
            v1 = UnitConversions::metersPerSecondToKilometersPerHour(v1);
        }
        if (v2.unit == UnitConversions::U_METERS_PER_SECOND) {
            v2 = UnitConversions::metersPerSecondToKilometersPerHour(v2);
        }
    }

    if (insideOutside) {
        label->setText(QString("%0 (%1 inside)")
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
          label->setText(QString("%0 (%1)")
                         .arg(QString(v1)).arg(v2s));
      }
    } else {
        label->setText(v1);
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

    if (value1.unit == UnitConversions::U_METERS_PER_SECOND && !imperial) {
        menu->addSeparator();
        action = menu->addAction(tr("km/h"), this, SLOT(toggle_kmh()));
        action->setCheckable(true);
        action->setChecked(kmh);
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

void WeatherValueWidget::toggle_kmh() {
    kmh = !kmh;
    if (!name.isNull()) {
        Settings::getInstance().setWeatherValueWidgetSetting(name, "kmh", kmh);
    }

    updateDisplay();
}

void WeatherValueWidget::setName(QString name) {
    this->name = name;
    kmh = Settings::getInstance().weatherValueWidgetSetting(name, "kmh", false).toBool();
}
