#include "mainwindow.h"
#include "user/visaconnection.h"
#include "user/enadriverv2.h"

#include <QApplication>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("VNA Driver Test — ENA E5080B");
    resize(900, 640);

    m_loopTimer = new QTimer(this);

    buildUi();
    connectSignals();
    setConnectedState(false);

    log("Sẵn sàng. Nhập địa chỉ VISA và nhấn Connect.");
}

MainWindow::~MainWindow()
{
    m_loopTimer->stop();
    if (m_vna)  { m_vna->disconnect(); delete m_vna;  }
    if (m_visa) { delete m_visa; }
}

// ─────────────────────────────────────────────────────────────────────────────
// buildUi() — tạo toàn bộ widget bằng code
// ─────────────────────────────────────────────────────────────────────────────

void MainWindow::buildUi()
{
    // Root splitter: cột trái (controls) | cột phải (results + log)
    auto *splitter   = new QSplitter(Qt::Horizontal, this);
    auto *leftWidget = new QWidget;
    auto *rightWidget= new QWidget;
    splitter->addWidget(leftWidget);
    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    setCentralWidget(splitter);

    auto *leftLayout  = new QVBoxLayout(leftWidget);
    auto *rightLayout = new QVBoxLayout(rightWidget);
    leftLayout->setSpacing(8);
    rightLayout->setSpacing(8);

    // ── 1. Kết nối ───────────────────────────────────────────────────────────
    {
        auto *grp    = new QGroupBox("1. Kết nối ENA E5080B");
        auto *layout = new QVBoxLayout(grp);

        auto *row1 = new QHBoxLayout;
        row1->addWidget(new QLabel("VISA Address:"));
        m_editVisaAddr = new QLineEdit("TCPIP0::192.168.1.100::inst0::INSTR");
        m_editVisaAddr->setMinimumWidth(260);
        row1->addWidget(m_editVisaAddr, 1);
        layout->addLayout(row1);

        auto *row2 = new QHBoxLayout;
        m_btnConnect    = new QPushButton("Connect");
        m_btnDisconnect = new QPushButton("Disconnect");
        row2->addWidget(m_btnConnect);
        row2->addWidget(m_btnDisconnect);
        layout->addLayout(row2);

        leftLayout->addWidget(grp);
    }

    // ── 2. Load State ─────────────────────────────────────────────────────────
    {
        auto *grp    = new QGroupBox("2. Load State (.csa trên máy ENA)");
        auto *layout = new QVBoxLayout(grp);

        layout->addWidget(new QLabel("Đường dẫn trên ổ cứng MÁY ENA:"));
        m_editCsaPath = new QLineEdit("D:\\States\\TRM_Calib.csa");
        layout->addWidget(m_editCsaPath);

        m_btnLoadState = new QPushButton("Load State + Set BUS Trigger");
        layout->addWidget(m_btnLoadState);

        leftLayout->addWidget(grp);
    }

    // ── 3. Cấu hình Channel 1 ────────────────────────────────────────────────
    {
        m_grpConfig  = new QGroupBox("3. Cấu hình Channel 1");
        auto *layout = new QVBoxLayout(m_grpConfig);

        // Freq
        auto *rowFreq = new QHBoxLayout;
        rowFreq->addWidget(new QLabel("Freq (GHz):"));
        m_spinFreqGHz = new QDoubleSpinBox;
        m_spinFreqGHz->setRange(0.0003, 18.0);
        m_spinFreqGHz->setDecimals(4);
        m_spinFreqGHz->setSingleStep(0.1);
        m_spinFreqGHz->setValue(9.5);
        rowFreq->addWidget(m_spinFreqGHz, 1);
        layout->addLayout(rowFreq);

        // IFBW
        auto *rowIFBW = new QHBoxLayout;
        rowIFBW->addWidget(new QLabel("IFBW (kHz):"));
        m_spinIFBWkHz = new QDoubleSpinBox;
        m_spinIFBWkHz->setRange(0.001, 1000.0);
        m_spinIFBWkHz->setDecimals(3);
        m_spinIFBWkHz->setValue(10.0);
        rowIFBW->addWidget(m_spinIFBWkHz, 1);
        layout->addLayout(rowIFBW);

        // Power
        auto *rowPow = new QHBoxLayout;
        rowPow->addWidget(new QLabel("Power (dBm):"));
        m_spinPowerdBm = new QDoubleSpinBox;
        m_spinPowerdBm->setRange(-85.0, 10.0);
        m_spinPowerdBm->setDecimals(1);
        m_spinPowerdBm->setValue(-10.0);
        rowPow->addWidget(m_spinPowerdBm, 1);
        layout->addLayout(rowPow);

        // Buttons
        auto *rowBtn = new QHBoxLayout;
        m_btnApply  = new QPushButton("Apply");
        m_btnReadCfg= new QPushButton("Read from ENA");
        rowBtn->addWidget(m_btnApply);
        rowBtn->addWidget(m_btnReadCfg);
        layout->addLayout(rowBtn);

        // Info labels
        m_lblSweepInfo = new QLabel("Sweep: —");
        m_lblTraces    = new QLabel("Traces: —");
        layout->addWidget(m_lblSweepInfo);
        layout->addWidget(m_lblTraces);

        leftLayout->addWidget(m_grpConfig);
    }

    // ── 4. Trigger ────────────────────────────────────────────────────────────
    {
        auto *grp    = new QGroupBox("4. Trigger");
        auto *layout = new QVBoxLayout(grp);

        m_btnTrigOnce = new QPushButton("Trigger Once → Đọc S21 + S43");
        layout->addWidget(m_btnTrigOnce);

        auto *rowLoop = new QHBoxLayout;
        rowLoop->addWidget(new QLabel("Loop delay (ms):"));
        m_spinDelay = new QSpinBox;
        m_spinDelay->setRange(100, 5000);
        m_spinDelay->setValue(400);
        rowLoop->addWidget(m_spinDelay);
        m_btnStartLoop = new QPushButton("Start Loop");
        m_btnStopLoop  = new QPushButton("Stop");
        rowLoop->addWidget(m_btnStartLoop);
        rowLoop->addWidget(m_btnStopLoop);
        layout->addLayout(rowLoop);

        leftLayout->addWidget(grp);
    }

    leftLayout->addStretch();

    // ── 5. Kết quả đo ────────────────────────────────────────────────────────
    {
        auto *grp    = new QGroupBox("Kết quả đo (SDATA)");
        auto *layout = new QVBoxLayout(grp);

        // Header row
        auto *hdr = new QHBoxLayout;
        hdr->addWidget(new QLabel(""), 2);
        auto *hdrAmp   = new QLabel("<b>Biên độ (dB)</b>");
        auto *hdrPhase = new QLabel("<b>Pha (°)</b>");
        hdrAmp->setAlignment(Qt::AlignCenter);
        hdrPhase->setAlignment(Qt::AlignCenter);
        hdr->addWidget(hdrAmp,   3);
        hdr->addWidget(hdrPhase, 3);
        layout->addLayout(hdr);

        // Tạo label kết quả với font lớn
        auto makeBigLabel = [](const QString &color) {
            auto *lbl = new QLabel("--");
            lbl->setAlignment(Qt::AlignCenter);
            lbl->setStyleSheet(
                QString("font-size:22px; font-weight:bold; color:%1;").arg(color));
            lbl->setMinimumHeight(40);
            return lbl;
        };

        // S21 row (TRM A)
        auto *rowS21 = new QHBoxLayout;
        auto *lblS21 = new QLabel("<b>S21 (TRM A)</b>");
        lblS21->setAlignment(Qt::AlignVCenter);
        m_lblS21Amp   = makeBigLabel("#1a6db5");
        m_lblS21Phase = makeBigLabel("#1a6db5");
        rowS21->addWidget(lblS21,        2);
        rowS21->addWidget(m_lblS21Amp,   3);
        rowS21->addWidget(m_lblS21Phase, 3);
        layout->addLayout(rowS21);

        // S43 row (TRM B)
        auto *rowS43 = new QHBoxLayout;
        auto *lblS43 = new QLabel("<b>S43 (TRM B)</b>");
        lblS43->setAlignment(Qt::AlignVCenter);
        m_lblS43Amp   = makeBigLabel("#1d9e75");
        m_lblS43Phase = makeBigLabel("#1d9e75");
        rowS43->addWidget(lblS43,        2);
        rowS43->addWidget(m_lblS43Amp,   3);
        rowS43->addWidget(m_lblS43Phase, 3);
        layout->addLayout(rowS43);

        rightLayout->addWidget(grp);
    }

    // ── 6. Log ────────────────────────────────────────────────────────────────
    {
        auto *grp    = new QGroupBox("Log");
        auto *layout = new QVBoxLayout(grp);

        m_log = new QPlainTextEdit;
        m_log->setReadOnly(true);
        m_log->setFont(QFont("Consolas", 9));
        layout->addWidget(m_log);

        auto *rowBtn = new QHBoxLayout;
        m_btnCheckErr = new QPushButton("Check SYST:ERR?");
        m_btnClearLog = new QPushButton("Clear Log");
        rowBtn->addStretch();
        rowBtn->addWidget(m_btnCheckErr);
        rowBtn->addWidget(m_btnClearLog);
        layout->addLayout(rowBtn);

        rightLayout->addWidget(grp, 1);
    }

    // Status bar
    statusBar()->showMessage("Disconnected");
}

// ─────────────────────────────────────────────────────────────────────────────
// connectSignals()
// ─────────────────────────────────────────────────────────────────────────────

void MainWindow::connectSignals()
{
    connect(m_btnConnect,    &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(m_btnDisconnect, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);
    connect(m_btnLoadState,  &QPushButton::clicked, this, &MainWindow::onLoadStateClicked);
    connect(m_btnApply,      &QPushButton::clicked, this, &MainWindow::onApplyConfigClicked);
    connect(m_btnReadCfg,    &QPushButton::clicked, this, &MainWindow::onReadConfigClicked);
    connect(m_btnTrigOnce,   &QPushButton::clicked, this, &MainWindow::onTriggerOnceClicked);
    connect(m_btnStartLoop,  &QPushButton::clicked, this, &MainWindow::onStartLoopClicked);
    connect(m_btnStopLoop,   &QPushButton::clicked, this, &MainWindow::onStopLoopClicked);
    connect(m_btnCheckErr,   &QPushButton::clicked, this, &MainWindow::onCheckErrorClicked);
    connect(m_btnClearLog,   &QPushButton::clicked, this, &MainWindow::onClearLogClicked);

    connect(m_loopTimer,  &QTimer::timeout,
            this, &MainWindow::onLoopTick);
    connect(m_spinDelay, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int ms){ m_loopTimer->setInterval(ms); });
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots
// ─────────────────────────────────────────────────────────────────────────────

void MainWindow::onConnectClicked()
{
    QString addr = m_editVisaAddr->text().trimmed();
    if (addr.isEmpty()) { log("[ERR] Địa chỉ VISA trống.", true); return; }

    if (m_vna)  { m_vna->disconnect(); delete m_vna;  m_vna  = nullptr; }
    if (m_visa) { delete m_visa;                       m_visa = nullptr; }

    log(QString("Đang kết nối: %1 ...").arg(addr));
    m_visa = new VisaConnection(addr);
    m_vna  = new EnaDriverV2(m_visa);

    if (!m_vna->connect()) {
        log("[ERR] Kết nối thất bại: " + m_visa->lastError(), true);
        delete m_vna;  m_vna  = nullptr;
        delete m_visa; m_visa = nullptr;
        return;
    }

    log("[OK]  " + m_vna->idn());
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

void MainWindow::onLoadStateClicked()
{
    QString path = m_editCsaPath->text().trimmed();
    if (path.isEmpty()) { log("[ERR] Đường dẫn .csa trống.", true); return; }

    log("Load state: " + path + " ...");
    if (!m_vna->loadState(path)) {
        log("[ERR] Load state thất bại. " + m_vna->errorQueue(), true);
        return;
    }

    if (!m_vna->setTriggerBusMode()) {
        log("[ERR] setTriggerBusMode thất bại.", true);
        return;
    }
    m_vna->setDisplayUpdate(false);
    log("[OK]  Load state xong. Trigger → BUS mode. Display OFF.");

    // Đọc lại cấu hình để đồng bộ UI
    onReadConfigClicked();
}

void MainWindow::onApplyConfigClicked()
{
    double freq  = m_spinFreqGHz ->value() * 1e9;
    double ifbw  = m_spinIFBWkHz ->value() * 1e3;
    double power = m_spinPowerdBm->value();

    log(QString("Apply: %1 GHz | IFBW %2 kHz | %3 dBm")
            .arg(freq/1e9, 0, 'f', 4)
            .arg(ifbw/1e3, 0, 'f', 3)
            .arg(power,    0, 'f', 1));

    bool ok = m_vna->setCwFrequency(freq)
              && m_vna->setIFBW(ifbw)
              && m_vna->setAllPortPower(power);

    ok ? log("[OK]  Cấu hình đã áp dụng.")
       : log("[ERR] " + m_vna->errorQueue(), true);
}

void MainWindow::onReadConfigClicked()
{
    m_vna->setSweepType("CW", 1);
    m_vna->setPoints(1, 1);
    m_vna->setCwFrequency(1e9, 1);
            m_vna->setTriggerBusMode(1); ///////////////////////////////////////////////////
    m_vna->deleteAllTraces(1);
            m_vna->createTrace("CH1_S21_1", "S21", 1, 1);
            m_vna->createTrace("CH1_S43_2", "S43", 2, 1);

    ChannelConfig cfg = m_vna->getChannelConfig(1);

    // Cập nhật spinbox mà không kích hoạt signal
    QSignalBlocker b1(m_spinFreqGHz), b2(m_spinIFBWkHz), b3(m_spinPowerdBm);
    m_spinFreqGHz ->setValue(cfg.cwFreqHz    / 1e9);
    m_spinIFBWkHz ->setValue(cfg.ifBwHz      / 1e3);
    m_spinPowerdBm->setValue(cfg.port1PowerDbm);

    m_lblSweepInfo->setText(
        QString("Sweep: %1  |  Points: %2  |  Avg: %3")
            .arg(cfg.sweepType)
            .arg(cfg.points)
            .arg(cfg.averageOn ? QString("ON ×%1").arg(cfg.averageCount) : "OFF"));
    m_lblTraces->setText("Traces: " + cfg.traceNames.join(", "));

    log(QString("[CFG] %1 GHz | IFBW %2 kHz | Traces: %3")
            .arg(cfg.cwFreqHz/1e9, 0, 'f', 4)
            .arg(cfg.ifBwHz/1e3,   0, 'f', 3)
            .arg(cfg.traceNames.join(", ")));
}

void MainWindow::onTriggerOnceClicked()
{

    bool triggerOk = m_vna->trigger(1, false);

    QString errQueue = m_vna->errorQueue();
    if (!errQueue.isEmpty() && !errQueue.contains("+0") && !errQueue.contains("No error")) {
        log(QString("[ERR] VNA báo lỗi trigger: %1").arg(errQueue), true);
        return;
    }

    if (!triggerOk) {
        log("[ERR] Quá thời gian phản hồi (Timeout) khi trigger!", true);
        return;
    }

    m_vna->selectTrace("CH1_S21_1");
    ComplexPoint s21 = m_vna->readComplexPoint();

    m_vna->selectTrace("CH1_S43_2");
    ComplexPoint s43 = m_vna->readComplexPoint();

    updateMeasDisplay(s21.magnitudeDb(), s21.phaseDeg(),
                      s43.magnitudeDb(), s43.phaseDeg());

    if (s21.valid && s43.valid) {
        log(QString("[MEAS] S21: %1 dB / %2°  |  S43: %3 dB / %4°")
                .arg(s21.magnitudeDb(), 0, 'f', 3).arg(s21.phaseDeg(), 0, 'f', 2)
                .arg(s43.magnitudeDb(), 0, 'f', 3).arg(s43.phaseDeg(), 0, 'f', 2));
    } else {
        log("[ERR] Đọc SDATA thất bại. " + m_vna->errorQueue(), true);
    }
}

void MainWindow::onStartLoopClicked()
{
    m_loopTimer->setInterval(m_spinDelay->value());
    m_btnStartLoop->setEnabled(false);
    m_btnStopLoop ->setEnabled(true);
    m_loopTimer->start();
    log(QString("Loop bắt đầu (interval %1 ms).").arg(m_spinDelay->value()));
}

void MainWindow::onStopLoopClicked()
{
    m_loopTimer->stop();
    m_btnStartLoop->setEnabled(true);
    m_btnStopLoop ->setEnabled(false);
    log("Loop dừng.");
}

void MainWindow::onLoopTick()
{
    if (!m_vna || !m_vna->isConnected()) { onStopLoopClicked(); return; }

    // fireAndForget=false → trigger() tự đợi waitComplete() bên trong
    if (!m_vna->trigger()) {
        log("[ERR] Trigger lỗi — dừng loop.", true);
        onStopLoopClicked();
        return;
    }

    m_vna->selectTrace("TrS21");
    ComplexPoint s21 = m_vna->readComplexPoint();

    m_vna->selectTrace("TrS43");
    ComplexPoint s43 = m_vna->readComplexPoint();

    // Cập nhật display, không log trong loop để tránh spam
    updateMeasDisplay(s21.magnitudeDb(), s21.phaseDeg(),
                      s43.magnitudeDb(), s43.phaseDeg());
}

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
    m_log->clear();
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

void MainWindow::log(const QString &msg, bool isError)
{
    QString ts   = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString line = QString("[%1] %2").arg(ts, msg);

    if (isError)
        m_log->appendHtml(
            QString("<span style='color:#c0392b;font-family:Consolas;font-size:9pt;'>%1</span>")
                .arg(line.toHtmlEscaped()));
    else
        m_log->appendPlainText(line);

    m_log->ensureCursorVisible();
}

void MainWindow::updateMeasDisplay(double s21Amp, double s21Phase,
                                   double s43Amp, double s43Phase)
{
    m_lblS21Amp  ->setText(QString("%1 dB").arg(s21Amp,   0, 'f', 3));
    m_lblS21Phase->setText(QString("%1 °") .arg(s21Phase, 0, 'f', 2));
    m_lblS43Amp  ->setText(QString("%1 dB").arg(s43Amp,   0, 'f', 3));
    m_lblS43Phase->setText(QString("%1 °") .arg(s43Phase, 0, 'f', 2));
}

void MainWindow::setConnectedState(bool connected)
{
    m_btnConnect   ->setEnabled(!connected);
    m_btnDisconnect->setEnabled(connected);
    m_btnLoadState ->setEnabled(connected);
    m_grpConfig    ->setEnabled(connected);
    m_btnTrigOnce  ->setEnabled(connected);
    m_btnStartLoop ->setEnabled(connected);
    m_btnStopLoop  ->setEnabled(false);
    m_btnCheckErr  ->setEnabled(connected);

    statusBar()->showMessage(connected ? "Connected" : "Disconnected");
}