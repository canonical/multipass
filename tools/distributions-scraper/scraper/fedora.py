import requests
from scraper.base import BaseScraper
from dateutil import parser as dateparser


class FedoraScraper(BaseScraper):
    @property
    def name(self):
        return "Fedora"

    def fetch(self):
        url = "https://fedoraproject.org/releases.json"
        response = requests.get(url, timeout=10)
        response.raise_for_status()
        artifacts = response.json()

        # Filter only Cloud Base Generic images
        images = [
            a for a in artifacts
            if a.get("variant") == "Cloud"
               and a.get("subvariant") == "Cloud_Base"
               and "Fedora-Cloud-Base-Generic" in a.get("link", "")
        ]

        if not images:
            raise RuntimeError("No Fedora Cloud Base Generic images found")

        # Find latest version (parse version as int, fall back to string compare)
        def version_key(a):
            v = a.get("version", "0")
            try:
                return tuple(map(int, v.replace("-", ".").split(".")))
            except ValueError:
                return (0,)

        latest_version = max(images, key=version_key)["version"]

        # Keep only latest version
        latest_images = [a for a in images if a["version"] == latest_version]

        items = {}
        for img in latest_images:
            arch = img["arch"]
            if arch == "aarch64":
                label = "arm64"
            elif arch == "x86_64":
                label = "x86_64"
            else:
                continue  # ignore unsupported arches

            # HEAD request to get size
            head_resp = requests.head(img["link"], timeout=10, allow_redirects=True)
            head_resp.raise_for_status()
            last_modified = head_resp.headers.get("Last-Modified")
            if last_modified:
                last_modified = dateparser.parse(last_modified)

            items[label] = {
                "image_location": img["link"],
                "id": img["sha256"],
                "version": last_modified.strftime("%Y%m%d"),
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
