#!/usr/bin/env python3

import os
import pprint
import requests
import sys

GITHUB_ENDPOINT = "https://api.github.com/graphql"
GITHUB_HEADERS = {
    "Authorization": "Bearer {}".format(os.environ["GITHUB_TOKEN"])
}
EVENT_COUNT = 10


class GraphQLQuery():
    api_endpoint = None
    headers = {}
    query = None
    request = None

    def run(self, variables={}):
        if None in (self.api_endpoint, self.query):
            raise Exception(
                "You need to subclass GraphQLQuery and provide the"
                " 'query' and 'api_endpoint' attributes")

        self.request = requests.post(
            self.api_endpoint, headers=self.headers,
            json={'query': self.query, 'variables': variables})

        if self.request.status_code != 200:
            raise Exception("Query failed with status code {}:\n{}".format(
                self.request.status_code, self.request.content.decode("utf-8")
            ))
        elif "errors" in self.request.json():
            raise Exception("Query failed with errors:\n{}".format(
                pprint.pformat(self.request.json())
            ))
        else:
            return self.request.json()


class GitHubQLQuery(GraphQLQuery):
    api_endpoint = GITHUB_ENDPOINT
    headers = GITHUB_HEADERS


class GetEvents(GitHubQLQuery):
    query = """
        query GetEvents($owner: String!, $name: String!, $pull_request: Int!, $count: Int!) {
          repository(owner: $owner, name: $name) {
            pullRequest(number: $pull_request) {
              id
              timelineItems(last: $count, itemTypes: [PULL_REQUEST_COMMIT, ISSUE_COMMENT, HEAD_REF_FORCE_PUSHED_EVENT]) {
                edges {
                  node {
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


class AddComment(GitHubQLQuery):
    query = """
        mutation AddComment($comment: AddCommentInput!) {
          addComment(input: $comment) {
            subject {
              id
            }
          }
        }
    """


class UpdateComment(GitHubQLQuery):
    query = """
        mutation UpdateComment($comment: UpdateIssueCommentInput!) {
          updateIssueComment(input: $comment) {
            issueComment {
              id
            }
          }
        }
    """


def main():
    build_id = sys.argv[1]
    build_url = sys.argv[2]
    comment_body = ["{} build available: [{}]({})".format(
        build_id, build_url.split("/")[-1], build_url)]

    events_d = GetEvents().run({
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
            if(event["node"]["author"]["login"] == viewer["login"]
               and event["node"]["viewerCanUpdate"]):
                # found a recent commit we can update
                for line in event["node"]["body"].splitlines():
                    # include all builds with a different id
                    if not line.startswith(build_id):
                        comment_body.append(line)

                comment_body.sort(key=lambda v: (v.upper(), v))

                UpdateComment().run({
                    "comment": {
                        "id": event["node"]["id"],
                        "body": "\n".join(comment_body),
                    }
                })

                sys.exit(0)
        except KeyError:
            # we've encountered a commit or a force event,
            # break out and add a new comment below
            break

    AddComment().run({
        "comment": {
            "subjectId": pull_request["id"],
            "body": "\n".join(comment_body),
        }
    })


if __name__ == "__main__":
    main()
