#ifndef DEVICEGRID_H
#define DEVICEGRID_H
#include <QWidget>
#include <QMap>
#include <QSet>
#include <QStringList>
#include <QPointer>
#include <QQueue>
#include "adbprocess.h"

class Dialog;
class VideoForm;
class QListWidget;
class QComboBox;
class QLineEdit;
class QCheckBox;
class QLabel;
class QGridLayout;
class QScrollArea;
class QSpinBox;
class QTimer;
class QPlainTextEdit;
class DeviceGrid : public QWidget {
    Q_OBJECT
    friend class DeviceGridTest;
public:
    explicit DeviceGrid(Dialog *settings, QWidget *parent = nullptr);
protected:
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
private:
    void refreshDevices(const QStringList &serials);
    void rebuildList();
    void relayout();
    void syncTargets();
    void addVideo(const QString &serial, const QString &name, VideoForm *video);
    void removeVideo(const QString &serial);
    void saveGroups();
    void batchFiles(bool apk);
    void runNextJob();
    void batchCommand();
    void cancelJobs();
    void updateJobsLabel();
    QStringList selectedDevices() const;
    QStringList actionTargets();
    struct Card { QWidget *widget = nullptr; QCheckBox *check = nullptr; VideoForm *video = nullptr; };
    struct Job { QString serial; QStringList args; QString description; };
    Dialog *m_settings;
    QListWidget *m_list;
    QComboBox *m_groups;
    QLineEdit *m_search;
    QCheckBox *m_sync;
    QLabel *m_status;
    QLabel *m_empty;
    QLabel *m_jobsLabel;
    QLineEdit *m_command;
    QPlainTextEdit *m_jobOutput;
    QGridLayout *m_grid;
    QScrollArea *m_scroll;
    QSpinBox *m_columns;
    QMap<QString, Card> m_cards;
    QMap<QString, QString> m_names;
    QMap<QString, QString> m_membership;
    QStringList m_devices;
    QSet<QString> m_selected;
    QString m_host;
    QString m_zoom;
    QQueue<Job> m_jobs;
    QMap<QString, qsc::AdbProcess*> m_activeJobs;
    int m_done = 0;
    int m_failed = 0;
};
#endif
