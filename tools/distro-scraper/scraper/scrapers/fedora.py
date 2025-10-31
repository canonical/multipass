import aiohttp
import asyncio
import datetime
from dateutil import parser
from ..base import BaseScraper


RELEASES_URL = "https://fedoraproject.org/releases.json"
DEFAULT_TIMEOUT = 10


class FedoraScraper(BaseScraper):
    def __init__(self):
        super().__init__()

    @property
    def name(self) -> str:
        return "Fedora"

    @staticmethod
    def _map_arch_label(arch: str) -> str | None:
        """
        Map Fedora architecture names to the labels used in our items output.

        Returns None for unsupported arches.
        """
        if arch == "aarch64":
            return "arm64"
        if arch == "x86_64":
            return "x86_64"
        return None

    def _find_latest_version(self, images: list[dict]) -> str:
        """
        Find the latest version string among a list of images.
        """
        if not images:
            raise RuntimeError("No images to determine latest version")

        latest = max(images, key=lambda a: self._parse_version(a.get("version", "0")))
        version = latest.get("version", "")
        self.logger.info("Determined latest version: %s", version)
        return version

    def _parse_version(self, version: str) -> int:
        """
        Parse version string to an integer.
        """
        try:
            return int(version)
        except ValueError:
            self.logger.info("Failed to parse version '%s' as int", version)
            return -1

    @staticmethod
    def _is_cloud_base_generic(artifact: dict) -> bool:
        """
        Return True if the artifact matches the Fedora Cloud Base Generic pattern.
        """
        return (
            artifact.get("variant") == "Cloud"
            and artifact.get("subvariant") == "Cloud_Base"
            and "Fedora-Cloud-Base-Generic" in artifact.get("link", "")
        )

    async def _fetch_artifacts(
        self,
        session: aiohttp.ClientSession,
        url: str = RELEASES_URL,
        timeout: int = DEFAULT_TIMEOUT,
    ) -> list[dict]:
        """
        Fetch the releases JSON and return the parsed artifacts list.
        """
        self.logger.info("Fetching Fedora releases from %s", url)
        async with session.get(
            url, timeout=aiohttp.ClientTimeout(total=timeout)
        ) as resp:
            resp.raise_for_status()
            return await resp.json()

    async def _get_last_modified_for_url(
        self, session: aiohttp.ClientSession, url: str, timeout: int = DEFAULT_TIMEOUT
    ) -> datetime.datetime | None:
        """
        HEAD the URL and return a parsed Last-Modified header if present.
        """
        self.logger.info("Sending HEAD request to %s", url)
        async with session.head(
            url, timeout=aiohttp.ClientTimeout(total=timeout), allow_redirects=True
        ) as resp:
            resp.raise_for_status()
            last_mod = resp.headers.get("Last-Modified")
            if last_mod:
                try:
                    return parser.parse(last_mod)
                except (ValueError, TypeError) as exc:
                    self.logger.debug(
                        "Failed to parse Last-Modified header '%s': %s", last_mod, exc
                    )
            return None

    async def fetch(self) -> dict:
        """
        Fetch Fedora Cloud Base Generic images and return normalized metadata.
        """
        async with aiohttp.ClientSession() as session:
            artifacts = await self._fetch_artifacts(session)

            images = [a for a in artifacts if self._is_cloud_base_generic(a)]
            if not images:
                raise RuntimeError("No Fedora Cloud Base Generic images found")

            latest_version = self._find_latest_version(images)
            latest_images = [a for a in images if a.get("version") == latest_version]
            if not latest_images:
                raise RuntimeError(
                    f"No images found for latest version {latest_version}"
                )

            # Filter out unsupported architectures first
            supported_images = []
            for img in latest_images:
                arch = img.get("arch")
                label = self._map_arch_label(arch)
                if label:
                    supported_images.append((img, label))
                else:
                    self.logger.info("Skipping unsupported architecture: %s", arch)

            # Fetch all last-modified dates asynchronously
            last_modified_tasks = [
                (
                    self._get_last_modified_for_url(session, img.get("link"))
                    if img.get("link")
                    else None
                )
                for img, _ in supported_images
            ]
            last_modified_results = await asyncio.gather(*last_modified_tasks)

            # Build items dictionary
            items: dict[str, dict] = {}
            for (img, label), last_modified in zip(
                supported_images, last_modified_results
            ):
                link = img.get("link")
                items[label] = {
                    "image_location": link,
                    "id": img.get("sha256"),
                    # match previous behaviour: use parsed Last-Modified date formatted as YYYYMMDD
                    "version": (
                        last_modified.strftime("%Y%m%d") if last_modified else ""
                    ),
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
