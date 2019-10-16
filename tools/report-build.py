#!/usr/bin/env python3

import os
import pprint
import requests
import sys

HEADERS = {"Authorization": "Bearer {}".format(os.environ["GITHUB_TOKEN"])}
EVENT_COUNT = 10


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


events_q = """
query GetEvents($owner: String!, $name: String!, $pull_request: Int!, $count: Int!) {
  repository(owner: $owner, name: $name) {
    pullRequest(number: $pull_request) {
      id
      timelineItems(last: $count, itemTypes: [PULL_REQUEST_COMMIT, ISSUE_COMMENT, HEAD_REF_FORCE_PUSHED_EVENT]) {
        edges {
          node {
            __typename
            ... on IssueComment {
              author {
                login
              }
              body
              id
              viewerCanUpdate
            }
          }
        }
      }
    }
  }
  viewer {
    login
  }
}
"""

add_comment_m = """
mutation AddComment($comment: AddCommentInput!) {
  addComment(input: $comment) {
    subject {
      id
    }
  }
}
"""

update_comment_m = """
mutation UpdateComment($comment: UpdateIssueCommentInput!) {
  updateIssueComment(input: $comment) {
    issueComment {
      id
    }
  }
}
"""


def main():
    previous_comment = None
    build_id = sys.argv[1]
    build_url = sys.argv[2]
    comment_body = ["{} build available: [{}]({})".format(
        build_id, build_url.split("/")[-1], build_url)]

    events_d = run_query(events_q, {
        "owner": os.environ["TRAVIS_REPO_SLUG"].split("/")[0],
        "name": os.environ["TRAVIS_REPO_SLUG"].split("/")[1],
        "pull_request": int(os.environ["TRAVIS_PULL_REQUEST"]),
        "count": EVENT_COUNT
    })["data"]

    viewer = events_d["viewer"]
    pull_request = events_d["repository"]["pullRequest"]
    events = pull_request["timelineItems"]["edges"]

    for event in reversed(events):
        try:
            if event["node"]["author"]["login"] == viewer["login"] and event["node"]["viewerCanUpdate"]:
                # found a recent commit we can update
                for line in event["node"]["body"].splitlines():
                    # include all builds with a different id
                    if not line.startswith(build_id):
                        comment_body.append(line)

                comment_body.sort(key=lambda v: (v.upper(), v))

                run_query(update_comment_m, {
                    "comment": {
                        "id": event["node"]["id"],
                        "body": "\n".join(comment_body),
                    }
                })

                sys.exit(0)
        except KeyError:
            # we've encountered a commit or a force event, add a new comment below
            break

    run_query(add_comment_m, {
        "comment": {
            "subjectId": pull_request["id"],
            "body": "\n".join(comment_body),
        }
    })


if __name__ == "__main__":
    main()
