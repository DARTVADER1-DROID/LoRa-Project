#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QVBoxLayout>
#include <QWidget>

namespace
{
void populateSerialPorts(QListWidget *portList, QLabel *statusLabel)
{
    portList->clear();

    const auto ports = QSerialPortInfo::availablePorts();

    if (ports.isEmpty()) {
        statusLabel->setText("No serial ports detected.");
        return;
    }

    statusLabel->setText(
        QString("%1 serial port(s) detected").arg(ports.size()));

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

        portList->addItem(details);
    }
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

    QObject::connect(refreshButton, &QPushButton::clicked,
                     [&portList, &statusLabel]() {
                         populateSerialPorts(portList, statusLabel);
                     });

    // Discover serial ports immediately when the application starts.
    populateSerialPorts(portList, statusLabel);

    window.setCentralWidget(centralWidget);
    window.show();

    return app.exec();
}
