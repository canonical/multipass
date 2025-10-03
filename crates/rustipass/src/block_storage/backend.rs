use crate::block_storage::types::{DiskFormat, Result};
use std::path::Path;

/// Trait for block device backend implementations
pub trait BlockDeviceBackend: Send + Sync {
    /// Create a new block device with the specified size
    fn create_disk(&self, path: &Path, size_bytes: u64, format: DiskFormat) -> Result<()>;

    /// Delete a block device
    fn delete_disk(&self, path: &Path) -> Result<()>;

    /// Get disk information
    fn get_disk_info(&self, path: &Path) -> Result<(u64, DiskFormat)>;

    /// Check if a disk exists
    fn disk_exists(&self, path: &Path) -> bool;

    /// Clone/copy a disk to a new location
    fn clone_disk(&self, source: &Path, destination: &Path) -> Result<()>;

    /// Resize a disk (cannot shrink)
    fn resize_disk(&self, path: &Path, new_size_bytes: u64) -> Result<()>;
}
