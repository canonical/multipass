import base64
import requests
from email.parser import Parser
from scraper.base import BaseScraper


def _fetch_items(codename, version):
    arch_map = {
        "amd64": "x86_64",
        "arm64": "arm64",
    }

    items = {}
    for arch, label in arch_map.items():
        url = f"https://cloud.debian.org/images/cloud/{codename}/latest/debian-{version}-generic-{arch}.json"
        response = requests.get(url, timeout=10)
        response.raise_for_status()
        manifest = response.json()

        # Find qcow2 upload entry
        for item in manifest.get("items"):
            kind = item.get("kind")
            labels = item.get("metadata").get("labels")
            annotations = item.get("metadata").get("annotations")
            data = item.get("data")

            if kind == "Upload" and labels.get("upload.cloud.debian.org/image-format") == "qcow2":
                image_ref = data.get("ref")
                if image_ref:
                    # Construct the full image URL
                    image_url = f"https://cloud.debian.org/images/cloud/{image_ref}"

                    # HEAD request to get size
                    head_resp = requests.head(image_url, timeout=10, allow_redirects=True)
                    head_resp.raise_for_status()
                    size = head_resp.headers.get("Content-Length")

                    # Convert base64 to hex
                    sha512_b64 = annotations.get("cloud.debian.org/digest")
                    sha512_b64 = sha512_b64[len("sha512:"):]

                    # Add missing padding if necessary
                    missing_padding = len(sha512_b64) % 4
                    if missing_padding:
                        sha512_b64 += "=" * (4 - missing_padding)

                    sha512_hex = f"sha512:{base64.b64decode(sha512_b64).hex()}"

                    items[label] = {
                        "image_location": image_url,
                        "id": sha512_hex,
                        "version": labels.get("cloud.debian.org/version").split("-")[0],
                        "size": int(size) if size is not None else None,
                    }
                break  # stop after finding qcow2

    return items


class DebianScraper(BaseScraper):
    @property
    def name(self):
        return "Debian"

    def fetch(self):
        url = "https://deb.debian.org/debian/dists/stable/Release"
        response = requests.get(url, timeout=10)
        response.raise_for_status()

        # Parse RFC822-style metadata
        content = response.text
        parser = Parser()
        parsed = parser.parsestr(content)

        raw_version = parsed.get("Version")
        release = parsed.get("Codename")
        version = raw_version.split(".")[0] if raw_version else None

        items = _fetch_items(release, version)

        return {
            "aliases": f"debian, {release}",
            "os": "Debian",
            "release": release,
            "release_codename": release.capitalize(),
            "release_title": version,
            "items": items
        }
