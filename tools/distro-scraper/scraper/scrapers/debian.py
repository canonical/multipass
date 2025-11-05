import base64
import aiohttp
import asyncio
from email.parser import Parser
from ..base import BaseScraper

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
    def _find_qcow2_upload(manifest: dict) -> dict | None:
        """
        Search a manifest for the first Upload entry with qcow2 image-format.

        Returns the matching item dict or None if not found.
        """
        for item in manifest.get("items", []):
            kind = item.get("kind")
            metadata = item.get("metadata", {})
            labels = metadata.get("labels", {})
            if (
                kind == "Upload"
                and labels.get("upload.cloud.debian.org/image-format") == "qcow2"
            ):
                return item
        return None

    async def _fetch_text(
        self, session: aiohttp.ClientSession, url: str, timeout: int = DEFAULT_TIMEOUT
    ) -> str:
        """
        GET a URL and return its text. Raises aiohttp.ClientError on bad response.
        """
        self.logger.info("Fetching Debian releases from %s", url)
        async with session.get(
            url, timeout=aiohttp.ClientTimeout(total=timeout)
        ) as resp:
            resp.raise_for_status()
            return await resp.text()

    async def _fetch_json(
        self, session: aiohttp.ClientSession, url: str, timeout: int = DEFAULT_TIMEOUT
    ) -> dict:
        """
        GET a URL and return JSON-decoded content.
        """
        self.logger.info("Fetching Debian manifest from %s", url)
        async with session.get(
            url, timeout=aiohttp.ClientTimeout(total=timeout)
        ) as resp:
            resp.raise_for_status()
            return await resp.json()

    def _parse_release_file(self, content: str) -> tuple[str, str]:
        """
        Parse RFC822-style Release file and return important fields.

        Returns a tuple with (Version, Codename)
        """
        parser = Parser()
        parsed = parser.parsestr(content)
        version = parsed.get("Version")
        codename = parsed.get("Codename")
        self.logger.info(
            "Parsed Release file: Version=%s, Codename=%s", version, codename
        )
        return version, codename

    async def _head_content_length(
        self, session: aiohttp.ClientSession, url: str, timeout: int = DEFAULT_TIMEOUT
    ) -> int | None:
        """
        HEAD the URL and return Content-Length as int if present, otherwise None.

        Raises aiohttp.ClientError on non-2xx responses.
        """
        self.logger.info("Sending HEAD request to %s", url)
        async with session.head(
            url, allow_redirects=True, timeout=aiohttp.ClientTimeout(total=timeout)
        ) as resp:
            resp.raise_for_status()
            length = resp.headers.get("Content-Length")
            if length is None:
                return None
            try:
                return int(length)
            except (TypeError, ValueError):
                self.logger.warning("Invalid Content-Length header: %s", length)
                return None

    def _decode_sha512_b64_to_hex(self, digest_annotation: str | None) -> str | None:
        """
        Convert a digest annotation like 'sha512:BASE64' to 'sha512:<hex>' or return None.

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

    async def _fetch_items(
        self, session: aiohttp.ClientSession, codename: str, version: str
    ) -> dict[str, dict]:
        """
        Fetch image manifests for known arches and build the items mapping.

        Preserves original mapping and output structure.
        """
        arch_map = {
            "amd64": "x86_64",
            "arm64": "arm64",
        }

        # Fetch all manifests concurrently
        manifest_urls = [
            MANIFEST_URL_TEMPLATE.format(codename=codename, version=version, arch=arch)
            for arch in arch_map.keys()
        ]
        manifests = await asyncio.gather(
            *[self._fetch_json(session, url) for url in manifest_urls]
        )

        items: dict[str, dict] = {}
        for (arch, label), manifest in zip(arch_map.items(), manifests):
            upload_item = self._find_qcow2_upload(manifest)
            if not upload_item:
                self.logger.info("No qcow2 upload found for %s %s", codename, arch)
                continue

            metadata = upload_item.get("metadata", {})
            labels = metadata.get("labels", {})
            annotations = metadata.get("annotations", {})
            data = upload_item.get("data", {})

            image_ref = data.get("ref")
            if not image_ref:
                self.logger.warning(
                    "Upload item missing data.ref for %s %s", codename, arch
                )
                continue

            image_url = IMAGE_BASE_URL + image_ref
            size = await self._head_content_length(session, image_url)
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

    async def fetch(self) -> dict:
        """
        Fetch Debian Cloud images and return normalized metadata.
        """
        async with aiohttp.ClientSession() as session:
            release_text = await self._fetch_text(session, RELEASE_FILE_URL)

            raw_version, codename = self._parse_release_file(release_text)
            if not codename:
                raise RuntimeError(
                    "Could not determine Debian codename from Release file"
                )

            version = raw_version.split(".")[0] if raw_version else None
            items = await self._fetch_items(session, codename, version)

            return {
                "aliases": f"debian, {codename}",
                "os": "Debian",
                "release": codename,
                "release_codename": codename.capitalize(),
                "release_title": version,
                "items": items,
            }
