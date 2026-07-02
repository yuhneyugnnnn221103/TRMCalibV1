#ifndef EnaDriverV2_H
#define EnaDriverV2_H

#define _USE_MATH_DEFINES

#include <QString>
#include <QStringList>
#include <cmath>
#include "visaconnection.h"

// ─────────────────────────────────────────────────────────────────────────────
// Data types
// ─────────────────────────────────────────────────────────────────────────────

// Một điểm đo phức (Re + Im) tại 1 tần số
struct ComplexPoint
{
    double re    = 0.0;
    double im    = 0.0;
    bool   valid = false;

    double magnitudeDb() const
    {
        double mag = std::sqrt(re * re + im * im);
        return (mag > 0.0) ? 20.0 * std::log10(mag) : -200.0;
    }
    double phaseDeg() const { return std::atan2(im, re) * 180.0 / M_PI; }
};

// Kết quả một trace đầy đủ (nhiều điểm tần số)
    struct TraceData
{
    QVector<double>       freqHz;      // mảng tần số
    QVector<ComplexPoint> points;      // Re+Im tại mỗi tần số
    QString               traceName;
    QString               parameter;  // "S21", "S11", ...
    bool                  valid = false;

    int count() const { return points.size(); }

    // Lấy biên độ dB tại index i
    double magnitudeDb(int i) const {
        return (i >= 0 && i < points.size()) ? points[i].magnitudeDb() : -200.0;
    }
    // Lấy pha (độ) tại index i
    double phaseDeg(int i) const {
        return (i >= 0 && i < points.size()) ? points[i].phaseDeg() : 0.0;
    }
};

// Marker result
struct MarkerResult
{
    int    markerId  = 1;
    double freqHz    = 0.0;
    double magnitude = 0.0;   // dB
    double phase     = 0.0;   // độ
    bool   valid     = false;
};

// Snapshot cấu hình channel
    struct ChannelConfig
{
    int         channel   = 1;
    QString     sweepType;      // "CW", "LIN", "LOG", "SEGM"
    double      startFreqHz = 0.0;
    double      stopFreqHz  = 0.0;
    double      cwFreqHz    = 0.0;
    int         points      = 1;
    double      ifBwHz      = 1000.0;
    double      port1PowerDbm = 0.0;
    double      port3PowerDbm = 0.0;
    bool        averageOn   = false;
    int         averageCount = 1;
    QStringList traceNames;
};
// ─────────────────────────────────────────────────────────────────────────────
// VnaDriver
//
// Driver VNA tổng quát cho Keysight ENA E5080B (4-port, Option 4x2).
//
// Tổ chức theo SCPI command group chuẩn của ENA:
//   Session        → kết nối VISA
//   Instrument     → reset, preset, idn
//   Channel        → cấu hình sweep, tần số, IFBW
//   Port           → công suất nguồn phát từng port
//   Trace          → tạo, xóa, chọn, liệt kê trace
//   Trigger        → bus mode, trigger, abort, wait
//   Data           → đọc SDATA, FDATA, tần số
//   Marker         → đặt, đọc, tìm peak
//   Display        → bật/tắt màn hình, format hiển thị
//   Calibration    → load/save cal set, bật tắt correction
//   Memory         → load/save state (.csa), screenshot
//   Status         → error queue, status byte, clear
// ─────────────────────────────────────────────────────────────────────────────
class EnaDriverV2
{
public:
    explicit EnaDriverV2(VisaConnection *visa);

    // ── Session ───────────────────────────────────────────────────────────────
    bool    connect();
    void    disconnect();
    bool    isConnected() const { return m_connected; }

    // ── Instrument ────────────────────────────────────────────────────────────
    // *RST + *CLS
    bool    reset();
    // :SYST:PRES — preset nhẹ hơn reset, giữ cấu hình network/GPIB
    bool    preset();
    // *IDN?
    QString idn();

    // ── Channel ───────────────────────────────────────────────────────────────
    // SENS<ch>:SWE:TYPE "CW"|"LIN"|"LOG"|"SEGM"
    bool    setSweepType(const QString &type, int ch = 1);
    // SENS<ch>:FREQ:STAR + STOP
    bool    setFreqRange(double startHz, double stopHz, int ch = 1);
    // SENS<ch>:FREQ:CW  (chỉ dùng khi sweepType = CW)
    bool    setCwFrequency(double freqHz, int ch = 1);
    // SENS<ch>:SWE:POIN
    bool    setPoints(int points, int ch = 1);
    // SENS<ch>:BWID
    bool    setIFBW(double ifBwHz, int ch = 1);
    // SENS<ch>:AVER:STAT + COUN
    bool    setAverage(bool enable, int count = 1, int ch = 1);
    // Đọc snapshot cấu hình hiện tại của channel
    ChannelConfig getChannelConfig(int ch = 1);

    // ── Port ──────────────────────────────────────────────────────────────────
    // SOUR<ch>:POW<port> <dBm>
    // port: 1, 2, 3, 4 (tuỳ option máy)
    bool    setPortPower(int port, double dbm, int ch = 1);
    // Set tất cả port về cùng mức công suất
    bool    setAllPortPower(double dbm, int ch = 1);

    // ── Trace ─────────────────────────────────────────────────────────────────
    // CALC<ch>:PAR:DEF:EXT '<name>','Sxy'
    // windowTraceNum: vị trí trace trong window (1, 2, 3...)
    bool    createTrace(const QString &name,
                     const QString &parameter,
                     int windowTraceNum = 1,
                     int ch = 1);
    // CALC<ch>:PAR:DEL '<name>'
    bool    deleteTrace(const QString &name, int ch = 1);
    // CALC<ch>:PAR:DEL:ALL
    bool    deleteAllTraces(int ch = 1);
    // CALC<ch>:PAR:SEL '<name>'
    bool    selectTrace(const QString &name, int ch = 1);
    // CALC<ch>:PAR:CAT:EXT? → danh sách tên trace
    QStringList listTraces(int ch = 1);

    // ── Trigger ───────────────────────────────────────────────────────────────
    // TRIG:SOUR BUS + TRIG:SCOP ACT + SENS<ch>:SWE:MODE HOLD
    bool    setTriggerBusMode(int ch = 1);
    // SENS<ch>:SWE:MODE CONT
    bool    setTriggerContinuous(int ch = 1);
    // SENS<ch>:SWE:MODE SING + TRIG:SING
    // fireAndForget=true: không đợi, gọi waitComplete() riêng
    bool    trigger(int ch = 1, bool fireAndForget = false);
    // :ABOR — hủy sweep đang chạy
    bool    abort();
    // *OPC? blocking — đợi sweep hoàn thành
    bool    waitComplete(int timeoutMs = 10000);

    // ── Data ──────────────────────────────────────────────────────────────────
    // CALC<ch>:DATA? SDATA → Re,Im cho trace đang select
    // CW mode: 1 điểm. LIN mode: points điểm
    ComplexPoint readComplexPoint(int ch = 1);      // CW, lấy điểm duy nhất
    TraceData    readComplexTrace(int ch = 1);      // LIN/LOG, lấy toàn bộ trace

    // CALC<ch>:DATA? FDATA → dữ liệu formatted theo CALC<ch>:FORM hiện tại
    double           readScalarPoint(int ch = 1);   // CW, 1 giá trị
    QVector<double>  readScalarTrace(int ch = 1);   // LIN/LOG, mảng giá trị

    // SENS<ch>:FREQ:DATA? → mảng tần số
    QVector<double>  readFrequencyAxis(int ch = 1);

    // ── Marker ────────────────────────────────────────────────────────────────
    // CALC<ch>:MARK<n>:STAT ON + FREQ <Hz>
    bool         setMarker(int markerId, double freqHz, int ch = 1);
    // CALC<ch>:MARK<n>:STAT OFF
    bool         deleteMarker(int markerId, int ch = 1);
    // CALC<ch>:MARK<n>:Y? → {mag dB, phase deg}
    MarkerResult getMarkerValue(int markerId, int ch = 1);
    // CALC<ch>:MARK<n>:FUNC:TYPE MAX + :EXEC
    bool         markerSearchMax(int markerId, int ch = 1);
    // CALC<ch>:MARK<n>:FUNC:TYPE MIN + :EXEC
    bool         markerSearchMin(int markerId, int ch = 1);

    // ── Display ───────────────────────────────────────────────────────────────
    // DISP:UPD ON|OFF
    bool    setDisplayUpdate(bool enable);
    // CALC<ch>:FORM MLOG|PHAS|REAL|IMAG|SMITH|...
    bool    setTraceFormat(const QString &format, int ch = 1);

    // ── Calibration ───────────────────────────────────────────────────────────
    // SENS<ch>:CORR:STAT ON|OFF
    bool    setCorrectionState(bool enable, int ch = 1);
    // SENS<ch>:CORR:STAT? → true/false
    bool    getCorrectionState(int ch = 1);

    // ── Memory ────────────────────────────────────────────────────────────────
    // MMEM:LOAD:STAT 1,"<path>"  — path trên ổ cứng MÁY ENA
    bool    loadState(const QString &path);
    // MMEM:STOR:STAT 1,"<path>"
    bool    saveState(const QString &path);
    // MMEM:LOAD:CKIT "<name>"  — load cal kit theo tên
    bool    loadCalKit(const QString &name);
    // MMEM:STOR:IMAG "<path>.png"  — lưu screenshot màn hình ENA
    bool    saveScreenshot(const QString &path);

    // ── Status ────────────────────────────────────────────────────────────────
    // SYST:ERR?  — đọc và xóa 1 lỗi từ error queue
    QString errorQueue();
    // Đọc hết error queue cho đến khi "No error"
    QStringList allErrors();
    // *STB?
    int     statusByte();
    // *CLS
    bool    clearStatus();
    // // *OPC?
    // bool    waitReady(int timeoutMs = 10000);

private:
    VisaConnection *m_visa;
    bool          m_connected = false;

    // Parse chuỗi "Re,Im[,Re,Im,...]" → vector ComplexPoint
    QVector<ComplexPoint> parseSdata(const QString &raw) const;

    // Parse chuỗi "v1,v2,v3,..." → vector double
    QVector<double>       parseDoubleArray(const QString &raw) const;
};

#endif