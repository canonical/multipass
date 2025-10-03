use std::collections::HashMap;
use std::fmt;
use std::path::PathBuf;

/// Result type for block storage operations
pub type Result<T> = std::result::Result<T, BlockStorageError>;

/// Error types for block storage operations
#[derive(Debug)]
pub enum BlockStorageError {
    IoError(std::io::Error),
    DiskNotFound(String),
    DiskAlreadyExists(String),
    DiskInUse(String),
    InvalidSize(String),
    QemuImgError(String),
    InvalidPath(String),
    AttachmentError(String),
}

impl fmt::Display for BlockStorageError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            BlockStorageError::IoError(e) => write!(f, "IO error: {}", e),
            BlockStorageError::DiskNotFound(name) => write!(f, "Disk '{}' not found", name),
            BlockStorageError::DiskAlreadyExists(name) => {
                write!(f, "Disk '{}' already exists", name)
            }
            BlockStorageError::DiskInUse(name) => write!(f, "Disk '{}' is currently in use", name),
            BlockStorageError::InvalidSize(msg) => write!(f, "Invalid disk size: {}", msg),
            BlockStorageError::QemuImgError(msg) => write!(f, "qemu-img error: {}", msg),
            BlockStorageError::InvalidPath(path) => write!(f, "Invalid path: {}", path),
            BlockStorageError::AttachmentError(msg) => write!(f, "Attachment error: {}", msg),
        }
    }
}

impl std::error::Error for BlockStorageError {}

impl From<std::io::Error> for BlockStorageError {
    fn from(error: std::io::Error) -> Self {
        BlockStorageError::IoError(error)
    }
}

/// Information about a block device
#[derive(Debug, Clone)]
pub struct BlockDeviceInfo {
    pub name: String,
    pub size: String,
    pub path: String,
    pub attached_to: String,
}

/// Represents a block device (disk)
#[derive(Debug, Clone)]
pub struct BlockDevice {
    pub name: String,
    pub path: PathBuf,
    pub size_bytes: u64,
    pub format: DiskFormat,
    pub attached_to: Option<String>,
    pub zone: String,
}

/// Supported disk formats
#[derive(Debug, Clone, PartialEq)]
pub enum DiskFormat {
    Qcow2,
    Raw,
    Vhd,
}

impl fmt::Display for DiskFormat {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            DiskFormat::Qcow2 => write!(f, "qcow2"),
            DiskFormat::Raw => write!(f, "raw"),
            DiskFormat::Vhd => write!(f, "vhd"),
        }
    }
}

/// Metadata for persisting block device information
#[derive(Debug, Clone)]
pub struct BlockDeviceMetadata {
    pub devices: HashMap<String, BlockDevice>,
}

impl BlockDeviceMetadata {
    pub fn new() -> Self {
        BlockDeviceMetadata {
            devices: HashMap::new(),
        }
    }

    pub fn add_device(&mut self, device: BlockDevice) {
        self.devices.insert(device.name.clone(), device);
    }

    pub fn remove_device(&mut self, name: &str) -> Option<BlockDevice> {
        self.devices.remove(name)
    }

    pub fn get_device(&self, name: &str) -> Option<&BlockDevice> {
        self.devices.get(name)
    }

    pub fn get_device_mut(&mut self, name: &str) -> Option<&mut BlockDevice> {
        self.devices.get_mut(name)
    }

    pub fn list_devices(&self) -> Vec<&BlockDevice> {
        self.devices.values().collect()
    }
}

/// Parse size string (e.g., "10G", "500M") to bytes
pub fn parse_size(size_str: &str) -> Result<u64> {
    let size_str = size_str.trim().to_uppercase();

    if size_str.is_empty() {
        return Err(BlockStorageError::InvalidSize(
            "Empty size string".to_string(),
        ));
    }

    let (number_part, unit_part) = if let Some(pos) = size_str.find(|c: char| c.is_alphabetic()) {
        (&size_str[..pos], &size_str[pos..])
    } else {
        (size_str.as_str(), "")
    };

    let number: f64 = number_part
        .parse()
        .map_err(|_| BlockStorageError::InvalidSize(format!("Invalid number: {}", number_part)))?;

    let multiplier = match unit_part {
        "" | "B" => 1,
        "K" | "KB" => 1024,
        "M" | "MB" => 1024 * 1024,
        "G" | "GB" => 1024 * 1024 * 1024,
        "T" | "TB" => 1024_u64.pow(4),
        _ => {
            return Err(BlockStorageError::InvalidSize(format!(
                "Unknown unit: {}",
                unit_part
            )))
        }
    };

    Ok((number * multiplier as f64) as u64)
}

/// Format bytes to human-readable size string
pub fn format_size(bytes: u64) -> String {
    const UNITS: &[&str] = &["B", "KB", "MB", "GB", "TB"];
    let mut size = bytes as f64;
    let mut unit_index = 0;

    while size >= 1024.0 && unit_index < UNITS.len() - 1 {
        size /= 1024.0;
        unit_index += 1;
    }

    if size.fract() == 0.0 {
        format!("{:.0}{}", size, UNITS[unit_index])
    } else {
        format!("{:.1}{}", size, UNITS[unit_index])
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_size() {
        assert_eq!(parse_size("100").unwrap(), 100);
        assert_eq!(parse_size("10K").unwrap(), 10 * 1024);
        assert_eq!(parse_size("5M").unwrap(), 5 * 1024 * 1024);
        assert_eq!(parse_size("2G").unwrap(), 2 * 1024 * 1024 * 1024);
        assert_eq!(
            parse_size("1.5G").unwrap(),
            (1.5 * 1024.0 * 1024.0 * 1024.0) as u64
        );
    }

    #[test]
    fn test_format_size() {
        assert_eq!(format_size(512), "512B");
        assert_eq!(format_size(1024), "1KB");
        assert_eq!(format_size(1536), "1.5KB");
        assert_eq!(format_size(1024 * 1024), "1MB");
        assert_eq!(format_size(5 * 1024 * 1024 * 1024), "5GB");
    }
}
