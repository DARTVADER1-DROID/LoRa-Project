#include <QApplication>
#include <QCheckBox>
#include <QDateTime>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace
{
constexpr qint32 SerialBaudRate = 9600;

QString usbIdentifier(const QSerialPortInfo &port)
{
    if (!port.hasVendorIdentifier() || !port.hasProductIdentifier()) {
        return {};
    }

    return QString("0x%1 : 0x%2")
        .arg(port.vendorIdentifier(), 4, 16, QChar('0'))
        .arg(port.productIdentifier(), 4, 16, QChar('0'))
        .toUpper();
}

QStringList portSnapshot(const QList<QSerialPortInfo> &ports)
{
    QStringList snapshot;

    for (const QSerialPortInfo &port : ports) {
        snapshot << port.portName()
                 << port.description()
                 << port.manufacturer()
                 << port.systemLocation()
                 << usbIdentifier(port);
    }

    return snapshot;
}

QString selectedPortName(const QTableWidget *table)
{
    const int row = table->currentRow();
    if (row < 0) {
        return {};
    }

    const auto *item = table->item(row, 0);
    return item ? item->data(Qt::UserRole).toString() : QString{};
}

void setConnectionStatus(QLabel *statusLabel,
                         QTableWidget *table,
                         const QString &status,
                         const QString &detail,
                         const QString &connectedPort = {})
{
    statusLabel->setText(status + (detail.isEmpty() ? QString{} : " — " + detail));

    for (int row = 0; row < table->rowCount(); ++row) {
        auto *portItem = table->item(row, 0);
        auto *statusItem = table->item(row, 5);
        if (!portItem || !statusItem) {
            continue;
        }

        const bool isConnected = !connectedPort.isEmpty() &&
            portItem->data(Qt::UserRole).toString() == connectedPort;

        statusItem->setText(isConnected ? "SERIAL LINK OPEN" : "AVAILABLE");
    }
}

void refreshSerialPorts(QTableWidget *table,
                        QLabel *countLabel,
                        QLabel *scanTimeLabel,
                        QLabel *statusLabel,
                        QSerialPort *serialPort,
                        QStringList *previousSnapshot)
{
    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    const QStringList currentSnapshot = portSnapshot(ports);

    if (serialPort->isOpen()) {
        bool connectedPortStillExists = false;
        for (const QSerialPortInfo &port : ports) {
            if (port.portName() == serialPort->portName()) {
                connectedPortStillExists = true;
                break;
            }
        }

        if (!connectedPortStillExists) {
            const QString removedPort = serialPort->portName();
            serialPort->close();
            setConnectionStatus(statusLabel, table, "FAULT",
                                removedPort + " was disconnected");
        }
    }

    if (currentSnapshot != *previousSnapshot) {
        const QString selected = selectedPortName(table);
        *previousSnapshot = currentSnapshot;
        table->setRowCount(ports.size());

        for (int row = 0; row < ports.size(); ++row) {
            const QSerialPortInfo &port = ports.at(row);
            const QStringList values = {
                port.portName(),
                port.description().isEmpty() ? "—" : port.description(),
                port.manufacturer().isEmpty() ? "—" : port.manufacturer(),
                port.systemLocation().isEmpty() ? "—" : port.systemLocation(),
                usbIdentifier(port).isEmpty() ? "—" : usbIdentifier(port),
                serialPort->isOpen() && serialPort->portName() == port.portName()
                    ? "SERIAL LINK OPEN" : "AVAILABLE"
            };

            for (int column = 0; column < values.size(); ++column) {
                auto *item = new QTableWidgetItem(values.at(column));
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                if (column == 0) {
                    item->setData(Qt::UserRole, port.portName());
                }
                table->setItem(row, column, item);
            }

            if (port.portName() == selected) {
                table->selectRow(row);
            }
        }
    }

    countLabel->setText(
        QString("%1 serial port(s) detected").arg(ports.size()));
    scanTimeLabel->setText(
        "Last scan: " + QDateTime::currentDateTime().toString("hh:mm:ss"));
}
} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("LoRa Messaging");

    QMainWindow window;
    window.setWindowTitle("LoRa Messaging");
    window.resize(1180, 720);
    window.setMinimumSize(820, 520);

    auto *centralWidget = new QWidget(&window);
    auto *layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(16);

    auto *heading = new QLabel("Device connections", centralWidget);
    heading->setObjectName("pageHeading");

    auto *subtitle = new QLabel(
        "Select a serial device and manage its USB connection.",
        centralWidget);
    subtitle->setObjectName("subtitle");

    auto *toolbar = new QHBoxLayout();
    auto *countLabel = new QLabel("Scanning for serial ports…", centralWidget);
    countLabel->setObjectName("countLabel");

    auto *refreshButton = new QPushButton("Refresh now", centralWidget);
    auto *autoScanCheck = new QCheckBox("Auto-scan every 2 seconds", centralWidget);
    autoScanCheck->setChecked(true);

    toolbar->addWidget(countLabel);
    toolbar->addStretch();
    toolbar->addWidget(autoScanCheck);
    toolbar->addWidget(refreshButton);

    auto *portTable = new QTableWidget(centralWidget);
    portTable->setColumnCount(6);
    portTable->setHorizontalHeaderLabels({
        "PORT", "DEVICE", "MANUFACTURER", "SYSTEM LOCATION",
        "USB VID : PID", "PORT STATUS"
    });
    portTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    portTable->setSelectionMode(QAbstractItemView::SingleSelection);
    portTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    portTable->setShowGrid(false);
    portTable->verticalHeader()->setVisible(false);
    portTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    portTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    portTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    portTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    portTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    portTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    portTable->setAlternatingRowColors(true);

    auto *connectionBar = new QHBoxLayout();
    auto *connectButton = new QPushButton("Connect", centralWidget);
    connectButton->setObjectName("primaryButton");
    auto *disconnectButton = new QPushButton("Disconnect", centralWidget);
    disconnectButton->setEnabled(false);
    connectionBar->addWidget(connectButton);
    connectionBar->addWidget(disconnectButton);
    connectionBar->addStretch();

    auto *statusLabel = new QLabel("DISCONNECTED — Select a port to connect.", centralWidget);
    statusLabel->setObjectName("statusLabel");
    statusLabel->setWordWrap(true);

    auto *connectionNote = new QLabel(
        "Serial link status checks USB/serial communication errors. It does not "
        "confirm that the Arduino firmware or LoRa radio is operational.",
        centralWidget);
    connectionNote->setObjectName("connectionNote");
    connectionNote->setWordWrap(true);

    auto *scanTimeLabel = new QLabel("Last scan: —", centralWidget);
    scanTimeLabel->setObjectName("scanTimeLabel");

    layout->addWidget(heading);
    layout->addWidget(subtitle);
    layout->addLayout(toolbar);
    layout->addWidget(portTable, 1);
    layout->addLayout(connectionBar);
    layout->addWidget(statusLabel);
    layout->addWidget(connectionNote);
    layout->addWidget(scanTimeLabel);

    window.setCentralWidget(centralWidget);

    window.setStyleSheet(R"(
        QMainWindow, QWidget {
            background: #f5f7fb;
            color: #172033;
            font-family: "Segoe UI", sans-serif;
            font-size: 10pt;
        }
        QLabel#pageHeading {
            font-size: 23pt;
            font-weight: 650;
            color: #111827;
        }
        QLabel#subtitle, QLabel#scanTimeLabel {
            color: #64748b;
        }
        QLabel#countLabel {
            font-weight: 600;
            color: #1d4ed8;
        }
        QTableWidget {
            background: white;
            alternate-background-color: #f8fafc;
            border: 1px solid #dbe2ea;
            border-radius: 8px;
            selection-background-color: #dbeafe;
            selection-color: #172033;
        }
        QHeaderView::section {
            background: #eef2f7;
            color: #475569;
            border: none;
            border-bottom: 1px solid #dbe2ea;
            padding: 11px 9px;
            font-size: 9pt;
            font-weight: 600;
        }
        QTableWidget::item {
            padding: 9px;
            border-bottom: 1px solid #edf0f4;
        }
        QPushButton {
            background: white;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 9px 16px;
        }
        QPushButton:hover {
            background: #eff6ff;
            border-color: #93c5fd;
        }
        QPushButton#primaryButton {
            background: #2563eb;
            color: white;
            border: 1px solid #2563eb;
            font-weight: 600;
        }
        QPushButton#primaryButton:hover {
            background: #1d4ed8;
        }
        QPushButton:disabled {
            color: #94a3b8;
            background: #e2e8f0;
            border-color: #e2e8f0;
        }
        QCheckBox {
            color: #475569;
            spacing: 7px;
        }
        QLabel#statusLabel {
            background: #f1f5f9;
            color: #334155;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 10px 12px;
            font-weight: 600;
        }
        QLabel#connectionNote {
            background: #eff6ff;
            color: #1e40af;
            border: 1px solid #bfdbfe;
            border-radius: 6px;
            padding: 10px 12px;
        }
    )");

    QSerialPort serialPort;
    QStringList previousSnapshot;
    auto *scanTimer = new QTimer(&window);
    scanTimer->setInterval(2000);

    const auto scan = [&]() {
        refreshSerialPorts(portTable, countLabel, scanTimeLabel, statusLabel,
                           &serialPort, &previousSnapshot);
        disconnectButton->setEnabled(serialPort.isOpen());
        connectButton->setEnabled(!serialPort.isOpen());
    };

    QObject::connect(refreshButton, &QPushButton::clicked, &window, scan);
    QObject::connect(scanTimer, &QTimer::timeout, &window, scan);
    QObject::connect(autoScanCheck, &QCheckBox::toggled,
                     &window, [scanTimer](bool enabled) {
                         if (enabled) {
                             scanTimer->start();
                         } else {
                             scanTimer->stop();
                         }
                     });

    QObject::connect(portTable, &QTableWidget::itemSelectionChanged,
                     &window, [connectButton, disconnectButton, &serialPort]() {
                         connectButton->setEnabled(!serialPort.isOpen());
                         disconnectButton->setEnabled(serialPort.isOpen());
                     });

    QObject::connect(connectButton, &QPushButton::clicked, &window,
                     [&]() {
                         const QString portName = selectedPortName(portTable);
                         if (portName.isEmpty()) {
                             setConnectionStatus(statusLabel, portTable,
                                                 "NOT CONNECTED",
                                                 "Select a serial port first");
                             return;
                         }

                         if (serialPort.isOpen()) {
                             serialPort.close();
                         }

                         serialPort.setPortName(portName);
                         serialPort.setBaudRate(SerialBaudRate);
                         serialPort.setDataBits(QSerialPort::Data8);
                         serialPort.setParity(QSerialPort::NoParity);
                         serialPort.setStopBits(QSerialPort::OneStop);
                         serialPort.setFlowControl(QSerialPort::NoFlowControl);

                         if (!serialPort.open(QIODevice::ReadWrite)) {
                             setConnectionStatus(statusLabel, portTable, "FAULT",
                                                 serialPort.errorString());
                             scan();
                             return;
                         }

                         setConnectionStatus(
                             statusLabel, portTable, "SERIAL LINK OPEN",
                             portName + " at " + QString::number(SerialBaudRate) +
                                 " baud; gateway not yet verified",
                             portName);
                         scan();
                     });

    QObject::connect(disconnectButton, &QPushButton::clicked, &window,
                     [&]() {
                         if (serialPort.isOpen()) {
                             serialPort.close();
                         }
                         setConnectionStatus(statusLabel, portTable,
                                             "DISCONNECTED", "Serial port closed");
                         scan();
                     });

    QObject::connect(&serialPort, &QSerialPort::errorOccurred, &window,
                     [&](QSerialPort::SerialPortError error) {
                         if (error == QSerialPort::NoError ||
                             error == QSerialPort::NotOpenError) {
                             return;
                         }

                         const QString fault = serialPort.errorString();
                         if (serialPort.isOpen()) {
                             serialPort.close();
                         }

                         setConnectionStatus(statusLabel, portTable,
                                             "FAULT", fault);
                         connectButton->setEnabled(true);
                         disconnectButton->setEnabled(false);
                     });

    scan();
    scanTimer->start();

    window.show();
    return app.exec();
}
