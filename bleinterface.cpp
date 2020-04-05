#include "bleinterface.h"
#include <QDebug>
#include <QEventLoop>

DeviceInfo::DeviceInfo(const QBluetoothDeviceInfo &info):
    QObject(), m_device(info)
{
}

QBluetoothDeviceInfo DeviceInfo::getDevice() const
{
    return m_device;
}

QString DeviceInfo::getAddress() const
{
#ifdef Q_OS_MAC
    // workaround for Core Bluetooth:
    return m_device.deviceUuid().toString();
#else
    return m_device.address().toString();
#endif
}

void DeviceInfo::setDevice(const QBluetoothDeviceInfo &device)
{
    m_device = device;
    emit deviceChanged();
}

BLEInterface::BLEInterface(QObject *parent) : QObject(parent),
    m_currentDevice(0),
    m_control(0),
    m_service(0),
    m_readTimer(0),
    m_connected(false),
    m_currentService(0)
{
    m_deviceDiscoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);

    connect(m_deviceDiscoveryAgent, SIGNAL(deviceDiscovered(const QBluetoothDeviceInfo&)),
            this, SLOT(addDevice(const QBluetoothDeviceInfo&)));
    connect(m_deviceDiscoveryAgent, SIGNAL(error(QBluetoothDeviceDiscoveryAgent::Error)),
            this, SLOT(onDeviceScanError(QBluetoothDeviceDiscoveryAgent::Error)));
    connect(m_deviceDiscoveryAgent, SIGNAL(finished()), this, SLOT(onScanFinished()));
}

BLEInterface::~BLEInterface()
{
    qDeleteAll(m_devices);
    m_devices.clear();
}

void BLEInterface::scanDevices()
{
    m_devicesNames.clear();
    qDeleteAll(m_devices);
    m_devices.clear();
    emit devicesNamesChanged(m_devicesNames);
    m_deviceDiscoveryAgent->start();
    emit statusInfoChanged("Scanning for devices...", true);
}

void BLEInterface::read(){
    if(m_service && m_readCharacteristic.isValid())
        m_service->readCharacteristic(m_readCharacteristic);
}

void BLEInterface::waitForWrite() {
    QEventLoop loop;
    connect(m_service, SIGNAL(characteristicWritten(QLowEnergyCharacteristic,QByteArray)),
            &loop, SLOT(quit()));
    loop.exec();
}

void BLEInterface::write(const QByteArray &data)
{
    qDebug() << "BLEInterface::write: " << data;
    if(m_service && m_writeCharacteristic.isValid()){
        if(data.length() > CHUNK_SIZE){
            int sentBytes = 0;
            while (sentBytes < data.length()) {
                m_service->writeCharacteristic(m_writeCharacteristic,
                                               data.mid(sentBytes, CHUNK_SIZE),
                                               m_writeMode);
                sentBytes += CHUNK_SIZE;
                if(m_writeMode == QLowEnergyService::WriteWithResponse){
                    waitForWrite();
                    if(m_service->error() != QLowEnergyService::NoError)
                        return;
                }
            }

        }
        else
            m_service->writeCharacteristic(m_writeCharacteristic, data, m_writeMode);
    }
}

void BLEInterface::addDevice(const QBluetoothDeviceInfo &device)
{
    if (device.coreConfigurations() & QBluetoothDeviceInfo::LowEnergyCoreConfiguration) {
        qWarning() << "Discovered LE Device name: " << device.name() << " Address: "
                   << device.address().toString();
        m_devicesNames.append(device.name());
        DeviceInfo *dev = new DeviceInfo(device);
        m_devices.append(dev);
        emit devicesNamesChanged(m_devicesNames);
        emit statusInfoChanged("Low Energy device found. Scanning for more...", true);
    }
    //...
}

void BLEInterface::onScanFinished()
{
    if (m_devicesNames.size() == 0)
        emit statusInfoChanged("No Low Energy devices found", false);
}

void BLEInterface::onDeviceScanError(QBluetoothDeviceDiscoveryAgent::Error error)
{
    if (error == QBluetoothDeviceDiscoveryAgent::PoweredOffError)
        emit statusInfoChanged("The Bluetooth adaptor is powered off, power it on before doing discovery.", false);
    else if (error == QBluetoothDeviceDiscoveryAgent::InputOutputError)
        emit statusInfoChanged("Writing or reading from the device resulted in an error.", false);
    else
        emit statusInfoChanged("An unknown error has occurred.", false);
}



void BLEInterface::connectCurrentDevice()
{
    if(m_devices.isEmpty())
        return;

    if (m_control) {
        m_control->disconnectFromDevice();
        delete m_control;
        m_control = 0;

    }
    m_control = new QLowEnergyController(m_devices[ m_currentDevice]->getDevice(), this);
    connect(m_control, SIGNAL(serviceDiscovered(QBluetoothUuid)),
            this, SLOT(onServiceDiscovered(QBluetoothUuid)));
    connect(m_control, SIGNAL(discoveryFinished()),
            this, SLOT(onServiceScanDone()));
    connect(m_control, SIGNAL(error(QLowEnergyController::Error)),
            this, SLOT(onControllerError(QLowEnergyController::Error)));
    connect(m_control, SIGNAL(connected()),
            this, SLOT(onDeviceConnected()));
    connect(m_control, SIGNAL(disconnected()),
            this, SLOT(onDeviceDisconnected()));

    m_control->connectToDevice();
}

void BLEInterface::onDeviceConnected()
{
    m_servicesUuid.clear();
    m_services.clear();
    setCurrentService(-1);
    emit servicesChanged(m_services);
    m_control->discoverServices();
}

void BLEInterface::onDeviceDisconnected()
{
    update_connected(false);
    emit statusInfoChanged("Service disconnected", false);
    qWarning() << "Remote device disconnected";
}

void BLEInterface::onServiceDiscovered(const QBluetoothUuid &gatt)
{
    Q_UNUSED(gatt)
    emit statusInfoChanged("Service discovered. Waiting for service scan to be done...", true);
}

void BLEInterface::onServiceScanDone()
{
    m_servicesUuid = m_control->services();
    if(m_servicesUuid.isEmpty())
        emit statusInfoChanged("Can't find any services.", true);
    else{
        m_services.clear();
        foreach (auto uuid, m_servicesUuid)
            m_services.append(uuid.toString());
        emit servicesChanged(m_services);
        m_currentService = -1;// to force call update_currentService(once)
        setCurrentService(0);
        emit statusInfoChanged("All services discovered.", true);
    }
}


void BLEInterface::disconnectDevice()
{
    m_readTimer->deleteLater();
    m_readTimer = NULL;

    if (m_devices.isEmpty()) {
        return;
    }

    //disable notifications
    if (m_notificationDesc.isValid() && m_service) {
        m_service->writeDescriptor(m_notificationDesc, QByteArray::fromHex("0000"));
    } else {
        m_control->disconnectFromDevice();
        delete m_service;
        m_service = 0;
    }
}

void BLEInterface::onControllerError(QLowEnergyController::Error error)
{
    emit statusInfoChanged("Cannot connect to remote device.", false);
    qWarning() << "Controller Error:" << error;
}

void BLEInterface::onCharacteristicChanged(const QLowEnergyCharacteristic &c,
                                           const QByteArray &value)
{
    Q_UNUSED(c)
    qDebug() << "Characteristic Changed: " << value;
    emit dataReceived(value);
}
void BLEInterface::onCharacteristicWrite(const QLowEnergyCharacteristic &c,
                                          const QByteArray &value)
{
    Q_UNUSED(c)
    qDebug() << "Characteristic Written: " << value;
}

void BLEInterface::update_currentService(int indx)
{
    delete m_service;
    m_service = 0;

    if (indx >= 0 && m_servicesUuid.count() > indx) {
        m_service = m_control->createServiceObject(
                    m_servicesUuid.at(indx), this);
    }

    if (!m_service) {
        emit statusInfoChanged("Service not found.", false);
        return;
    }

    connect(m_service, SIGNAL(stateChanged(QLowEnergyService::ServiceState)),
            this, SLOT(onServiceStateChanged(QLowEnergyService::ServiceState)));
    connect(m_service, SIGNAL(characteristicChanged(QLowEnergyCharacteristic,QByteArray)),
            this, SLOT(onCharacteristicChanged(QLowEnergyCharacteristic,QByteArray)));
    connect(m_service, SIGNAL(characteristicRead(QLowEnergyCharacteristic,QByteArray)),
            this, SLOT(onCharacteristicRead(QLowEnergyCharacteristic,QByteArray)));
    connect(m_service, SIGNAL(characteristicWritten(QLowEnergyCharacteristic,QByteArray)),
            this, SLOT(onCharacteristicWrite(QLowEnergyCharacteristic,QByteArray)));
    connect(m_service, SIGNAL(error(QLowEnergyService::ServiceError)),
            this, SLOT(serviceError(QLowEnergyService::ServiceError)));

    if(m_service->state() == QLowEnergyService::DiscoveryRequired) {
        emit statusInfoChanged("Connecting to service...", true);
        m_service->discoverDetails();
    }
    else
        searchCharacteristic();
}
void BLEInterface::onCharacteristicRead(const QLowEnergyCharacteristic &c,
                                        const QByteArray &value){
    Q_UNUSED(c)
    qDebug() << "Characteristic Read: " << value;
    emit dataReceived(value);
}

void BLEInterface::searchCharacteristic(){
    if(m_service){
        foreach (QLowEnergyCharacteristic c, m_service->characteristics()) {
            if(c.isValid()){
                if (c.properties() & QLowEnergyCharacteristic::WriteNoResponse ||
                    c.properties() & QLowEnergyCharacteristic::Write) {
                    m_writeCharacteristic = c;
                    update_connected(true);
                    if(c.properties() & QLowEnergyCharacteristic::WriteNoResponse)
                        m_writeMode = QLowEnergyService::WriteWithoutResponse;
                    else
                        m_writeMode = QLowEnergyService::WriteWithResponse;

                }
                if (c.properties() & QLowEnergyCharacteristic::Read) {
                    m_readCharacteristic = c;
                    if(!m_readTimer){
                        m_readTimer = new QTimer(this);
                        connect(m_readTimer, &QTimer::timeout, this, &BLEInterface::read);
                        m_readTimer->start(READ_INTERVAL_MS);
                    }
                }
                m_notificationDesc = c.descriptor(
                            QBluetoothUuid::ClientCharacteristicConfiguration);
                if (m_notificationDesc.isValid()) {
                    m_service->writeDescriptor(m_notificationDesc, QByteArray::fromHex("0100"));
                }
            }
        }
    }
}


void BLEInterface::onServiceStateChanged(QLowEnergyService::ServiceState s)
{
    qDebug() << "serviceStateChanged, state: " << s;
    if (s == QLowEnergyService::ServiceDiscovered) {
        searchCharacteristic();
    }
}
void BLEInterface::serviceError(QLowEnergyService::ServiceError e)
{
    qWarning() << "Service error:" << e;
}
