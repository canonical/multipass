import logging
from abc import ABC, abstractmethod


class BaseScraper(ABC):
    """
    Abstract base class for all scrapers.
    """

    def __init__(self):
        """
        Initialize the scraper with a logger based on the module name.
        """
        self.logger = logging.getLogger(self.__class__.__module__)

    @abstractmethod
    def fetch(self) -> dict:
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
