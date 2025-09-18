// Block storage module for managing extra disks in Multipass
pub mod backend;
pub mod manager;
pub mod qemu;
pub mod types;

pub use manager::BlockDeviceManager;
pub use types::{BlockDevice, BlockDeviceInfo, BlockStorageError, Result};

// FFI functions for C++ integration
pub fn create_rust_block_device_manager(
    storage_path: &str,
) -> std::result::Result<Box<BlockDeviceManager>, Box<dyn std::error::Error>> {
    let path = std::path::PathBuf::from(storage_path);
    eprintln!(
        "[rustipass] Creating block device manager with storage path: {}",
        storage_path
    );
    let manager = BlockDeviceManager::new(path.to_string_lossy().to_string());
    eprintln!("[rustipass] Block device manager created successfully");
    Ok(Box::new(manager))
}

pub fn create_block_device(
    manager: &mut BlockDeviceManager,
    name: &str,
    size: &str,
    _format: &str, // format is ignored for now since create_disk doesn't take it
) -> std::result::Result<String, Box<dyn std::error::Error>> {
    eprintln!(
        "[rustipass] Creating block device: name={}, size={}",
        name, size
    );
    let id = manager
        .create_disk(name, size)
        .map_err(|e| Box::new(e) as Box<dyn std::error::Error>)?;
    eprintln!("[rustipass] Block device created with ID: {}", id);
    Ok(id)
}

pub fn delete_block_device(
    manager: &mut BlockDeviceManager,
    id: &str,
) -> std::result::Result<(), Box<dyn std::error::Error>> {
    eprintln!("[rustipass] Deleting block device with ID: {}", id);
    manager
        .delete_disk(id)
        .map_err(|e| Box::new(e) as Box<dyn std::error::Error>)?;
    eprintln!("[rustipass] Block device deleted successfully");
    Ok(())
}

pub fn attach_block_device(
    manager: &mut BlockDeviceManager,
    id: &str,
    instance_name: &str,
) -> std::result::Result<(), Box<dyn std::error::Error>> {
    eprintln!(
        "[rustipass] Attaching block device {} to instance {}",
        id, instance_name
    );
    manager
        .attach_disk(id, instance_name)
        .map_err(|e| Box::new(e) as Box<dyn std::error::Error>)?;
    eprintln!("[rustipass] Block device attached successfully");
    Ok(())
}

pub fn detach_block_device(
    manager: &mut BlockDeviceManager,
    id: &str,
) -> std::result::Result<(), Box<dyn std::error::Error>> {
    eprintln!("[rustipass] Detaching block device with ID: {}", id);
    // Find which instance this disk is attached to, for now use empty string
    // We'll need to enhance this later when we have proper disk state tracking
    manager
        .detach_disk(id, "")
        .map_err(|e| Box::new(e) as Box<dyn std::error::Error>)?;
    eprintln!("[rustipass] Block device detached successfully");
    Ok(())
}

pub fn list_block_devices(
    manager: &BlockDeviceManager,
) -> std::result::Result<Vec<BlockDeviceInfo>, Box<dyn std::error::Error>> {
    eprintln!("[rustipass] Listing all block devices");
    let devices = manager
        .list_disks()
        .map_err(|e| Box::new(e) as Box<dyn std::error::Error>)?;
    eprintln!("[rustipass] Found {} block devices", devices.len());
    Ok(devices)
}

pub fn get_block_device(
    manager: &BlockDeviceManager,
    id: &str,
) -> std::result::Result<BlockDeviceInfo, Box<dyn std::error::Error>> {
    eprintln!("[rustipass] Getting block device with ID: {}", id);
    // Since get_disk doesn't exist yet, we'll find it from the list
    let devices = manager
        .list_disks()
        .map_err(|e| Box::new(e) as Box<dyn std::error::Error>)?;
    for device in devices {
        if device.name == id {
            eprintln!("[rustipass] Retrieved block device: {}", device.name);
            return Ok(device);
        }
    }
    eprintln!("[rustipass] Block device not found: {}", id);
    Err(format!("Block device not found: {}", id).into())
}

pub fn get_attached_devices_for_instance(
    manager: &BlockDeviceManager,
    instance_name: &str,
) -> std::result::Result<Vec<String>, Box<dyn std::error::Error>> {
    eprintln!(
        "[rustipass] Getting attached devices for instance: {}",
        instance_name
    );
    // Since get_attached_disks doesn't exist yet, we'll filter from list_disks
    let devices = manager
        .list_disks()
        .map_err(|e| Box::new(e) as Box<dyn std::error::Error>)?;
    let attached_devices: Vec<String> = devices
        .into_iter()
        .filter(|device| device.attached_to == instance_name)
        .map(|device| device.name)
        .collect();
    eprintln!(
        "[rustipass] Found {} attached devices for instance {}",
        attached_devices.len(),
        instance_name
    );
    Ok(attached_devices)
}
