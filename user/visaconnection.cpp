#include "visaconnection.h"
#include <QDebug>

/*************************************************************************/
VisaConnection::VisaConnection(const QString &visaAddress)
    : m_visaAddress(visaAddress)
{}
/*************************************************************************/
VisaConnection::~VisaConnection()
{
    close();
}
/*************************************************************************/
bool VisaConnection::open(int defaultTimeoutMs)
{
    ViStatus status;

    // Mở VISA Resource Manager
    status = viOpenDefaultRM(&m_rm);
    if (status < VI_SUCCESS) {
        m_lastError = QString("viOpenDefaultRM failed: %1")
        .arg(visaStatusToString(status));
        qWarning() << "[VISA]" << m_lastError;
        return false;
    }

    // Mở session tới thiết bị (địa chỉ)
    QByteArray addr = m_visaAddress.toLocal8Bit();
    status = viOpen(m_rm, addr.data(), VI_NULL, VI_NULL, &m_session);

    if (status < VI_SUCCESS) {
        m_lastError = QString("viOpen failed for '%1': %2")
        .arg(m_visaAddress)
            .arg(visaStatusToString(status));
        qWarning() << "[VISA]" << m_lastError;
        viClose(m_rm);
        m_rm = VI_NULL;
        return false;
    }

    // Timeout 5 giây cho mỗi lần đọc/ghi
    viSetAttribute(m_session, VI_ATTR_TMO_VALUE, defaultTimeoutMs);

    // Kết thúc chuỗi đọc bằng '\n' (LF) — chuẩn cho ENA E5080B
    viSetAttribute(m_session, VI_ATTR_TERMCHAR,     '\n');
    viSetAttribute(m_session, VI_ATTR_TERMCHAR_EN,  VI_TRUE);

    m_isOpen = true;
    qDebug() << "[VISA] Opened:" << m_visaAddress;
    return true;
}
/*************************************************************************/
void VisaConnection::close()
{
    if (m_session != VI_NULL) {
        viClose(m_session);
        m_session = VI_NULL;
    }
    if (m_rm != VI_NULL) {
        viClose(m_rm);
        m_rm = VI_NULL;
    }

    m_isOpen = false;
    qDebug() << "[ENA] Session closed.";
}
/*************************************************************************/
bool VisaConnection::send(const QString &command)
{
    if (!m_isOpen) return false;

    // Thêm '\n' vào cuối lệnh SCPI
    QByteArray cmd = (command + "\n").toLocal8Bit();
    ViUInt32 retCount = 0;

    ViStatus status = viWrite(m_session,
                              reinterpret_cast<ViBuf>(cmd.data()),
                              static_cast<ViUInt32>(cmd.size()),
                              &retCount);
    if (status < VI_SUCCESS) {
        m_lastError = QString("viWrite('%1') failed: %2")
        .arg(command)
            .arg(visaStatusToString(status));
        qWarning() << "[ENA]" << m_lastError;
        return false;
    }
    return true;
}
/*************************************************************************/
QString VisaConnection::query(const QString &command)
{
    if (!m_isOpen) return {};

    // Gửi lệnh
    if (!send(command))
        return {};

    // Đọc phản hồi
    char buf[4096] = {};
    ViUInt32 retCount = 0;

    ViStatus status = viRead(m_session,
                             reinterpret_cast<ViBuf>(buf),
                             sizeof(buf) - 1,
                             &retCount);
    if (status < VI_SUCCESS) {
        m_lastError = QString("viRead (response to '%1') failed: %2")
        .arg(command)
            .arg(visaStatusToString(status));
        qWarning() << "[ENA]" << m_lastError;
        return {};
    }

    buf[retCount] = '\0';
    return QString::fromLocal8Bit(buf).trimmed();
}
/*************************************************************************/
void VisaConnection::setTimeout(int ms)
{
    if (m_isOpen)
        viSetAttribute(m_session, VI_ATTR_TMO_VALUE,
                       static_cast<ViUInt32>(ms));
}
/*************************************************************************/
QString VisaConnection::visaStatusToString(ViStatus status) const
{
    // VISA cung cấp hàm viStatusDesc để lấy mô tả lỗi
    ViChar desc[256] = {};
    viStatusDesc(m_rm != VI_NULL ? m_rm : VI_NULL, status, desc);
    return QString("0x%1 (%2)")
        .arg(static_cast<unsigned>(status), 8, 16, QChar('0'))
        .arg(QString::fromLocal8Bit(desc).trimmed());
}
