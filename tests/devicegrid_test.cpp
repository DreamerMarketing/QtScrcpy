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
#include <QPushButton>
#include <QOpenGLWidget>
#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <QPlainTextEdit>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTcpServer>
#include <QScopeGuard>
#include "adbcommand.h"
#include "../QtScrcpy/QtScrcpyCore/src/devicemanage/devicemanage.h"
#include "../QtScrcpy/QtScrcpyCore/src/device/device.h"
#include "devicegrid.h"
#include "../QtScrcpy/groupcontroller/groupcontroller.h"
#include "dialog.h"
#include "videoform.h"
#include "keepratiowidget.h"

class DeviceGridTest : public QObject {
    Q_OBJECT
    QTemporaryDir m_storage;
    bool m_stubAvailable = false;
private slots:
    void initTestCase() {
        QApplication::setQuitOnLastWindowClosed(false);
        QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
        QVERIFY(m_storage.isValid());
        qputenv("QTSCRCPY_CONFIG_PATH", m_storage.path().toUtf8());
        qputenv("QTSCRCPY_KEYMAP_PATH", m_storage.path().toUtf8());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_storage.path());
        if (qEnvironmentVariableIsEmpty("QTSCRCPY_TEST_SERIAL") && qEnvironmentVariableIsEmpty("QTSCRCPY_TEST_SERIALS")) {
            QString stub = QCoreApplication::applicationDirPath() + "/AdbTestStub";
#ifdef Q_OS_WIN
            stub += ".exe";
#endif
            QVERIFY(QFile::exists(stub));
            QProcess probe;
            probe.start(stub, {"devices"});
            m_stubAvailable = probe.waitForStarted(3000) && probe.waitForFinished(3000) && probe.exitCode() == 0;
            if (m_stubAvailable) qputenv("QTSCRCPY_ADB_PATH", stub.toUtf8());
            else qWarning() << "ADB subprocess stub unavailable:" << probe.errorString();
        }
    }
    void realDeviceVideo() {
        QStringList serials = qEnvironmentVariable("QTSCRCPY_TEST_SERIALS").split(',', Qt::SkipEmptyParts);
        if (serials.isEmpty() && !qEnvironmentVariableIsEmpty("QTSCRCPY_TEST_SERIAL")) serials << qEnvironmentVariable("QTSCRCPY_TEST_SERIAL");
        serials.removeDuplicates();
        if (serials.isEmpty()) QSKIP("Set QTSCRCPY_TEST_SERIALS (comma-separated) for hardware verification");
        const QString serial = serials.first();
        Dialog settings;
        settings.findChild<QLineEdit*>("bitRateEdit")->setText("8");
        settings.findChild<QComboBox*>("bitRateBox")->setCurrentText("Mbps");
        DeviceGrid grid(&settings);
        const auto disconnectDevices = qScopeGuard([] { qsc::IDeviceManage::getInstance().disconnectAllDevice(); });
        grid.show();
        for (const auto &target : serials) {
            QTRY_VERIFY_WITH_TIMEOUT(grid.m_devices.contains(target), 10000);
            grid.m_selected.insert(target);
        }
        grid.rebuildList();
        for (auto button : grid.findChildren<QPushButton*>())
            if (button->text() == QStringLiteral("开始投屏")) QTest::mouseClick(button, Qt::LeftButton);
        for (const auto &target : serials) {
            QTRY_VERIFY_WITH_TIMEOUT(grid.m_cards.contains(target), 15000);
            QPointer<QOpenGLWidget> render = grid.m_cards[target].video->findChild<QOpenGLWidget*>();
            QVERIFY(render);
            QTRY_VERIFY_WITH_TIMEOUT(render && !render->isHidden(), 15000);
            QVERIFY(render->isValid());
            const QImage frame = render->grabFramebuffer();
            QVERIFY(!frame.isNull());
            QVERIFY(frame.save("hardware-frame-" + QString::fromLatin1(target.toUtf8().toHex()) + ".png"));
        }
        QCOMPARE(grid.m_cards.size(), serials.size());
        grid.grab().save("hardware-workspace.png");
        auto click = [&grid](const QString &text) {
            for (auto button : grid.findChildren<QPushButton*>()) {
                if (button->text() == text) { QTest::mouseClick(button, Qt::LeftButton); return true; }
            }
            return false;
        };
        QTest::mouseDClick(grid.m_cards[serial].video, Qt::LeftButton);
        QCOMPARE(grid.m_zoom, serial);
        QTest::mouseDClick(grid.m_cards[serial].video, Qt::LeftButton);
        QVERIFY(grid.m_zoom.isEmpty());
        QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
        const QString apk = qEnvironmentVariable("QTSCRCPY_TEST_APK");
        QTemporaryDir storage;
        const QString payload = storage.filePath("qtscrcpy-button-check.txt");
        QFile file(payload);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("QtScrcpy file transfer verified\n");
        file.close();
        for (const auto &path : QStringList{apk, payload}) {
            if (path.isEmpty()) continue;
            QTimer dialogs;
            connect(&dialogs, &QTimer::timeout, &grid, [path] {
                if (auto dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget())) {
                    dialog->findChild<QLineEdit*>("fileNameEdit")->setText(QDir::fromNativeSeparators(path));
                    QMetaObject::invokeMethod(dialog, "accept", Qt::QueuedConnection);
                } else if (auto confirmation = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                    QTest::mouseClick(confirmation->button(QMessageBox::Yes), Qt::LeftButton);
                }
            });
            dialogs.start(100);
            QVERIFY(click(path == apk ? QStringLiteral("安装 APK") : QStringLiteral("发送文件")));
            QTRY_VERIFY_WITH_TIMEOUT(grid.m_jobs.isEmpty() && grid.m_activeJobs.isEmpty(), 30000);
            QCOMPARE(grid.m_done, serials.size());
            QCOMPARE(grid.m_failed, 0);
        }
        grid.m_command->setText("printf 'QtScrcpy command OK\\n'; getprop ro.product.model");
        QTimer::singleShot(100, &grid, [] {
            auto message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (message) QTest::mouseClick(message->button(QMessageBox::Yes), Qt::LeftButton);
        });
        QVERIFY(click(QStringLiteral("执行命令")));
        QTRY_VERIFY_WITH_TIMEOUT(grid.m_jobs.isEmpty() && grid.m_activeJobs.isEmpty(), 15000);
        QCOMPARE(grid.m_done, serials.size());
        QCOMPARE(grid.m_failed, 0);
        QVERIFY(grid.m_jobOutput->toPlainText().contains("QtScrcpy command OK"));
        // Verify the transferred bytes separately, through each explicit ADB target.
        for (const auto &target : serials) {
            qsc::AdbProcess verify;
            bool finished = false;
            bool success = false;
            connect(&verify, &qsc::AdbProcess::adbProcessResult, &grid, [&](qsc::AdbProcess::ADB_EXEC_RESULT result) {
                if (result == qsc::AdbProcess::AER_SUCCESS_START) return;
                finished = true;
                success = result == qsc::AdbProcess::AER_SUCCESS_EXEC;
            });
            verify.execute(target, {"shell", "cat /sdcard/Download/qtscrcpy-button-check.txt"});
            QTRY_VERIFY_WITH_TIMEOUT(finished, 10000);
            QVERIFY(success);
            QCOMPARE(verify.getStdOut().trimmed(), QString("QtScrcpy file transfer verified"));
            finished = false;
            success = false;
            verify.execute(target, {"shell", "printf ' first\\n'; sleep 1; printf '\\344'; sleep 1; "
                "printf '\\270\\255\\346\\226\\207\\n'; printf ' error\\n' >&2; sleep 1; printf ' next\\n' >&2"});
            QTRY_VERIFY_WITH_TIMEOUT(finished, 10000);
            QVERIFY(success);
            QCOMPARE(verify.getStdOut().replace("\r\n", "\n"), QStringLiteral(" first\n中文\n"));
            QCOMPARE(verify.getErrorOut().replace("\r\n", "\n"), QString(" error\n next\n"));
        }
        QVERIFY(click(QStringLiteral("停止投屏")));
        QTRY_VERIFY(grid.m_cards.isEmpty());
    }
    void parallelJobsKeepPerDeviceOrder() {
        if (!m_stubAvailable) QSKIP("ADB subprocess stub unavailable; parallel process integration not verified");
        const QString logPath = m_storage.filePath("parallel.jsonl");
        qputenv("QTSCRCPY_STUB_LOG", logPath.toUtf8());
        Dialog settings;
        DeviceGrid grid(&settings);
        grid.m_jobs.enqueue({"a", {"install", "-r", "first app.apk"}, "first"});
        grid.m_jobs.enqueue({"a", {"install", "-r", "second app.apk"}, "second"});
        grid.m_jobs.enqueue({"b", {"install", "-r", "first app.apk"}, "first"});
        grid.m_jobs.enqueue({"fail", {"shell", "getprop ro.product.model"}, "failure"});
        grid.runNextJob();
        QCOMPARE(grid.m_activeJobs.size(), 3);
        QCOMPARE(grid.m_jobs.size(), 1);
        QTRY_VERIFY_WITH_TIMEOUT(grid.m_activeJobs.isEmpty() && grid.m_jobs.isEmpty(), 10000);
        QCOMPARE(grid.m_done, 4);
        QCOMPARE(grid.m_failed, 1);
        QVERIFY(grid.m_jobOutput->toPlainText().contains("test device disconnected"));
        QFile log(logPath);
        QVERIFY(log.open(QIODevice::ReadOnly));
        QMap<QString, qint64> starts, ends;
        int aStarts = 0;
        while (!log.atEnd()) {
            const auto entry = QJsonDocument::fromJson(log.readLine()).object();
            const auto serial = entry["serial"].toString();
            const auto args = entry["args"].toArray();
            if (args.at(0).toString() != "install") continue;
            const auto time = entry["time"].toVariant().toLongLong();
            if (entry["event"] == "start") {
                if (serial == "a" && ++aStarts == 2) QVERIFY(time >= ends["a"]);
                if (!starts.contains(serial)) starts[serial] = time;
            } else if (!ends.contains(serial)) ends[serial] = time;
        }
        QCOMPARE(aStarts, 2);
        QVERIFY(starts.contains("a") && starts.contains("b") && ends.contains("a") && ends.contains("b"));
        QVERIFY(starts["a"] < ends["b"] && starts["b"] < ends["a"]);
        qunsetenv("QTSCRCPY_STUB_LOG");
    }
    void customCommandsPreserveShellSyntax() {
        const QString shell = "printf '%s' 'hello world' | tr a-z A-Z";
        QCOMPARE(adbCommandArguments("adb shell " + shell), QStringList({"shell", shell}));
        QCOMPARE(adbCommandArguments("install -r \"C:/test app.apk\""), QStringList({"install", "-r", "C:/test app.apk"}));
        QVERIFY(adbCommandArguments("  ").isEmpty());
        if (!m_stubAvailable) QSKIP("ADB subprocess stub unavailable; command integration not verified");
        Dialog settings;
        DeviceGrid grid(&settings);
        disconnect(&settings, nullptr, &grid, nullptr);
        grid.refreshDevices({"a", "b", "unselected"});
        grid.m_selected = {"a", "b"};
        grid.m_command->setText("adb shell " + shell);
        QTimer::singleShot(50, &grid, [] {
            auto message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (message) QTest::mouseClick(message->button(QMessageBox::Yes), Qt::LeftButton);
        });
        grid.batchCommand();
        QCOMPARE(QStringList(grid.m_activeJobs.keys()), QStringList({"a", "b"}));
        for (auto process : grid.m_activeJobs) QCOMPARE(QStringList(process->arguments().mid(2)), QStringList({"shell", shell}));
        QTRY_VERIFY_WITH_TIMEOUT(grid.m_activeJobs.isEmpty(), 5000);
        QCOMPARE(grid.m_done, 2);
        QCOMPARE(grid.m_failed, 0);
        QVERIFY(grid.m_jobOutput->toPlainText().contains("hello world"));
    }
    void cancellationStopsQueuedAndActiveJobs() {
        if (!m_stubAvailable) QSKIP("ADB subprocess stub unavailable; cancellation integration not verified");
        Dialog settings;
        DeviceGrid grid(&settings);
        grid.m_jobs.enqueue({"a", {"long-running"}, "slow"});
        grid.m_jobs.enqueue({"a", {"install", "later.apk"}, "queued"});
        grid.m_jobs.enqueue({"b", {"long-running"}, "slow"});
        grid.runNextJob();
        QCOMPARE(grid.m_activeJobs.size(), 2);
        QList<QPointer<qsc::AdbProcess>> processes;
        for (auto process : grid.m_activeJobs) processes << process;
        grid.cancelJobs();
        QVERIFY(grid.m_jobs.isEmpty());
        QVERIFY(grid.m_activeJobs.isEmpty());
        for (const auto &process : processes) QTRY_VERIFY(!process || !process->isRuning());
        QCOMPARE(grid.m_done, 0);
    }
    void concurrentConnectionsReserveDistinctPorts() {
        auto &manager = static_cast<qsc::DeviceManage&>(qsc::IDeviceManage::getInstance());
        QTcpServer occupied;
        QVERIFY(occupied.listen(QHostAddress::LocalHost, 0));
        const auto originalStart = manager.m_localPortStart;
        manager.m_localPortStart = occupied.serverPort();
        const quint16 available = manager.getFreePort();
        manager.m_localPortStart = originalStart;
        QVERIFY(available && available != occupied.serverPort());
        qsc::DeviceParams params;
        params.serial = "port-a";
        params.useReverse = true;
        QVERIFY(manager.connectDevice(params));
        params.serial = "port-b";
        params.useReverse = false;
        QVERIFY(manager.connectDevice(params));
        const auto portA = manager.m_ports.value("port-a");
        const auto portB = manager.m_ports.value("port-b");
        QVERIFY(portA && portB && portA != portB);
        manager.disconnectAllDevice();
        QVERIFY(!manager.getDevice("port-a"));
        QVERIFY(!manager.getDevice("port-b"));
        QVERIFY(manager.m_ports.isEmpty());
    }
    void installedAdbParallelRoutingAndCancellation() {
        if (m_stubAvailable || !QFile::exists(qEnvironmentVariable("QTSCRCPY_ADB_PATH")))
            QSKIP("Set QTSCRCPY_ADB_PATH to the installed adb to check real subprocess routing");
        // Host version calls and waits for deliberately nonexistent serials do
        // not install software or operate on any connected phone.
        Dialog settings;
        DeviceGrid grid(&settings);
        const QString a = "qtscrcpy-regression-no-device-a";
        const QString b = "qtscrcpy-regression-no-device-b";
        const QString absent = "qtscrcpy-regression-no-device-error";
        grid.m_jobs.enqueue({a, {"version"}, "version-1"});
        grid.m_jobs.enqueue({a, {"version"}, "version-2"});
        grid.m_jobs.enqueue({b, {"version"}, "version-b"});
        grid.m_jobs.enqueue({absent, {"shell", "getprop ro.product.model"}, "missing device"});
        grid.runNextJob();
        QCOMPARE(grid.m_activeJobs.size(), 3);
        QCOMPARE(grid.m_jobs.size(), 1);
        for (auto it = grid.m_activeJobs.begin(); it != grid.m_activeJobs.end(); ++it) {
            QCOMPARE(it.value()->arguments().value(0), QString("-s"));
            QCOMPARE(it.value()->arguments().value(1), it.key());
        }
        QTRY_VERIFY_WITH_TIMEOUT(grid.m_activeJobs.isEmpty() && grid.m_jobs.isEmpty(), 10000);
        QCOMPARE(grid.m_done, 4);
        QCOMPARE(grid.m_failed, 1);
        const auto output = grid.m_jobOutput->toPlainText();
        QVERIFY(output.contains("Android Debug Bridge version"));
        QVERIFY(output.contains("not found"));
        QVERIFY(output.indexOf(a + QStringLiteral("] 完成：version-1")) < output.indexOf(a + QStringLiteral("] 开始：version-2")));
        grid.m_jobs.enqueue({a, {"wait-for-device"}, "waiting"});
        grid.m_jobs.enqueue({a, {"version"}, "queued"});
        grid.m_jobs.enqueue({b, {"wait-for-device"}, "waiting"});
        grid.runNextJob();
        QCOMPARE(grid.m_activeJobs.size(), 2);
        QCOMPARE(grid.m_jobs.size(), 1);
        QList<QPointer<qsc::AdbProcess>> processes;
        for (auto process : grid.m_activeJobs) processes << process;
        grid.cancelJobs();
        QVERIFY(grid.m_activeJobs.isEmpty() && grid.m_jobs.isEmpty());
        for (const auto &process : processes) QTRY_VERIFY(!process || !process->isRuning());
        QCOMPARE(grid.m_done, 4);
    }
    void disconnectListenersCanAccessDevice() {
        // Emulate a connected device's synchronous notification during teardown.
        class ConnectedDevice : public qsc::Device {
        public:
            explicit ConnectedDevice(qsc::DeviceParams params) : qsc::Device(params) {}
            ~ConnectedDevice() override { emit deviceDisconnected(getSerial()); }
        };
        auto &manager = static_cast<qsc::DeviceManage&>(qsc::IDeviceManage::getInstance());
        qsc::DeviceParams params;
        params.serial = "disconnect-notification";
        auto device = new ConnectedDevice(params);
        manager.m_devices.insert(params.serial, device);
        manager.m_ports.insert(params.serial, 27183);
        connect(device, &qsc::IDevice::deviceDisconnected, &manager, &qsc::DeviceManage::onDeviceDisconnected);
        QObject listener;
        bool available = false;
        connect(&manager, &qsc::IDeviceManage::deviceDisconnected, &listener, [&](const QString &serial) {
            available = manager.getDevice(serial) == device;
        });
        QVERIFY(manager.disconnectDevice(params.serial));
        QVERIFY(available);
        QVERIFY(!manager.getDevice(params.serial));
        QVERIFY(!manager.m_ports.contains(params.serial));
    }
    void unselectedActionsExplainWhatToDo() {
        Dialog settings;
        DeviceGrid grid(&settings);
        disconnect(&settings, nullptr, &grid, nullptr);
        for (auto timer : grid.findChildren<QTimer*>()) timer->stop();
        grid.refreshDevices({"a"});
        QCOMPARE(grid.selectedDevices(), QStringList{"a"});
        grid.m_selected.clear();
        grid.rebuildList();
        grid.show();
        for (const auto &text : QStringList{QStringLiteral("开始投屏"), QStringLiteral("停止投屏"),
                 QStringLiteral("安装 APK"), QStringLiteral("发送文件"), QStringLiteral("发送文字"), QStringLiteral("执行命令"), QStringLiteral("移入当前分组")}) {
            bool explained = false;
            QTimer::singleShot(0, &grid, [&explained] {
                if (auto message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                    explained = message->text().contains(QStringLiteral("勾选"));
                    message->accept();
                }
            });
            for (auto button : grid.findChildren<QPushButton*>())
                if (button->text() == text) QTest::mouseClick(button, Qt::LeftButton);
            QVERIFY2(explained, qPrintable(text));
        }
    }
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
        QVERIFY(grid.m_cards["a"].widget->height() <= grid.m_scroll->viewport()->height());
        auto a = grid.m_cards["a"].video;
        QVERIFY(!a->isWindow());
        QVERIFY(a->parentWidget() == grid.m_cards["a"].widget);
        QSignalSpy zoom(a, &VideoForm::zoomRequested);
        QTest::mouseDClick(a, Qt::LeftButton);
        QCOMPARE(zoom.count(), 1);
        QCOMPARE(grid.m_zoom, QString("a"));
        QVERIFY(grid.m_cards["a"].widget->height() <= grid.m_scroll->viewport()->height());
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
