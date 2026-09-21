#include <QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QScrollArea>
#include <QSpinBox>
#include <QSignalSpy>
#include "devicegrid.h"
#include "../QtScrcpy/groupcontroller/groupcontroller.h"
#include "dialog.h"
#include "videoform.h"
#include "keepratiowidget.h"

class DeviceGridTest : public QObject {
    Q_OBJECT
private slots:
    void synchronizationOnlyTargetsSelectedPeers() {
        auto &group = GroupController::instance();
        group.setGridTargets({"a", "b"}, "a", true);
        QVERIFY(group.isHost("a")); // The source must never receive its own event twice.
        QVERIFY(!group.isHost("b"));
        QVERIFY(group.isHost("c")); // Unchecked device is excluded.
        group.setGridTargets({"a", "b"}, "c", true);
        QVERIFY(group.isHost("a"));
        QVERIFY(group.isHost("b")); // An unchecked source cannot broadcast.
        group.setGridTargets({"a", "b"}, "a", false);
        QVERIFY(group.isHost("b"));
        QVERIFY(group.getFrameSize("disconnected").isEmpty());
    }
    void selectionSearchAndGrouping() {
        QTemporaryDir storage;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, storage.path());
        Dialog settings;
        DeviceGrid grid(&settings);
        disconnect(&settings, nullptr, &grid, nullptr);
        for (auto timer : settings.findChildren<QTimer*>()) timer->stop();
        for (auto timer : grid.findChildren<QTimer*>()) timer->stop();
        grid.refreshDevices({"a", "b", "c"});
        QCOMPARE(grid.m_list->count(), 3);
        grid.m_list->item(0)->setCheckState(Qt::Checked);
        grid.m_list->item(1)->setCheckState(Qt::Checked);
        QCOMPARE(grid.selectedDevices(), QStringList({"a", "b"}));
        grid.m_membership["a"] = "运营";
        grid.m_membership["b"] = "运营";
        grid.m_groups->addItem("运营", "运营");
        grid.m_groups->setCurrentIndex(1);
        QCOMPARE(grid.m_list->count(), 2);
        grid.m_search->setText("a");
        QCOMPARE(grid.m_list->count(), 1);
        QCOMPARE(grid.selectedDevices().size(), 2);
        grid.saveGroups();
        QSettings saved;
        QCOMPARE(saved.value("grid/groups").toStringList(), QStringList({"运营"}));
        QCOMPARE(saved.value("grid/membership/61").toString(), QString("运营"));
        grid.refreshDevices({"a", "c"});
        QCOMPARE(grid.selectedDevices(), QStringList({"a"}));
    }
    void embeddedCardsZoomAndDisconnect() {
        Dialog settings;
        DeviceGrid grid(&settings);
        disconnect(&settings, nullptr, &grid, nullptr);
        for (auto timer : settings.findChildren<QTimer*>()) timer->stop();
        for (auto timer : grid.findChildren<QTimer*>()) timer->stop();
        grid.refreshDevices({"a", "b", "c"});
        for (const auto &serial : QStringList({"a", "b", "c"})) {
            auto video = new VideoForm(false, false, false);
            video->setSerial(serial);
            video->setEmbedded();
            video->updateShowSize(QSize(1080, 2400));
            grid.addVideo(serial, serial, video);
        }
        grid.show();
        QTest::qWait(100);
        auto a = grid.m_cards["a"].video;
        QVERIFY(!a->isWindow());
        QVERIFY(a->parentWidget() == grid.m_cards["a"].widget);
        QSignalSpy zoom(a, &VideoForm::zoomRequested);
        QTest::mouseDClick(a, Qt::LeftButton);
        QCOMPARE(zoom.count(), 1);
        QCOMPARE(grid.m_zoom, QString("a"));
        QVERIFY(grid.m_cards["b"].widget->isHidden());
        QTest::mouseDClick(a, Qt::LeftButton);
        QVERIFY(grid.m_zoom.isEmpty());
        QVERIFY(!grid.m_cards["b"].widget->isHidden());
        grid.m_cards["a"].check->setChecked(true);
        QVERIFY(grid.m_selected.contains("a"));
        grid.removeVideo("a");
        QCOMPARE(grid.m_cards.size(), 2);
        QVERIFY(grid.m_host.isEmpty());
    }
    void videoAlwaysFitsCard() {
        KeepRatioWidget ratio;
        auto display = new QWidget;
        ratio.setWidget(display);
        ratio.resize(200, 150);
        ratio.show();
        for (float aspect : {0.45f, 1.0f, 2.0f}) {
            ratio.setWidthHeightRatio(aspect);
            QTest::qWait(10);
            QVERIFY(display->width() <= ratio.width());
            QVERIFY(display->height() <= ratio.height());
            QVERIFY(display->x() >= 0);
            QVERIFY(display->y() >= 0);
        }
    }
};
QTEST_MAIN(DeviceGridTest)
#include "devicegrid_test.moc"
