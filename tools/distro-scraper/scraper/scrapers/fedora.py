import aiohttp
import asyncio
import json
from dateutil import parser
from ..base import BaseScraper, DEFAULT_TIMEOUT, make_session
from ..models import SUPPORTED_ARCHITECTURES


RELEASES_URL = "https://fedoraproject.org/releases.json"

ARCH_MAP = {
    "arm64": "aarch64",
    "power64le": "ppc64le",
}


class FedoraScraper(BaseScraper):
    def __init__(self):
        super().__init__()

    @property
    def name(self) -> str:
        return "Fedora"

    def _latest_version(self, releases: list[dict]) -> str:
        """
        Return the highest numeric version.
        """
        versions = [r["version"] for r in releases if r["version"].isdigit()]
        if not versions:
            raise RuntimeError("No numeric release versions found")
        latest = str(max(int(v) for v in versions))
        self.logger.info("Latest Fedora version: %s", latest)
        return latest

    async def _fetch_image_for_arch(
        self,
        session: aiohttp.ClientSession,
        releases: list[dict],
        version: str,
        label: str,
    ) -> tuple[str, dict]:
        """
        Locate and return image metadata for a single architecture.
        """
        fedora_arch = ARCH_MAP.get(label, label)

        images = [
            r
            for r in releases
            if r["version"] == version
            and r["arch"] == fedora_arch
            and r["subvariant"] == "Cloud_Base"
            and r["link"].endswith(".qcow2")
        ]
        if not images:
            raise RuntimeError(
                f"No Generic qcow2 found for arch={fedora_arch} version={version}"
            )
        image = images[0]
        qcow2_url = image["link"]

        self.logger.info("Sending HEAD request to %s", qcow2_url)
        async with session.head(
            qcow2_url,
            timeout=aiohttp.ClientTimeout(total=DEFAULT_TIMEOUT),
            allow_redirects=True,
        ) as resp:
            resp.raise_for_status()
            last_mod_str = resp.headers.get("Last-Modified")

        last_mod = None
        if last_mod_str:
            try:
                last_mod = parser.parse(last_mod_str)
            except (ValueError, TypeError) as exc:
                self.logger.debug(
                    "Failed to parse Last-Modified '%s': %s", last_mod_str, exc
                )

        return label, {
            "image_location": qcow2_url,
            "id": image["sha256"],
            "version": last_mod.strftime("%Y%m%d") if last_mod else "",
            "size": int(image["size"]),
        }

    async def fetch(self) -> dict:
        """
        Fetch Fedora Cloud Base Generic images for all supported architectures.
        """
        async with make_session() as session:
            releases = json.loads(await self._fetch_text(session, RELEASES_URL))
            version = self._latest_version(releases)

            results = await asyncio.gather(
                *[
                    self._fetch_image_for_arch(session, releases, version, label)
                    for label in SUPPORTED_ARCHITECTURES
                ],
                return_exceptions=True,
            )

            items: dict[str, dict] = {}
            for label, result in zip(SUPPORTED_ARCHITECTURES, results):
                if isinstance(result, Exception):
                    self.logger.error("Failed to fetch arch %s: %s", label, result)
                else:
                    _, data = result
                    items[label] = data

            if not items:
                raise RuntimeError("Failed to fetch images for all architectures")

            return {
                "aliases": "fedora",
                "os": "Fedora",
                "release": "",
                "release_codename": version,
                "release_title": version,
                "items": items,
            }
