#!/usr/bin/env python3

import argparse
import copy
import os
import pprint

import requests

from github import Github


GITHUB_ENDPOINT = "https://api.github.com/graphql"
GITHUB_TOKEN = os.environ["GITHUB_TOKEN"]
GITHUB_HEADERS = {
    "Authorization": "Bearer {}".format(GITHUB_TOKEN),
    "Accept": "application/vnd.github.queen-beryl-preview+json",
}
COMMENT_TYPES = ("IssueComment", "CommitComment")
EVENT_COUNT = 50


def dict_merge(a, b):
    """recursively merges dict's. not just simple a['key'] = b['key'], if
    both a and b have a key whose value is a dict then dict_merge is called
    on both values and the result stored in the returned dictionary."""
    if not isinstance(b, dict):
        return b
    result = copy.deepcopy(a)
    for k, v in b.items():
        if k in result and isinstance(result[k], dict):
            result[k] = dict_merge(result[k], v)
        else:
            result[k] = copy.deepcopy(v)
    return result


class GraphQLQuery:
    api_endpoint = None
    data = None
    headers = {}
    query = None
    request = None
    variables = None

    def __init__(self, variables={}):
        self.variables = variables

    def run(self, variables={}):
        if None in (self.api_endpoint, self.query):
            raise Exception("You need to subclass GraphQLQuery and provide the 'query' and 'api_endpoint' attributes")

        self.request = requests.post(
            self.api_endpoint,
            headers=self.headers,
            json={"query": self.query, "variables": dict_merge(variables, self.variables)},
        )

        if self.request.status_code != 200:
            raise Exception(
                "Query failed with status code {}:\n{}".format(
                    self.request.status_code, self.request.content.decode("utf-8")
                )
            )
        elif "errors" in self.request.json():
            raise Exception("Query failed with errors:\n{}".format(pprint.pformat(self.request.json())))
        else:
            self.data = self.request.json()["data"]
            return self


class GitHubQLQuery(GraphQLQuery):
    api_endpoint = GITHUB_ENDPOINT
    headers = GITHUB_HEADERS


class GetEvents(GitHubQLQuery):
    query = """
        query GetEvents($owner: String!,
                        $name: String!,
                        $pull_request: Int!,
                        $count: Int!) {
          repository(owner: $owner, name: $name) {
            pullRequest(number: $pull_request) {
              id
              timelineItems(last: $count, itemTypes: [
                BASE_REF_CHANGED_EVENT,
                BASE_REF_FORCE_PUSHED_EVENT,
                HEAD_REF_FORCE_PUSHED_EVENT,
                ISSUE_COMMENT,
                PULL_REQUEST_COMMIT
              ]) {
                edges {
                  node {
                    __typename
                    ... on IssueComment {
                      author {
                        login
                      }
                      body
                      databaseId
                      id
                      isMinimized
                      viewerCanMinimize
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

    @property
    def pull_request(self):
        return self.data["repository"]["pullRequest"]

    @property
    def events(self):
        return self.pull_request["timelineItems"]["edges"]


class GetCommitComments(GitHubQLQuery):
    query = """
        query GetCommitComments($owner: String!,
                                $name: String!,
                                $branch: String = null,
                                $commit: String!,
                                $count: Int = 10) {
        repository(owner: $owner, name: $name) {
          ref(qualifiedName: $branch) {
            associatedPullRequests(states: OPEN, last: 1) {
              edges {
                node {
                  id
                  number
                }
              }
            }
          }
          object(expression: $commit) {
            id
            ... on Commit {
              parents(last: 1) {
                edges {
                  node {
                    associatedPullRequests(last: 1) {
                      edges {
                        node {
                          id
                          number
                        }
                      }
                    }
                  }
                }
              }
              comments(last: $count) {
                edges {
                  node {
                    __typename
                    author {
                      login
                    }
                    body
                    databaseId
                    id
                    isMinimized
                    viewerCanMinimize
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

    @property
    def ref(self):
        return self.data["repository"]["ref"]

    @property
    def pull_request(self):
        try:
            return self.ref["associatedPullRequests"]["edges"][0]["node"]
        except (TypeError, IndexError):
            return None

    @property
    def parent_pull_request(self):
        try:
            return self.data["repository"]["object"]["parents"]["edges"][0]["node"]["associatedPullRequests"]["edges"][
                0
            ]["node"]
        except IndexError:
            return None

    @property
    def events(self):
        return self.data["repository"]["object"]["comments"]["edges"]


class AddComment(GitHubQLQuery):
    query = """
        mutation AddComment($comment: AddCommentInput!) {
          addComment(input: $comment) {
            commentEdge {
              node {
                url
              }
            }
          }
        }
    """

    @property
    def url(self):
        return self.data["addComment"]["commentEdge"]["node"]["url"]


class UpdateComment(GitHubQLQuery):
    query = """
        mutation UpdateComment($comment: UpdateIssueCommentInput!) {
          updateIssueComment(input: $comment) {
            issueComment {
              url
            }
          }
        }
    """

    @property
    def url(self):
        return self.data["updateIssueComment"]["issueComment"]["url"]


class MinimizeComment(GitHubQLQuery):
    query = """
        mutation MinimizeComment($input: MinimizeCommentInput!) {
          minimizeComment(input: $input) {
            minimizedComment {
              ...on IssueComment {
                url
              }
              ...on CommitComment {
                url
              }
            }
          }
        }
    """

    @property
    def url(self):
        return self.data["minimizeComment"]["minimizedComment"]["url"]


class GitHubV3Call:
    variables = {}

    def __init__(self, variables={}):
        self.github = Github(GITHUB_TOKEN)
        self.variables = variables

    def run(self, variables={}):
        raise NotImplementedError("You need to subclass GitHubV3Call and provide the run(self, variables={}) method")

    def _vars(self, variables):
        return dict_merge(self.variables, variables)


class RepoCall(GitHubV3Call):
    def run(self, variables={}):
        self._repo = self.github.get_repo("{owner}/{name}".format(**self._vars(variables)))
        return self


class CommentURLMixin:
    @property
    def url(self):
        return self.data.html_url


class AddCommitComment(RepoCall, CommentURLMixin):
    def run(self, variables={}):
        super().run(variables)
        self.data = self._repo.get_commit(self._vars(variables)["sha"]).create_comment(
            self._vars(variables)["comment"]["body"]
        )
        return self


class UpdateCommitComment(RepoCall, CommentURLMixin):
    def run(self, variables={}):
        super().run(variables)
        self.data = self._repo.get_comment(self._vars(variables)["databaseId"])
        self.data.edit(self._vars(variables)["comment"]["body"])
        self.data.update()
        return self


class ArgParser(argparse.ArgumentParser):
    def __init__(self):
        super().__init__(description="Report build availability to GitHub")
        target = self.add_mutually_exclusive_group()
        target.add_argument(
            "-b",
            "--branch",
            help="report on the last PR associated with the given branch, rather than on the commit being built."
            " Falls back to the commit if a PR is not found",
        )
        target.add_argument(
            "-p",
            "--parent",
            action="store_true",
            help="similar to --branch, but look up the last pull request of the last parent (i.e. the branch that got"
            " merged into the current commit)",
        )
        self.add_argument(
            "build_type", help='a short string describing the build type (e.g. "Snap",' ' "macOS", "Windows")'
        )
        self.add_argument(
            "body",
            nargs="+",
            help="Markdown-formatted string(s) for each individual publishing (e.g. a command to install, a URL)",
        )


def main():
    parser = ArgParser()
    args = parser.parse_args()

    comment_body = ["{} build available: {}".format(args.build_type, " or ".join(args.body))]

    owner, name = os.environ["TRAVIS_REPO_SLUG"].split("/")
    travis_event = os.environ["TRAVIS_EVENT_TYPE"]

    common_d = {
        "owner": owner,
        "name": name,
        "count": EVENT_COUNT,
    }

    if travis_event == "pull_request":
        events_o = GetEvents(common_d).run({"pull_request": int(os.environ["TRAVIS_PULL_REQUEST"]),})
        add_comment = AddComment(dict(common_d, **{"comment": {"subjectId": events_o.pull_request["id"]}}))
        update_comment = UpdateComment()
        events = events_o.events

    elif travis_event == "push":
        events_o = GetCommitComments(common_d).run(
            {"branch": args.branch and f"refs/heads/{args.branch}", "commit": os.environ["TRAVIS_COMMIT"],}
        )

        # if there's an open pull request associated
        # with the given branch or parent
        pull_request = args.branch and events_o.pull_request or args.parent and events_o.parent_pull_request

        if pull_request:
            add_comment = AddComment(dict(common_d, **{"comment": {"subjectId": pull_request["id"]}}))
            update_comment = UpdateComment()
            events = GetEvents(common_d).run({"pull_request": pull_request["number"]}).events
        # otherwise report on the commit
        else:
            add_comment = AddCommitComment(dict(common_d, **{"sha": os.environ["TRAVIS_COMMIT"]}))
            update_comment = UpdateCommitComment(common_d)
            events = events_o.events
    else:
        raise Exception("This tool needs to run in a Travis PR or a branch build")

    viewer = events_o.data["viewer"]

    for event in reversed(events):
        if event["node"]["__typename"] not in COMMENT_TYPES:
            break

        if event["node"]["author"]["login"] == viewer["login"] and event["node"]["viewerCanUpdate"]:
            # found a recent commit we can update
            for line in event["node"]["body"].splitlines():
                # include all builds with a different build_type
                if not line.startswith(f"{args.build_type} "):
                    comment_body.append(line)

            comment_body.sort(key=lambda v: (v.upper(), v))

            comment = update_comment.run(
                {
                    "comment": {"id": event["node"]["id"], "body": "\n".join(comment_body),},
                    "databaseId": event["node"]["databaseId"],
                }
            )
            print(f"Updated comment: {comment.url}")

            return

    for event in events:
        if event["node"]["__typename"] not in COMMENT_TYPES:
            continue

        if (
            event["node"]["author"]["login"] == viewer["login"]
            and not event["node"]["isMinimized"]
            and event["node"]["viewerCanMinimize"]
        ):
            MinimizeComment().run({"input": {"subjectId": event["node"]["id"], "classifier": "OUTDATED",}})

    comment = add_comment.run({"comment": {"body": "\n".join(comment_body),}})

    print(f"Added comment: {comment.url}")


if __name__ == "__main__":
    main()
