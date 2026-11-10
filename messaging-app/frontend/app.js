const invoke = window.__TAURI__.core.invoke;
const portsBody = document.querySelector("#ports-body");
const portCount = document.querySelector("#port-count");
const lastScan = document.querySelector("#last-scan");
const scanIndicator = document.querySelector("#scan-indicator");
const refreshButton = document.querySelector("#refresh-button");
const portSelect = document.querySelector("#port-select");
const connectButton = document.querySelector("#connect-button");
const disconnectButton = document.querySelector("#disconnect-button");
const connectionIndicator = document.querySelector("#connection-indicator");
const connectionDetail = document.querySelector("#connection-detail");

let ports = [];
let connection = { state: "disconnected", portName: null, detail: "No serial port is connected." };
let scanInProgress = false;

function formatUsbId(value) {
  return value == null ? "—" : `0x${value.toString(16).padStart(4, "0").toUpperCase()}`;
}

function addCell(row, value) {
  const cell = document.createElement("td");
  cell.textContent = value || "—";
  row.append(cell);
}

function setStatus(element, state, label) {
  element.textContent = label;
  element.className = `status-pill status-${state}`;
}

function renderConnection() {
  const connected = connection.state === "connected";
  const fault = connection.state === "fault";
  setStatus(connectionIndicator, connected ? "ok" : fault ? "error" : "neutral",
    connection.state.toUpperCase());
  const identity = connection.deviceIdentity;
  const identityText = identity
    ? ` Identified Arduino LoRa device · ID ${identity.deviceId} · Boot ${identity.bootId} · ${identity.firmware} · LoRa ${identity.loraStatus} · Protocol ${identity.protocol}.`
    : connected
      ? " No supported firmware identity response received; this may be a generic serial device or older firmware."
      : "";
  connectionDetail.textContent = (connection.lastError
    ? `${connection.detail} ${connection.lastError}`
    : connection.detail) + identityText;
  disconnectButton.disabled = !connected;
  connectButton.disabled = connected || !portSelect.value;
  portSelect.disabled = connected;
}

function renderPorts() {
  const previousSelection = portSelect.value;
  portsBody.replaceChildren();
  portSelect.replaceChildren();

  const placeholder = document.createElement("option");
  placeholder.value = "";
  placeholder.textContent = ports.length ? "Select a serial port…" : "No serial ports detected";
  portSelect.append(placeholder);

  if (!ports.length) {
    const row = document.createElement("tr");
    const cell = document.createElement("td");
    cell.colSpan = 7;
    cell.className = "empty-state";
    cell.textContent = "No serial ports detected. Connect an Arduino or another serial device.";
    row.append(cell);
    portsBody.append(row);
  }

  for (const port of ports) {
    const option = document.createElement("option");
    option.value = port.portName;
    option.textContent = `${port.portName} — ${port.product || port.manufacturer || port.portType}`;
    portSelect.append(option);

    const row = document.createElement("tr");
    addCell(row, port.portName);
    addCell(row, port.portType);
    addCell(row, port.manufacturer);
    addCell(row, port.product);
    addCell(row, port.vendorId == null
      ? "—"
      : `${formatUsbId(port.vendorId)} : ${formatUsbId(port.productId)}`);

    const availabilityCell = document.createElement("td");
    const availableTag = document.createElement("span");
    availableTag.className = "device-tag status-ok";
    availableTag.textContent = "AVAILABLE";
    availabilityCell.append(availableTag);
    row.append(availabilityCell);

    const statusCell = document.createElement("td");
    const statusTag = document.createElement("span");
    const isConnected = port.isConnected;
    const hasFault = port.hasFault;
    statusTag.className = `device-tag ${isConnected ? "status-ok" : hasFault ? "status-error" : ""}`;
    statusTag.textContent = isConnected ? "PORT OPEN" : hasFault ? "FAULT" : "NOT CONNECTED";
    statusCell.append(statusTag);
    row.append(statusCell);

    row.className = isConnected || hasFault ? "selected-row" : "";
    row.addEventListener("click", () => {
      if (!portSelect.disabled) {
        portSelect.value = port.portName;
        renderConnection();
      }
    });
    portsBody.append(row);
  }

  const selected = ports.some(port => port.portName === previousSelection)
    ? previousSelection
    : connection.portName || "";
  portSelect.value = selected;
  renderConnection();
}

async function refreshConnectionStatus() {
  try {
    connection = await invoke("get_connection_status");
  } catch (error) {
    connection = {
      state: "fault",
      detail: "Unable to read connection status.",
      lastError: String(error)
    };
  }
  renderConnection();
}

async function scanPorts() {
  if (scanInProgress) return;
  scanInProgress = true;
  refreshButton.disabled = true;
  setStatus(scanIndicator, "neutral", "SCANNING");

  try {
    ports = await invoke("list_serial_ports");
    await refreshConnectionStatus();
    renderPorts();
    portCount.textContent = `${ports.length} serial port(s) detected`;
    setStatus(scanIndicator, "ok", "UP TO DATE");
  } catch (error) {
    portsBody.replaceChildren();
    const row = document.createElement("tr");
    const cell = document.createElement("td");
    cell.colSpan = 7;
    cell.className = "empty-state error-text";
    cell.textContent = `Unable to enumerate serial ports: ${error}`;
    row.append(cell);
    portsBody.append(row);
    portCount.textContent = "Port scan failed";
    setStatus(scanIndicator, "error", "SCAN ERROR");
  } finally {
    lastScan.textContent = `Last scan: ${new Date().toLocaleTimeString()}`;
    refreshButton.disabled = false;
    scanInProgress = false;
  }
}

async function connectSelectedPort() {
  const portName = portSelect.value;
  if (!portName) return;
  connectButton.disabled = true;
  connectionDetail.textContent = `Opening ${portName} at 9600 baud…`;

  try {
    connection = await invoke("connect_serial_port", { portName });
  } catch (error) {
    connection = {
      state: "fault",
      detail: "Connection failed.",
      deviceIdentity: null,
      lastError: String(error)
    };
  }

  renderConnection();
  await scanPorts();
}

async function disconnectPort() {
  disconnectButton.disabled = true;
  try {
    connection = await invoke("disconnect_serial_port");
  } catch (error) {
    connection = {
      state: "fault",
      detail: "Disconnect operation failed.",
      lastError: String(error)
    };
  }
  renderConnection();
  await scanPorts();
}

portSelect.addEventListener("change", renderConnection);
refreshButton.addEventListener("click", scanPorts);
connectButton.addEventListener("click", connectSelectedPort);
disconnectButton.addEventListener("click", disconnectPort);

scanPorts();
window.setInterval(scanPorts, 2000);
