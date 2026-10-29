#!/usr/bin/env python3
# coding: utf-8

"""Request SBOMs for Multipass release installers (.pkg / .msi) via the
Canonical SBOM REST API (https://sbom-request.canonical.com/docs).

Based on canonical/kernos/tools/gen-sbom.py.
"""

import argparse
import logging
import time
from pathlib import Path
from typing import Any

import requests

API_BASE_URL = "https://sbom-request.canonical.com/api/v1"
CHUNK_SIZE_BYTES = 5 * 1024 * 1024  # 5 MB
POLLING_INTERVAL_SECONDS = 10
POLLING_TIMEOUT_SECONDS = 3600  # 60 minutes
DEFAULT_REQUEST_TIMEOUT = 30

# Map file extension -> (API type path, artifactFormat, variant value)
EXTENSION_TO_TYPE: dict[str, tuple[str, str, str]] = {
    ".pkg": ("macos", "pkg", "installer_(pkg)"),
    ".msi": ("windows", "msi", "installer"),
}


def artifact_type_for(path: Path) -> tuple[str, str, str]:
    """Return (api_type, artifact_format, variant) for the given installer extension."""
    result = EXTENSION_TO_TYPE.get(path.suffix.lower())
    if not result:
        raise ValueError(f"Unsupported installer extension: {path.suffix!r}")
    return result


def register_artifact(
    file_path: Path,
    api_type: str,
    artifact_format: str,
    variant: str,
    email: str,
    version: str,
    architecture: str,
    release: str,
) -> str:
    """Register the artifact with the SBOM service and return its artifactId."""

    logging.info(f"Registering '{api_type}' artifact: '{file_path.name}'")

    payload: dict[str, Any] = {
        "maintainer": "Canonical",
        "email": email,
        "version": version,
        "department": {"value": "devex", "type": "predefined"},
        "team": {"value": "multipass", "type": "predefined"},
        "artifactName": file_path.stem,
        "filename": file_path.name,
        "artifactFormat": artifact_format,
        "variant": {"value": variant, "type": "predefined"},
        "architecture": {"value": architecture, "type": "predefined"},
        "release": {"value": release, "type": "predefined"},
    }

    url = f"{API_BASE_URL}/artifacts/{api_type}/upload"
    resp = requests.post(url, json=payload, timeout=DEFAULT_REQUEST_TIMEOUT)
    if not resp.ok:
        logging.error(f"Registration failed ({resp.status_code}): {resp.text}")
    resp.raise_for_status()
    artifact_id = resp.json().get("data", {}).get("artifactId")
    if not artifact_id:
        raise ValueError("'artifactId' not found in registration response.")
    logging.info(f"  -> artifactId: {artifact_id}")
    return artifact_id


def upload_artifact_chunks(artifact_id: str, file_path: Path) -> None:
    """Upload the installer file in resumable chunks."""
    if not file_path.exists():
        raise FileNotFoundError(f"File not found: {file_path}")

    file_size = file_path.stat().st_size
    total_chunks = (file_size + CHUNK_SIZE_BYTES - 1) // CHUNK_SIZE_BYTES
    upload_url = f"{API_BASE_URL}/artifacts/upload/chunk/{artifact_id}"

    logging.info(
        f"Uploading {file_path.name} ({file_size} bytes, {total_chunks} chunks)..."
    )
    with file_path.open("rb") as f:
        for i in range(1, total_chunks + 1):
            chunk_data = f.read(CHUNK_SIZE_BYTES)
            form_data = {
                "resumableChunkNumber": str(i),
                "resumableChunkSize": str(CHUNK_SIZE_BYTES),
                "resumableCurrentChunkSize": str(len(chunk_data)),
                "resumableTotalSize": str(file_size),
                "resumableType": "application/octet-stream",
                "resumableIdentifier": f"{file_size}-{file_path.name}",
                "resumableFilename": file_path.name,
                "resumableRelativePath": file_path.name,
                "resumableTotalChunks": str(total_chunks),
            }
            files = {"file": (file_path.name, chunk_data)}
            resp = requests.post(upload_url, data=form_data, files=files, timeout=60)
            resp.raise_for_status()
            logging.info(f"  Chunk {i}/{total_chunks} uploaded.")


def finalize_upload(artifact_id: str) -> None:
    """Complete the chunked upload to trigger SBOM generation."""
    url = f"{API_BASE_URL}/artifacts/upload/complete/{artifact_id}"
    requests.post(url, timeout=DEFAULT_REQUEST_TIMEOUT).raise_for_status()
    logging.info("Upload finalised; SBOM generation started.")


def poll_for_sbom_status(artifact_id: str, label: str) -> None:
    """Poll until SBOM generation completes or the timeout is reached."""
    status_url = f"{API_BASE_URL}/artifacts/status/{artifact_id}"
    start = time.monotonic()

    while time.monotonic() - start < POLLING_TIMEOUT_SECONDS:
        resp = requests.get(status_url, timeout=DEFAULT_REQUEST_TIMEOUT)
        resp.raise_for_status()
        data = resp.json().get("data", {})
        status = data.get("status", "unknown")
        elapsed = time.monotonic() - start

        logging.info(f"  [{label}] status: {status} (elapsed: {elapsed:.0f}s)")
        if status == "completed":
            return
        if status == "failed":
            raise RuntimeError(f"SBOM generation failed for {label}: {data}")
        time.sleep(POLLING_INTERVAL_SECONDS)

    raise TimeoutError(
        f"Timed out after {POLLING_TIMEOUT_SECONDS}s waiting for SBOM: {label}"
    )


def download_sbom(artifact_id: str, output_path: Path) -> None:
    """Download the completed SBOM JSON to output_path."""
    url = f"{API_BASE_URL}/artifacts/sbom/{artifact_id}"
    resp = requests.get(url, headers={"Accept": "application/json"}, timeout=60)
    resp.raise_for_status()

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(resp.text)
    logging.info(f"SBOM saved: {output_path}")


def main() -> None:
    logging.basicConfig(level=logging.INFO, format="%(levelname)s: %(message)s")

    parser = argparse.ArgumentParser(
        description="Request SBOMs for Multipass release installers (.pkg / .msi)"
    )
    parser.add_argument("--email", required=True, help="Notification email address")
    parser.add_argument(
        "--version", required=True, help="Release version string (e.g. 1.16.3)"
    )
    parser.add_argument(
        "--arch", required=True, help="Architecture (e.g. amd64, arm64, universal)"
    )
    parser.add_argument("--release", required=True, help="Current release cycle")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("sboms-installers"),
        help="Directory to write SBOM files into (default: sboms-installers/)",
    )
    parser.add_argument(
        "file",
        type=Path,
        help="Installer file to process (.pkg or .msi)",
    )
    args = parser.parse_args()

    api_type, artifact_format, variant = artifact_type_for(args.file)
    artifact_id = register_artifact(
        args.file,
        api_type,
        artifact_format,
        variant,
        args.email,
        args.version,
        args.arch,
        args.release,
    )
    upload_artifact_chunks(artifact_id, args.file)
    finalize_upload(artifact_id)
    poll_for_sbom_status(artifact_id, args.file.name)
    output_file = args.output_dir / f"{args.file.stem}.spdx.json"
    download_sbom(artifact_id, output_file)
    logging.info("SBOM generated successfully.")


if __name__ == "__main__":
    main()
