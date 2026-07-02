#ifndef E5_8_B_DRIVER_H
#define E5_8_B_DRIVER_H

#define _USE_MATH_DEFINES

#include <QString>
#include <cmath>
#include "visaconnection.h"

// ─────────────────────────────────────────────────────────────────────────────
// Kết quả một lần đo CW (1 điểm, SDATA = Re + Im)
// ─────────────────────────────────────────────────────────────────────────────
struct CwMeasurement
{
    double re    = 0.0;
    double im    = 0.0;
    bool   valid = false;   // false nếu parse thất bại hoặc chưa đo

    double amplitudeDb() const
    {
        double mag = std::sqrt(re * re + im * im);
        return (mag > 0.0) ? 20.0 * std::log10(mag) : -200.0;
    }

    double phaseDeg() const
    {
        return std::atan2(im, re) * 180.0 / M_PI;
    }
};

// Cấu hình hiện tại đọc từ Channel 1 — dùng để đồng bộ UI sau loadState()
struct EnaConfig
{
    double freqHz   = 0.0;
    double ifBwHz   = 0.0;
    double powerDbm = 0.0;
    bool   averageOn = false;
    int    points    = 1;
};

// Kết quả đo 2 TRM trong một lần trigger
struct DualMeasurement
{
    CwMeasurement trmA;   // S21 — Port1→Port2
    CwMeasurement trmB;   // S43 — Port3→Port4
};

// ─────────────────────────────────────────────────────────────────────────────
// EnaDriver
//
// Điều khiển ENA E5080B Option 4x2 cho bài toán calib TRM.
// 1 Channel, 2 trace (S21 + S43), CW mode, SDATA.
//
// Thứ tự gọi chuẩn:
//   connect()
//   → loadState(csaPath)
//   → prepareForAutomation()
//   → [tuỳ chọn] setCwFrequency(), setIfBandwidth(), setSourcePower()
//   → measureSingle(trace) hoặc measureDual()  × N
 //     (CalibSequencer quyết định gọi hàm nào tuỳ số TRM đang cắm)
//   → disconnect()
// ─────────────────────────────────────────────────────────────────────────────
class E5080B_driver
{
public:
    // Tên trace phải khớp với tên đã lưu trong file .csa
    static constexpr const char *TRACE_S21 = "TrS21";
    static constexpr const char *TRACE_S43 = "TrS43";

    explicit E5080B_driver(VisaConnection *conn);

    // ── 1. Kết nối ───────────────────────────────────────────────────────────
    QString connect();                          // Mở session, trả về IDN string
    bool    loadState(const QString &csaPath); // Load file .csa trên máy ENA
    void    disconnect();                       // Bật màn hình, đóng session

    // ── 2. Chuẩn bị đo ──────────────────────────────────────────────────────
    bool prepareForAutomation();               // Ghi đè trigger BUS/HOLD, verify trace
    bool setCwFrequency(double freqHz);        // Cập nhật tần số CW vào Channel 1
    bool setIfBandwidth(double ifBwHz);        // Cập nhật IFBW vào Channel 1
    bool setSourcePower(double dbm);           // Cập nhật công suất nguồn phát

    // Đọc lại cấu hình hiện tại từ Channel 1 — dùng để đồng bộ UI sau loadState()
    EnaConfig getCurrentConfig();

    // ── 3. Đo lường ──────────────────────────────────────────────────────────
    // Phát 1 xung trigger, đợi ENA hoàn thành sweep (cả 2 trace, vì cùng Channel 1)
    // Gọi 1 lần trước khi đọc dữ liệu — measureSingle/measureDual tự gọi bên trong
    bool triggerSweep();

    // Đọc SDATA của 1 trace cụ thể — dùng khi chỉ có 1 TRM cắm vào ENA
    // traceName: TRACE_S21 (TRM A) hoặc TRACE_S43 (TRM B)
    // Tự trigger trước khi đọc — gọi độc lập, không cần gọi triggerSweep() riêng
    CwMeasurement measureSingle(const QString &traceName);

    // Trigger 1 lần, đọc cả 2 trace — dùng khi cả 2 TRM đều cắm vào
    DualMeasurement measureDual();

    // ── 4. Tiện ích ──────────────────────────────────────────────────────────
    QString checkSystemError();               // SYST:ERR?
    bool    waitReady(int timeoutMs = 10000); // *OPC? blocking
    QString getIdnString();                   // *IDN?

    bool isConnected() const { return m_connected; }

private:
    VisaConnection *m_conn;
    bool            m_connected = false;

    // Đọc SDATA của trace đang được select → CwMeasurement
    CwMeasurement readSdata();
};

#endif // E5_8_B_DRIVER_H
