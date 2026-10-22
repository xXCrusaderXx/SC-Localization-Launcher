#include <QSignalSpy>
#include <QTest>
#include <memory>

#include "frontend/SingleInstance.hpp"
#include "logging/logger/custom_logger/custom_logger.h"

class SingleInstanceTest : public QObject
{
    Q_OBJECT

   private:
    // Unique per run, so parallel test runs never share a lock
    const QString name = QString("sc-localization-launcher-test-%1").arg(QCoreApplication::applicationPid());

   private slots:
    void initTestCase () { LoggerFramework::GlobalLogger::registerLogger(std::make_shared<LoggerFramework::CustomLogger>()); }

    void secondStartActivatesTheFirst ()
    {
        SingleInstance first(name);
        QVERIFY(first.isPrimary());
        QSignalSpy activated(&first, &SingleInstance::activationRequested);

        SingleInstance second(name);
        QVERIFY(!second.isPrimary());
        QVERIFY(second.activatePrimary());
        QTRY_COMPARE(activated.count(), 1);
    }

    void lockIsFreeAgainAfterTheFirstEnds ()
    {
        {
            SingleInstance first(name);
            QVERIFY(first.isPrimary());
        }
        SingleInstance next(name);
        QVERIFY(next.isPrimary());
    }
};

QTEST_GUILESS_MAIN(SingleInstanceTest)
#include "SingleInstanceTest.moc"
