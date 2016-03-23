#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_bleInterface = new BLEInterface(this);
    m_bleInterface->scanDevices();
    connect(m_bleInterface, &BLEInterface::dataReceived,
            this, &MainWindow::dataReceived);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_bleScanButton_clicked()
{
    m_bleInterface->scanDevices();

}

void MainWindow::on_connectButton_clicked()
{
    m_bleInterface->set_currentDevice(ui->devicesComboBox->currentIndex());
    m_bleInterface->connectCurrentDevice();
}

void MainWindow::on_comboBox_currentIndexChanged(int index)
{
    m_bleInterface->setCurrentService(index);
}

void MainWindow::on_sendButton_clicked()
{
    QByteArray data;
    if(ui->asciiRadioButton->isChecked())
        data =  QByteArray(ui->lineEdit->text().toLatin1());
    else
        data = QByteArray::fromHex(ui->lineEdit->text().toLatin1());
    m_bleInterface->write(data);
}
void MainWindow::dataReceived(QByteArray data){
    if(ui->asciiRadioButton->isChecked()){
        ui->receivedTextEdit->append("\n");
        ui->receivedTextEdit->append(data);
    }
    else{
        ui->receivedTextEdit->append(data.toHex());
    }
}
