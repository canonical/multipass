#! /usr/bin/env python

# Initial update script for the starter pack.
#
# Requires some manual intervention, but makes identifying updates and differences easier.
#
# For debugging, please run this script with DEBUGGING=1
# e.g. user@device:~/git/Canonical/sphinx-docs-starter-pack/docs$ DEBUGGING=1 python .sphinx/update_sp.py


import glob
import logging
import os
import requests
import re
import subprocess
import sys
from requests.exceptions import RequestException
from packaging.version import parse as parse_version

SPHINX_DIR = os.path.abspath(os.path.dirname(__file__))
DOCS_DIR = os.path.abspath(os.path.join(SPHINX_DIR, '..'))
REQUIREMENTS = os.path.join(DOCS_DIR, "requirements.txt")
SPHINX_UPDATE_DIR = os.path.join(SPHINX_DIR, "update")
GITHUB_REPO = "canonical/sphinx-docs-starter-pack"
GITHUB_API_BASE = f"https://api.github.com/repos/{GITHUB_REPO}"
GITHUB_API_SPHINX_DIR = f"{GITHUB_API_BASE}/contents/docs/.sphinx"
GITHUB_RAW_BASE = f"https://raw.githubusercontent.com/{GITHUB_REPO}/main"

TIMEOUT = 10  # seconds

# Check if debugging
if os.getenv("DEBUGGING"):
    logging.basicConfig(level=logging.DEBUG)


def main():
    # Check local version
    logging.debug("Checking local version")
    try:
        with open(os.path.join(SPHINX_DIR, "version")) as f:
            local_version = f.read().strip()
    except FileNotFoundError:
        print("WARNING\nWARNING\nWARNING")
        print(
            "You need to update to at least version 1.0.0 of the starter pack to start using the update function."
        )
        print("You may experience issues using this functionality.")
        logging.debug("No local version found. Setting version to None")
        local_version = "None"
    except Exception as e:
        logging.debug(e)
        raise Exception("ERROR executing check local version")
    logging.debug(f"Local version = {local_version}")

    # Check release version
    latest_release = query_api(GITHUB_API_BASE + "/releases/latest").json()["tag_name"]
    logging.debug(f"Latest release = {latest_release}")

    # Perform actions only if local version is older than release version
    logging.debug("Comparing versions")
    if parse_version(local_version) < parse_version(latest_release):
        logging.debug("Local version is older than the release version.")
        print("Starter pack is out of date.\n")

        # Identify and download '.sphinx' dir files to '.sphinx/update'
        files_updated, new_files = update_static_files()

        # Write new version to file to '.sphinx/update'

        download_file(
            GITHUB_RAW_BASE + "/docs/.sphinx/version",
            os.path.join(SPHINX_UPDATE_DIR, "version"),
        )

        # Provide changelog to identify other significant changes
        changelog = query_api(GITHUB_RAW_BASE + "/CHANGELOG.md")
        logging.debug("Changelog obtained")
        version_regex = re.compile(r"#+ +" + re.escape(local_version) + r" *\n")
        print("SEE CURRENT CHANGELOG:")
        print(re.split(version_regex, changelog.text)[0])

        # Provide information on any files identified for updates
        if files_updated:
            logging.debug("Updated files found and downloaded")
            print("Differences have been identified in static files.")
            print("Updated files have been downloaded to '.sphinx/update'.")
            print("Validate and move these files into your '.sphinx/' directory.")
        else:
            logging.debug("No files found to update")
        # Provide information on NEW files
        if new_files:
            logging.debug("New files found and downloaded")
            print(
                "NOTE: New files have been downloaded\n",
                "See 'NEWFILES.txt' for all downloaded files\n",
                "Validate and merge these files into your '.sphinx/' directory",
            )
        else:
            logging.debug("No new files found to download")
    else:
        logging.debug("Local version and release version are the same")
        print("This version is up to date.")

    # Check requirements are the same
    new_requirements = []
    try:
        with open(REQUIREMENTS, "r") as file:
            logging.debug("Checking requirements")

            local_reqs = set(file.read().splitlines()) - {""}
            requirements = set(
                query_api(GITHUB_RAW_BASE + "/docs/requirements.txt").text.splitlines()
            )

            new_requirements = requirements - local_reqs

            for req in new_requirements:
                logging.debug(f"{req} not found in local requirements.txt")

            for req in requirements & local_reqs:
                logging.debug(f"{req} already exists in local requirements.txt")

            if new_requirements != set():
                print(
                    "You may need to add the following packages to your requirements.txt file:"
                )
                for r in new_requirements:
                    print(f"{r}\n")
    except FileNotFoundError:
        print("requirements.txt not found")
        print(
            "The updated starter pack has moved requirements.txt out of the '.sphinx' dir"
        )
        print("requirements.txt not checked, please update your requirements manually")


def update_static_files():
    """Checks local files against remote for new and different files, downloads to '.sphinx/updates'"""
    files, paths = get_local_files_and_paths()
    new_file_list = []

    for item in query_api(GITHUB_API_SPHINX_DIR).json():
        logging.debug(f"Checking {item['name']}")
        # Checks existing files in '.sphinx' starter pack static root for changed SHA
        if item["name"] in files and item["type"] == "file":
            index = files.index(item["name"])
            if item["sha"] != get_git_revision_hash(paths[index]):
                logging.debug(f"Local {item['name']} is different to remote")
                download_file(
                    item["download_url"], os.path.join(SPHINX_UPDATE_DIR, item["name"])
                )
                if item["name"] == "update_sp.py":
                    # Indicate update script needs to be updated and re-run
                    print("WARNING")
                    print(
                        "THIS UPDATE SCRIPT IS OUT OF DATE. YOU MAY NEED TO RUN ANOTHER UPDATE AFTER UPDATING TO THE FILE IN '.sphinx/updates'."
                    )
                    print("WARNING\n")
            else:
                logging.debug("File hashes are equal")
        # Checks nested files '.sphinx/**/**.*' for changed SHA (single level of depth)
        elif item["type"] == "dir":
            logging.debug(item["name"] + " is a directory")
            for nested_item in query_api(
                f"{GITHUB_API_SPHINX_DIR}/{item['name']}"
            ).json():
                logging.debug(f"Checking {nested_item['name']}")
                if nested_item["name"] in files:
                    index = files.index(nested_item["name"])
                    if nested_item["sha"] != get_git_revision_hash(paths[index]):
                        logging.debug(
                            f"Local {nested_item['name']} is different to remote"
                        )
                        download_file(
                            nested_item["download_url"],
                            os.path.join(
                                SPHINX_UPDATE_DIR, item["name"], nested_item["name"]
                            ),
                        )
                # Downloads NEW nested files
                else:
                    logging.debug(f"No local version found of {nested_item['name']}")
                    if nested_item["type"] == "file":
                        new_file_list.append(nested_item["name"])
                        download_file(
                            nested_item["download_url"],
                            os.path.join(
                                SPHINX_UPDATE_DIR, item["name"], nested_item["name"]
                            ),
                        )
        # Downloads NEW files in '.sphinx' starter pack static root
        else:
            if item["type"] == "file":
                logging.debug(f"No local version found of {item['name']}")
                download_file(
                    item["download_url"], os.path.join(SPHINX_UPDATE_DIR, item["name"])
                )
                if item["name"] != "version":
                    new_file_list.append(item["name"])
    # Writes return value for parent function
    if os.path.exists(os.path.join(SPHINX_UPDATE_DIR)):
        logging.debug("Files have been downloaded")
        files_updated = True
    else:
        logging.debug("No downloads found")
        files_updated = False
    # Writes return value for parent function
    if new_file_list != []:
        # Provides more information on new files
        with open(f"{SPHINX_DIR}/NEWFILES.txt", "w") as f:
            for entry in new_file_list:
                f.write(f"{entry}\n")
        logging.debug("Some downloaded files are new")
        return files_updated, True
    return files_updated, False


# Checks git hash of a file
def get_git_revision_hash(file) -> str:
    """Get SHA of local files"""
    logging.debug(f"Getting hash of {os.path.basename(file)}")
    return subprocess.check_output(["git", "hash-object", file]).decode("ascii").strip()


# Examines local files
def get_local_files_and_paths():
    """Identify '.sphinx' local files and paths"""
    logging.debug("Checking local files and paths")
    try:
        files = []
        paths = []
        patterns = [".*", "**.*"]
        files, paths = [], []

        for pattern in patterns:
            for file in glob.iglob(os.path.join(SPHINX_DIR, pattern), recursive=True):
                files.append(os.path.basename(file))
                paths.append(file)
        return files, paths
    except Exception as e:
        logging.debug(e)
        raise RuntimeError("get_local_files_and_paths()") from e


# General API query with timeout and RequestException
def query_api(url):
    """Query an API with a globally set timeout"""
    logging.debug(f"Querying {url}")
    try:
        r = requests.get(url, timeout=TIMEOUT)
        return r
    except RequestException as e:
        raise RuntimeError(f"Failed query_api(): {url}") from e


# General file download function
def download_file(url, output_path):
    """Download a file to a specified path"""
    logging.debug(f"Downloading {os.path.basename(output_path)}")
    try:
        os.makedirs(os.path.dirname(output_path), exist_ok=True)
        with open(output_path, "wb") as file:
            file.write(query_api(url).content)
    except Exception as e:
        logging.debug(e)
        raise RuntimeError(f"Failed download_file(): {url}") from e


if __name__ == "__main__":
    sys.exit(main())  # Keep return code
