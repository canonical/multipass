use crate::block_storage::backend::BlockDeviceBackend;
use crate::block_storage::types::{BlockStorageError, DiskFormat, Result};
use std::path::Path;
use std::process::Command;

/// QEMU backend implementation for block devices
pub struct QemuBackend;

impl QemuBackend {
    pub fn new() -> Self {
        QemuBackend
    }

    /// Execute qemu-img command and return output
    fn execute_qemu_img(&self, args: &[&str]) -> Result<String> {
        let output = Command::new("qemu-img").args(args).output().map_err(|e| {
            BlockStorageError::QemuImgError(format!("Failed to execute qemu-img: {}", e))
        })?;

        if !output.status.success() {
            let stderr = String::from_utf8_lossy(&output.stderr);
            return Err(BlockStorageError::QemuImgError(format!(
                "qemu-img failed: {}",
                stderr
            )));
        }

        Ok(String::from_utf8_lossy(&output.stdout).to_string())
    }

    /// Parse qemu-img info output to get disk size and format
    fn parse_disk_info(&self, info_output: &str) -> Result<(u64, DiskFormat)> {
        let mut size_bytes = 0u64;
        let mut format = DiskFormat::Qcow2;

        for line in info_output.lines() {
            if line.starts_with("virtual size:") {
                // Parse line like: "virtual size: 10 GiB (10737418240 bytes)"
                if let Some(bytes_start) = line.find('(') {
                    if let Some(bytes_end) = line.find(" bytes") {
                        let bytes_str = &line[bytes_start + 1..bytes_end];
                        size_bytes = bytes_str.parse().map_err(|_| {
                            BlockStorageError::QemuImgError(format!(
                                "Failed to parse disk size from: {}",
                                line
                            ))
                        })?;
                    }
                }
            } else if line.starts_with("file format:") {
                let format_str = line.split(':').nth(1).unwrap_or("").trim();
                format = match format_str {
                    "qcow2" => DiskFormat::Qcow2,
                    "raw" => DiskFormat::Raw,
                    "vhd" | "vpc" => DiskFormat::Vhd,
                    _ => DiskFormat::Qcow2,
                };
            }
        }

        if size_bytes == 0 {
            return Err(BlockStorageError::QemuImgError(
                "Failed to parse disk size from qemu-img info".to_string(),
            ));
        }

        Ok((size_bytes, format))
    }
}

impl BlockDeviceBackend for QemuBackend {
    fn create_disk(&self, path: &Path, size_bytes: u64, format: DiskFormat) -> Result<()> {
        let path_str = path
            .to_str()
            .ok_or_else(|| BlockStorageError::InvalidPath(format!("{:?}", path)))?;

        let format_str = format.to_string();
        let size_str = size_bytes.to_string();

        self.execute_qemu_img(&["create", "-f", &format_str, path_str, &size_str])?;

        Ok(())
    }

    fn delete_disk(&self, path: &Path) -> Result<()> {
        std::fs::remove_file(path).map_err(|e| BlockStorageError::IoError(e))
    }

    fn get_disk_info(&self, path: &Path) -> Result<(u64, DiskFormat)> {
        let path_str = path
            .to_str()
            .ok_or_else(|| BlockStorageError::InvalidPath(format!("{:?}", path)))?;

        let output = self.execute_qemu_img(&["info", path_str])?;
        self.parse_disk_info(&output)
    }

    fn disk_exists(&self, path: &Path) -> bool {
        path.exists() && path.is_file()
    }

    fn clone_disk(&self, source: &Path, destination: &Path) -> Result<()> {
        let source_str = source
            .to_str()
            .ok_or_else(|| BlockStorageError::InvalidPath(format!("{:?}", source)))?;
        let dest_str = destination
            .to_str()
            .ok_or_else(|| BlockStorageError::InvalidPath(format!("{:?}", destination)))?;

        // Get source format
        let (_, format) = self.get_disk_info(source)?;
        let format_str = format.to_string();

        self.execute_qemu_img(&["convert", "-O", &format_str, source_str, dest_str])?;

        Ok(())
    }

    fn resize_disk(&self, path: &Path, new_size_bytes: u64) -> Result<()> {
        let path_str = path
            .to_str()
            .ok_or_else(|| BlockStorageError::InvalidPath(format!("{:?}", path)))?;

        // Get current size to ensure we're not shrinking
        let (current_size, _) = self.get_disk_info(path)?;
        if new_size_bytes < current_size {
            return Err(BlockStorageError::InvalidSize(format!(
                "Cannot shrink disk from {} to {} bytes",
                current_size, new_size_bytes
            )));
        }

        let size_str = new_size_bytes.to_string();

        self.execute_qemu_img(&["resize", path_str, &size_str])?;

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use tempfile::tempdir;

    #[test]
    fn test_parse_disk_info() {
        let backend = QemuBackend::new();
        let info_output = r#"
image: test.qcow2
file format: qcow2
virtual size: 10 GiB (10737418240 bytes)
disk size: 196 KiB
cluster_size: 65536
"#;

        let (size, format) = backend.parse_disk_info(info_output).unwrap();
        assert_eq!(size, 10737418240);
        assert_eq!(format, DiskFormat::Qcow2);
    }

    #[test]
    #[ignore] // Requires qemu-img to be installed
    fn test_create_and_delete_disk() {
        let backend = QemuBackend::new();
        let dir = tempdir().unwrap();
        let disk_path = dir.path().join("test.qcow2");

        // Create disk
        backend
            .create_disk(&disk_path, 1024 * 1024 * 100, DiskFormat::Qcow2)
            .unwrap();
        assert!(disk_path.exists());

        // Get info
        let (size, format) = backend.get_disk_info(&disk_path).unwrap();
        assert_eq!(size, 1024 * 1024 * 100);
        assert_eq!(format, DiskFormat::Qcow2);

        // Delete disk
        backend.delete_disk(&disk_path).unwrap();
        assert!(!disk_path.exists());
    }
}
