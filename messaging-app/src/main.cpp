#include <QApplication>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QSerialPortInfo>
#include <QVBoxLayout>
#include <QWidget>

namespace
{
void populateSerialPorts(QListWidget *portList)
{
    const auto ports = QSerialPortInfo::availablePorts();

    if (ports.isEmpty()) {
        portList->addItem("No serial ports detected.");
        return;
    }

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

    auto *heading = new QLabel("Available serial ports", centralWidget);
    auto *portList = new QListWidget(centralWidget);

    layout->addWidget(heading);
    layout->addWidget(portList);

    populateSerialPorts(portList);

    window.setCentralWidget(centralWidget);
    window.show();

    return app.exec();
}
