use crate::block_storage::backend::BlockDeviceBackend;
use crate::block_storage::qemu::QemuBackend;
use crate::block_storage::types::{
    format_size, parse_size, BlockDevice, BlockDeviceInfo, BlockDeviceMetadata, BlockStorageError,
    DiskFormat, Result,
};
use std::collections::HashMap;
use std::fs;
use std::path::PathBuf;
use std::sync::{Arc, Mutex};

/// Manages block devices across the system
pub struct BlockDeviceManager {
    #[allow(dead_code)]
    data_dir: PathBuf,
    block_devices_dir: PathBuf,
    backend: Box<dyn BlockDeviceBackend>,
    metadata: Arc<Mutex<BlockDeviceMetadata>>,
    zone: String,
}

impl BlockDeviceManager {
    /// Create a new BlockDeviceManager
    pub fn new(data_dir: String) -> Self {
        let data_path = PathBuf::from(&data_dir);
        let zone = "zone1".to_string(); // Default zone, can be made configurable
        let block_devices_dir = data_path.join(&zone).join("block-devices");

        // Ensure the block devices directory exists
        let _ = fs::create_dir_all(&block_devices_dir);

        let mut manager = BlockDeviceManager {
            data_dir: data_path,
            block_devices_dir,
            backend: Box::new(QemuBackend::new()),
            metadata: Arc::new(Mutex::new(BlockDeviceMetadata::new())),
            zone,
        };

        // Load existing metadata
        let _ = manager.load_metadata();

        manager
    }

    /// Generate a unique disk name
    fn generate_disk_name(&self) -> String {
        let letters = "abcdefhijkmnpqrstuvwxyz";
        let alphanumeric = "abcdefghijkmnopqrstuvwxyz0123456789";

        for _ in 0..1000 {
            let mut name = String::from("disk-");

            // Generate 2-character suffix with at least one letter
            if rand::random::<bool>() {
                // First char is letter
                name.push(
                    letters
                        .chars()
                        .nth(rand::random::<usize>() % letters.len())
                        .unwrap(),
                );
                name.push(
                    alphanumeric
                        .chars()
                        .nth(rand::random::<usize>() % alphanumeric.len())
                        .unwrap(),
                );
            } else {
                // Second char is letter
                name.push(
                    alphanumeric
                        .chars()
                        .nth(rand::random::<usize>() % alphanumeric.len())
                        .unwrap(),
                );
                name.push(
                    letters
                        .chars()
                        .nth(rand::random::<usize>() % letters.len())
                        .unwrap(),
                );
            }

            if !self.has_block_device(&name) {
                return name;
            }
        }

        // Fallback to UUID-based name
        format!(
            "disk-{}",
            uuid::Uuid::new_v4()
                .to_string()
                .chars()
                .take(8)
                .collect::<String>()
        )
    }

    /// Check if a block device exists
    pub fn has_block_device(&self, name: &str) -> bool {
        let metadata = self.metadata.lock().unwrap();
        metadata.get_device(name).is_some()
    }

    /// Create a new disk
    pub fn create_disk(&self, name: &str, size_str: &str) -> Result<String> {
        let disk_name = if name.is_empty() {
            self.generate_disk_name()
        } else {
            // Check for name collision
            if self.has_block_device(name) {
                return Err(BlockStorageError::DiskAlreadyExists(name.to_string()));
            }
            name.to_string()
        };

        // Parse size
        let size_bytes = parse_size(size_str)?;

        // Create disk path
        let disk_path = self.block_devices_dir.join(format!("{}.qcow2", disk_name));

        // Create the disk using the backend
        self.backend
            .create_disk(&disk_path, size_bytes, DiskFormat::Qcow2)?;

        // Create block device metadata
        let block_device = BlockDevice {
            name: disk_name.clone(),
            path: disk_path,
            size_bytes,
            format: DiskFormat::Qcow2,
            attached_to: None,
            zone: self.zone.clone(),
        };

        // Add to metadata
        {
            let mut metadata = self.metadata.lock().unwrap();
            metadata.add_device(block_device);
        }

        // Save metadata
        self.save_metadata()?;

        Ok(disk_name)
    }

    /// Delete a disk
    pub fn delete_disk(&self, name: &str) -> Result<()> {
        let device = {
            let metadata = self.metadata.lock().unwrap();
            metadata
                .get_device(name)
                .cloned()
                .ok_or_else(|| BlockStorageError::DiskNotFound(name.to_string()))?
        };

        // Check if disk is in use
        if device.attached_to.is_some() {
            return Err(BlockStorageError::DiskInUse(name.to_string()));
        }

        // Delete the disk file
        self.backend.delete_disk(&device.path)?;

        // Remove from metadata
        {
            let mut metadata = self.metadata.lock().unwrap();
            metadata.remove_device(name);
        }

        // Save metadata
        self.save_metadata()?;

        Ok(())
    }

    /// List all disks
    pub fn list_disks(&self) -> Result<Vec<BlockDeviceInfo>> {
        let metadata = self.metadata.lock().unwrap();
        let devices = metadata.list_devices();

        let mut result = Vec::new();
        for device in devices {
            result.push(BlockDeviceInfo {
                name: device.name.clone(),
                size: format_size(device.size_bytes),
                path: device.path.to_string_lossy().to_string(),
                attached_to: device.attached_to.clone().unwrap_or_default(),
            });
        }

        Ok(result)
    }

    /// Attach a disk to an instance
    pub fn attach_disk(&self, disk_name: &str, instance_name: &str) -> Result<()> {
        let mut metadata = self.metadata.lock().unwrap();

        let device = metadata
            .get_device_mut(disk_name)
            .ok_or_else(|| BlockStorageError::DiskNotFound(disk_name.to_string()))?;

        if let Some(ref attached) = device.attached_to {
            return Err(BlockStorageError::AttachmentError(format!(
                "Disk '{}' is already attached to '{}'",
                disk_name, attached
            )));
        }

        device.attached_to = Some(instance_name.to_string());
        drop(metadata);

        // Save metadata
        self.save_metadata()?;

        Ok(())
    }

    /// Detach a disk from an instance
    pub fn detach_disk(&self, disk_name: &str, instance_name: &str) -> Result<()> {
        let mut metadata = self.metadata.lock().unwrap();

        let device = metadata
            .get_device_mut(disk_name)
            .ok_or_else(|| BlockStorageError::DiskNotFound(disk_name.to_string()))?;

        match &device.attached_to {
            Some(attached) if attached == instance_name => {
                device.attached_to = None;
            }
            Some(attached) => {
                return Err(BlockStorageError::AttachmentError(format!(
                    "Disk '{}' is attached to '{}', not '{}'",
                    disk_name, attached, instance_name
                )));
            }
            None => {
                return Err(BlockStorageError::AttachmentError(format!(
                    "Disk '{}' is not attached to any instance",
                    disk_name
                )));
            }
        }

        drop(metadata);

        // Save metadata
        self.save_metadata()?;

        Ok(())
    }

    /// Save metadata to disk
    fn save_metadata(&self) -> Result<()> {
        let metadata_path = self.block_devices_dir.join("metadata.json");
        let metadata = self.metadata.lock().unwrap();

        let json = serde_json::to_string_pretty(&*metadata).map_err(|e| {
            BlockStorageError::IoError(std::io::Error::new(
                std::io::ErrorKind::Other,
                format!("Failed to serialize metadata: {}", e),
            ))
        })?;

        fs::write(metadata_path, json)?;

        Ok(())
    }

    /// Load metadata from disk
    fn load_metadata(&mut self) -> Result<()> {
        let metadata_path = self.block_devices_dir.join("metadata.json");

        if !metadata_path.exists() {
            return Ok(());
        }

        let json = fs::read_to_string(metadata_path)?;
        let loaded_metadata: BlockDeviceMetadata = serde_json::from_str(&json).map_err(|e| {
            BlockStorageError::IoError(std::io::Error::new(
                std::io::ErrorKind::Other,
                format!("Failed to deserialize metadata: {}", e),
            ))
        })?;

        let mut metadata = self.metadata.lock().unwrap();
        *metadata = loaded_metadata;

        Ok(())
    }

    /// Get a specific block device
    pub fn get_block_device(&self, name: &str) -> Option<BlockDevice> {
        let metadata = self.metadata.lock().unwrap();
        metadata.get_device(name).cloned()
    }
}

// Add serde support for metadata persistence
use serde::{Deserialize, Serialize};

impl Serialize for BlockDeviceMetadata {
    fn serialize<S>(&self, serializer: S) -> std::result::Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        self.devices.serialize(serializer)
    }
}

impl<'de> Deserialize<'de> for BlockDeviceMetadata {
    fn deserialize<D>(deserializer: D) -> std::result::Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        let devices = HashMap::deserialize(deserializer)?;
        Ok(BlockDeviceMetadata { devices })
    }
}

impl Serialize for BlockDevice {
    fn serialize<S>(&self, serializer: S) -> std::result::Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        use serde::ser::SerializeStruct;
        let mut state = serializer.serialize_struct("BlockDevice", 6)?;
        state.serialize_field("name", &self.name)?;
        state.serialize_field("path", &self.path.to_string_lossy())?;
        state.serialize_field("size_bytes", &self.size_bytes)?;
        state.serialize_field("format", &self.format.to_string())?;
        state.serialize_field("attached_to", &self.attached_to)?;
        state.serialize_field("zone", &self.zone)?;
        state.end()
    }
}

impl<'de> Deserialize<'de> for BlockDevice {
    fn deserialize<D>(deserializer: D) -> std::result::Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        #[derive(Deserialize)]
        struct BlockDeviceHelper {
            name: String,
            path: String,
            size_bytes: u64,
            format: String,
            attached_to: Option<String>,
            zone: String,
        }

        let helper = BlockDeviceHelper::deserialize(deserializer)?;

        let format = match helper.format.as_str() {
            "qcow2" => DiskFormat::Qcow2,
            "raw" => DiskFormat::Raw,
            "vhd" => DiskFormat::Vhd,
            _ => DiskFormat::Qcow2,
        };

        Ok(BlockDevice {
            name: helper.name,
            path: PathBuf::from(helper.path),
            size_bytes: helper.size_bytes,
            format,
            attached_to: helper.attached_to,
            zone: helper.zone,
        })
    }
}

// Add required dependencies
use rand;
use uuid;
