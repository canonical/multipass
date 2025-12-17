import argparse
import asyncio
import json
import pathlib
import sys
import logging
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


def write_output_file(output: dict, path: pathlib.Path) -> None:
    """
    Attempt to merge output with existing data at the given path.

    If the file exists, load it and merge with new data. Only distributions
    present in 'output' will be updated; others are preserved.

    If the file does not exist or is invalid, it will be created anew.
    """
    # Load existing data if file exists
    existing_data = {}
    if path.exists():
        try:
            raw_data = json.loads(path.read_text())
            # Validate existing data against schema
            for dist_name, dist_data in raw_data.items():
                try:
                    validated = ScraperResult(**dist_data)
                    existing_data[dist_name] = validated.model_dump()
                except ValidationError as e:
                    logger.warning(
                        "Existing data for '%s' is invalid and will be discarded: %s",
                        dist_name,
                        e,
                    )
            logger.info(
                "Loaded existing data from %s (%d valid distribution(s))",
                path,
                len(existing_data),
            )
        except (json.JSONDecodeError, OSError) as e:
            logger.warning("Could not load existing output file: %s", e)

    # Merge new data into existing
    existing_data.update(output)

    # Write merged output
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w") as f:
        json.dump(existing_data, f, indent=4, sort_keys=True)
        f.write("\n")

    logger.info("Output written to %s", path)


async def run_scraper(scraper_instance: BaseScraper) -> tuple[str, dict | None]:
    """
    Run a single scraper.fetch and capture exceptions.
    """
    name = scraper_instance.name
    result = None

    try:
        result = await scraper_instance.fetch()
    except Exception as e:
        logger.exception("Scraper '%s' failed: %s", name, e)
        return name, None

    # Validate JSON structure
    try:
        validated = ScraperResult(**result)
        logger.info("Scraper '%s' succeeded", name)
        return name, validated.model_dump()
    except ValidationError as e:
        logger.error("Scraper '%s' returned invalid structure:\n%s", name, e)
    except Exception as e:
        logger.exception("Unexpected error validating scraper '%s': %s", name, e)

    return name, None


async def run_all_scrapers(output_file: pathlib.Path) -> None:
    """
    Run all registered scrapers concurrently and write output.
    """
    scrapers = load_scrapers()
    output = {}

    # Run scrapers concurrently using asyncio.gather
    tasks = [run_scraper(s) for s in scrapers]
    completed = await asyncio.gather(*tasks)

    # Populate output
    failed_scrapers = []
    for name, data in completed:
        if data:
            output[name] = data
        else:
            failed_scrapers.append(name)

    # Write final JSON output
    write_output_file(output, output_file)
    if not failed_scrapers:
        logger.info("All scrapers succeeded.")
    else:
        logger.warning("Some scrapers failed: %s", ", ".join(failed_scrapers))

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
