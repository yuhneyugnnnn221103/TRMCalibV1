#include "enadriverv2.h"
#include <QDebug>

EnaDriverV2::EnaDriverV2(VisaConnection *visa)
    : m_visa(visa)
{}

// ─────────────────────────────────────────────────────────────────────────────
// Session
// ─────────────────────────────────────────────────────────────────────────────

bool EnaDriverV2::connect()
{
    if (!m_visa->open()) {
        qWarning() << "[ENA] connect() failed:" << m_visa->lastError();
        return false;
    }
    QString id = idn();
    if (id.isEmpty()) {
        qWarning() << "[ENA] *IDN? no response";
        m_visa->close();
        return false;
    }
    m_connected = true;
    qDebug() << "[ENA] Connected:" << id;
    return true;
}

void EnaDriverV2::disconnect()
{
    if (m_connected) {
        m_visa->send("DISP:UPD ON");
        waitComplete(3000);
    }
    m_visa->close();
    m_connected = false;
    qDebug() << "[ENA] Disconnected.";
}

// ─────────────────────────────────────────────────────────────────────────────
// Instrument
// ─────────────────────────────────────────────────────────────────────────────

bool EnaDriverV2::reset()
{
    if (!m_connected) return false;
    m_visa->send("*RST");
    m_visa->send("*CLS");
    return waitComplete(15000);
}

bool EnaDriverV2::preset()
{
    if (!m_connected) return false;
    m_visa->send(":SYST:PRES");
    return waitComplete(10000);
}

QString EnaDriverV2::idn()
{
    return m_visa->query("*IDN?");
}

// ─────────────────────────────────────────────────────────────────────────────
// Channel
// ─────────────────────────────────────────────────────────────────────────────

bool EnaDriverV2::setSweepType(const QString &type, int ch)
{
    if (!m_connected) return false;
    return m_visa->send(QString("SENS%1:").arg(ch) + "SWE:TYPE " + type.toUpper());
}

bool EnaDriverV2::setFreqRange(double startHz, double stopHz, int ch)
{
    if (!m_connected) return false;
    m_visa->send(QString("SENS%1:").arg(ch) + QString("FREQ:STAR %1").arg(startHz, 0, 'f', 0));
    m_visa->send(QString("SENS%1:").arg(ch) + QString("FREQ:STOP %1").arg(stopHz,  0, 'f', 0));
    return true;
}

bool EnaDriverV2::setCwFrequency(double freqHz, int ch)
{
    if (!m_connected) return false;
    return m_visa->send(
        QString("SENS%1:").arg(ch) + QString("FREQ:CW %1").arg(freqHz, 0, 'f', 0));
}

bool EnaDriverV2::setPoints(int points, int ch)
{
    if (!m_connected) return false;
    return m_visa->send(QString("SENS%1:").arg(ch) + QString("SWE:POIN %1").arg(points));
}

bool EnaDriverV2::setIFBW(double ifBwHz, int ch)
{
    if (!m_connected) return false;
    return m_visa->send(
        QString("SENS%1:").arg(ch) + QString("BWID %1").arg(ifBwHz, 0, 'f', 0));
}

bool EnaDriverV2::setAverage(bool enable, int count, int ch)
{
    if (!m_connected) return false;
    m_visa->send(QString("SENS%1:").arg(ch) + QString("AVER:STAT %1").arg(enable ? "ON" : "OFF"));
    if (enable && count > 1)
        m_visa->send(QString("SENS%1:").arg(ch) + QString("AVER:COUN %1").arg(count));
    return true;
}

ChannelConfig EnaDriverV2::getChannelConfig(int ch)
{
    ChannelConfig cfg;
    if (!m_connected) return cfg;

    cfg.channel    = ch;
    cfg.sweepType  = m_visa->query(QString("SENS%1:").arg(ch) + "SWE:TYPE?").trimmed();
    cfg.startFreqHz= m_visa->query(QString("SENS%1:").arg(ch) + "FREQ:STAR?").trimmed().toDouble();
    cfg.stopFreqHz = m_visa->query(QString("SENS%1:").arg(ch) + "FREQ:STOP?").trimmed().toDouble();
    cfg.cwFreqHz   = m_visa->query(QString("SENS%1:").arg(ch) + "FREQ:CW?").trimmed().toDouble();
    cfg.points     = m_visa->query(QString("SENS%1:").arg(ch) + "SWE:POIN?").trimmed().toInt();
    cfg.ifBwHz     = m_visa->query(QString("SENS%1:").arg(ch) + "BWID?").trimmed().toDouble();
    cfg.port1PowerDbm = m_visa->query(QString("SOUR%1:").arg(ch) + "POW1?").trimmed().toDouble();
    cfg.port3PowerDbm = m_visa->query(QString("SOUR%1:").arg(ch) + "POW3?").trimmed().toDouble();
    cfg.averageOn  = (m_visa->query(QString("SENS%1:").arg(ch) + "AVER:STAT?").trimmed() == "1");
    cfg.averageCount = m_visa->query(QString("SENS%1:").arg(ch) + "AVER:COUN?").trimmed().toInt();
    cfg.traceNames = listTraces(ch);

    return cfg;
}

// ─────────────────────────────────────────────────────────────────────────────
// Port
// ─────────────────────────────────────────────────────────────────────────────

bool EnaDriverV2::setPortPower(int port, double dbm, int ch)
{
    if (!m_connected) return false;
    return m_visa->send(
        QString("SOUR%1:").arg(ch) + QString("POW%1 %2").arg(port).arg(dbm, 0, 'f', 2));
}

bool EnaDriverV2::setAllPortPower(double dbm, int ch)
{
    if (!m_connected) return false;
    // E5080B 4-port: Port 1, 2, 3, 4
    for (int p = 1; p <= 4; ++p) {
        if (!setPortPower(p, dbm, ch)) return false;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Trace
// ─────────────────────────────────────────────────────────────────────────────

bool EnaDriverV2::createTrace(const QString &name,
                            const QString &parameter,
                            int windowTraceNum,
                            int ch)
{
    if (!m_connected) return false;
    m_visa->send(QString("CALC%1:").arg(ch) +
                  QString("PAR:DEF:EXT '%1','%2'").arg(name, parameter));
    m_visa->send(QString("DISP:WIND%1:TRAC%2:FEED '%3'")
                      .arg(ch).arg(windowTraceNum).arg(name));
    return true;
}

bool EnaDriverV2::deleteTrace(const QString &name, int ch)
{
    if (!m_connected) return false;
    return m_visa->send(QString("CALC%1:").arg(ch) + QString("PAR:DEL '%1'").arg(name));
}

bool EnaDriverV2::deleteAllTraces(int ch)
{
    if (!m_connected) return false;
    return m_visa->send(QString("CALC%1:").arg(ch) + "PAR:DEL:ALL");
}

bool EnaDriverV2::selectTrace(const QString &name, int ch)
{
    if (!m_connected) return false;
    return m_visa->send(QString("CALC%1:").arg(ch) + QString("PAR:SEL '%1'").arg(name));
}

QStringList EnaDriverV2::listTraces(int ch)
{
    if (!m_connected) return {};
    // Response: "'TrS21','S21','TrS43','S43'" → ["TrS21","TrS43"]
    QString raw = m_visa->query(QString("CALC%1:").arg(ch) + "PAR:CAT:EXT?");
    raw.remove('"').remove('\'');
    QStringList tokens = raw.split(',');
    QStringList names;
    for (int i = 0; i < tokens.size() - 1; i += 2)
        names << tokens[i].trimmed();
    return names;
}

// ─────────────────────────────────────────────────────────────────────────────
// Trigger
// ─────────────────────────────────────────────────────────────────────────────

bool EnaDriverV2::setTriggerBusMode(int ch)
{
    if (!m_connected) return false;
    m_visa->send(QString("SENS%1:").arg(ch) + "SWE:MODE HOLD");
    m_visa->send("TRIG:SOUR MAN");
    // m_visa->send("TRIG:SCOP ACT");
    return waitComplete();
}

bool EnaDriverV2::setTriggerContinuous(int ch)  // Không dùng khi Calib tự động trigger từ khối điều khiển/PC
{
    if (!m_connected) return false;
    m_visa->send("TRIG:SOUR INT");
    m_visa->send(QString("SENS%1:").arg(ch) + "SWE:MODE CONT");
    return true;
}

bool EnaDriverV2::trigger(int ch, bool fireAndForget)
{
    if (!m_connected) return false;
    m_visa->send(QString("SENS%1:").arg(ch) + "SWE:MODE SING");
    m_visa->send(QString("INIT%1:IMM").arg(ch));
    if (!fireAndForget)
        return waitComplete();
    return true;
}

bool EnaDriverV2::abort()
{
    if (!m_connected) return false;
    return m_visa->send(":ABOR");
}

bool EnaDriverV2::waitComplete(int timeoutMs)
{
    if (!m_connected) return false;
    m_visa->setTimeout(timeoutMs);
    QString resp = m_visa->query("*OPC?");
    m_visa->setTimeout(10000);

    qDebug() << "[DEBUG] OPC Response:" << resp;

    return resp.trimmed() == "+1";
}

// ─────────────────────────────────────────────────────────────────────────────
// Data
// ─────────────────────────────────────────────────────────────────────────────

ComplexPoint EnaDriverV2::readComplexPoint(int ch)
{
    ComplexPoint pt;
    if (!m_connected) return pt;

    QString raw = m_visa->query(QString("CALC%1:").arg(ch) + "DATA? SDATA");
    auto pts = parseSdata(raw);
    if (!pts.isEmpty()) pt = pts.first();
    return pt;
}

TraceData EnaDriverV2::readComplexTrace(int ch)
{
    TraceData td;
    if (!m_connected) return td;

    td.freqHz = readFrequencyAxis(ch);

    QString raw = m_visa->query(QString("CALC%1:").arg(ch) + "DATA? SDATA");
    td.points = parseSdata(raw);

    if (!td.freqHz.isEmpty() && td.points.size() == td.freqHz.size())
        td.valid = true;

    return td;
}

double EnaDriverV2::readScalarPoint(int ch)
{
    if (!m_connected) return 0.0;
    return m_visa->query(QString("CALC%1:").arg(ch) + "DATA? FDATA").trimmed().toDouble();
}

QVector<double> EnaDriverV2::readScalarTrace(int ch)
{
    if (!m_connected) return {};
    return parseDoubleArray(m_visa->query(QString("CALC%1:").arg(ch) + "DATA? FDATA"));
}

QVector<double> EnaDriverV2::readFrequencyAxis(int ch)
{
    if (!m_connected) return {};
    return parseDoubleArray(m_visa->query(QString("SENS%1:").arg(ch) + "FREQ:DATA?"));
}

// ─────────────────────────────────────────────────────────────────────────────
// Marker
// ─────────────────────────────────────────────────────────────────────────────

bool EnaDriverV2::setMarker(int markerId, double freqHz, int ch)
{
    if (!m_connected) return false;
    QString pfx = QString("CALC%1:").arg(ch) + QString("MARK%1:").arg(markerId);
    m_visa->send(pfx + "STAT ON");
    m_visa->send(pfx + QString("FREQ %1").arg(freqHz, 0, 'f', 0));
    return true;
}

bool EnaDriverV2::deleteMarker(int markerId, int ch)
{
    if (!m_connected) return false;
    return m_visa->send(QString("CALC%1:").arg(ch) +
                         QString("MARK%1:STAT OFF").arg(markerId));
}

MarkerResult EnaDriverV2::getMarkerValue(int markerId, int ch)
{
    MarkerResult mr;
    mr.markerId = markerId;
    if (!m_connected) return mr;

    // CALC<ch>:MARK<n>:X? → tần số hiện tại của marker
    QString xStr = m_visa->query(QString("CALC%1:").arg(ch) +
                                 QString("MARK%1:X?").arg(markerId));
    mr.freqHz = xStr.trimmed().toDouble();

    // CALC<ch>:MARK<n>:Y? → "magnitude,phase" hoặc "Re,Im" tuỳ FORM
    QString yStr = m_visa->query(QString("CALC%1:").arg(ch) +
                                 QString("MARK%1:Y?").arg(markerId));
    QStringList parts = yStr.trimmed().split(',');
    if (parts.size() >= 2) {
        mr.magnitude = parts[0].toDouble();
        mr.phase     = parts[1].toDouble();
        mr.valid     = true;
    }
    return mr;
}

bool EnaDriverV2::markerSearchMax(int markerId, int ch)
{
    if (!m_connected) return false;
    QString pfx = QString("CALC%1:").arg(ch) + QString("MARK%1:FUNC:").arg(markerId);
    m_visa->send(pfx + "TYPE MAX");
    m_visa->send(pfx + "EXEC");
    return true;
}

bool EnaDriverV2::markerSearchMin(int markerId, int ch)
{
    if (!m_connected) return false;
    QString pfx = QString("CALC%1:").arg(ch) + QString("MARK%1:FUNC:").arg(markerId);
    m_visa->send(pfx + "TYPE MIN");
    m_visa->send(pfx + "EXEC");
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Display
// ─────────────────────────────────────────────────────────────────────────────

bool EnaDriverV2::setDisplayUpdate(bool enable)
{
    if (!m_connected) return false;
    return m_visa->send(QString("DISP:UPD %1").arg(enable ? "ON" : "OFF"));
}

bool EnaDriverV2::setTraceFormat(const QString &format, int ch)
{
    if (!m_connected) return false;
    return m_visa->send(QString("CALC%1:").arg(ch) + "FORM " + format.toUpper());
}

// ─────────────────────────────────────────────────────────────────────────────
// Calibration
// ─────────────────────────────────────────────────────────────────────────────

bool EnaDriverV2::setCorrectionState(bool enable, int ch)
{
    if (!m_connected) return false;
    return m_visa->send(
        QString("SENS%1:").arg(ch) + QString("CORR:STAT %1").arg(enable ? "ON" : "OFF"));
}

bool EnaDriverV2::getCorrectionState(int ch)
{
    if (!m_connected) return false;
    return m_visa->query(QString("SENS%1:").arg(ch) + "CORR:STAT?").trimmed() == "1";
}

// ─────────────────────────────────────────────────────────────────────────────
// Memory
// ─────────────────────────────────────────────────────────────────────────────

bool EnaDriverV2::loadState(const QString &path)
{
    if (!m_connected) return false;
    m_visa->send(QString("MMEM:LOAD:STAT 1,\"%1\"").arg(path));
    return waitComplete(15000);
}

bool EnaDriverV2::saveState(const QString &path)
{
    if (!m_connected) return false;
    m_visa->send(QString("MMEM:STOR:STAT 1,\"%1\"").arg(path));
    return waitComplete(10000);
}

bool EnaDriverV2::loadCalKit(const QString &name)
{
    if (!m_connected) return false;
    m_visa->send(QString("MMEM:LOAD:CKIT \"%1\"").arg(name));
    return waitComplete(5000);
}

bool EnaDriverV2::saveScreenshot(const QString &path)
{
    if (!m_connected) return false;
    m_visa->send(QString("MMEM:STOR:IMAG \"%1\"").arg(path));
    return waitComplete(5000);
}

// ─────────────────────────────────────────────────────────────────────────────
// Status
// ─────────────────────────────────────────────────────────────────────────────

QString EnaDriverV2::errorQueue()
{
    return m_visa->query("SYST:ERR?");
}

QStringList EnaDriverV2::allErrors()
{
    QStringList errors;
    if (!m_connected) return errors;
    while (true) {
        QString err = errorQueue();
        if (err.startsWith("+0") || err.startsWith("0,\"No error"))
            break;
        errors << err.trimmed();
        if (errors.size() > 20) break; // phòng vòng lặp vô hạn
    }
    return errors;
}

int EnaDriverV2::statusByte()
{
    if (!m_connected) return -1;
    return m_visa->query("*STB?").trimmed().toInt();
}

bool EnaDriverV2::clearStatus()
{
    if (!m_connected) return false;
    return m_visa->send("*CLS");
}

// bool EnaDriverV2::waitReady(int timeoutMs)
// {
//     if (!m_connected) return false;
//     m_visa->setTimeout(timeoutMs);
//     QString resp = m_visa->query("*OPC?");
//     m_visa->setTimeout(10000);
//     return resp.trimmed() == "1";
// }

// ─────────────────────────────────────────────────────────────────────────────
// Private
// ─────────────────────────────────────────────────────────────────────────────

QVector<ComplexPoint> EnaDriverV2::parseSdata(const QString &raw) const
{
    QVector<ComplexPoint> result;
    if (raw.isEmpty()) return result;

    // Bỏ IEEE header '#N<bytes>' nếu firmware trả về
    QString data = raw.trimmed();
    if (data.startsWith('#')) {
        int n = data.mid(1, 1).toInt();
        data  = data.mid(2 + n);
    }

    QStringList tokens = data.trimmed().split(',');
    for (int i = 0; i + 1 < tokens.size(); i += 2) {
        bool okRe = false, okIm = false;
        ComplexPoint pt;
        pt.re    = tokens[i].trimmed().toDouble(&okRe);
        pt.im    = tokens[i + 1].trimmed().toDouble(&okIm);
        pt.valid = okRe && okIm;
        result << pt;
    }
    return result;
}

QVector<double> EnaDriverV2::parseDoubleArray(const QString &raw) const
{
    QVector<double> result;
    if (raw.isEmpty()) return result;

    QString data = raw.trimmed();
    if (data.startsWith('#')) {
        int n = data.mid(1, 1).toInt();
        data  = data.mid(2 + n);
    }

    for (const QString &s : data.trimmed().split(',')) {
        bool ok = false;
        double v = s.trimmed().toDouble(&ok);
        if (ok) result << v;
    }
    return result;
}