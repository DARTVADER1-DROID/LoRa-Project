const portsBody = document.querySelector("#ports-body");
const portCount = document.querySelector("#port-count");
const lastScan = document.querySelector("#last-scan");
const scanIndicator = document.querySelector("#scan-indicator");
const refreshButton = document.querySelector("#refresh-button");

function formatUsbId(value) {
  return value == null ? "—" : `0x${value.toString(16).padStart(4, "0").toUpperCase()}`;
}

function addCell(row, value) {
  const cell = document.createElement("td");
  cell.textContent = value || "—";
  row.append(cell);
}

function renderPorts(ports) {
  portsBody.replaceChildren();

  if (ports.length === 0) {
    const row = document.createElement("tr");
    const cell = document.createElement("td");
    cell.colSpan = 5;
    cell.className = "empty-state";
    cell.textContent = "No serial ports detected. Connect an Arduino or another serial device.";
    row.append(cell);
    portsBody.append(row);
    return;
  }

  for (const port of ports) {
    const row = document.createElement("tr");
    addCell(row, port.portName);

    const typeCell = document.createElement("td");
    const tag = document.createElement("span");
    tag.className = "device-tag";
    tag.textContent = port.portType;
    typeCell.append(tag);
    row.append(typeCell);

    addCell(row, port.manufacturer);
    addCell(row, port.product);
    addCell(row, port.vendorId == null
      ? "—"
      : `${formatUsbId(port.vendorId)} : ${formatUsbId(port.productId)}`);

    portsBody.append(row);
  }
}

async function scanPorts() {
  refreshButton.disabled = true;
  scanIndicator.textContent = "SCANNING";
  scanIndicator.className = "status-pill";

  try {
    const ports = await window.__TAURI__.core.invoke("list_serial_ports");
    renderPorts(ports);
    portCount.textContent = `${ports.length} serial port(s) detected`;
    scanIndicator.textContent = "UP TO DATE";
    scanIndicator.className = "status-pill status-ok";
  } catch (error) {
    portsBody.replaceChildren();
    const row = document.createElement("tr");
    const cell = document.createElement("td");
    cell.colSpan = 5;
    cell.className = "empty-state error-text";
    cell.textContent = `Unable to enumerate serial ports: ${error}`;
    row.append(cell);
    portsBody.append(row);
    portCount.textContent = "Port scan failed";
    scanIndicator.textContent = "ERROR";
    scanIndicator.className = "status-pill status-error";
  } finally {
    lastScan.textContent = `Last scan: ${new Date().toLocaleTimeString()}`;
    refreshButton.disabled = false;
  }
}

refreshButton.addEventListener("click", scanPorts);
scanPorts();
window.setInterval(scanPorts, 2000);
