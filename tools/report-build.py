#!/usr/bin/env python3

import os
import pprint
import requests
import sys

HEADERS = {"Authorization": "Bearer {}".format(os.environ["GITHUB_TOKEN"])}


def run_query(query, variables={}):
    request = requests.post('https://api.github.com/graphql',
                            json={'query': query, 'variables': variables},
                            headers=HEADERS)
    if request.status_code != 200:
        raise Exception("Query failed with status code {}:\n{}".format(
            request.status_code, request.content.decode("utf-8")
        ))
    elif "errors" in request.json():
        raise Exception("Query failed with errors:\n{}".format(
            pprint.pformat(request.json())
        ))
    else:
        return request.json()


pull_q = """
query GetPullRequest($owner:String!, $name:String!, $number:Int!) {
  repository(owner: $owner, name: $name) {
    pullRequest(number: $number) {
      id
    }
  }
}
"""

comment_m = """
mutation AddComment($comment: AddCommentInput!) {
  addComment(input: $comment) {
    subject {
      id
    }
  }
}
"""


def main():
    comment_body = "{} build available: [{}]({})".format(
        build_id, build_url.split("/")[-1], build_url)

    pull_request = run_query(pull_q, {
        "owner": os.environ["TRAVIS_REPO_SLUG"].split("/")[0],
        "name": os.environ["TRAVIS_REPO_SLUG"].split("/")[1],
        "number": int(os.environ["TRAVIS_PULL_REQUEST"]),
    })["data"]["repository"]["pullRequest"]

    run_query(comment_m, {
        "comment": {
            "subjectId": pull_request["id"],
            "body": comment_body
        }
    })


if __name__ == "__main__":
    main()
