#! /usr/bin/env python

import os
import shutil
import subprocess
import tempfile
import sys
import logging
import argparse

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)

SPHINX_DIR = os.path.join(os.getcwd(), ".sphinx")

GITHUB_REPO = "canonical/documentation-style-guide"
GITHUB_CLONE_URL = f"https://github.com/{GITHUB_REPO}.git"

# Source paths to copy from repo
VALE_FILE_LIST = [
    "styles/Canonical",
    "styles/config/vocabularies/Canonical",
    "styles/config/dictionaries",
    "vale.ini"
]

def clone_repo_and_copy_paths(file_source_dest, overwrite=False):
    """
    Clone the repository to a temporary directory and copy required files

    Args:
        file_source_dest: dictionary of file paths to copy from the repository,
            and their destination paths
        overwrite: boolean flag to overwrite existing files in the destination

    Returns:
        bool: True if all files were copied successfully, False otherwise
    """

    if not file_source_dest:
        logging.error("No files to copy")
        return False

    # Create temporary directory on disk for cloning
    temp_dir = tempfile.mkdtemp()
    logging.info("Cloning repository <%s> to temporary directory: %s", GITHUB_REPO, temp_dir)
    clone_cmd = ["git", "clone", "--depth", "1", GITHUB_CLONE_URL, temp_dir]

    try:
        result = subprocess.run(
            clone_cmd,
            capture_output=True,
            text=True,
            check=True
        )
        logging.debug("Git clone output: %s", result.stdout)
    except subprocess.CalledProcessError as e:
        logging.error("Git clone failed: %s", e.stderr)
        return False

    # Copy files from the cloned repository to the destination paths
    is_copy_success = True
    for source, dest in file_source_dest.items():
        source_path = os.path.join(temp_dir, source)

        if not os.path.exists(source_path):
            is_copy_success = False
            logging.error("Source path not found: %s", source_path)
            continue

        if not copy_files_to_path(source_path, dest, overwrite):
            is_copy_success = False
            logging.error("Failed to copy %s to %s", source_path, dest)

    # Clean up temporary directory
    logging.info("Cleaning up temporary directory: %s", temp_dir)
    shutil.rmtree(temp_dir)

    return is_copy_success

def copy_files_to_path(source_path, dest_path, overwrite=False):
    """
    Copy a file or directory from source to destination

    Args:
        source_path: Path to the source file or directory
        dest_path: Path to the destination
        overwrite: Boolean flag to overwrite existing files in the destination

    Returns:
        bool: True if copy was successful, False otherwise
    """
    # Skip if source file doesn't exist
    if not os.path.exists(source_path):
        logging.warning("Source path not found: %s", source_path)
        return False

    logging.info("Copying %s to %s", source_path, dest_path)
    # Handle existing files
    if os.path.exists(dest_path):
        if overwrite:
            logging.info("  Destination exists, overwriting: %s", dest_path)
            if os.path.isdir(dest_path):
                shutil.rmtree(dest_path)
            else:
                os.remove(dest_path)
        else:
            logging.info("  Destination exists, skip copying (use overwrite=True to replace): %s",
                         dest_path)
            return True     # Skip copying

    # Copy the source to destination
    try:
        if os.path.isdir(source_path):
            # entire directory
            shutil.copytree(source_path, dest_path)
        else:
            # individual files
            shutil.copy2(source_path, dest_path)
        return True
    except (shutil.Error, OSError) as e:
        logging.error("Copy failed: %s", e)
        return False

def parse_arguments():
    parser = argparse.ArgumentParser(description="Download Vale configuration files")
    parser.add_argument("--no-overwrite", action="store_true", help="Don't overwrite existing files")
    return parser.parse_args()

def main():
    # Define local directory paths
    vale_files_dict = {file: os.path.join(SPHINX_DIR, file) for file in VALE_FILE_LIST}

    # Parse command line arguments, default to overwrite_enabled = True
    overwrite_enabled = not parse_arguments().no_overwrite

    # Download into /tmp through git clone
    if not clone_repo_and_copy_paths(vale_files_dict, overwrite=overwrite_enabled):
        logging.error("Failed to download files from repository")
        return 1

    logging.info("Download complete")
    return 0


if __name__ == "__main__":
    sys.exit(main())  # Keep return code
