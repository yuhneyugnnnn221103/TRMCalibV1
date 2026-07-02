#include "e5080b_driver.h"
#include <QDebug>

/*************************************************************************/
E5080B_driver::E5080B_driver(VisaConnection *conn)
    : m_conn(conn)
{}

// ─────────────────────────────────────────────────────────────────────────────
// 1. Kết nối
// ─────────────────────────────────────────────────────────────────────────────

QString E5080B_driver::connect()
{
    if (!m_conn->open()) {
        qWarning() << "[ENA] connect() failed:" << m_conn->lastError();
        return {};
    }

    QString idn = getIdnString();
    if (idn.isEmpty()) {
        qWarning() << "[ENA] *IDN? no response";
        m_conn->close();
        return {};
    }

    m_connected = true;
    qDebug() << "[ENA] Connected:" << idn;
    return idn;
}

bool E5080B_driver::loadState(const QString &csaPath)
{
    if (!m_connected) return false;

    // csaPath là đường dẫn trên ổ cứng máy ENA, ví dụ:
    // "D:\\States\\TRM_9300MHz.csa"
    QString cmd = QString("MMEM:LOAD:STAT 1,\"%1\"").arg(csaPath);
    if (!m_conn->send(cmd)) return false;

    // Load state có thể mất vài giây — đợi OPC với timeout dài hơn
    if (!waitReady(15000)) {
        qWarning() << "[ENA] loadState() timeout:" << csaPath;
        return false;
    }

    QString err = checkSystemError();
    if (!err.startsWith("+0") && !err.startsWith("0,\"No error")) {
        qWarning() << "[ENA] loadState() SCPI error:" << err;
        return false;
    }

    qDebug() << "[ENA] State loaded:" << csaPath;
    return true;
}

void E5080B_driver::disconnect()
{
    if (m_connected) {
        // Bật lại màn hình ENA trước khi ngắt kết nối
        m_conn->send("DISP:UPD ON");
        waitReady(3000);
    }
    m_conn->close();
    m_connected = false;
    qDebug() << "[ENA] Disconnected.";
}

// ─────────────────────────────────────────────────────────────────────────────
// 2. Chuẩn bị đo
// ─────────────────────────────────────────────────────────────────────────────

bool E5080B_driver::prepareForAutomation()
{
    if (!m_connected) return false;

    // Tắt update màn hình để tăng tốc độ giao tiếp SCPI
    m_conn->send("DISP:UPD OFF");

    // Ghi đè trigger về BUS + HOLD — phòng trường hợp file .csa
    // được lưu với trigger Internal (continuous sweep)
    m_conn->send("TRIG:SOUR BUS");
    m_conn->send("SENS1:SWE:MODE HOLD");

    // Trigger scope = ACTive: chỉ trigger Channel 1 đang active
    m_conn->send("TRIG:SCOP ACT");

    if (!waitReady()) {
        qWarning() << "[ENA] prepareForAutomation() waitReady timeout";
        return false;
    }

    // Kiểm tra trace tồn tại trong Channel 1 (chỉ cảnh báo, không fail cứng —
    // hệ thống cho phép chỉ cắm 1 TRM, CalibSequencer sẽ chỉ gọi
    // measureSingle() cho trace tương ứng với TRM thực sự đang cắm)
    QString traceList = m_conn->query("CALC1:PAR:CAT:EXT?");
    bool hasS21 = traceList.contains(TRACE_S21);
    bool hasS43 = traceList.contains(TRACE_S43);

    if (!hasS21 && !hasS43) {
        qWarning() << "[ENA] Không tìm thấy trace nào trong Channel 1:"
                   << traceList;
        return false;   // file .csa sai hoàn toàn — đây mới thực sự là lỗi
    }

    if (!hasS21) qWarning() << "[ENA]" << TRACE_S21 << "không có trong Channel 1 (chỉ TRM B khả dụng)";
    if (!hasS43) qWarning() << "[ENA]" << TRACE_S43 << "không có trong Channel 1 (chỉ TRM A khả dụng)";

    qDebug() << "[ENA] Ready for automation. Traces:" << traceList.trimmed();
    return true;
}

bool E5080B_driver::setCwFrequency(double freqHz)
{
    if (!m_connected) return false;

    // Cập nhật trực tiếp vào Channel 1 — không cần tạo lại channel hay trace
    QString cmd = QString("SENS1:FREQ:CW %1")
                      .arg(freqHz, 0, 'f', 0);
    if (!m_conn->send(cmd)) return false;

    if (!waitReady()) return false;

    qDebug() << "[ENA] CW frequency set to" << freqHz / 1e9 << "GHz";
    return true;
}

bool E5080B_driver::setIfBandwidth(double ifBwHz)
{
    if (!m_connected) return false;

    QString cmd = QString("SENS1:BWID %1")
                      .arg(ifBwHz, 0, 'f', 0);
    if (!m_conn->send(cmd)) return false;

    if (!waitReady()) return false;

    qDebug() << "[ENA] IFBW set to" << ifBwHz << "Hz";
    return true;
}

bool E5080B_driver::setSourcePower(double dbm)
{
    if (!m_connected) return false;

    // Áp dụng cho cả Source 1 (Port1/2) và Source 2 (Port3/4)
    // để TRM A và TRM B được kích thích cùng mức công suất
    QString cmd1 = QString("SOUR1:POW1 %1").arg(dbm, 0, 'f', 2);
    QString cmd2 = QString("SOUR1:POW3 %1").arg(dbm, 0, 'f', 2);

    if (!m_conn->send(cmd1)) return false;
    if (!m_conn->send(cmd2)) return false;

    if (!waitReady()) return false;

    qDebug() << "[ENA] Source power set to" << dbm << "dBm (Port1 & Port3)";
    return true;
}

EnaConfig E5080B_driver::getCurrentConfig()
{
    EnaConfig cfg;
    if (!m_connected) return cfg;

    cfg.freqHz    = m_conn->query("SENS1:FREQ:CW?").toDouble();
    cfg.ifBwHz    = m_conn->query("SENS1:BWID?").toDouble();
    cfg.powerDbm  = m_conn->query("SOUR1:POW1?").toDouble();
    cfg.averageOn = (m_conn->query("SENS1:AVER?").trimmed() == "1");
    cfg.points    = m_conn->query("SENS1:SWE:POIN?").toInt();

    return cfg;
}







