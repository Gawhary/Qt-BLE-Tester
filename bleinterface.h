#ifndef BLEINTERFACE_H
#define BLEINTERFACE_H

#include <QObject>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QTimer>


#include "lib-qt-qml-tricks/src/qqmlhelpers.h"

#define READ_INTERVAL_MS 3000
#define CHUNK_SIZE 20

class DeviceInfo: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString deviceName READ getName NOTIFY deviceChanged)
    Q_PROPERTY(QString deviceAddress READ getAddress NOTIFY deviceChanged)
public:
    DeviceInfo(const QBluetoothDeviceInfo &device);
    void setDevice(const QBluetoothDeviceInfo &device);
    QString getName() const { return m_device.name(); }
    QString getAddress() const;
    QBluetoothDeviceInfo getDevice() const;

signals:
    void deviceChanged();

private:
    QBluetoothDeviceInfo m_device;
};

class BLEInterface : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
    Q_PROPERTY(int currentService READ currentService WRITE setCurrentService NOTIFY currentServiceChanged)
    QML_WRITABLE_PROPERTY(int, currentDevice)
    QML_READONLY_PROPERTY(QStringList, devicesNames)
    QML_READONLY_PROPERTY(QStringList, services)
public:
    explicit BLEInterface(QObject *parent = 0);
    ~BLEInterface();

    void connectCurrentDevice();
    void disconnectDevice();
    Q_INVOKABLE void scanDevices();
    void write(const QByteArray& data);

    bool isConnected() const
    {
        return m_connected;
    }

    int currentService() const
    {
        return m_currentService;
    }

public slots:
    void setCurrentService(int currentService)
    {
        if (m_currentService == currentService)
            return;
        update_currentService(currentService);
        m_currentService = currentService;
        emit currentServiceChanged(currentService);
    }

signals:
    void statusInfoChanged(QString info, bool isGood);
    void dataReceived(const QByteArray &data);
    void connectedChanged(bool connected);

    void currentServiceChanged(int currentService);

private slots:
    //QBluetothDeviceDiscoveryAgent
    void addDevice(const QBluetoothDeviceInfo&);
    void onScanFinished();
    void onDeviceScanError(QBluetoothDeviceDiscoveryAgent::Error);

    //QLowEnergyController
    void onServiceDiscovered(const QBluetoothUuid &);
    void onServiceScanDone();
    void onControllerError(QLowEnergyController::Error);
    void onDeviceConnected();
    void onDeviceDisconnected();

    //QLowEnergyService
    void onServiceStateChanged(QLowEnergyService::ServiceState s);
    void onCharacteristicChanged(const QLowEnergyCharacteristic &c,
                              const QByteArray &value);
    void serviceError(QLowEnergyService::ServiceError e);

    void read();
    void onCharacteristicRead(const QLowEnergyCharacteristic &c, const QByteArray &value);
    void onCharacteristicWrite(const QLowEnergyCharacteristic &c, const QByteArray &value);
    void update_currentService(int currentSerice);

private:
    inline void waitForWrite();
    void update_connected(bool connected){
        if(connected != m_connected){
            m_connected = connected;
            emit connectedChanged(connected);
        }
    }

    QBluetoothDeviceDiscoveryAgent *m_deviceDiscoveryAgent;
    QLowEnergyDescriptor m_notificationDesc;
    QLowEnergyController *m_control;
    QList<QBluetoothUuid> m_servicesUuid;
    QLowEnergyService *m_service;
    QLowEnergyCharacteristic m_readCharacteristic;
    QLowEnergyCharacteristic m_writeCharacteristic;
    QList<DeviceInfo*> m_devices;
//    bool m_foundService;
    QTimer *m_readTimer;
    bool m_connected;
    void searchCharacteristic();
    int m_currentService;
    QLowEnergyService::WriteMode m_writeMode;
};

#endif // BLEINTERFACE_H
