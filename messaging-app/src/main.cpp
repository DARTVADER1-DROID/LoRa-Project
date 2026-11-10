#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace
{
QStringList discoverSerialPorts()
{
    QStringList discoveredPorts;

    const auto ports = QSerialPortInfo::availablePorts();

    for (const QSerialPortInfo &port : ports) {
        QString details = port.portName();

        if (!port.description().isEmpty()) {
            details += " — " + port.description();
        }

        if (!port.manufacturer().isEmpty()) {
            details += "\nManufacturer: " + port.manufacturer();
        }

        if (!port.systemLocation().isEmpty()) {
            details += "\nSystem location: " + port.systemLocation();
        }

        if (port.hasVendorIdentifier() && port.hasProductIdentifier()) {
            details += QString("\nUSB VID: 0x%1  PID: 0x%2")
                           .arg(port.vendorIdentifier(), 4, 16, QChar('0'))
                           .arg(port.productIdentifier(), 4, 16, QChar('0'))
                           .toUpper();
        }

        discoveredPorts.append(details);
    }

    return discoveredPorts;
}

void refreshSerialPorts(QListWidget *portList,
                        QLabel *statusLabel,
                        QStringList *previousPorts)
{
    const QStringList currentPorts = discoverSerialPorts();

    // Avoid rebuilding the list when the available ports have not changed.
    if (currentPorts == *previousPorts) {
        return;
    }

    *previousPorts = currentPorts;
    portList->clear();

    if (currentPorts.isEmpty()) {
        statusLabel->setText("No serial ports detected.");
        return;
    }

    statusLabel->setText(
        QString("%1 serial port(s) detected").arg(currentPorts.size()));

    portList->addItems(currentPorts);
}
} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle("LoRa Messaging");
    window.resize(900, 600);

    auto *centralWidget = new QWidget(&window);
    auto *layout = new QVBoxLayout(centralWidget);
    auto *toolbarLayout = new QHBoxLayout();

    auto *heading = new QLabel("Available serial ports", centralWidget);
    auto *statusLabel = new QLabel(centralWidget);
    auto *refreshButton = new QPushButton("Refresh", centralWidget);
    auto *portList = new QListWidget(centralWidget);

    toolbarLayout->addWidget(statusLabel);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(refreshButton);

    layout->addWidget(heading);
    layout->addLayout(toolbarLayout);
    layout->addWidget(portList);

    QStringList previousPorts;

    // Discover ports immediately when the application starts.
    refreshSerialPorts(portList, statusLabel, &previousPorts);

    // Recheck the operating system's available serial ports every 2 seconds.
    auto *portScanTimer = new QTimer(&window);
    portScanTimer->setInterval(2000);

    QObject::connect(portScanTimer, &QTimer::timeout,
                     [&portList, &statusLabel, &previousPorts]() {
                         refreshSerialPorts(portList, statusLabel, &previousPorts);
                     });

    QObject::connect(refreshButton, &QPushButton::clicked,
                     [&portList, &statusLabel, &previousPorts]() {
                         refreshSerialPorts(portList, statusLabel, &previousPorts);
                     });

    portScanTimer->start();

    window.setCentralWidget(centralWidget);
    window.show();

    return app.exec();
}
