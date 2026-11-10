use serde::Serialize;

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
struct SerialPort {
    port_name: String,
    port_type: String,
    manufacturer: Option<String>,
    product: Option<String>,
    serial_number: Option<String>,
    vendor_id: Option<u16>,
    product_id: Option<u16>,
}

#[tauri::command]
fn list_serial_ports() -> Result<Vec<SerialPort>, String> {
    let ports = serialport::available_ports().map_err(|error| error.to_string())?;

    let ports = ports
        .into_iter()
        .map(|port| {
            let mut result = SerialPort {
                port_name: port.port_name,
                port_type: String::from("Unknown"),
                manufacturer: None,
                product: None,
                serial_number: None,
                vendor_id: None,
                product_id: None,
            };

            match port.port_type {
                serialport::SerialPortType::UsbPort(usb) => {
                    result.port_type = String::from("USB");
                    result.manufacturer = usb.manufacturer;
                    result.product = usb.product;
                    result.serial_number = usb.serial_number;
                    result.vendor_id = Some(usb.vid);
                    result.product_id = Some(usb.pid);
                }
                serialport::SerialPortType::PciPort => {
                    result.port_type = String::from("PCI");
                }
                serialport::SerialPortType::BluetoothPort => {
                    result.port_type = String::from("Bluetooth");
                }
                serialport::SerialPortType::Unknown => {}
            }

            result
        })
        .collect();

    Ok(ports)
}

fn main() {
    tauri::Builder::default()
        .invoke_handler(tauri::generate_handler![list_serial_ports])
        .run(tauri::generate_context!())
        .expect("error while running LoRa Messaging");
}
