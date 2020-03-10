#!/usr/bin/env python3
# coding: utf-8

import json
import logging
import multiprocessing
import os
import pathlib
import pprint
import sys
import subprocess
import tempfile

from launchpadlib import errors as lp_errors  # fades
from launchpadlib.credentials import RequestTokenAuthorizationEngine, UnencryptedFileCredentialStore
from launchpadlib.launchpad import Launchpad
import requests  # fades


logger = logging.getLogger("multipass.process_snaps")
logger.addHandler(logging.StreamHandler())
logger.setLevel(logging.INFO)

APPLICATION = "multipass-ci"
LAUNCHPAD = "production"
RELEASE = "bionic"
TEAM = "multipass-ci-bot"
SOURCE_NAME = "multipass"
SNAPS = {
    "multipass": {"candidate": {"recipe": "multipass-candidate"}},
}

STORE_URL = "https://api.snapcraft.io/api/v1/snaps" "/details/{snap}?channel={channel}"
STORE_HEADERS = {"X-Ubuntu-Series": "16", "X-Ubuntu-Architecture": "{arch}"}

CHECK_NOTICES_PATH = "/snap/bin/review-tools.check-notices"


def get_store_snap(processor, snap, channel):
    logger.debug("Checking for snap %s on %s in channel %s", snap, processor, channel)
    data = {
        "snap": snap,
        "channel": channel,
        "arch": processor,
    }
    resp = requests.get(STORE_URL.format(**data), headers={k: v.format(**data) for k, v in STORE_HEADERS.items()})
    logger.debug("Got store response: %s", resp)

    try:
        result = json.loads(resp.content)
    except json.JSONDecodeError:
        logger.error("Could not parse store response: %s", resp.content)
    else:
        return result


def fetch_url(entry):
    dest, uri = entry
    r = requests.get(uri, stream=True)
    logger.debug("Downloading %s to %s…", uri, dest)
    if r.status_code == 200:
        with open(dest, "wb") as f:
            for chunk in r:
                f.write(chunk)
    return dest


def check_snap_notices(store_snaps):
    with tempfile.TemporaryDirectory(dir=pathlib.Path.home()) as dir:
        snaps = multiprocessing.Pool(8).map(
            fetch_url,
            (
                (pathlib.Path(dir) / f"{snap['package_name']}_{snap['revision']}.snap", snap["download_url"])
                for snap in store_snaps
            ),
        )

        try:
            notices = subprocess.check_output([CHECK_NOTICES_PATH] + snaps, stderr=subprocess.STDOUT, encoding="UTF-8")
            logger.debug("Got check_notices output:\n%s", notices)
        except subprocess.CalledProcessError as e:
            logger.error("Failed to check notices:\n%s", e.output)
            sys.exit(2)
        else:
            notices = json.loads(notices)
            return notices


if __name__ == "__main__":
    check_notices = os.path.isfile(CHECK_NOTICES_PATH) and os.access(CHECK_NOTICES_PATH, os.X_OK) and CHECK_NOTICES_PATH

    if not check_notices:
        logger.info("`review-tools` not found, skipping USN checks…")
        sys.exit(0)

    try:
        lp = Launchpad.login_with(
            APPLICATION,
            LAUNCHPAD,
            version="devel",
            authorization_engine=RequestTokenAuthorizationEngine(LAUNCHPAD, APPLICATION),
            credential_store=UnencryptedFileCredentialStore(os.path.expanduser("~/.launchpadlib/credentials")),
        )
    except NotImplementedError:
        raise RuntimeError("Invalid credentials.")

    ubuntu = lp.distributions["ubuntu"]
    logger.debug("Got ubuntu: %s", ubuntu)

    team = lp.people[TEAM]
    logger.debug("Got team: %s", team)

    errors = []

    for snap, channels in SNAPS.items():
        for channel, snap_map in channels.items():
            logger.info("Processing channel %s for snap %s…", channel, snap)

            try:
                snap_recipe = lp.snaps.getByName(owner=team, name=snap_map["recipe"])
                logger.debug("Got snap: %s", snap_recipe)
            except lp_errors.NotFound as ex:
                logger.error("Snap not found: %s", snap_map["recipe"])
                errors.append(ex)
                continue

            if len(snap_recipe.pending_builds) > 0:
                logger.info("Skipping %s: snap builds pending…", snap_recipe.web_link)
                continue

            store_snaps = tuple(
                filter(
                    lambda snap: snap.get("result") != "error",
                    (get_store_snap(processor.name, snap, channel) for processor in snap_recipe.processors),
                )
            )

            logger.debug("Got store versions: %s", {snap["architecture"][0]: snap["version"] for snap in store_snaps})

            snap_notices = check_snap_notices(store_snaps)[snap]

            if any(snap_notices.values()):
                logger.info("Found USNs:\n%s", pprint.pformat(snap_notices))
            else:
                logger.info("Skipping %s: no USNs found", snap)
                continue

            logger.info("Triggering %s…", snap_recipe.description or snap_recipe.name)

            snap_recipe.requestBuilds(
                archive=snap_recipe.auto_build_archive,
                pocket=snap_recipe.auto_build_pocket,
                channels=snap_recipe.auto_build_channels,
            )
            logger.debug("Triggered builds: %s", snap_recipe.web_link)

    for error in errors:
        logger.debug(error)

    if errors:
        sys.exit(1)
