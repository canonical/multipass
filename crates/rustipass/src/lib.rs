pub mod block_storage;
pub mod petname;

// Re-export petname public API for FFI
pub use petname::{make_name, new_petname, Petname};

// Re-export block storage public API for FFI
pub use block_storage::{
    attach_block_device, create_block_device, create_rust_block_device_manager,
    delete_block_device, detach_block_device, get_attached_devices_for_instance, get_block_device,
    list_block_devices, BlockDeviceInfo, BlockDeviceManager,
};

// CXX FFI Bridge for petname module
#[cxx::bridge(namespace = "multipass::petname")]
mod petname_ffi {
    extern "Rust" {
        type Petname;
        fn new_petname(num_words: i32, separator: &str) -> Result<Box<Petname>>;
        fn make_name(petname: &mut Petname) -> Result<String>;
    }
}

// CXX FFI Bridge for block storage module
#[cxx::bridge(namespace = "multipass::block_storage")]
mod block_storage_ffi {
    // Match the BlockInfo from the spec:
    // message BlockInfo {
    //     string name = 1;
    //     string size = 2;
    //     string path = 3;
    //     string attached_to = 4;
    // }
    struct BlockDeviceInfo {
        name: String,
        size: String,
        path: String,
        attached_to: String,
    }

    extern "Rust" {
        type BlockDeviceManager;

        fn create_rust_block_device_manager(storage_path: &str) -> Result<Box<BlockDeviceManager>>;

        fn create_block_device(
            manager: &mut BlockDeviceManager,
            name: &str,
            size: &str,
            format: &str,
        ) -> Result<String>;

        fn delete_block_device(manager: &mut BlockDeviceManager, id: &str) -> Result<()>;

        fn attach_block_device(
            manager: &mut BlockDeviceManager,
            id: &str,
            instance_name: &str,
        ) -> Result<()>;

        fn detach_block_device(manager: &mut BlockDeviceManager, id: &str) -> Result<()>;

        fn list_block_devices_ffi(manager: &BlockDeviceManager) -> Result<Vec<BlockDeviceInfo>>;

        fn get_block_device_ffi(manager: &BlockDeviceManager, id: &str) -> Result<BlockDeviceInfo>;

        fn get_attached_devices_for_instance(
            manager: &BlockDeviceManager,
            instance_name: &str,
        ) -> Result<Vec<String>>;
    }
}

// FFI wrapper functions that convert types appropriately
pub fn list_block_devices_ffi(
    manager: &block_storage::BlockDeviceManager,
) -> Result<Vec<block_storage_ffi::BlockDeviceInfo>, Box<dyn std::error::Error>> {
    eprintln!("[rustipass] Listing all block devices (FFI)");
    let devices = manager
        .list_disks()
        .map_err(|e| Box::new(e) as Box<dyn std::error::Error>)?;
    let result: Vec<block_storage_ffi::BlockDeviceInfo> = devices
        .into_iter()
        .map(|device| block_storage_ffi::BlockDeviceInfo {
            name: device.name,
            size: device.size,
            path: device.path,
            attached_to: device.attached_to,
        })
        .collect();
    eprintln!("[rustipass] Found {} block devices (FFI)", result.len());
    Ok(result)
}

pub fn get_block_device_ffi(
    manager: &block_storage::BlockDeviceManager,
    id: &str,
) -> Result<block_storage_ffi::BlockDeviceInfo, Box<dyn std::error::Error>> {
    eprintln!("[rustipass] Getting block device with ID: {} (FFI)", id);
    // Find the device from the list since get_disk doesn't exist yet
    let devices = manager
        .list_disks()
        .map_err(|e| Box::new(e) as Box<dyn std::error::Error>)?;
    for device in devices {
        if device.name == id {
            let result = block_storage_ffi::BlockDeviceInfo {
                name: device.name.clone(),
                size: device.size,
                path: device.path,
                attached_to: device.attached_to,
            };
            eprintln!("[rustipass] Retrieved block device: {} (FFI)", device.name);
            return Ok(result);
        }
    }
    eprintln!("[rustipass] Block device not found: {} (FFI)", id);
    Err(format!("Block device not found: {}", id).into())
}

// Future modules will have their own FFI bridges:
// #[cxx::bridge(namespace = "multipass::utils")]
// mod utils_ffi {
//     extern "Rust" {
//         // utils functions here
//     }
// }

// #[cxx::bridge(namespace = "multipass::network")]
// mod network_ffi {
//     extern "Rust" {
//         // network functions here
//     }
// }
