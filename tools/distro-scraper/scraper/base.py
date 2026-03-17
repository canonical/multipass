import logging
import aiohttp
from abc import ABC, abstractmethod


DEFAULT_TIMEOUT = 10


class BaseScraper(ABC):
    """
    Abstract base class for all scrapers.
    """

    def __init__(self):
        """
        Initialize the scraper with a logger based on the module name.
        """
        self.logger = logging.getLogger(self.__class__.__module__)

    async def _fetch_text(
        self, session: aiohttp.ClientSession, url: str, timeout: int = DEFAULT_TIMEOUT
    ) -> str:
        """
        GET a URL and return its text body.
        """
        self.logger.info("Fetching %s", url)
        async with session.get(
            url, timeout=aiohttp.ClientTimeout(total=timeout)
        ) as resp:
            resp.raise_for_status()
            return await resp.text()

    async def _fetch_json(
        self, session: aiohttp.ClientSession, url: str, timeout: int = DEFAULT_TIMEOUT
    ) -> dict | None:
        """
        GET a URL and return JSON-decoded content, or None if the resource is not found (404).
        """
        self.logger.info("Fetching %s", url)
        async with session.get(
            url, timeout=aiohttp.ClientTimeout(total=timeout)
        ) as resp:
            if resp.status == 404:
                self.logger.info("Resource not found (404): %s", url)
                return None
            resp.raise_for_status()
            return await resp.json()

    async def _head_content_length(
        self, session: aiohttp.ClientSession, url: str, timeout: int = DEFAULT_TIMEOUT
    ) -> int | None:
        """
        HEAD the URL and return Content-Length as an int, or None if absent.
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

    @abstractmethod
    async def fetch(self) -> dict:
        """
        Fetch data from the source and return it as a dict.
        """
        pass

    @property
    @abstractmethod
    def name(self) -> str:
        """
        Unique name for the scraper (used in logs or output).
        """
        pass
