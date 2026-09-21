#include "devicegrid.h"
#include "dialog.h"
#include "videoform.h"
#include "config.h"
#include "../groupcontroller/groupcontroller.h"
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

DeviceGrid::DeviceGrid(Dialog *settings, QWidget *parent)
    : QWidget(parent), m_settings(settings), m_transfer(this)
{
    settings->setGridMode();
    m_transferTimeout = new QTimer(this);
    m_transferTimeout->setSingleShot(true);
    connect(m_transferTimeout, &QTimer::timeout, this, [this] { m_transfer.kill(); });
    setWindowTitle(QStringLiteral("婚字头 · 多设备工作台"));
    resize(1320, 860);
    setMinimumSize(900, 620);
    setStyleSheet(
        "DeviceGrid {background:#f3f5f9;color:#182336;}"
        "QWidget {font-family:'PingFang SC';font-size:13px;}"
        "QWidget#sidebar,QWidget#workspace {background:#ffffff;border:1px solid #e3e8ef;border-radius:12px;}"
        "QLabel,QCheckBox {color:#344054;border:none;background:transparent;}"
        "QPushButton {background:#ffffff;color:#344054;border:1px solid #dce2eb;border-radius:7px;padding:0px 12px;min-height:36px;}"
        "QPushButton:hover {background:#f0f4ff;border-color:#b7c9ee;}"
        "QPushButton:pressed {background:#e5edff;}"
        "QPushButton:disabled {color:#a0aaba;background:#f5f7fa;}"
        "QPushButton#primary {background:#3765e8;color:white;border:1px solid #3765e8;font-weight:600;}"
        "QPushButton#primary:hover {background:#2855d8;}"
        "QPushButton#quiet {background:transparent;border:none;color:#68778e;}"
        "QLineEdit,QComboBox,QSpinBox {background:#f8f9fc;color:#27364b;border:1px solid #e0e5ee;border-radius:7px;padding:0px 10px;min-height:36px;selection-background-color:#3765e8;}"
        "QLineEdit:focus,QComboBox:focus {border-color:#7697ee;background:white;}"
        "QComboBox QAbstractItemView {background:white;color:#27364b;selection-background-color:#e9efff;selection-color:#2449a7;}"
        "QListWidget {background:#f8f9fc;color:#344054;border:none;border-radius:8px;outline:none;}"
        "QListWidget::item {padding:14px 8px;border-bottom:1px solid #edf0f5;}"
        "QListWidget::item:selected {background:#eaf0ff;color:#2449a7;}"
        "QCheckBox {spacing:7px;}"
        "QCheckBox::indicator {width:16px;height:16px;}"
        "QSplitter {background:#f3f5f9;border:none;}"
        "QSplitter::handle {background:#f3f5f9;border:none;width:16px;}"
        "QComboBox::drop-down {border:none;background:#74849c;width:24px;border-top-right-radius:6px;border-bottom-right-radius:6px;}"
        "QSpinBox::up-button,QSpinBox::down-button {background:#74849c;border:none;width:20px;}"
        "QCheckBox::indicator:unchecked {image:none;background:white;border:1px solid #bbc6d6;border-radius:4px;}"
        "QScrollArea {border:none;background:#151b26;border-radius:10px;}"
        "QWidget#gridBody {background:#151b26;}"
        "QWidget#deviceCard {background:#222c3d;border:1px solid #344157;border-radius:10px;}"
        "QWidget#deviceCard QCheckBox {color:#e9edf5;}"
        "QScrollBar:vertical {background:transparent;width:7px;margin:4px;}"
        "QScrollBar::handle:vertical {background:#b9c3d3;border-radius:3px;min-height:28px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical {height:0px;}"
    );
    auto root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 12);
    root->setSpacing(18);
    auto top = new QHBoxLayout;
    auto title = new QLabel(QStringLiteral("婚字头 · 多设备工作台"));
    title->setStyleSheet("font-size:23px;font-weight:600;color:#16243b;padding:6px 0;");
    top->addWidget(title);
    top->addStretch();
    auto button = [](const QString &text, QBoxLayout *layout) {
        auto b = new QPushButton(text); layout->addWidget(b); return b;
    };
    auto refresh = button(QStringLiteral("刷新设备"), top);
    auto config = button(QStringLiteral("连接与设置"), top);
    connect(refresh, &QPushButton::clicked, settings, &Dialog::refreshGridDevices);
    connect(config, &QPushButton::clicked, settings, &Dialog::bringToFront);
    root->addLayout(top);
    auto splitter = new QSplitter;
    splitter->setHandleWidth(16);
    auto side = new QWidget;
    side->setObjectName("sidebar");
    side->setMinimumWidth(260);
    auto left = new QVBoxLayout(side);
    left->setContentsMargins(16, 18, 16, 16);
    left->setSpacing(12);
    auto deviceTitle = new QLabel(QStringLiteral("设备管理"));
    deviceTitle->setStyleSheet("font-size:16px;font-weight:600;color:#182b49;");
    left->addWidget(deviceTitle);
    m_search = new QLineEdit;
    m_search->setPlaceholderText(QStringLiteral("搜索设备名称或序列号"));
    left->addWidget(m_search);
    auto groupRow = new QHBoxLayout;
    m_groups = new QComboBox;
    m_groups->addItem(QStringLiteral("全部设备"), "");
    QSettings saved;
    for (const auto &name : saved.value("grid/groups").toStringList()) m_groups->addItem(name, name);
    saved.beginGroup("grid/membership");
    for (const auto &key : saved.childKeys())
        m_membership[QString::fromUtf8(QByteArray::fromHex(key.toLatin1()))] = saved.value(key).toString();
    saved.endGroup();
    groupRow->addWidget(m_groups, 1);
    auto add = button("+", groupRow);
    auto remove = button("−", groupRow);
    add->setFixedWidth(34); remove->setFixedWidth(34);
    add->setToolTip(QStringLiteral("新建分组")); remove->setToolTip(QStringLiteral("删除当前分组"));
    left->addLayout(groupRow);
    auto assign = new QPushButton(QStringLiteral("移入当前分组"));
    left->addWidget(assign);
    auto selection = new QHBoxLayout;
    auto all = button(QStringLiteral("全选可见"), selection);
    auto none = button(QStringLiteral("清空勾选"), selection);
    all->setObjectName("quiet"); none->setObjectName("quiet");
    left->addLayout(selection);
    m_list = new QListWidget;
    left->addWidget(m_list, 1);
    auto hint = new QLabel(QStringLiteral("1  勾选设备并开始投屏\n2  开启同步，点击任一已勾选画面作为主控\n3  双击画面放大，再次双击返回"));
    hint->setWordWrap(true);
    hint->setStyleSheet("color:#8894a8;font-size:11px;padding:8px 0;");
    left->addWidget(hint);
    splitter->addWidget(side);
    auto right = new QWidget;
    right->setObjectName("workspace");
    auto content = new QVBoxLayout(right);
    content->setContentsMargins(16, 16, 16, 12);
    content->setSpacing(14);
    auto actions = new QHBoxLayout;
    auto start = button(QStringLiteral("开始投屏"), actions);
    start->setObjectName("primary");
    auto stop = button(QStringLiteral("停止投屏"), actions);
    auto apk = button(QStringLiteral("安装 APK"), actions);
    auto files = button(QStringLiteral("发送文件"), actions);
    m_sync = new QCheckBox(QStringLiteral("同步操作"));
    actions->addWidget(m_sync);
    actions->addStretch();
    m_columns = new QSpinBox;
    m_columns->setRange(1, 6);
    m_columns->setValue(3);
    m_columns->setSuffix(QStringLiteral(" 列"));
    actions->addWidget(m_columns);
    content->addLayout(actions);
    m_scroll = new QScrollArea;
    m_scroll->setWidgetResizable(true);
    auto body = new QWidget;
    body->setObjectName("gridBody");
    m_grid = new QGridLayout(body);
    m_grid->setSpacing(12);
    m_grid->setAlignment(Qt::AlignTop);
    m_scroll->setWidget(body);
    content->addWidget(m_scroll, 1);
    m_empty = new QLabel(QStringLiteral("<div style='color:#e5ebf5;font-size:24px;font-weight:600'>连接设备，开始工作</div><br><div style='color:#93a3bc;font-size:14px'>使用 USB 连接安卓手机，并允许 USB 调试<br><br>在左侧勾选设备，点击「开始投屏」</div>"));
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setMinimumHeight(360);
    m_empty->setStyleSheet("font-size:17px;color:#91a4b8;");
    m_grid->addWidget(m_empty, 0, 0);
    auto textRow = new QHBoxLayout;
    auto input = new QLineEdit;
    input->setPlaceholderText(QStringLiteral("输入文字，发送到勾选手机的当前输入框"));
    auto send = new QPushButton(QStringLiteral("发送文字"));
    send->setObjectName("primary");
    textRow->addWidget(input, 1);
    textRow->addWidget(send);
    content->addLayout(textRow);
    m_jobsLabel = new QLabel(QStringLiteral("批量文件发送到 /sdcard/Download/"));
    m_jobsLabel->setStyleSheet("color:#98a2b3;font-size:11px;");
    content->addWidget(m_jobsLabel);
    splitter->addWidget(right);
    splitter->setSizes({290, 1010});
    splitter->setChildrenCollapsible(false);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    root->addWidget(splitter, 1);
    m_status = new QLabel;
    m_status->setStyleSheet("color:#76849a;font-size:12px;padding:0px 2px;");
    root->addWidget(m_status);

    connect(settings, &Dialog::gridDevicesChanged, this, &DeviceGrid::refreshDevices);
    connect(settings, &Dialog::gridVideoReady, this, &DeviceGrid::addVideo);
    connect(settings, &Dialog::gridDeviceStopped, this, &DeviceGrid::removeVideo);
    connect(&qsc::IDeviceManage::getInstance(), &qsc::IDeviceManage::deviceConnected, this,
        [this](bool success, const QString &serial, const QString &, const QSize &) {
        if (!success) m_status->setText(QStringLiteral("投屏连接失败：%1，请检查手机授权；详细原因见连接与设置").arg(serial));
    });
    connect(m_search, &QLineEdit::textChanged, this, &DeviceGrid::rebuildList);
    connect(m_groups, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { rebuildList(); });
    connect(m_columns, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] { relayout(); });
    connect(m_sync, &QCheckBox::toggled, this, [this] { syncTargets(); });
    connect(m_list, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
        const QString serial = item->data(Qt::UserRole).toString();
        if (item->checkState() == Qt::Checked) m_selected.insert(serial); else m_selected.remove(serial);
        if (m_cards.contains(serial)) { QSignalBlocker block(m_cards[serial].check); m_cards[serial].check->setChecked(m_selected.contains(serial)); }
        syncTargets();
    });
    connect(all, &QPushButton::clicked, this, [this] {
        for (int i = 0; i < m_list->count(); ++i) m_list->item(i)->setCheckState(Qt::Checked);
    });
    connect(none, &QPushButton::clicked, this, [this] { m_selected.clear(); rebuildList(); syncTargets(); });
    connect(add, &QPushButton::clicked, this, [this] {
        bool ok;
        const QString name = QInputDialog::getText(this, QStringLiteral("新建分组"), QStringLiteral("分组名称"), QLineEdit::Normal, "", &ok).trimmed();
        if (!ok || name.isEmpty()) return;
        int index = m_groups->findData(name);
        if (index < 0) { m_groups->addItem(name, name); index = m_groups->count() - 1; }
        m_groups->setCurrentIndex(index); saveGroups();
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        const QString group = m_groups->currentData().toString();
        if (group.isEmpty()) return;
        for (auto it = m_membership.begin(); it != m_membership.end();) {
            if (it.value() == group) it = m_membership.erase(it); else ++it;
        }
        m_groups->removeItem(m_groups->currentIndex()); saveGroups(); rebuildList();
    });
    connect(assign, &QPushButton::clicked, this, [this] {
        QStringList groups{QStringLiteral("未分组")};
        for (int i = 1; i < m_groups->count(); ++i) groups << m_groups->itemText(i);
        if (m_selected.isEmpty()) { m_status->setText(QStringLiteral("请先勾选设备")); return; }
        bool ok;
        QString group = QInputDialog::getItem(this, QStringLiteral("移动设备"), QStringLiteral("目标分组"), groups, 0, false, &ok);
        if (!ok) return;
        for (const auto &serial : m_selected) {
            if (group == groups.first()) m_membership.remove(serial); else m_membership[serial] = group;
        }
        saveGroups(); rebuildList();
    });
    connect(start, &QPushButton::clicked, this, [this] {
        for (const auto &serial : selectedDevices()) m_settings->startGridDevice(serial);
    });
    connect(stop, &QPushButton::clicked, this, [this] {
        const auto selected = selectedDevices();
        for (const auto &serial : selected) qsc::IDeviceManage::getInstance().disconnectDevice(serial);
    });
    connect(apk, &QPushButton::clicked, this, [this] { batchFiles(true); });
    connect(files, &QPushButton::clicked, this, [this] { batchFiles(false); });
    connect(send, &QPushButton::clicked, this, [this, input] {
        if (input->text().isEmpty()) return;
        GroupController::instance().setGridTargets({}, "", false);
        QApplication::clipboard()->setText(input->text());
        int count = 0;
        for (const auto &serial : selectedDevices()) {
            auto device = qsc::IDeviceManage::getInstance().getDevice(serial);
            if (device && m_cards.contains(serial)) { device->setDeviceClipboard(true); ++count; }
        }
        syncTargets();
        m_status->setText(QStringLiteral("已向 %1 台手机发送粘贴请求").arg(count));
    });
    connect(&m_transfer, &qsc::AdbProcess::adbProcessResult, this, [this](qsc::AdbProcess::ADB_EXEC_RESULT result) {
        if (result == qsc::AdbProcess::AER_SUCCESS_START) return;
        if (!m_transferring) return;
        m_transferTimeout->stop();
        const Job job = m_jobs.dequeue();
        bool success = result == qsc::AdbProcess::AER_SUCCESS_EXEC;
        if (!success) ++m_failed;
        ++m_done;
        m_transferring = false;
        m_jobsLabel->setText(QStringLiteral("已处理 %1 · 失败 %2 · 剩余 %3 ｜ %4：%5")
            .arg(m_done).arg(m_failed).arg(m_jobs.size()).arg(job.serial, success ? QStringLiteral("完成") : m_transfer.getErrorOut().left(160)));
        QTimer::singleShot(0, this, &DeviceGrid::runNextJob);
    });
    auto timer = new QTimer(this);
    connect(timer, &QTimer::timeout, settings, &Dialog::refreshGridDevices);
    timer->start(5000);
    QTimer::singleShot(0, settings, &Dialog::refreshGridDevices);
    syncTargets();
}

QStringList DeviceGrid::selectedDevices() const
{
    QStringList result;
    for (const auto &serial : m_devices) if (m_selected.contains(serial)) result << serial;
    return result;
}

void DeviceGrid::refreshDevices(const QStringList &serials)
{
    m_devices = serials;
    for (auto it = m_selected.begin(); it != m_selected.end();) {
        if (!serials.contains(*it)) it = m_selected.erase(it); else ++it;
    }
    rebuildList();
    syncTargets();
}

void DeviceGrid::rebuildList()
{
    QSignalBlocker block(m_list);
    m_list->clear();
    const QString search = m_search->text().trimmed();
    const QString group = m_groups->currentData().toString();
    for (const auto &serial : m_devices) {
        const QString name = m_names.value(serial, Config::getInstance().getNickName(serial));
        if (!group.isEmpty() && m_membership.value(serial) != group) continue;
        if (!search.isEmpty() && !(name + serial).contains(search, Qt::CaseInsensitive)) continue;
        auto item = new QListWidgetItem((name.isEmpty() ? serial : name + "\n" + serial) +
            (m_cards.contains(serial) ? QStringLiteral("  · 投屏中") : QStringLiteral("  · 已连接")), m_list);
        item->setData(Qt::UserRole, serial);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(m_selected.contains(serial) ? Qt::Checked : Qt::Unchecked);
    }
    for (auto it = m_cards.begin(); it != m_cards.end(); ++it) {
        QSignalBlocker check(it->check); it->check->setChecked(m_selected.contains(it.key()));
    }
    relayout();
}

void DeviceGrid::addVideo(const QString &serial, const QString &name, VideoForm *video)
{
    if (m_cards.contains(serial)) return;
    m_names[serial] = name;
    auto card = new QWidget;
    card->setObjectName("deviceCard");
    auto layout = new QVBoxLayout(card);
    layout->setContentsMargins(8, 8, 8, 8);
    auto row = new QHBoxLayout;
    auto check = new QCheckBox(name.isEmpty() ? serial : name);
    check->setToolTip(serial);
    check->setChecked(m_selected.contains(serial));
    row->addWidget(check, 1);
    auto zoom = new QPushButton(QStringLiteral("放大"));
    zoom->setFixedWidth(60);
    row->addWidget(zoom);
    layout->addLayout(row);
    video->setParent(card);
    video->setWindowFlags(Qt::Widget);
    layout->addWidget(video, 1);
    video->show();
    m_cards.insert(serial, {card, check, video});
    connect(check, &QCheckBox::toggled, this, [this, serial](bool selected) {
        if (selected) m_selected.insert(serial); else m_selected.remove(serial);
        rebuildList(); syncTargets();
    });
    auto toggle = [this, serial] { m_zoom = m_zoom == serial ? QString() : serial; relayout(); };
    connect(zoom, &QPushButton::clicked, this, toggle);
    connect(video, &VideoForm::zoomRequested, this, toggle);
    connect(video, &VideoForm::activated, this, [this](const QString &host) { m_host = host; syncTargets(); });
    rebuildList(); syncTargets();
}

void DeviceGrid::removeVideo(const QString &serial)
{
    if (!m_cards.contains(serial)) return;
    auto card = m_cards.take(serial);
    m_grid->removeWidget(card.widget);
    card.widget->hide();
    card.widget->deleteLater();
    if (m_zoom == serial) m_zoom.clear();
    if (m_host == serial) m_host.clear();
    rebuildList(); syncTargets();
}

void DeviceGrid::relayout()
{
    while (auto item = m_grid->takeAt(0)) delete item;
    QSet<QString> visible;
    for (int i = 0; i < m_list->count(); ++i) visible.insert(m_list->item(i)->data(Qt::UserRole).toString());
    if (!m_zoom.isEmpty() && !visible.contains(m_zoom)) m_zoom.clear();
    const int columns = m_zoom.isEmpty() ? m_columns->value() : 1;
    int count = 0;
    for (auto it = m_cards.begin(); it != m_cards.end(); ++it) {
        const bool show = visible.contains(it.key()) && (m_zoom.isEmpty() || m_zoom == it.key());
        it->widget->setVisible(show);
        if (!show) continue;
        int width = qMax(180, (m_scroll->viewport()->width() - 32 - 12 * (columns - 1)) / columns);
        int height = m_zoom.isEmpty() ? qBound(280, int(width * 1.8), 580) : qMax(450, m_scroll->viewport()->height() - 24);
        it->widget->setFixedHeight(height);
        m_grid->addWidget(it->widget, count / columns, count % columns);
        ++count;
    }
    m_empty->setVisible(count == 0);
    if (!count) m_grid->addWidget(m_empty, 0, 0);
}

void DeviceGrid::syncTargets()
{
    GroupController::instance().setGridTargets(selectedDevices(), m_host, m_sync->isChecked());
    m_status->setText(QStringLiteral("在线 %1 台 · 投屏 %2 台 · 勾选 %3 台   %4")
        .arg(m_devices.size()).arg(m_cards.size()).arg(m_selected.size())
        .arg(m_sync->isChecked() ? (m_selected.contains(m_host) ? QStringLiteral("同步已开启 · 主控 ") + m_names.value(m_host, m_host) : QStringLiteral("请点击已勾选画面选择主控")) : QStringLiteral("同步未开启")));
}

void DeviceGrid::saveGroups()
{
    QSettings settings;
    QStringList names;
    for (int i = 1; i < m_groups->count(); ++i) names << m_groups->itemData(i).toString();
    settings.setValue("grid/groups", names);
    settings.beginGroup("grid/membership");
    settings.remove("");
    for (auto it = m_membership.begin(); it != m_membership.end(); ++it)
        settings.setValue(QString::fromLatin1(it.key().toUtf8().toHex()), it.value());
    settings.endGroup();
}

void DeviceGrid::batchFiles(bool apk)
{
    const auto targets = selectedDevices();
    if (targets.isEmpty()) { m_status->setText(QStringLiteral("请先勾选设备")); return; }
    const QStringList files = QFileDialog::getOpenFileNames(this, apk ? QStringLiteral("选择 APK") : QStringLiteral("选择文件"), "", apk ? "*.apk" : "*");
    if (files.isEmpty()) return;
    if (QMessageBox::question(this, QStringLiteral("批量操作"), QStringLiteral("将 %1 个文件%2到 %3 台勾选手机？").arg(files.size()).arg(apk ? QStringLiteral("安装") : QStringLiteral("发送")).arg(targets.size())) != QMessageBox::Yes) return;
    if (m_jobs.isEmpty()) { m_done = 0; m_failed = 0; }
    for (const auto &serial : targets) for (const auto &file : files) m_jobs.enqueue({serial, file, apk});
    runNextJob();
}

void DeviceGrid::runNextJob()
{
    if (m_transferring || m_jobs.isEmpty()) return;
    m_transferring = true;
    m_transferTimeout->start(120000);
    const auto &job = m_jobs.head();
    m_jobsLabel->setText(QStringLiteral("正在%1：%2 → %3 · 待处理 %4")
        .arg(job.apk ? QStringLiteral("安装") : QStringLiteral("发送"), QFileInfo(job.file).fileName(), job.serial).arg(m_jobs.size()));
    if (job.apk) m_transfer.install(job.serial, job.file);
    else m_transfer.push(job.serial, job.file, "/sdcard/Download/" + QFileInfo(job.file).fileName());
}

void DeviceGrid::resizeEvent(QResizeEvent *event) { QWidget::resizeEvent(event); relayout(); }
void DeviceGrid::closeEvent(QCloseEvent *event)
{
    if (!m_jobs.isEmpty() && QMessageBox::question(this, QStringLiteral("退出"), QStringLiteral("还有文件正在处理，确定中止并退出？")) != QMessageBox::Yes) { event->ignore(); return; }
    m_transfer.kill();
    GroupController::instance().setGridTargets({}, "", false);
    qsc::IDeviceManage::getInstance().disconnectAllDevice();
    event->accept();
    qApp->quit();
}
