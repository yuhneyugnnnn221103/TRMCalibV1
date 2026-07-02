#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDateTime>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("VNA Driver Test — ENA E5080B");

    // Timer cho chế độ loop
    m_loopTimer = new QTimer(this);
    m_loopTimer->setInterval(ui->spinLoopDelay->value());
    connect(m_loopTimer, &QTimer::timeout, this, &MainWindow::onLoopTick);

    // Kết nối signals từ UI
    connect(ui->btnConnect,    &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(ui->btnDisconnect, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);
    connect(ui->btnLoadState,  &QPushButton::clicked, this, &MainWindow::onLoadStateClicked);
    connect(ui->btnApplyConfig,&QPushButton::clicked, this, &MainWindow::onApplyConfigClicked);
    connect(ui->btnReadConfig, &QPushButton::clicked, this, &MainWindow::onReadConfigClicked);
    connect(ui->btnTrigOnce,   &QPushButton::clicked, this, &MainWindow::onTriggerOnceClicked);
    connect(ui->btnStartLoop,  &QPushButton::clicked, this, &MainWindow::onStartLoopClicked);
    connect(ui->btnStopLoop,   &QPushButton::clicked, this, &MainWindow::onStopLoopClicked);
    connect(ui->btnCheckErr,   &QPushButton::clicked, this, &MainWindow::onCheckErrorClicked);
    connect(ui->btnClearLog,   &QPushButton::clicked, this, &MainWindow::onClearLogClicked);

    // Đồng bộ interval timer khi người dùng đổi spinbox
    connect(ui->spinLoopDelay, QOverload<int>::of(&QSpinBox::valueChanged),
            m_loopTimer, &QTimer::setInterval);

    setConnectedState(false);
    log("Sẵn sàng. Nhập địa chỉ VISA và nhấn Connect.");
}

MainWindow::~MainWindow()
{
    m_loopTimer->stop();
    if (m_vna)  { m_vna->disconnect(); delete m_vna;  }
    if (m_visa) { delete m_visa; }
    delete ui;
}

// ─────────────────────────────────────────────────────────────────────────────
// Kết nối / ngắt kết nối
// ─────────────────────────────────────────────────────────────────────────────

void MainWindow::onConnectClicked()
{
    QString addr = ui->editVisaAddr->text().trimmed();
    if (addr.isEmpty()) {
        log("[ERR] Địa chỉ VISA trống.", true);
        return;
    }

    // Dọn session cũ nếu có
    if (m_vna)  { m_vna->disconnect(); delete m_vna;  m_vna  = nullptr; }
    if (m_visa) { delete m_visa;                       m_visa = nullptr; }

    log(QString("Đang kết nối: %1 ...").arg(addr));

    m_visa = new VisaResource(addr);
    m_vna  = new EnaDriverV2(m_visa);

    if (!m_vna->connect()) {
        log("[ERR] Kết nối thất bại: " + m_visa->lastError(), true);
        delete m_vna;  m_vna  = nullptr;
        delete m_visa; m_visa = nullptr;
        return;
    }

    log("[OK]  Kết nối thành công: " + m_vna->idn());
    setConnectedState(true);
}

void MainWindow::onDisconnectClicked()
{
    m_loopTimer->stop();
    if (m_vna) {
        m_vna->disconnect();
        delete m_vna;  m_vna  = nullptr;
        delete m_visa; m_visa = nullptr;
    }
    setConnectedState(false);
    log("Đã ngắt kết nối.");
}

// ─────────────────────────────────────────────────────────────────────────────
// Load .csa
// ─────────────────────────────────────────────────────────────────────────────

void MainWindow::onLoadStateClicked()
{
    QString path = ui->editCsaPath->text().trimmed();
    if (path.isEmpty()) {
        log("[ERR] Đường dẫn .csa trống.", true);
        return;
    }

    log(QString("Load state: %1 ...").arg(path));

    if (!m_vna->loadState(path)) {
        log("[ERR] Load state thất bại. " + m_vna->errorQueue(), true);
        return;
    }

    // Sau khi load state: ghi đè trigger về BUS mode cho automation
    if (!m_vna->setTriggerBusMode()) {
        log("[ERR] setTriggerBusMode thất bại.", true);
        return;
    }
    m_vna->setDisplayUpdate(false);

    log("[OK]  Load state xong. Trigger đã set về BUS mode.");

    // Tự động đọc lại cấu hình để đồng bộ UI
    onReadConfigClicked();
}

// ─────────────────────────────────────────────────────────────────────────────
// Cấu hình
// ─────────────────────────────────────────────────────────────────────────────

void MainWindow::onApplyConfigClicked()
{
    double freq  = ui->spinFreqGHz->value()  * 1e9;
    double ifbw  = ui->spinIFBWkHz->value()  * 1e3;
    double power = ui->spinPowerdBm->value();

    log(QString("Áp dụng cấu hình: freq=%1 GHz, IFBW=%2 kHz, power=%3 dBm")
            .arg(freq/1e9, 0, 'f', 3)
            .arg(ifbw/1e3, 0, 'f', 1)
            .arg(power,    0, 'f', 1));

    bool ok = m_vna->setCwFrequency(freq)
              && m_vna->setIFBW(ifbw)
              && m_vna->setAllPortPower(power);

    if (ok) log("[OK]  Cấu hình đã áp dụng.");
    else    log("[ERR] Áp dụng cấu hình thất bại: " + m_vna->errorQueue(), true);
}

void MainWindow::onReadConfigClicked()
{
    ChannelConfig cfg = m_vna->getChannelConfig(1);

    // Điền vào spinbox (block signals để tránh kích hoạt không cần thiết)
    ui->spinFreqGHz->blockSignals(true);
    ui->spinIFBWkHz->blockSignals(true);
    ui->spinPowerdBm->blockSignals(true);

    ui->spinFreqGHz ->setValue(cfg.cwFreqHz   / 1e9);
    ui->spinIFBWkHz ->setValue(cfg.ifBwHz     / 1e3);
    ui->spinPowerdBm->setValue(cfg.port1PowerDbm);

    ui->spinFreqGHz->blockSignals(false);
    ui->spinIFBWkHz->blockSignals(false);
    ui->spinPowerdBm->blockSignals(false);

    // Hiển thị danh sách trace
    ui->labelTraces->setText("Traces: " + cfg.traceNames.join(", "));
    ui->labelSweepType->setText("Sweep: " + cfg.sweepType
                                + QString("  Points: %1").arg(cfg.points));

    log(QString("[INFO] Config: %1 GHz | IFBW %2 kHz | Traces: %3")
            .arg(cfg.cwFreqHz/1e9, 0, 'f', 3)
            .arg(cfg.ifBwHz/1e3,   0, 'f', 1)
            .arg(cfg.traceNames.join(", ")));
}

// ─────────────────────────────────────────────────────────────────────────────
// Trigger 1 lần
// ─────────────────────────────────────────────────────────────────────────────

void MainWindow::onTriggerOnceClicked()
{
    // Trigger + đợi hoàn thành
    if (!m_vna->trigger()) {
        log("[ERR] Trigger thất bại: " + m_vna->errorQueue(), true);
        return;
    }

    // Đọc S21 (TRM A — Port1→Port2)
    m_vna->selectTrace(EnaDriverV2::TRACE_S21);
    ComplexPoint s21 = m_vna->readComplexPoint();

    // Đọc S43 (TRM B — Port3→Port4)
    m_vna->selectTrace(EnaDriverV2::TRACE_S43);
    ComplexPoint s43 = m_vna->readComplexPoint();

    updateMeasDisplay(s21, s43);
}

// ─────────────────────────────────────────────────────────────────────────────
// Trigger liên tục (loop)
// ─────────────────────────────────────────────────────────────────────────────

void MainWindow::onStartLoopClicked()
{
    log(QString("Bắt đầu loop trigger (interval %1 ms) ...")
            .arg(ui->spinLoopDelay->value()));
    ui->btnStartLoop->setEnabled(false);
    ui->btnStopLoop ->setEnabled(true);
    m_loopTimer->start();
}

void MainWindow::onStopLoopClicked()
{
    m_loopTimer->stop();
    ui->btnStartLoop->setEnabled(true);
    ui->btnStopLoop ->setEnabled(false);
    log("Dừng loop.");
}

void MainWindow::onLoopTick()
{
    // Trigger + đọc S21 + S43 trong mỗi tick
    // Nếu VNA chưa kết nối, dừng loop
    if (!m_vna || !m_vna->isConnected()) {
        onStopLoopClicked();
        return;
    }

    if (!m_vna->trigger()) {
        log("[ERR] Trigger lỗi — dừng loop.", true);
        onStopLoopClicked();
        return;
    }

    m_vna->selectTrace(EnaDriverV2::TRACE_S21);
    ComplexPoint s21 = m_vna->readComplexPoint();

    m_vna->selectTrace(EnaDriverV2::TRACE_S43);
    ComplexPoint s43 = m_vna->readComplexPoint();

    updateMeasDisplay(s21, s43);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tiện ích
// ─────────────────────────────────────────────────────────────────────────────

void MainWindow::onCheckErrorClicked()
{
    QStringList errs = m_vna->allErrors();
    if (errs.isEmpty())
        log("[INFO] Không có lỗi SCPI.");
    else
        for (const QString &e : errs)
            log("[SCPI ERR] " + e, true);
}

void MainWindow::onClearLogClicked()
{
    ui->textLog->clear();
}

void MainWindow::log(const QString &msg, bool isError)
{
    QString ts   = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString line = QString("[%1] %2").arg(ts, msg);

    // Màu đỏ cho lỗi — dùng HTML append
    if (isError)
        ui->textLog->appendHtml(
            QString("<span style='color:#c0392b;'>%1</span>").arg(line));
    else
        ui->textLog->appendPlainText(line);

    ui->textLog->ensureCursorVisible();
}

void MainWindow::updateMeasDisplay(const ComplexPoint &s21,
                                   const ComplexPoint &s43)
{
    // S21 — TRM A
    if (s21.valid) {
        ui->lblS21Amp  ->setText(QString("%1 dB").arg(s21.magnitudeDb(), 0, 'f', 3));
        ui->lblS21Phase->setText(QString("%1 °") .arg(s21.phaseDeg(),    0, 'f', 2));
    } else {
        ui->lblS21Amp  ->setText("--");
        ui->lblS21Phase->setText("--");
    }

    // S43 — TRM B
    if (s43.valid) {
        ui->lblS43Amp  ->setText(QString("%1 dB").arg(s43.magnitudeDb(), 0, 'f', 3));
        ui->lblS43Phase->setText(QString("%1 °") .arg(s43.phaseDeg(),    0, 'f', 2));
    } else {
        ui->lblS43Amp  ->setText("--");
        ui->lblS43Phase->setText("--");
    }

    // Log ngắn gọn (không log trong loop để không spam)
    if (!m_loopTimer->isActive()) {
        log(QString("[MEAS] S21: %1 dB / %2° | S43: %3 dB / %4°")
                .arg(s21.magnitudeDb(), 0, 'f', 3)
                .arg(s21.phaseDeg(),    0, 'f', 2)
                .arg(s43.magnitudeDb(), 0, 'f', 3)
                .arg(s43.phaseDeg(),    0, 'f', 2));
    }
}

void MainWindow::setConnectedState(bool connected)
{
    ui->btnConnect    ->setEnabled(!connected);
    ui->btnDisconnect ->setEnabled(connected);
    ui->btnLoadState  ->setEnabled(connected);
    ui->btnApplyConfig->setEnabled(connected);
    ui->btnReadConfig ->setEnabled(connected);
    ui->btnTrigOnce   ->setEnabled(connected);
    ui->btnStartLoop  ->setEnabled(connected);
    ui->btnStopLoop   ->setEnabled(false);
    ui->btnCheckErr   ->setEnabled(connected);
    ui->grpConfig     ->setEnabled(connected);

    ui->statusbar->showMessage(connected ? "Connected" : "Disconnected");
}