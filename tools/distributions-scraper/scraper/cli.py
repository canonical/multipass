import argparse
import asyncio
import json
import pathlib
import sys
import logging
from concurrent.futures import ThreadPoolExecutor
from importlib.metadata import entry_points
from scraper.base import BaseScraper


logger = logging.getLogger(__name__)


def configure_logging():
    """Configure root logger for CLI usage."""
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
        stream=sys.stdout,
    )

def load_scrapers():
    """Load all scraper classes from registered entry points."""
    scrapers = []

    eps = entry_points(group='dist_scraper.scrapers')
    for ep in eps:
        logger.info("Loading scraper plugin: %s", ep.name)
        scraper_class = ep.load()
        if isinstance(scraper_class, type) and issubclass(scraper_class, BaseScraper):
            scrapers.append(scraper_class())
        else:
            logger.warning("Entry point %s did not provide a valid scraper class", ep.name)

    return scrapers


def write_output_file(output, path: pathlib.Path):
    """Write JSON output to the given path, creating parent directories as needed."""
    path.parent.mkdir(parents=True, exist_ok=True)
    json_str = json.dumps(output, indent=2, sort_keys=True) + "\n"
    path.write_text(json_str)
    logger.info("Output written to %s", path)

async def run_scraper(scraper_instance: BaseScraper, executor: ThreadPoolExecutor):
    """Run a single scraper.fetch in the provided executor and capture exceptions.

    Returns (name, result_or_none, error_message_or_none).
    """
    loop = asyncio.get_event_loop()
    name = scraper_instance.name
    try:
        result = await loop.run_in_executor(executor, scraper_instance.fetch)
        logger.info("Scraper %s succeeded", name)
        return name, result, None
    except Exception as e:
        logger.exception("Scraper %s failed", name)
        return name, None, str(e)


async def run_all_scrapers(output_file: pathlib.Path):
    """Run all registered scrapers concurrently and write output."""
    scrapers = load_scrapers()
    output = {}
    errors = {}

    # Run scrapers concurrently using a thread pool
    with ThreadPoolExecutor(max_workers=len(scrapers)) as executor:
        tasks = [run_scraper(s, executor) for s in scrapers]
        completed = await asyncio.gather(*tasks)

    # Populate output and errors
    for name, data, error in completed:
        if error:
            errors[name] = error
        elif data:
            output[name] = data

    # Only include errors if there are any
    if errors:
        output["errors"] = errors
        for name, err in errors.items():
            print(f"- {name}: {err}")
        # Write partial output for debugging
        print(f"Output:\n{json.dumps(output, indent=2, sort_keys=True)}")
        sys.exit(1)

    # Write final JSON output
    write_output_file(output, output_file)
    logger.info("All scrapers succeeded.")


def main():
    parser = argparse.ArgumentParser(
        description="Scrape distribution information from various Linux distributions"
    )
    parser.add_argument(
        "output_file",
        type=pathlib.Path,
        help="Path to the output JSON file"
    )
    args = parser.parse_args()

    configure_logging()

    try:
        asyncio.run(run_all_scrapers(args.output_file))
    except KeyboardInterrupt:
        logger.info("Interrupted by user")
        sys.exit(130)
