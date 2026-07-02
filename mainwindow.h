#pragma once

#include <QMainWindow>
#include <QTimer>

// Forward declarations — tránh include nặng trong header
class VisaConnection;
class EnaDriverV2;

class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QDoubleSpinBox;
class QSpinBox;
class QPlainTextEdit;
class QStatusBar;

// ─────────────────────────────────────────────────────────────────────────────
// MainWindow
//
// Test UI thuần code C++ Qt — không dùng .ui / Qt Designer.
// Test các chức năng EnaDriverV2 sẽ dùng trong calib TRM:
//   1. Kết nối ENA qua LAN (VISA)
//   2. Load file .csa có sẵn trên máy ENA
//   3. Override cấu hình CW: tần số, IFBW, power
//   4. Trigger đơn lẻ → đọc S21 và S43
//   5. Loop trigger real-time
//   6. Kiểm tra error queue
// ─────────────────────────────────────────────────────────────────────────────
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onLoadStateClicked();
    void onApplyConfigClicked();
    void onReadConfigClicked();
    void onTriggerOnceClicked();
    void onStartLoopClicked();
    void onStopLoopClicked();
    void onLoopTick();
    void onCheckErrorClicked();
    void onClearLogClicked();

private:
    void buildUi();         // tạo toàn bộ widget bằng code
    void connectSignals();  // kết nối signals/slots

    void log(const QString &msg, bool isError = false);
    void updateMeasDisplay(double s21Amp, double s21Phase,
                           double s43Amp, double s43Phase);
    void setConnectedState(bool connected);

    // ── Instruments ──────────────────────────────────────────────────────────
    VisaConnection *m_visa  = nullptr;
    EnaDriverV2    *m_vna   = nullptr;
    QTimer       *m_loopTimer = nullptr;

    // ── Widgets: Connection ──────────────────────────────────────────────────
    QLineEdit    *m_editVisaAddr  = nullptr;
    QPushButton  *m_btnConnect    = nullptr;
    QPushButton  *m_btnDisconnect = nullptr;

    // ── Widgets: Load State ──────────────────────────────────────────────────
    QLineEdit    *m_editCsaPath   = nullptr;
    QPushButton  *m_btnLoadState  = nullptr;

    // ── Widgets: Config ──────────────────────────────────────────────────────
    QDoubleSpinBox *m_spinFreqGHz  = nullptr;
    QDoubleSpinBox *m_spinIFBWkHz  = nullptr;
    QDoubleSpinBox *m_spinPowerdBm = nullptr;
    QPushButton    *m_btnApply     = nullptr;
    QPushButton    *m_btnReadCfg   = nullptr;
    QLabel         *m_lblSweepInfo = nullptr;
    QLabel         *m_lblTraces    = nullptr;
    QGroupBox      *m_grpConfig    = nullptr;

    // ── Widgets: Trigger ─────────────────────────────────────────────────────
    QPushButton  *m_btnTrigOnce   = nullptr;
    QPushButton  *m_btnStartLoop  = nullptr;
    QPushButton  *m_btnStopLoop   = nullptr;
    QSpinBox     *m_spinDelay     = nullptr;

    // ── Widgets: Measurement result ──────────────────────────────────────────
    QLabel       *m_lblS21Amp     = nullptr;
    QLabel       *m_lblS21Phase   = nullptr;
    QLabel       *m_lblS43Amp     = nullptr;
    QLabel       *m_lblS43Phase   = nullptr;

    // ── Widgets: Log ─────────────────────────────────────────────────────────
    QPlainTextEdit *m_log         = nullptr;
    QPushButton    *m_btnCheckErr = nullptr;
    QPushButton    *m_btnClearLog = nullptr;
};