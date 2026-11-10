#include <QApplication>
#include <QCheckBox>
#include <QDateTime>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace
{
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

void refreshSerialPorts(QTableWidget *portTable,
                        QLabel *countLabel,
                        QLabel *scanTimeLabel,
                        const QStringList *previousSnapshot)
{
    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    const QStringList currentSnapshot = portSnapshot(ports);

    if (currentSnapshot != *previousSnapshot) {
        auto *mutableSnapshot = const_cast<QStringList *>(previousSnapshot);
        *mutableSnapshot = currentSnapshot;

        portTable->setRowCount(ports.size());

        for (int row = 0; row < ports.size(); ++row) {
            const QSerialPortInfo &port = ports.at(row);
            const QStringList values = {
                port.portName(),
                port.description().isEmpty() ? "—" : port.description(),
                port.manufacturer().isEmpty() ? "—" : port.manufacturer(),
                port.systemLocation().isEmpty() ? "—" : port.systemLocation(),
                usbIdentifier(port).isEmpty() ? "—" : usbIdentifier(port)
            };

            for (int column = 0; column < values.size(); ++column) {
                auto *item = new QTableWidgetItem(values.at(column));
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                portTable->setItem(row, column, item);
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
    window.resize(1100, 680);
    window.setMinimumSize(760, 480);

    auto *centralWidget = new QWidget(&window);
    auto *layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(18);

    auto *heading = new QLabel("Device connections", centralWidget);
    heading->setObjectName("pageHeading");

    auto *subtitle = new QLabel(
        "Discover serial devices connected to this computer.",
        centralWidget);
    subtitle->setObjectName("subtitle");

    auto *toolbar = new QHBoxLayout();
    auto *countLabel = new QLabel("Scanning for serial ports…", centralWidget);
    countLabel->setObjectName("countLabel");

    auto *refreshButton = new QPushButton("Refresh now", centralWidget);
    refreshButton->setObjectName("primaryButton");

    auto *autoScanCheck = new QCheckBox("Auto-scan every 2 seconds", centralWidget);
    autoScanCheck->setChecked(true);

    toolbar->addWidget(countLabel);
    toolbar->addStretch();
    toolbar->addWidget(autoScanCheck);
    toolbar->addWidget(refreshButton);

    auto *portTable = new QTableWidget(centralWidget);
    portTable->setColumnCount(5);
    portTable->setHorizontalHeaderLabels({
        "PORT", "DEVICE", "MANUFACTURER", "SYSTEM LOCATION", "USB VID : PID"
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
    portTable->setAlternatingRowColors(true);

    auto *connectionNote = new QLabel(
        "Port discovery only — a listed device has not yet been opened or "
        "verified as a LoRa gateway.",
        centralWidget);
    connectionNote->setObjectName("connectionNote");
    connectionNote->setWordWrap(true);

    auto *scanTimeLabel = new QLabel("Last scan: —", centralWidget);
    scanTimeLabel->setObjectName("scanTimeLabel");

    layout->addWidget(heading);
    layout->addWidget(subtitle);
    layout->addLayout(toolbar);
    layout->addWidget(portTable, 1);
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
            padding: 9px 14px;
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
        QCheckBox {
            color: #475569;
            spacing: 7px;
        }
        QLabel#connectionNote {
            background: #eff6ff;
            color: #1e40af;
            border: 1px solid #bfdbfe;
            border-radius: 6px;
            padding: 10px 12px;
        }
    )");

    QStringList previousSnapshot;
    auto *scanTimer = new QTimer(&window);
    scanTimer->setInterval(2000);

    const auto scan = [&]() {
        refreshSerialPorts(portTable, countLabel, scanTimeLabel,
                           &previousSnapshot);
    };

    QObject::connect(refreshButton, &QPushButton::clicked, &window, scan);
    QObject::connect(scanTimer, &QTimer::timeout, &window, scan);
    QObject::connect(autoScanCheck, &QCheckBox::toggled,
                     scanTimer, &QTimer::setActive);

    scan(); // Initial discovery as soon as the application starts.
    scanTimer->start();

    window.show();
    return app.exec();
}
