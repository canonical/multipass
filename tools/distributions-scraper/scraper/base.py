from abc import ABC, abstractmethod

class BaseScraper(ABC):
    """Abstract base class for all scrapers."""

    @abstractmethod
    def fetch(self):
        """Fetch data from the source and return it as a dict."""
        pass

    @property
    @abstractmethod
    def name(self):
        """Unique name for the scraper (used in logs or output)."""
        pass
