#ifndef VISACONNECTION_H
#define VISACONNECTION_H

#include <QString>
#include <visa.h>

// ─────────────────────────────────────────────────────────────────────────────
// VisaConnection
//
// Wrapper tối giản cho VISA C API (Keysight IO Libraries Suite).
// Quản lý một VISA session: mở, đóng, ghi, đọc.
//
// Dùng chung cho tất cả thiết bị Keysight giao tiếp qua VISA:
//   - ENA E5080B    (TCPIP)
//   - Switch P9164C (USB)
// ─────────────────────────────────────────────────────────────────────────────

class VisaConnection
{
public:
    // Visa Address
    explicit VisaConnection(const QString &visaAddress);
    ~VisaConnection();

    // Mở Visa Session
    bool open(int defaultTimeoutMs = 5000);

    // Đóng session
    void close();

    // Gửi lệnh SCPI, không yêu cầu phản hồi
    bool send(const QString &command);

    // Gửi query, đọc về chuỗi phản hồi (rỗng nếu lỗi)
    QString query(const QString &command);

    // Đặt timeout cho lần read/write tiếp theo (ms)
    // Dùng khi một lệnh cụ thể cần timeout dài hơn mặc định
    void setTimeout(int ms);

    QString lastError()  const { return m_lastError; }
    bool    isOpen()     const { return m_isOpen; }
    QString visaAddress() const { return m_visaAddress; }

private:
    QString   m_visaAddress;
    ViSession m_rm      = VI_NULL;   // Resource Manager
    ViSession m_session = VI_NULL;   // Session tới thiết bị
    bool      m_isOpen  = false;
    QString   m_lastError;

    QString visaStatusToString(ViStatus status) const;
};

#endif // VISACONNECTION_H
