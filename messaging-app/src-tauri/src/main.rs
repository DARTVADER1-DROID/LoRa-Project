use serde::Serialize;
use std::sync::Mutex;
use std::time::Duration;
use tauri::State;

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
struct SerialPortInfo {
    port_name: String,
    port_type: String,
    manufacturer: Option<String>,
    product: Option<String>,
    serial_number: Option<String>,
    vendor_id: Option<u16>,
    product_id: Option<u16>,
    is_available: bool,
    is_connected: bool,
    has_fault: bool,
}

struct ActiveConnection {
    port_name: String,
    _port: Box<dyn serialport::SerialPort>,
}

#[derive(Default)]
struct ConnectionState {
    active: Option<ActiveConnection>,
    fault_port_name: Option<String>,
    last_error: Option<String>,
}

#[derive(Default)]
struct AppState {
    connection: Mutex<ConnectionState>,
}

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
struct ConnectionStatus {
    state: String,
    port_name: Option<String>,
    detail: String,
    last_error: Option<String>,
}

fn connection_status(state: &ConnectionState) -> ConnectionStatus {
    if let Some(active) = &state.active {
        return ConnectionStatus {
            state: "connected".into(),
            port_name: Some(active.port_name.clone()),
            detail: "The operating system has an open serial handle. Firmware and LoRa health are not verified.".into(),
            last_error: None,
        };
    }

    if let Some(error) = &state.last_error {
        return ConnectionStatus {
            state: "fault".into(),
            port_name: state.fault_port_name.clone(),
            detail: "The last serial operation failed or the device disappeared.".into(),
            last_error: Some(error.clone()),
        };
    }

    ConnectionStatus {
        state: "disconnected".into(),
        port_name: None,
        detail: "No serial port is connected.".into(),
        last_error: None,
    }
}

#[tauri::command]
fn list_serial_ports(state: State<'_, AppState>) -> Result<Vec<SerialPortInfo>, String> {
    let connection = state.connection.lock()
        .map_err(|_| "Connection state is unavailable".to_string())?;
    let connected_name = connection.active.as_ref().map(|active| active.port_name.as_str());
    let fault_name = connection.fault_port_name.as_deref();
    let ports = serialport::available_ports().map_err(|error| error.to_string())?;

    Ok(ports.into_iter().map(|port| {
        let mut result = SerialPortInfo {
            port_name: port.port_name,
            port_type: "Unknown".into(),
            manufacturer: None,
            product: None,
            serial_number: None,
            vendor_id: None,
            product_id: None,
            is_available: true,
            is_connected: false,
            has_fault: false,
        };

        match port.port_type {
            serialport::SerialPortType::UsbPort(usb) => {
                result.port_type = "USB".into();
                result.manufacturer = usb.manufacturer;
                result.product = usb.product;
                result.serial_number = usb.serial_number;
                result.vendor_id = Some(usb.vid);
                result.product_id = Some(usb.pid);
            }
            serialport::SerialPortType::PciPort => result.port_type = "PCI".into(),
            serialport::SerialPortType::BluetoothPort => result.port_type = "Bluetooth".into(),
            serialport::SerialPortType::Unknown => {}
        }

        result.is_connected = connected_name == Some(result.port_name.as_str());
        result.has_fault = fault_name == Some(result.port_name.as_str());
        result
    }).collect())
}

#[tauri::command]
fn connect_serial_port(port_name: String, state: State<'_, AppState>) -> Result<ConnectionStatus, String> {
    let mut connection = state.connection.lock()
        .map_err(|_| "Connection state is unavailable".to_string())?;

    if connection.active.as_ref().is_some_and(|active| active.port_name == port_name) {
        return Ok(connection_status(&connection));
    }

    connection.active = None;
    connection.fault_port_name = None;
    connection.last_error = None;

    let available = serialport::available_ports()
        .map_err(|error| error.to_string())?
        .into_iter()
        .any(|port| port.port_name == port_name);

    if !available {
        let error = format!("Serial port {port_name} is no longer available.");
        connection.fault_port_name = Some(port_name);
        connection.last_error = Some(error.clone());
        return Err(error);
    }

    match serialport::new(&port_name, 9600).timeout(Duration::from_millis(250)).open() {
        Ok(port) => {
            connection.active = Some(ActiveConnection { port_name, _port: port });
            Ok(connection_status(&connection))
        }
        Err(error) => {
            let message = format!("Unable to open serial port: {error}");
            connection.fault_port_name = Some(port_name);
            connection.last_error = Some(message.clone());
            Err(message)
        }
    }
}

#[tauri::command]
fn disconnect_serial_port(state: State<'_, AppState>) -> Result<ConnectionStatus, String> {
    let mut connection = state.connection.lock()
        .map_err(|_| "Connection state is unavailable".to_string())?;
    connection.active = None;
    connection.fault_port_name = None;
    connection.last_error = None;
    Ok(connection_status(&connection))
}

#[tauri::command]
fn get_connection_status(state: State<'_, AppState>) -> Result<ConnectionStatus, String> {
    let mut connection = state.connection.lock()
        .map_err(|_| "Connection state is unavailable".to_string())?;

    if let Some(active) = &connection.active {
        let active_name = active.port_name.clone();
        let still_present = serialport::available_ports()
            .map_err(|error| error.to_string())?
            .into_iter()
            .any(|port| port.port_name == active_name);

        if !still_present {
            connection.active = None;
            connection.fault_port_name = Some(active_name.clone());
            connection.last_error = Some(format!(
                "Device on {active_name} was disconnected or is no longer visible to the operating system."
            ));
        }
    }

    Ok(connection_status(&connection))
}

fn main() {
    tauri::Builder::default()
        .manage(AppState::default())
        .invoke_handler(tauri::generate_handler![
            list_serial_ports,
            connect_serial_port,
            disconnect_serial_port,
            get_connection_status
        ])
        .run(tauri::generate_context!())
        .expect("error while running LoRa Messaging");
}
