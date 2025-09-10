import importlib
import pkgutil
import asyncio
import json
import pathlib
import sys
from concurrent.futures import ThreadPoolExecutor
from scraper.base import BaseScraper
import scraper


OUTPUT_FILE = (
    pathlib.Path(__file__).resolve().parent.parent.parent
    / "data"
    / "distributions"
    / "distribution-info.json"
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


async def run_scraper(scraper_instance: BaseScraper, executor: ThreadPoolExecutor):
    loop = asyncio.get_event_loop()
    name = scraper_instance.name
    try:
        result = await loop.run_in_executor(executor, scraper_instance.fetch)
        return name, result, None
    except Exception as e:
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

    OUTPUT_FILE.parent.mkdir(parents=True, exist_ok=True)

    # Only include errors if there are any
    if errors:
        output["errors"] = errors
        print("❌ Scraper errors detected:")
        for name, err in errors.items():
            print(f"- {name}: {err}")
        # Write partial output for debugging
        print(f"Output:\n{json.dumps(output, indent=2, sort_keys=True)}")
        sys.exit(1)
    else:
        # Write final JSON output
        json_str = json.dumps(output, indent=2, sort_keys=True) + "\n"
        OUTPUT_FILE.write_text(json_str)
        print(f"All scrapers succeeded. JSON output written to {OUTPUT_FILE}")


if __name__ == "__main__":
    asyncio.run(main())
