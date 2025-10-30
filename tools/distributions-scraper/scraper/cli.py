import argparse
import asyncio
import json
import pathlib
import sys
import logging
from concurrent.futures import ThreadPoolExecutor
from importlib.metadata import entry_points
from pydantic import ValidationError
from scraper.base import BaseScraper
from scraper.models import ScraperResult


logger = logging.getLogger(__name__)


def configure_logging() -> None:
    """
    Configure root logger for CLI usage.
    """
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
        stream=sys.stdout,
    )


def load_scrapers() -> list[BaseScraper]:
    """
    Load all scraper classes from registered entry points.
    """
    scrapers = []

    eps = entry_points(group="dist_scraper.scrapers")
    for ep in eps:
        logger.info("Loading scraper plugin: %s", ep.name)
        scraper_class = ep.load()
        if isinstance(scraper_class, type) and issubclass(scraper_class, BaseScraper):
            scrapers.append(scraper_class())
        else:
            logger.warning(
                "Entry point %s did not provide a valid scraper class", ep.name
            )

    return scrapers


def write_output_file(output, path: pathlib.Path) -> None:
    """
    Write JSON output to the given path, creating parent directories as needed.
    """
    path.parent.mkdir(parents=True, exist_ok=True)
    json_str = json.dumps(output, indent=2, sort_keys=True) + "\n"
    path.write_text(json_str)
    logger.info("Output written to %s", path)


async def run_scraper(
    scraper_instance: BaseScraper, executor: ThreadPoolExecutor
) -> tuple[str, dict | None]:
    """
    Run a single scraper.fetch in the provided executor and capture exceptions.
    """
    loop = asyncio.get_event_loop()
    name = scraper_instance.name
    result = None

    try:
        result = await loop.run_in_executor(executor, scraper_instance.fetch)
    except Exception as e:
        logger.exception("Scraper '%s' failed: %s", name, e)

    # Validate JSON structure
    try:
        validated = ScraperResult(**result)
        logger.info("Scraper '%s' succeeded", name)
        return name, validated.model_dump()
    except ValidationError as e:
        logger.error("Scraper '%s' returned invalid structure:\n%s", name, e)

    return name, None


async def run_all_scrapers(output_file: pathlib.Path) -> None:
    """
    Run all registered scrapers concurrently and write output.
    """
    scrapers = load_scrapers()
    output = {}

    # Run scrapers concurrently using a thread pool
    with ThreadPoolExecutor(max_workers=len(scrapers)) as executor:
        tasks = [run_scraper(s, executor) for s in scrapers]
        completed = await asyncio.gather(*tasks)

    # Populate output
    for name, data in completed:
        if data:
            output[name] = data

    # Write final JSON output
    write_output_file(output, output_file)
    logger.info("All scrapers succeeded.")


def main():
    parser = argparse.ArgumentParser(
        description="Scrape distribution information from various Linux distributions"
    )
    parser.add_argument(
        "output_file", type=pathlib.Path, help="Path to the output JSON file"
    )
    args = parser.parse_args()

    configure_logging()

    try:
        asyncio.run(run_all_scrapers(args.output_file))
    except KeyboardInterrupt:
        logger.info("Interrupted by user")
        exit(130)
