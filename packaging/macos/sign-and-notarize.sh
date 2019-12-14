#!/usr/bin/env bash
set -eu

# Open installer package, codesign its contents, then signs and notarizes it

### check that we have all required tools
for i in codesign pkgbuild productsign xcrun xmllint ; do
    if [ ! -x "$(which ${i})" ]; then
        echo "Unable to execute command $i, cannot continue"
        exit 1
    fi
done

# handle arguments
POSITIONAL=()

function help_and_exit {
    echo "Usage:"
    echo " $(basename "$0") --app-signer <identity> --installer-signer <identity> --notarize-id <apple-id> --notarize-password <password> PKGFILE"
    echo ""
    echo "This utility takes a MacOS installer package to expands it, codesign its contents, repack and sign."
    echo "It can also submit the signed package to Apple's notarization facility, retrieve the result and if successful, update the package."
    echo ""
    echo "Argument details are as follows:"
    echo "    --app-signer <identity> --installer-signer <identity>"
    echo "These identities are SHA1 keys identifying cert+private-key pairs provided by Apple. Use 'security find-identity -v' to see which you have available"
    echo "Note there are separate identities for application signing and installer signing"
    echo ""
    echo "Optional notarization requires:"
    echo "    --notarize-id <apple-id> --notarize-password <password>"
    echo "These are your Apple ID and the app-specific password for 'altool' - refer to this doc for details:"
    echo "https://developer.apple.com/documentation/security/notarizing_your_app_before_distribution/customizing_the_notarization_workflow "
    exit 1
}

while [[ $# -gt 0 ]]; do
    key=$1
    case $key in
    --app-signer)
        SIGN_APP="$2"
        shift 2
        ;;
    --installer-signer)
        SIGN_PKG="$2"
        shift 2
        ;;
    --notarize-id)
        NOTARIZE_ID="$2"
        shift 2
        ;;
    --notarize-password)
        NOTARIZE_PASSWORD="$2"
        shift 2
        ;;
    -h|--help)
        help_and_exit
        ;;
    *) # unknown option
        POSITIONAL+=("$1")
        shift
        ;;
    esac
done

set -- "${POSITIONAL[@]}" # restore positional parameters

if [[ ${#POSITIONAL[@]} != 1 ]]; then
    help_and_exit
fi
PKGFILE="$1"


if [ -z "${SIGN_APP+x}" ]; then
    echo "Missing --app-signer argument"
    help_and_exit
fi

if [ -z "${SIGN_PKG+x}" ]; then
    echo "Missing --installer-signer argument"
    help_and_exit
fi

if [ ! -f "${PKGFILE}" ]; then
    echo "${PKGFILE}: file not found"
    exit 1
fi 

function check_already_signed
{
    PKG="$1"
    spctl --assess --type install "$PKG" &> /dev/null
    status=$?
    return ${status}
}

if check_already_signed "${PKGFILE}"; then
    echo "${PKGFILE} is already signed, aborting"
    exit 1
fi


PKGFILENAME=$(basename "${PKGFILE}")
if [ -f "${PKGFILENAME}" ]; then
    echo "Making backup copy of ${PKGFILE}"
    cp -v "${PKGFILE}" "${PKGFILENAME}.orig"
fi

function sign_installer {
    PKG="$1"
    mv "$PKG" "tmp.${PKG}"
    productsign --sign "${SIGN_PKG}" "tmp.${PKG}" "$PKG"
    rm "tmp.${PKG}"
}

function codesign_binaries {
    DIR="$1"
    # sign every file in the directory
    find "${DIR}" -type f -not -name hyperkit -exec \
        codesign -v --timestamp --options runtime --force --strict \
            --prefix com.canonical.multipass. \
            --sign "${SIGN_APP}" "{}" \;
}



WORKDIR=$(mktemp -d)
function clean_workdir
{
    rm -rf "${WORKDIR}"
}
trap clean_workdir EXIT

echo "Work directory: ${WORKDIR}"
PKG_ROOT="${WORKDIR}/root"

# Extract main package
pkgutil --expand "${PKGFILE}" "${PKG_ROOT}"

# For each component in the package, extract their Payloads
pushd "${PKG_ROOT}"
for i in *.pkg ; do
    mkdir "${WORKDIR}/${i}"
    tar xzvpf "${i}/Payload" -C "${WORKDIR}/${i}"

    codesign_binaries "${WORKDIR}/${i}"

    rm "${i}/Payload"
    tar -czv --format cpio -f "${i}/Payload" -C "${WORKDIR}/${i}" .
done
popd

# Compress final result back into package and sign
pkgutil --flatten "${PKG_ROOT}" "${PKGFILENAME}"

sign_installer "${PKGFILENAME}"

echo "Signed install package: ${PKGFILENAME}"


####
#### Notarization ######
####

if [ -z "${NOTARIZE_ID+x}" ] || [ -z "${NOTARIZE_PASSWORD+x}" ]; then
    echo "Required Notarization credentials not supplied (--notarize-id, --notarize-password), bailing"
    exit 2
fi


# Extract necessary metadata from pkg
TITLE=$(xmllint --xpath "string(//title)" "${PKG_ROOT}/Distribution")
VERSION=$(xmllint --xpath "string(//product/@version)" "${PKG_ROOT}/Distribution")
echo "Title: '${TITLE}'" 
echo "Version: '${VERSION}'"

# Generate a unique bundle id for this submission (replacing + with -)
BUNDLE_ID="${TITLE}.${VERSION//+/-}.$(date +%s)"

# send notarization
echo -n "Sending ${PKGFILENAME} for notarization..."
_tmpout=$(mktemp)

set -x
xcrun altool --notarize-app -f "${PKGFILENAME}" \
             --primary-bundle-id "${BUNDLE_ID}" \
             --username "${NOTARIZE_ID}" \
             --password "${NOTARIZE_PASSWORD}" >${_tmpout} 2>&1
set +x

# check the request uuid
_requuid=$(cat "${_tmpout}" | grep "RequestUUID" | awk '{ print $3 }')
echo "RequestUUID: ${_requuid}"

if [ -z "${_requuid}" ]; then
    echo "There was an error:"
    echo "==================================================================="
    cat "${_tmpout}"
    echo "==================================================================="
    echo "Error getting RequestUUID, notarization unsuccessful"
    exit 3
fi

echo "Waiting for notarization to be complete (this could take up to an hour, depending on Apple's servers).."

function print_tasks
{
    echo "Waiting cancelled!"
    echo ""
    echo "To manually monitor notarization process with"
    echo "    xcrun altool --notarization-info '${_requuid}' --username '${NOTARIZE_ID}' --password '${NOTARIZE_PASSWORD}'"
    echo "and if successful, staple the notarization to the package with"
    echo "    xcrun stapler staple -v '${PKGFILENAME}'"
}
trap print_tasks SIGINT


for c in {80..0}; do
    sleep 60
    xcrun altool --notarization-info "${_requuid}" \
                 --username "${NOTARIZE_ID}" \
                 --password "${NOTARIZE_PASSWORD}" >${_tmpout} 2>&1
    _status=$(cat "${_tmpout}" | grep "Status:" | awk '{ print $2 }')
    if [ "${_status}" == "invalid" ]; then
        echo "Error: Got invalid notarization!"
        echo "==================================================================="
        cat "${_tmpout}"
        echo "==================================================================="
        exit 4
    fi

    if [ "${_status}" == "success" ]; then
        echo -n "Notarization successful! Stapling..."
        xcrun stapler staple -v "${PKGFILENAME}"
        break
    fi
    echo "Notarization in progress, waiting..."
done

# Verifying notarized
if ! xcrun stapler validate "${PKGFILENAME}" | grep worked ; then 
    echo "Error: final package verification failed";
    exit 5
fi

echo "..done. ${PKGFILENAME} is notarized and ready to upload"
