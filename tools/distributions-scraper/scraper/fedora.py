import requests
from typing import Dict, List
from dateutil import parser as dateparser
from scraper.base import BaseScraper


RELEASES_URL = "https://fedoraproject.org/releases.json"
DEFAULT_TIMEOUT = 10


class FedoraScraper(BaseScraper):
    def __init__(self):
        super().__init__()

    @property
    def name(self) -> str:
        return "Fedora"

    @staticmethod
    def _map_arch_label(arch: str):
        """Map Fedora architecture names to the labels used in our items output.

        Returns None for unsupported arches.
        """
        if arch == "aarch64":
            return "arm64"
        if arch == "x86_64":
            return "x86_64"
        return None

    def _find_latest_version(self, images: List[Dict]):
        """Find the latest version string among a list of images."""
        if not images:
            raise RuntimeError("No images to determine latest version")

        latest = max(images, key=lambda a: self._parse_version(a.get("version", "0")))
        version = latest.get("version", "")
        self.logger.info("Determined latest version: %s", version)
        return version

    def _parse_version(self, version: str):
        try:
            return int(version)
        except ValueError:
            self.logger.info("Failed to parse version '%s' as int", version)
            return -1

    @staticmethod
    def _is_cloud_base_generic(artifact: Dict):
        """Return True if the artifact matches the Fedora Cloud Base Generic pattern."""
        return (
            artifact.get("variant") == "Cloud"
            and artifact.get("subvariant") == "Cloud_Base"
            and "Fedora-Cloud-Base-Generic" in artifact.get("link", "")
        )

    def _fetch_artifacts(self, url: str = RELEASES_URL, timeout: int = DEFAULT_TIMEOUT):
        """Fetch the releases JSON and return the parsed artifacts list."""
        self.logger.info("Fetching Fedora releases from %s", url)
        resp = requests.get(url, timeout=timeout)
        resp.raise_for_status()
        return resp.json()

    def _get_last_modified_for_url(self, url: str, timeout: int = DEFAULT_TIMEOUT):
        """HEAD the URL and return a parsed Last-Modified header if present."""
        self.logger.info("Sending HEAD request to %s", url)
        resp = requests.head(url, timeout=timeout, allow_redirects=True)
        resp.raise_for_status()
        last_mod = resp.headers.get("Last-Modified")
        if last_mod:
            try:
                return dateparser.parse(last_mod)
            except (ValueError, TypeError) as exc:
                self.logger.debug(
                    "Failed to parse Last-Modified header '%s': %s", last_mod, exc
                )
        return None

    def fetch(self):
        """Main entrypoint: fetch Fedora Cloud Base Generic images and return normalized metadata."""
        artifacts = self._fetch_artifacts()

        images = [a for a in artifacts if self._is_cloud_base_generic(a)]
        if not images:
            raise RuntimeError("No Fedora Cloud Base Generic images found")

        latest_version = self._find_latest_version(images)
        latest_images = [a for a in images if a.get("version") == latest_version]
        if not latest_images:
            raise RuntimeError(f"No images found for latest version {latest_version}")

        items: Dict[str, Dict] = {}
        for img in latest_images:
            arch = img.get("arch")
            label = self._map_arch_label(arch)
            if not label:
                self.logger.info("Skipping unsupported architecture: %s", arch)
                continue

            last_modified = None
            link = img.get("link")
            if link:
                last_modified = self._get_last_modified_for_url(link)

            # Compose item entry
            items[label] = {
                "image_location": link,
                "id": img.get("sha256"),
                # match previous behaviour: use parsed Last-Modified date formatted as YYYYMMDD
                "version": last_modified.strftime("%Y%m%d") if last_modified else "",
                "size": int(img.get("size", 0)),
            }

        return {
            "aliases": "fedora",
            "os": "Fedora",
            "release": "",
            "release_codename": latest_version,
            "release_title": latest_version,
            "items": items,
        }
