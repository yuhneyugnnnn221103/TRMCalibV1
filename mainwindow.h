#pragma once

#include <QMainWindow>
#include <QTimer>
#include "user/visaconnection.h"
#include "user/enadriverv2.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// ─────────────────────────────────────────────────────────────────────────────
// MainWindow — Test UI cho EnaDriverV2 (ENA E5080B)
//
// Các chức năng test theo đúng workflow calib TRM:
//   1. Kết nối ENA qua LAN (VISA)
//   2. Load file .csa có sẵn trên máy ENA
//   3. Cấu hình CW frequency, IFBW, power (override từ UI)
//   4. Trigger đơn lẻ → đọc S21 và S43 (SDATA)
//   5. Trigger liên tục (auto-loop) → cập nhật kết quả real-time
//   6. Đọc error queue, hiển thị log
// ─────────────────────────────────────────────────────────────────────────────
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Kết nối / ngắt kết nối
    void onConnectClicked();
    void onDisconnectClicked();

    // Load .csa từ máy ENA
    void onLoadStateClicked();

    // Override thông số (ghi thẳng vào channel 1)
    void onApplyConfigClicked();

    // Đọc cấu hình hiện tại từ máy về UI
    void onReadConfigClicked();

    // Trigger 1 lần → đọc S21 + S43
    void onTriggerOnceClicked();

    // Bắt đầu / dừng trigger liên tục
    void onStartLoopClicked();
    void onStopLoopClicked();

    // Timer tick: trigger + đọc data
    void onLoopTick();

    // Kiểm tra error queue
    void onCheckErrorClicked();

    // Xóa log
    void onClearLogClicked();

private:
    // Helpers
    void log(const QString &msg, bool isError = false);
    void updateMeasDisplay(const ComplexPoint &s21,
                           const ComplexPoint &s43);
    void setConnectedState(bool connected);

    Ui::MainWindow      *ui;
    VisaConnection        *m_visa   = nullptr;
    EnaDriverV2           *m_vna    = nullptr;
    QTimer              *m_loopTimer = nullptr;
};