import base64
import requests
from typing import Dict, Optional
from email.parser import Parser
from scraper.base import BaseScraper

RELEASE_FILE_URL = "https://deb.debian.org/debian/dists/stable/Release"
MANIFEST_URL_TEMPLATE = "https://cloud.debian.org/images/cloud/{codename}/latest/debian-{version}-generic-{arch}.json"
IMAGE_BASE_URL = "https://cloud.debian.org/images/cloud/"
DEFAULT_TIMEOUT = 10


class DebianScraper(BaseScraper):
    def __init__(self):
        super().__init__()

    @property
    def name(self) -> str:
        return "Debian"

    @staticmethod
    def _find_qcow2_upload(manifest: Dict):
        """Search a manifest for the first Upload entry with qcow2 image-format.

        Returns the matching item dict or None if not found.
        """
        for item in manifest.get("items") or []:
            kind = item.get("kind")
            metadata = item.get("metadata") or {}
            labels = metadata.get("labels") or {}
            if (
                kind == "Upload"
                and labels.get("upload.cloud.debian.org/image-format") == "qcow2"
            ):
                return item
        return None

    def _fetch_text(self, url: str, timeout: int = DEFAULT_TIMEOUT):
        """GET a URL and return its text. Raises requests.HTTPError on bad response."""
        self.logger.info("Fetching Debian releases from %s", url)
        resp = requests.get(url, timeout=timeout)
        resp.raise_for_status()
        return resp.text

    def _fetch_json(self, url: str, timeout: int = DEFAULT_TIMEOUT):
        """GET a URL and return JSON-decoded content."""
        self.logger.info("Fetching Debian manifest from %s", url)
        resp = requests.get(url, timeout=timeout)
        resp.raise_for_status()
        return resp.json()

    def _parse_release_file(self, content: str):
        """Parse RFC822-style Release file and return important fields.

        Returns a dict with keys: Version, Codename
        """
        parser = Parser()
        parsed = parser.parsestr(content)
        version = parsed.get("Version")
        codename = parsed.get("Codename")
        self.logger.info(
            "Parsed Release file: Version=%s, Codename=%s", version, codename
        )
        return {"Version": version, "Codename": codename}

    def _head_content_length(self, url: str, timeout: int = DEFAULT_TIMEOUT):
        """HEAD the URL and return Content-Length as int if present, otherwise None.

        Raises requests.HTTPError on non-2xx responses.
        """
        self.logger.info("Sending HEAD request to %s", url)
        resp = requests.head(url, allow_redirects=True, timeout=timeout)
        resp.raise_for_status()
        length = resp.headers.get("Content-Length")
        if length is None:
            return None
        try:
            return int(length)
        except (TypeError, ValueError):
            self.logger.warning("Invalid Content-Length header: %s", length)
            return None

    def _decode_sha512_b64_to_hex(self, digest_annotation: Optional[str]):
        """Convert a digest annotation like 'sha512:BASE64' to 'sha512:<hex>' or return None.

        Handles missing padding in base64 and logs errors instead of crashing.
        """
        if not digest_annotation:
            return None

        prefix = "sha512:"
        if not digest_annotation.startswith(prefix):
            self.logger.info("Unexpected digest format: %s", digest_annotation)
            return None

        b64 = digest_annotation[len(prefix) :]
        # Add missing padding if necessary
        missing_padding = len(b64) % 4
        if missing_padding:
            b64 += "=" * (4 - missing_padding)

        try:
            decoded = base64.b64decode(b64)
            return f"{prefix}{decoded.hex()}"
        except TypeError as exc:
            self.logger.warning(
                "Failed to decode base64 digest: %s (%s)", digest_annotation, exc
            )
            return None

    def _fetch_items(self, codename: str, version: str):
        """Fetch image manifests for known arches and build the items mapping.

        Preserves original mapping and output structure.
        """
        arch_map = {
            "amd64": "x86_64",
            "arm64": "arm64",
        }

        items: Dict[str, Dict] = {}
        for arch, label in arch_map.items():
            manifest_url = MANIFEST_URL_TEMPLATE.format(
                codename=codename, version=version, arch=arch
            )
            manifest = self._fetch_json(manifest_url)

            upload_item = self._find_qcow2_upload(manifest)
            if not upload_item:
                self.logger.info("No qcow2 upload found for %s %s", codename, arch)
                continue

            metadata = upload_item.get("metadata") or {}
            labels = metadata.get("labels") or {}
            annotations = metadata.get("annotations") or {}
            data = upload_item.get("data") or {}

            image_ref = data.get("ref")
            if not image_ref:
                self.logger.warning(
                    "Upload item missing data.ref for %s %s", codename, arch
                )
                continue

            image_url = IMAGE_BASE_URL + image_ref
            size = self._head_content_length(image_url)
            sha512_hex = self._decode_sha512_b64_to_hex(
                annotations.get("cloud.debian.org/digest")
            )

            # Take the version label as in the original implementation (split on '-')
            raw_version_label = labels.get("cloud.debian.org/version")
            short_version = None
            if raw_version_label:
                short_version = raw_version_label.split("-")[0]

            items[label] = {
                "image_location": image_url,
                "id": sha512_hex,
                "version": short_version,
                "size": size,
            }

        return items

    def fetch(self) -> Dict:
        release_text = self._fetch_text(RELEASE_FILE_URL)
        parsed = self._parse_release_file(release_text)

        raw_version = parsed.get("Version")
        codename = parsed.get("Codename")
        if not codename:
            raise RuntimeError("Could not determine Debian codename from Release file")

        version = raw_version.split(".")[0] if raw_version else None
        items = self._fetch_items(codename, version)

        return {
            "aliases": f"debian, {codename}",
            "os": "Debian",
            "release": codename,
            "release_codename": codename.capitalize(),
            "release_title": version,
            "items": items,
        }
