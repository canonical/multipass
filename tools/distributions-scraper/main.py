import importlib
import pkgutil
import asyncio
import json
import pathlib
import sys
import logging
from concurrent.futures import ThreadPoolExecutor
from scraper.base import BaseScraper
import scraper


DEFAULT_OUTPUT_FILE = (
    pathlib.Path(__file__).resolve().parent.parent.parent
    / "data"
    / "distributions"
    / "distribution-info.json"
)

logger = logging.getLogger(__name__)


def configure_logging(level: int = logging.INFO):
    """Configure root logger for CLI usage."""
    logging.basicConfig(
        level=level,
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
        stream=sys.stdout,
    )

def load_scrapers():
    """Dynamically load all scraper classes from scraper/ package."""
    scrapers = []
    for _, module_name, _ in pkgutil.iter_modules(scraper.__path__):
        if module_name == "base":
            continue

        module = importlib.import_module(f"scraper.{module_name}")
        for attr_name in dir(module):
            obj = getattr(module, attr_name)
            if isinstance(obj, type) and issubclass(obj, BaseScraper) and obj is not BaseScraper:
                scrapers.append(obj())
    return scrapers


def write_output_file(output, path = DEFAULT_OUTPUT_FILE):
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


async def main():
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
    write_output_file(output)
    logger.info("All scrapers succeeded.")


if __name__ == "__main__":
    configure_logging()
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        logging.getLogger(__name__).info("Interrupted by user")
        raise
