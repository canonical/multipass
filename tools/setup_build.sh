if [[ $_ == $0 ]]; then
  echo "This needs to be sourced!"

else
  # tag all PR builds with their number
  if [ "${TRAVIS_EVENT_TYPE}" == "pull_request" ]; then
    MULTIPASS_BUILD_LABEL="pr${TRAVIS_PULL_REQUEST}"

  # and all other non-branch builds with their Travis job number
  elif [ "${TRAVIS_EVENT_TYPE}" != "push" ]; then
    MULTIPASS_BUILD_LABEL="ci${TRAVIS_JOB_NUMBER}"
  fi

  # can't publish anything without credentials
  if [ "${TRAVIS_SECURE_ENV_VARS}" == "true" ]; then

    # when it's a release branch or tag
    if [[ "${TRAVIS_BRANCH}" =~ ${MULTIPASS_RELEASE_BRANCH_PATTERN} \
          || "${TRAVIS_PULL_REQUEST_BRANCH}" =~ ${MULTIPASS_RELEASE_BRANCH_PATTERN} \
          || "${TRAVIS_BRANCH}" =~ ${MULTIPASS_RELEASE_TAG_PATTERN} ]]; then

      # only publish pushes, on candidate/*
      if [ "${TRAVIS_EVENT_TYPE}" == "push" ]; then
        MULTIPASS_PUBLISH="true"
        MULTIPASS_SNAP_CHANNEL="candidate/${BASH_REMATCH[1]}"

        # but report on the PR if it's a release branch
        if [[ "${TRAVIS_BRANCH}" == release/* ]]; then
          MULTIPASS_REPORT_OPTIONS+=("--branch" "${TRAVIS_BRANCH}")
        fi
      fi

    # other branches publish on the commits by default
    # skip staging as the same commit will be published from master if all is good
    elif [[ "${TRAVIS_EVENT_TYPE}" == "push" \
            && "${TRAVIS_BRANCH}" != "staging" ]]; then
      MULTIPASS_PUBLISH="true"

      # except for `trying`, which publishes on the parent's PR
      if [[ "${TRAVIS_BRANCH}" == "trying" \
            && "${TRAVIS_COMMIT_MESSAGE}" =~ ^Try\ #([0-9]+): ]]; then
        MULTIPASS_BUILD_LABEL="pr${BASH_REMATCH[1]}"
        MULTIPASS_SNAP_CHANNEL="edge/${MULTIPASS_BUILD_LABEL}"
        MULTIPASS_REPORT_OPTIONS+=("--parent")
      fi

    # publish all other PRs on edge/pr*
    elif [ "${TRAVIS_EVENT_TYPE}" == "pull_request" ]; then
      MULTIPASS_PUBLISH="true"
      MULTIPASS_SNAP_CHANNEL="edge/${MULTIPASS_BUILD_LABEL}"
    fi
  fi

  echo MULTIPASS_BUILD_LABEL=${MULTIPASS_BUILD_LABEL}
  echo MULTIPASS_PUBLISH=${MULTIPASS_PUBLISH}
  echo MULTIPASS_SNAP_CHANNEL=${MULTIPASS_SNAP_CHANNEL}
  echo MULTIPASS_REPORT_OPTIONS=${MULTIPASS_REPORT_OPTIONS}

fi
